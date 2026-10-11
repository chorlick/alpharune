#include "mcts_agent.h"

#include "core/game_state.h"
#include "core/intent.h"
#include "core/game_object.h"
#include "engine/game_engine.h"  // StepResult / StepKind

#include "openspiel/action_vocab.h"
#include "openspiel/riftbound_state.h"

#include "open_spiel/spiel.h"
#include "open_spiel/algorithms/mcts.h"
#include "open_spiel/algorithms/is_mcts.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>

namespace riftbound {

namespace {

// A heuristic evaluator that scores Riftbound positions directly from
// the engine state instead of running noisy random rollouts to terminal.
//
// Why: at small sim budgets (5–20), RandomRolloutEvaluator gives MCTS
// almost no useful signal — each rollout takes ~200 random actions to
// reach a terminal and the variance across rollouts swamps the
// inter-action differences MCTS is trying to compare. Empirically we
// saw MCTS-5 lose ~63% to random because the noisy rollouts left MCTS
// picking among un-differentiated children. Replacing the rollout with
// a constant-time heuristic restores the signal/budget ratio so MCTS
// at low sims can actually beat random.
//
// Heuristic (from P1's perspective):
//   • Primary: score difference, normalised to [-1, +1] by victory_score.
//   • Secondary: total on-board might (small weight) — proxies for
//     near-term scoring threat / combat advantage.
//   • Tertiary: hand size advantage (very small weight).
class RiftboundHeuristicEvaluator : public ::open_spiel::algorithms::Evaluator {
public:
    std::vector<double> Evaluate(const ::open_spiel::State& state) override {
        // Terminal: use the canonical Returns directly so MCTS sees
        // unambiguous win/loss signals at known game-ends.
        if (state.IsTerminal()) {
            return state.Returns();
        }
        const auto* rb = dynamic_cast<const openspiel::RiftboundState*>(&state);
        if (!rb) return {0.0, 0.0};
        const auto& s = rb->engineState();

        const auto& p1 = s.player(PlayerId::Player1);
        const auto& p2 = s.player(PlayerId::Player2);
        const double victory = std::max(1, s.mode.victory_score);

        // Score-only heuristic. A previous version added might / units /
        // hand-size signals; they appeared to mislead MCTS at low sim
        // budgets (sims=5 dropped from 37% to 20% vs random). The
        // game-winning signal is score, period — that's what the bot
        // should chase, and noisy proxies for "position strength" only
        // pollute the value estimate when the search budget is too small
        // to disentangle them via tree expansion. With more sims this
        // could be reintroduced as a tunable; today, keep it minimal.
        double val = static_cast<double>(p1.score - p2.score) / victory;
        if (val >  1.0) val =  1.0;
        if (val < -1.0) val = -1.0;
        return {val, -val};
    }

