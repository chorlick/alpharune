#include "training/self_play.h"
#include "training/heuristic_evaluator.h"
#include "training/replay_buffer.h"
#include "training/ring_logger.h"

#include "ml/libtorch_evaluator_with_model.h"
#include "ml/entity_tokens.h"
#include "ml/feature_extractor.h"

#include "openspiel/riftbound_game.h"
#include "openspiel/riftbound_state.h"

#include "open_spiel/spiel.h"
#include "open_spiel/algorithms/mcts.h"
#include "open_spiel/algorithms/evaluate_bots.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <memory>
#include <random>
#include <vector>

namespace riftbound::training {

namespace {

// Per-decision record kept around until the game finishes (so we know
// the outcome `z` to attach). The buffer doesn't see incomplete
// tuples — they're materialized in one shot at game end.
struct PendingTuple {
    // V2 path: entity-token snapshot. Empty when running V3.
    ::riftbound::ml::EntityTokens tokens;
    // V3 path: flat state-feature snapshot. Empty when running V2.
    std::vector<float>    flat_state;
    std::vector<int32_t>  legal_actions;
    std::vector<float>    policy_over_legal;
    int                   acting_player = 0;
};

// Why the game loop exited — needed to distinguish "real draw" from
// "engine stalled mid-game" when investigating short-decision draws.
enum class ExitReason : int {
    Terminal = 0,    // state.IsTerminal() became true (natural game end)
    DecisionCap = 1, // hit cfg.max_decisions cap (timeout draw)
    LegalEmpty = 2,  // action generator returned 0 legal actions mid-game
};
inline const char* exitTag(ExitReason r) {
    switch (r) {
        case ExitReason::Terminal:    return "term";
        case ExitReason::DecisionCap: return "cap";
        case ExitReason::LegalEmpty:  return "stuck";
    }
    return "?";
}

// Play one game with the given evaluator, recording (tokens, π, _) at
// every decision. After terminal, computes z per acting player and
// flushes everything to the buffer.
struct GameResult {
    int decisions = 0;
    std::vector<double> returns;
    ExitReason exit_reason = ExitReason::Terminal;
};

GameResult playOneGame(
    const ::open_spiel::Game& game,
    std::shared_ptr<::open_spiel::algorithms::Evaluator> evaluator,
    const SelfPlayConfig& cfg,
    std::mt19937& rng,
    ReplayBuffer& buffer,
    int iter,
    int game_idx)
{
    auto state = game.NewInitialState();
    auto& ring = threadRingLogger();
    {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "sims=%d max_dec=%d use_model=%d",
                      cfg.mcts_sims, cfg.max_decisions, (int)cfg.use_model);
        ring.record(iter, game_idx, 0, "GAME_START", buf);
    }

    // AlphaZero recipe: Dirichlet noise at MCTS root ensures exploration
    // even when the model's prior is confidently wrong. Without it,
    // self-play deterministically follows the prior → no diversity →
    // model overfits to one line.
    ::open_spiel::algorithms::MCTSBot bot(
        game, evaluator,
        cfg.uct_c, cfg.mcts_sims,
        /*max_memory_mb=*/256, /*solve=*/false,
        /*seed=*/static_cast<int>(rng()),
        /*verbose=*/false,
        ::open_spiel::algorithms::ChildSelectionPolicy::UCT,
        /*dirichlet_alpha=*/cfg.dirichlet_alpha,
        /*dirichlet_epsilon=*/cfg.dirichlet_fraction);

    std::vector<PendingTuple> pending;
    pending.reserve(static_cast<size_t>(cfg.max_decisions));

    // Cast once — RiftboundGame/State are the only types this driver
    // works with. Throws if the wrong game is passed (programmer error).
    const auto& rb_game = dynamic_cast<const ::riftbound::openspiel::RiftboundGame&>(game);

    int decisions = 0;
    ExitReason exit_reason = ExitReason::Terminal;
    while (true) {
        ring.touch();
        if (state->IsTerminal()) {
            exit_reason = ExitReason::Terminal;
            break;
        }
        if (decisions >= cfg.max_decisions) {
            exit_reason = ExitReason::DecisionCap;
            break;
        }
        auto legal = state->LegalActions();
        if (legal.empty()) {
            exit_reason = ExitReason::LegalEmpty;
            break;
        }
        const auto cp = state->CurrentPlayer();
        {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "cp=%d k=%zu", cp, legal.size());
            ring.record(iter, game_idx, decisions, "LEGAL_ACTIONS", buf);
        }

        // Capture state snapshot for the acting player BEFORE applying
        // the chosen action. Information-set masking is built into both
        // extractEntityTokens (V2 path) and extractStateFeatures (V3
        // path): opp hand + main_deck identities are zeroed/hidden when
        // perspective != owner.
        PendingTuple t;
        t.acting_player = cp;
        const auto& rb_state =
            dynamic_cast<const ::riftbound::openspiel::RiftboundState&>(*state);
        auto pid = (cp == 0) ? ::riftbound::PlayerId::Player1
                              : ::riftbound::PlayerId::Player2;
        if (cfg.model_kind == "v3") {
            t.flat_state = ::riftbound::ml::extractStateFeatures(
                rb_state.engineState(), pid, rb_game.cardDb());
        } else {
            t.tokens = ::riftbound::ml::extractEntityTokens(
                rb_state.engineState(), pid, rb_game.cardDb());
        }

        // Run MCTS — root.children[i] holds visit counts.
        ring.record(iter, game_idx, decisions, "MCTS_BEGIN", "");
        auto root = bot.MCTSearch(*state);
        ring.record(iter, game_idx, decisions, "MCTS_END", "");

        int64_t total_visits = 0;
        for (const auto& child : root->children) {
            total_visits += child.explore_count;
        }
        t.legal_actions.reserve(legal.size());
        t.policy_over_legal.reserve(legal.size());
        for (auto action : legal) {
            t.legal_actions.push_back(static_cast<int32_t>(action));
            float p = 0.0f;
            for (const auto& child : root->children) {
                if (child.action == action) {
                    p = (total_visits > 0)
                          ? static_cast<float>(child.explore_count)
                              / static_cast<float>(total_visits)
                          : 0.0f;
                    break;
                }
            }
            t.policy_over_legal.push_back(p);
        }
        pending.push_back(std::move(t));

        // AlphaZero recipe: temperature-sampled action selection during
        // self-play. τ=1 for the first N decisions samples proportional
        // to visit count → diverse openings; τ=0 after switches to
        // BestChild (argmax) for sharper midgame/endgame. Without this,
        // every self-play game with the same model picks identical
        // actions → tiny effective data diversity.
        const double tau = (decisions < cfg.temperature_drop_at_decision)
                             ? cfg.temperature : 0.0;
        ::open_spiel::Action act;
        if (tau <= 0.0 || total_visits == 0) {
            act = root->BestChild().action;
        } else {
            // Sample proportional to visit_count^(1/tau).
            std::vector<double> weights;
            weights.reserve(root->children.size());
            double sum = 0.0;
            for (const auto& child : root->children) {
                double w = std::pow(static_cast<double>(child.explore_count),
                                    1.0 / tau);
                weights.push_back(w);
                sum += w;
            }
            std::uniform_real_distribution<double> u(0.0, sum);
            double pick = u(rng);
            double cum = 0.0;
            act = root->children.front().action;
            for (size_t i = 0; i < root->children.size(); ++i) {
                cum += weights[i];
                if (pick <= cum) {
                    act = root->children[i].action;
                    break;
                }
            }
        }
        {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "act=%lld visits=%lld",
                          (long long)act, (long long)total_visits);
            ring.record(iter, game_idx, decisions, "INTENT_APPLIED", buf);
        }
        state->ApplyAction(act);
        ++decisions;
    }

    GameResult out;
    out.decisions = decisions;
    out.returns = state->Returns();
    out.exit_reason = exit_reason;
    {
        char buf[96];
        std::snprintf(buf, sizeof(buf), "exit=%s dec=%d p1=%.1f p2=%.1f",
                      exitTag(exit_reason), decisions,
                      out.returns.size() > 0 ? out.returns[0] : 0.0,
                      out.returns.size() > 1 ? out.returns[1] : 0.0);
        ring.record(iter, game_idx, decisions, "GAME_END", buf);
    }

    for (auto& t : pending) {
        ReplaySample s;
        s.tokens            = std::move(t.tokens);
        s.flat_state        = std::move(t.flat_state);
        s.legal_actions     = std::move(t.legal_actions);
        s.policy_over_legal = std::move(t.policy_over_legal);
        s.value = static_cast<float>(out.returns[t.acting_player]);
        buffer.push(std::move(s));
    }
    return out;
}

} // namespace