    ::open_spiel::ActionsAndProbs Prior(const ::open_spiel::State& state) override {
        // Strategically-biased prior. The point at sims=5 isn't to be
        // CORRECT about action quality (5 sims aren't enough for that)
        // — it's to FOCUS the limited budget on actions that actually
        // change the game state. Random uniform priors waste sims on
        // EndTurn/Pass which add no information. By skewing the prior
        // toward plays / moves-to-battlefield / combat-damage / score-
        // changing actions, MCTSBot's UCT formula preferentially
        // expands those children first — so MCTS-5 ends up sampling
        // the meaningful moves rather than the noise moves.
        //
        // Weights (rough Riftbound intuition):
        //   Score / Conquer paths   high (winning condition)
        //   PlayCard                high (advances board)
        //   StandardMove → BF       high (toward scoring)
        //   ActivateAbility         medium-high
        //   AssignCombatDamage      high (commits combat)
        //   PlayReaction/Action     medium
        //   MakeChoice              medium (cards usually have a "right" pick)
        //   StandardMove → Base     low (defensive)
        //   EndTurn / Pass*         low (passive)
        //   Concede                 ~0 (never voluntarily)
        const auto* rb = dynamic_cast<const openspiel::RiftboundState*>(&state);
        auto legal = state.LegalActions();
        ::open_spiel::ActionsAndProbs out;
        if (legal.empty()) return out;

        // If we can't cast or there's no Intent metadata, fall back
        // to uniform — never worse than the default.
        if (!rb) {
            double p = 1.0 / static_cast<double>(legal.size());
            out.reserve(legal.size());
            for (auto a : legal) out.emplace_back(a, p);
            return out;
        }

        const auto intents = rb->legalIntents();
        const auto& engine_state = rb->engineState();
        std::vector<double> weights(legal.size(), 0.0);
        double total = 0.0;

        for (size_t i = 0; i < legal.size(); ++i) {
            // Match legal[i] back to an intent via encode.
            double w = 1.0;  // default
            for (const auto& it : intents) {
                if (openspiel::encodeAction(it, engine_state) !=
                    static_cast<int>(legal[i])) continue;
                switch (it.type) {
                    case IntentType::PlayCard:
                    case IntentType::PlayReaction:
                    case IntentType::PlayActionCard:
                        w = 4.0; break;
                    case IntentType::AssignCombatDamage:
                        w = 4.0; break;
                    case IntentType::StandardMove:
                        // Move to a battlefield is meaningful; move to
                        // base is usually retreating / shuffling.
                        if (it.move_destination.has_value() &&
                            std::holds_alternative<BattlefieldLocation>(*it.move_destination)) {
                            w = 3.0;
                        } else {
                            w = 0.6;
                        }
                        break;
                    case IntentType::ActivateAbility:
                    case IntentType::ActivateReactionAbility:
                    case IntentType::ActivateActionAbility:
                        w = 2.5; break;
                    case IntentType::MakeChoice:
                        w = 1.5; break;
                    case IntentType::MulliganDecision:
                    case IntentType::ChooseBattlefield:
                    case IntentType::PlayFirstDecision:
                        w = 1.0; break;
                    case IntentType::EndTurn:
                    case IntentType::PassPriority:
                    case IntentType::PassFocus:
                        w = 0.3; break;
                    case IntentType::Concede:
                        w = 0.01; break;
                    default:
                        w = 1.0; break;
                }
                break;
            }
            weights[i] = w;
            total += w;
        }

        out.reserve(legal.size());
        if (total > 0.0) {
            for (size_t i = 0; i < legal.size(); ++i) {
                out.emplace_back(legal[i], weights[i] / total);
            }
        } else {
            // Defensive fallback — uniform.
            double p = 1.0 / static_cast<double>(legal.size());
            for (auto a : legal) out.emplace_back(a, p);
        }
        return out;
    }
};

} // namespace

namespace {

std::string buildGameStr(const std::string& deck1,
                        const std::string& deck2,
                        uint64_t seed) {
    // RiftboundGame's `seed` GameParameter is registered as int, so
    // any uint64 that doesn't fit (e.g. nondeterministic seeds from
    // std::random_device) hits OpenSpiel's CHECK_TRUE in the param
    // parser. Squash to a positive 31-bit value. Only the Game's default
    // seed is affected: the states this agent builds are given the full
    // engine seed directly.
    int seed_int = static_cast<int>(seed & 0x7FFFFFFFu);
    std::ostringstream s;
    s << "riftbound(deck1=" << deck1
      << ",deck2=" << deck2
      << ",seed=" << seed_int
      << ")";
    return s.str();
}

/// Keeps an OpenSpiel state in step with the live game the agent is playing
/// in, and hands out the root to search from at each decision.
///
/// The mirror is a second engine fed the live game's exact choices
/// (`GameState::action_history`) from the same decks and seed, so it is in
/// the live position down to the RNG stream, at any kind of decision —
/// including the ones a GameState copy cannot be resumed from (chain
/// priority, showdown, combat, cost payment, prompts). It only ever replays
/// the choices made since the agent's previous decision.
class LiveMirror {
public:
    LiveMirror(std::shared_ptr<const ::open_spiel::Game> game,
               uint64_t engine_seed, uint64_t rng_seed)
        : game_(std::move(game)), engine_seed_(engine_seed), rng_(rng_seed) {}