SelfPlayStats runSelfPlay(const ::open_spiel::Game& game,
                           ::riftbound::ml::V2Model model,
                           ReplayBuffer& buffer,
                           const SelfPlayConfig& cfg,
                           std::mt19937& rng,
                           int iter,
                           int game_idx)
{
    // Phase 6t — MCTS-bootstrap with HEURISTIC value evaluator.
    // Previously used RandomRolloutEvaluator (Phase 6q), which produced
    // ~0 value estimates on long mirror-match rollouts. With no
    // informative value signal, MCTS fell back entirely on the prior;
    // any small prior bias (slot-0 EndTurn) compounded across visits
    // and produced a degenerate "always EndTurn" policy by iter 6
    // (observed: 80% EndTurn picks vs random's 30%).
    //
    // HeuristicValueEvaluator uses score_differential / 8.0 as the
    // value signal — bounded [-1, +1], non-zero whenever scores
    // diverge. Even a small score lead pushes MCTS away from
    // EndTurn-spam toward actions that actually score. Prior is
    // uniform-over-legal to prevent action-id bias.
    std::shared_ptr<::open_spiel::algorithms::Evaluator> evaluator;
    if (cfg.use_model) {
        evaluator = ::riftbound::ml::makeEvaluatorFromModel(model);
    } else {
        evaluator = std::make_shared<HeuristicValueEvaluator>();
    }

    SelfPlayStats stats;
    const auto t_start = std::chrono::steady_clock::now();
    // Use total_pushes() (monotonic) instead of size() (clamps to
    // capacity) so the per-iter "tuples added" count keeps reporting
    // correctly once the ring buffer fills.
    int64_t prior_total_pushes = buffer.total_pushes();

    for (int g = 0; g < cfg.num_games; ++g) {
        auto r = playOneGame(game, evaluator, cfg, rng, buffer, iter, game_idx);
        ++stats.games_played;
        stats.total_decisions += r.decisions;
        if (r.returns.size() >= 2) {
            if (r.returns[0] > 0.0)      ++stats.p1_wins;
            else if (r.returns[1] > 0.0) ++stats.p2_wins;
            else                          ++stats.draws;
        }
        switch (r.exit_reason) {
            case ExitReason::Terminal:    ++stats.exit_terminal; break;
            case ExitReason::DecisionCap: ++stats.exit_cap;      break;
            case ExitReason::LegalEmpty:  ++stats.exit_stuck;    break;
        }
        stats.last_exit_reason = static_cast<int>(r.exit_reason);
        if (cfg.verbose) {
            auto elapsed = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - t_start).count();
            std::cerr << "  self-play game " << (g + 1) << "/"
                      << cfg.num_games
                      << "  decisions=" << r.decisions
                      << "  W/L/D=" << stats.p1_wins << "/"
                      << stats.p2_wins << "/" << stats.draws
                      << "  buffer=" << buffer.size()
                      << "  elapsed=" << static_cast<int>(elapsed) << "s\n";
        }
    }
    stats.tuples_added = buffer.total_pushes() - prior_total_pushes;
    return stats;
}

// Phase 6v — V3 self-play loop. Mirrors runSelfPlay() but constructs a
// V3 evaluator when use_model is true. The shared playOneGame() looks
// at cfg.model_kind to decide whether to capture flat_state (V3) or
// entity tokens (V2).
SelfPlayStats runSelfPlayV3(const ::open_spiel::Game& game,
                             ::riftbound::ml::V3Model model,
                             ReplayBuffer& buffer,
                             const SelfPlayConfig& cfg,
                             std::mt19937& rng,
                             int iter,
                             int game_idx)
{
    std::shared_ptr<::open_spiel::algorithms::Evaluator> evaluator;
    if (cfg.use_model) {
        evaluator = ::riftbound::ml::makeEvaluatorFromModel(model);
    } else {
        evaluator = std::make_shared<HeuristicValueEvaluator>();
    }

    SelfPlayStats stats;
    const auto t_start = std::chrono::steady_clock::now();
    int64_t prior_total_pushes = buffer.total_pushes();

    for (int g = 0; g < cfg.num_games; ++g) {
        auto r = playOneGame(game, evaluator, cfg, rng, buffer, iter, game_idx);
        ++stats.games_played;
        stats.total_decisions += r.decisions;
        if (r.returns.size() >= 2) {
            if (r.returns[0] > 0.0)      ++stats.p1_wins;
            else if (r.returns[1] > 0.0) ++stats.p2_wins;
            else                          ++stats.draws;
        }
        switch (r.exit_reason) {
            case ExitReason::Terminal:    ++stats.exit_terminal; break;
            case ExitReason::DecisionCap: ++stats.exit_cap;      break;
            case ExitReason::LegalEmpty:  ++stats.exit_stuck;    break;
        }
        stats.last_exit_reason = static_cast<int>(r.exit_reason);
        if (cfg.verbose) {
            auto elapsed = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - t_start).count();
            std::cerr << "  self-play[V3] game " << (g + 1) << "/"
                      << cfg.num_games
                      << "  decisions=" << r.decisions
                      << "  W/L/D=" << stats.p1_wins << "/"
                      << stats.p2_wins << "/" << stats.draws
                      << "  buffer=" << buffer.size()
                      << "  elapsed=" << static_cast<int>(elapsed) << "s\n";
        }
    }
    stats.tuples_added = buffer.total_pushes() - prior_total_pushes;
    return stats;
}

} // namespace riftbound::training