    /// The state to search from, or nullptr if this decision cannot be
    /// searched. Valid until the next call.
    const openspiel::RiftboundState* rootFor(
        const GameState& state, const std::vector<Intent>& legal_actions) {
        // An agent is not told which seat it is being asked for. The mirror
        // knows (its engine asks a seat), so leave the decider open here and
        // let the mirror supply it.
        ::riftbound::StepResult step;
        step.kind        = ::riftbound::StepKind::NeedDecision;
        step.legal       = legal_actions;
        step.perspective = PlayerId::None;

        if (!unreproducible_) {
            if (mirror_ && mirror_->syncToHistory(state, step)) {
                return mirror_.get();
            }
            mirror_ = openspiel::RiftboundState::rebuildFromHistory(
                game_, engine_seed_, state, step);
            if (mirror_) return mirror_.get();
            // The history does not reproduce the live game: the seed we were
            // given is not the engine's, or the state was edited outside the
            // engine (the play UI's god mode). That does not heal; stop
            // paying for a full replay at every decision.
            unreproducible_ = true;
        }

        // No exact copy. A snapshot can still be resumed at a top-level
        // main-phase decision — the board is right, only the future
        // shuffles are the agent's guess rather than the live game's.
        if (GameEngine::isResumableDecision(state, legal_actions)) {
            // At this decision the decider is the turn player, whom every
            // offered Intent names.
            step.perspective = decidingPlayer(state, legal_actions);
            approx_ = openspiel::RiftboundState::makeFromSnapshot(
                game_, engine_seed_, state, std::move(step),
                ::riftbound::GameResult{}, rng_());
            return approx_.get();
        }
        return nullptr;
    }

    /// For a decision that could not be searched: any legal action, chosen
    /// uniformly. (Always taking the first would be "always pass".)
    const Intent& unsearched(const std::vector<Intent>& legal_actions) {
        std::uniform_int_distribution<size_t> pick(0, legal_actions.size() - 1);
        return legal_actions[pick(rng_)];
    }

    bool exact() const { return !unreproducible_; }

private:
    std::shared_ptr<const ::open_spiel::Game> game_;
    uint64_t engine_seed_;
    std::mt19937_64 rng_;
    std::unique_ptr<openspiel::RiftboundState> mirror_;
    std::unique_ptr<openspiel::RiftboundState> approx_;
    bool unreproducible_ = false;
};

} // namespace

// ─── MctsAgent ─────────────────────────────────────────────────────────────

struct MctsAgent::Impl {
    std::shared_ptr<const ::open_spiel::Game> game;
    std::unique_ptr<::open_spiel::algorithms::MCTSBot> bot;
    int sims;
    std::unique_ptr<LiveMirror> mirror;

    Impl(std::string d1, std::string d2,
         uint64_t engine_seed, uint64_t mcts_seed, int sim_count)
        : sims(sim_count) {
        game = ::open_spiel::LoadGame(buildGameStr(d1, d2, engine_seed));
        if (!game) {
            throw std::runtime_error(
                "MctsAgent: LoadGame('riftbound') returned null — "
                "check deck paths and OpenSpiel registration.");
        }
        // `engine_seed` MUST equal the live engine's runGame seed: the
        // mirror replays the live game's choices on it.
        mirror = std::make_unique<LiveMirror>(game, engine_seed,
                                              mcts_seed ^ 0xA5A5A5A5u);
        // Empirical results (30-game samples vs random, miss_fortune deck,
        // uct_c=2.0):
        //   RandomRollout, n_rollouts=1, uct_c=2.0:  37%
        //   Score-only heuristic, uct_c=2.0:         43%
        //   Multi-signal heuristic, uct_c=2.0:       20% (misleading proxies)
        //   RandomRollout, n_rollouts=3, uct_c=2.0:  33% (slower; no win)
        //
        // The score-only heuristic gives the cleanest "did we score?"
        // signal and is O(1) per leaf (no rollouts). At low sim budgets
        // the variance of rollouts swamps inter-action differences;
        // a deterministic, slightly-correlated signal beats a noisy
        // strong one.
        auto evaluator = std::make_shared<RiftboundHeuristicEvaluator>();
        bot = std::make_unique<::open_spiel::algorithms::MCTSBot>(
            *game, evaluator,
            /*uct_c=*/1.4,
            /*max_simulations=*/sims,
            /*max_memory_mb=*/256,
            /*solve=*/false,
            /*seed=*/static_cast<int>(mcts_seed ^ 0x5A5A5A5A),
            /*verbose=*/false,
            // PUCT (AlphaZero-style): the formula is
            //   Q/N + c * prior * sqrt(parent_N) / (N + 1)
            // so a strategic prior in the evaluator (favoring plays /
            // moves-to-BF / combat) actually steers exploration. The
            // default UCT policy uses only visits + value and ignores
            // priors entirely — which made the prior in
            // RiftboundHeuristicEvaluator dead code. Switching here
            // brings the prior into the selection loop.
            ::open_spiel::algorithms::ChildSelectionPolicy::PUCT);
    }
};

MctsAgent::MctsAgent(std::string deck1_path,
                     std::string deck2_path,
                     std::string /*registry_path*/,
                     uint64_t    engine_seed,
                     uint64_t    mcts_seed,
                     int         sims)
    : impl_(std::make_unique<Impl>(std::move(deck1_path),
                                   std::move(deck2_path),
                                   engine_seed, mcts_seed, sims)) {}

MctsAgent::~MctsAgent() = default;

Intent MctsAgent::selectAction(const GameState& state,
                               const std::vector<Intent>& legal_actions) {
    if (legal_actions.empty()) {
        return Intent::concede(decidingPlayer(state, legal_actions));
    }

    // The search root: a state in the live game's position whose
    // CurrentPlayer() is the player deciding here (not the turn player on
    // chain priority, showdown focus, combat damage, a second mulligan).
    const openspiel::RiftboundState* os_state =
        impl_->mirror->rootFor(state, legal_actions);

    static const bool dbg = std::getenv("RIFTBOUND_MCTS_DEBUG") != nullptr;
    if (dbg) {
        std::cerr << "[MCTS] decision_idx=" << state.decision_index
                  << " turn_player=" << static_cast<int>(state.turn.turn_player)
                  << " acting="
                  << static_cast<int>(decidingPlayer(state, legal_actions))
                  << " exact=" << impl_->mirror->exact()
                  << " legal=" << legal_actions.size();
        if (os_state && !os_state->IsTerminal()) {
            std::cerr << " clone_cur=" << os_state->CurrentPlayer()
                      << " clone_legal=" << os_state->LegalActions().size();
        }
        std::cerr << "\n";
    }

    if (!os_state || os_state->IsTerminal()) {
        if (dbg) std::cerr << "[MCTS] -> unsearched (no root for this decision)\n";
        return impl_->mirror->unsearched(legal_actions);
    }
    auto root = impl_->bot->MCTSearch(*os_state);
    if (!root) {
        if (dbg) std::cerr << "[MCTS] -> unsearched (MCTSearch null)\n";
        return impl_->mirror->unsearched(legal_actions);
    }
    ::open_spiel::Action chosen_action = root->BestChild().action;
    if (dbg) {
        std::cerr << "[MCTS] chose_action=" << chosen_action
                  << " explore=" << root->explore_count
                  << " value=" << (root->explore_count > 0
                                   ? root->total_reward / root->explore_count : 0.0)
                  << "\n";
    }
    const Intent* intent = openspiel::decodeAction(
        static_cast<int>(chosen_action), legal_actions, state);
    if (!intent) {
        if (dbg) {
            std::cerr << "[MCTS] -> fallback (decodeAction returned null; "
                      << "action " << chosen_action
                      << " not in live legal set of size "
                      << legal_actions.size() << ")";
            // Encode every live Intent to a slot so we can see what
            // slots the live engine had available vs what MCTS picked.
            std::cerr << " live_slots={";
            for (size_t i = 0; i < legal_actions.size(); ++i) {
                if (i > 0) std::cerr << ",";
                std::cerr << openspiel::encodeAction(legal_actions[i], state);
            }
            std::cerr << "}\n";
        }
        return impl_->mirror->unsearched(legal_actions);
    }
    if (dbg) {
        std::cerr << "[MCTS] -> decoded to: " << intent->describe() << "\n";
    }
    return *intent;
}

// ─── IsMctsAgent ───────────────────────────────────────────────────────────

struct IsMctsAgent::Impl {
    std::shared_ptr<const ::open_spiel::Game> game;
    std::unique_ptr<::open_spiel::algorithms::ISMCTSBot> bot;
    int sims;
    std::unique_ptr<LiveMirror> mirror;

    Impl(std::string d1, std::string d2,
         uint64_t engine_seed, uint64_t mcts_seed, int sim_count)
        : sims(sim_count) {
        game = ::open_spiel::LoadGame(buildGameStr(d1, d2, engine_seed));
        if (!game) {
            throw std::runtime_error(
                "IsMctsAgent: LoadGame('riftbound') returned null — "
                "check deck paths and OpenSpiel registration.");
        }
        mirror = std::make_unique<LiveMirror>(game, engine_seed,
                                              mcts_seed ^ 0xC3C3C3C3u);
        auto evaluator = std::make_shared<RiftboundHeuristicEvaluator>();
        bot = std::make_unique<::open_spiel::algorithms::ISMCTSBot>(
            /*seed=*/static_cast<int>(mcts_seed ^ 0x3C3C3C3C),
            evaluator,
            /*uct_c=*/2.0,
            /*max_simulations=*/sims,
            /*max_world_samples=*/
                ::open_spiel::algorithms::kUnlimitedNumWorldSamples,
            /*final_policy_type=*/
                ::open_spiel::algorithms::ISMCTSFinalPolicyType::kNormalizedVisitCount,
            /*use_observation_string=*/true,
            /*allow_inconsistent_action_sets=*/false);
        // Minimum-viable resampler: Clone() rather than determinize hidden
        // info. Same TODO as openspiel_match — proper resampling from the
        // unseen card pool is queued.
        bot->SetResampler(
            [](const ::open_spiel::State& st,
               ::open_spiel::Player /*pl*/,
               std::function<double()> /*rng*/)
                -> std::unique_ptr<::open_spiel::State> {
                return st.Clone();
            });
    }
};

IsMctsAgent::IsMctsAgent(std::string deck1_path,
                         std::string deck2_path,
                         std::string /*registry_path*/,
                         uint64_t    engine_seed,
                         uint64_t    mcts_seed,
                         int         sims)
    : impl_(std::make_unique<Impl>(std::move(deck1_path),
                                   std::move(deck2_path),
                                   engine_seed, mcts_seed, sims)) {}

IsMctsAgent::~IsMctsAgent() = default;

Intent IsMctsAgent::selectAction(const GameState& state,
                                 const std::vector<Intent>& legal_actions) {
    if (legal_actions.empty()) {
        return Intent::concede(decidingPlayer(state, legal_actions));
    }
    const openspiel::RiftboundState* os_state =
        impl_->mirror->rootFor(state, legal_actions);

    if (!os_state || os_state->IsTerminal()) {
        return impl_->mirror->unsearched(legal_actions);
    }
    ::open_spiel::Action chosen_action = impl_->bot->Step(*os_state);
    const Intent* intent = openspiel::decodeAction(
        static_cast<int>(chosen_action), legal_actions, state);
    if (!intent) return impl_->mirror->unsearched(legal_actions);
    return *intent;
}

} // namespace riftbound
