#include "mcts_agent.h"

#include "core/game_state.h"
#include "core/intent.h"
#include "core/game_object.h"
#include "engine/game_engine.h"  // StepResult / StepKind

#include "agents/determinizer.h"
#include "openspiel/action_vocab.h"
#include "openspiel/riftbound_game.h"
#include "openspiel/riftbound_state.h"

#include "open_spiel/spiel.h"
#include "open_spiel/algorithms/mcts.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

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

    /// Prior weight of one Intent, before normalisation (see Prior()).
    static double intentWeight(const Intent& it) {
        double w = 1.0;  // default
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
        return w;
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
        ::open_spiel::ActionsAndProbs out;

        // If we can't cast or there's no Intent metadata, fall back
        // to uniform — never worse than the default.
        if (!rb) {
            auto legal = state.LegalActions();
            if (legal.empty()) return out;
            double p = 1.0 / static_cast<double>(legal.size());
            out.reserve(legal.size());
            for (auto a : legal) out.emplace_back(a, p);
            return out;
        }
        if (rb->IsTerminal()) return out;   // LegalActions() is empty

        // One id per distinct Intent (openspiel::actionIds), in ascending
        // order as RiftboundState::LegalActions gives them. An Intent the
        // engine listed twice has one id: it is counted once.
        const auto& intents = rb->legalIntents();
        const auto& engine_state = rb->engineState();
        const std::vector<int> ids = openspiel::actionIds(intents, engine_state);
        std::vector<std::pair<::open_spiel::Action, double>> weighted;
        weighted.reserve(intents.size());
        for (size_t i = 0; i < intents.size(); ++i) {
            weighted.emplace_back(static_cast<::open_spiel::Action>(ids[i]),
                                  intentWeight(intents[i]));
        }
        std::stable_sort(weighted.begin(), weighted.end(),
                         [](const auto& a, const auto& b) {
                             return a.first < b.first;
                         });
        weighted.erase(std::unique(weighted.begin(), weighted.end(),
                                   [](const auto& a, const auto& b) {
                                       return a.first == b.first;
                                   }),
                       weighted.end());
        if (weighted.empty()) return out;

        std::vector<::open_spiel::Action> legal;
        std::vector<double> weights;
        legal.reserve(weighted.size());
        weights.reserve(weighted.size());
        double total = 0.0;
        for (const auto& [action, w] : weighted) {
            legal.push_back(action);
            weights.push_back(w);
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
            // The ids the live engine had on offer vs what MCTS picked.
            std::cerr << " live_ids={";
            const std::vector<int> live_ids = openspiel::actionIds(legal_actions, state);
            for (size_t i = 0; i < live_ids.size(); ++i) {
                if (i > 0) std::cerr << ",";
                std::cerr << live_ids[i];
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

namespace {

// ─── Information-set MCTS ──────────────────────────────────────────────────

/// Information-set MCTS: the algorithm of OpenSpiel's `ISMCTSBot`
/// (open_spiel/algorithms/is_mcts.cc) in the one configuration this agent
/// uses — UCT selection, a fresh world per simulation, nodes keyed by
/// (deciding player, ObservationString) — with four differences:
///
/// * A node's children are kept in ACTION ORDER (std::map). `ISMCTSBot` keeps
///   them in an absl::flat_hash_map and iterates it to build the tie-break
///   candidates of every UCT step and the final policy; absl seeds every
///   table from an address, so the order was not a function of the bot's
///   seed and a seeded game differed from run to run (plans/09 item 9.E2).
///   Here every choice is a function of the seed and the position only
///   (IsMctsAgentTest.TheSameSeedsPlayTheSameGame).
/// * The root is one node whose children are the options of the LIVE
///   decision (by index into its legal list), found in each world by Intent
///   equality. A world's action id for an option can name a card the world
///   drew (a prompt's object is keyed by its card), and its observation key
///   carries the offered ids; keyed like any other node the root would split
///   into one node per world.
/// * With sampled worlds, every node is keyed by what the ROOT's player
///   observes (single-observer ISMCTS), not by what the player to move
///   observes: the opponent's own observation names the cards each world drew
///   for them, so every opponent decision would be a new node in every world.
///   (Measured: keyed that way, honest ISMCTS-50 won 6 of 20 against random
///   where random itself wins 34%; keyed by the root player, 21 of 30.)
/// * The move played is the most visited root option (ties by the RNG), not
///   a draw in proportion to the visits; the visits are reported
///   (SearchSummary) for a recorder.
class IsMctsSearch {
public:
    /// Makes the world of one simulation, at the root's decision.
    using WorldMaker = std::function<std::unique_ptr<openspiel::RiftboundState>()>;

    /// `root_player_keys`: key every node by what the ROOT's player observes
    /// (single-observer ISMCTS) instead of by what the player to move
    /// observes. With sampled worlds the opponent's own observation names the
    /// cards each world drew for them, so keyed by it every opponent decision
    /// is a new node in every world and a simulation ends at the first one.
    IsMctsSearch(uint64_t seed,
                 std::shared_ptr<::open_spiel::algorithms::Evaluator> evaluator,
                 double uct_c, int max_simulations, bool root_player_keys)
        : rng_(seed), evaluator_(std::move(evaluator)), uct_c_(uct_c),
          max_simulations_(max_simulations), root_player_keys_(root_player_keys) {}

    std::mt19937_64& rng() { return rng_; }

    /// Search the decision whose options are `live_legal`, one world per
    /// simulation from `make_world`.
    SearchSummary step(const WorldMaker& make_world, const std::vector<Intent>& live_legal) {
        nodes_.clear();
        root_.clear();
        SearchSummary out;
        out.searched = true;
        for (int sim = 0; sim < max_simulations_; ++sim) {
            std::unique_ptr<openspiel::RiftboundState> world = make_world();
            simulateRoot(*world, live_legal);
            ++out.simulations;
        }
        out.visits.assign(live_legal.size(), 0);
        out.value.assign(live_legal.size(), 0.0);
        int total = 0;
        double sum = 0.0;
        for (const auto& [index, child] : root_) {
            out.visits[index] = child.visits;
            out.value[index] = child.visits > 0 ? child.return_sum / child.visits : 0.0;
            total += child.visits;
            sum += child.return_sum;
        }
        out.root_value = total > 0 ? sum / total : 0.0;
        // Most visited, ties broken by the RNG.
        int best = -1;
        std::vector<int> tied;
        for (const auto& [index, child] : root_) {
            if (child.visits > best) { best = child.visits; tied.assign(1, index); }
            else if (child.visits == best) tied.push_back(index);
        }
        if (tied.empty()) {
            out.chosen = 0;
        } else {
            std::uniform_int_distribution<size_t> dist(0, tied.size() - 1);
            out.chosen = tied[tied.size() == 1 ? 0 : dist(rng_)];
        }
        return out;
    }

private:
    using Key = std::pair<::open_spiel::Player, std::string>;
    struct Child {
        int visits = 0;
        double return_sum = 0.0;
    };
    struct Node {
        std::map<::open_spiel::Action, Child> children;   // ordered: see the class comment
        int total_visits = kUnexpanded;
    };
    static constexpr int kUnexpanded = -1;
    static constexpr double kTieTolerance = 1e-5;

    Key keyOf(const ::open_spiel::State& state) const {
        return {state.CurrentPlayer(), root_player_keys_ ? state.ObservationString(root_player_)
                                                         : state.ObservationString()};
    }

    /// One simulation from the root: choose among the live options (by
    /// index), play that option in this world, continue below.
    void simulateRoot(openspiel::RiftboundState& world, const std::vector<Intent>& live_legal) {
        if (world.IsTerminal()) return;
        const ::open_spiel::Player player = world.CurrentPlayer();
        root_player_ = player;
        const std::vector<Intent> intents = world.legalIntents();   // a copy: valid past ApplyAction
        const std::vector<int> ids = openspiel::actionIds(intents, world.engineState());
        // The world's action id of each live option it offers.
        std::map<int, ::open_spiel::Action> offered;
        for (size_t i = 0; i < live_legal.size(); ++i) {
            for (size_t j = 0; j < intents.size(); ++j) {
                if (intents[j] == live_legal[i]) {
                    offered[static_cast<int>(i)] = ids[j];
                    break;
                }
            }
        }
        if (offered.empty()) return;

        int chosen = -1;
        bool untried = false;
        for (const auto& [index, action] : offered) {
            if (root_.find(index) == root_.end()) { untried = true; break; }
        }
        if (untried) {
            std::vector<int> order;
            for (const auto& [index, action] : offered) order.push_back(index);
            std::shuffle(order.begin(), order.end(), rng_);
            for (int index : order) {
                if (root_.find(index) == root_.end()) { chosen = index; break; }
            }
        } else {
            int total = 0;
            for (const auto& [index, action] : offered) total += root_[index].visits;
            std::vector<int> candidates;
            double best = -std::numeric_limits<double>::infinity();
            std::vector<std::pair<int, double>> values;
            for (const auto& [index, action] : offered) {
                const Child& c = root_[index];
                const double v = c.return_sum / c.visits +
                    uct_c_ * std::sqrt(std::log(static_cast<double>(std::max(total, 1))) / c.visits);
                values.emplace_back(index, v);
                best = std::max(best, v);
            }
            for (const auto& [index, v] : values)
                if (v > best - kTieTolerance) candidates.push_back(index);
            std::uniform_int_distribution<size_t> dist(0, candidates.size() - 1);
            chosen = candidates[candidates.size() == 1 ? 0 : dist(rng_)];
        }
        root_[chosen].visits++;
        world.ApplyAction(offered[chosen]);
        const std::vector<double> returns = simulate(world);
        root_[chosen].return_sum += returns[player];
    }

    std::vector<double> simulate(::open_spiel::State& state) {
        if (state.IsTerminal()) return state.Returns();

        const std::vector<::open_spiel::Action> legal = state.LegalActions();
        const ::open_spiel::Player player = state.CurrentPlayer();
        // std::map nodes: a reference stays valid while deeper simulations
        // insert more.
        Node& node = nodes_[keyOf(state)];
        if (node.total_visits == kUnexpanded) {
            node.total_visits = 0;              // stepped out of the tree: expand
            return evaluator_->Evaluate(state);
        }

        ::open_spiel::Action chosen = ::open_spiel::kInvalidAction;
        // An action not tried yet, in a shuffled order so the move order does
        // not bias which is tried first. (In different worlds one node can
        // offer different actions; only those offered here are candidates.)
        bool untried = false;
        for (::open_spiel::Action a : legal) {
            if (node.children.find(a) == node.children.end()) { untried = true; break; }
        }
        if (untried) {
            std::vector<::open_spiel::Action> order = legal;
            std::shuffle(order.begin(), order.end(), rng_);
            for (::open_spiel::Action a : order) {
                if (node.children.find(a) == node.children.end()) { chosen = a; break; }
            }
        }
        if (chosen == ::open_spiel::kInvalidAction) chosen = selectUct(node, legal);

        // Count the visit before descending: with imperfect recall the same
        // (node, action) can come up again inside this simulation.
        node.total_visits++;
        node.children[chosen].visits++;

        state.ApplyAction(chosen);
        std::vector<double> returns = simulate(state);
        node.children[chosen].return_sum += returns[player];
        return returns;
    }

    ::open_spiel::Action selectUct(const Node& node,
                                   const std::vector<::open_spiel::Action>& legal) {
        double best = -std::numeric_limits<double>::infinity();
        std::vector<std::pair<::open_spiel::Action, double>> values;
        values.reserve(legal.size());
        for (::open_spiel::Action action : legal) {   // ascending, like the map
            const Child& child = node.children.at(action);
            const double value = child.return_sum / child.visits +
                uct_c_ * std::sqrt(std::log(static_cast<double>(node.total_visits)) /
                                   child.visits);
            values.emplace_back(action, value);
            best = std::max(best, value);
        }
        std::vector<::open_spiel::Action> candidates;
        for (const auto& [action, value] : values)
            if (value > best - kTieTolerance) candidates.push_back(action);
        if (candidates.size() == 1) return candidates.front();
        std::uniform_int_distribution<size_t> dist(0, candidates.size() - 1);
        return candidates[dist(rng_)];
    }

    std::mt19937_64 rng_;
    std::shared_ptr<::open_spiel::algorithms::Evaluator> evaluator_;
    double uct_c_;
    int max_simulations_;
    bool root_player_keys_;
    ::open_spiel::Player root_player_ = 0;
    std::map<Key, Node> nodes_;
    std::map<int, Child> root_;   // by index into the live legal list
};

} // namespace

// ─── IsMctsAgent ───────────────────────────────────────────────────────────

struct IsMctsAgent::Impl {
    std::shared_ptr<const ::open_spiel::Game> game;
    std::unique_ptr<IsMctsSearch> search;
    int sims;
    WorldModel world_model;
    std::unique_ptr<LiveMirror> mirror;
    std::unique_ptr<WorldSampler> sampler;   // Sampled only
    SearchSummary last;
    bool check_worlds = false;
    long worlds_checked = 0;

    Impl(std::string d1, std::string d2,
         uint64_t engine_seed, uint64_t mcts_seed, int sim_count, WorldModel model)
        : sims(sim_count), world_model(model) {
        game = ::open_spiel::LoadGame(buildGameStr(d1, d2, engine_seed));
        if (!game) {
            throw std::runtime_error(
                "IsMctsAgent: LoadGame('riftbound') returned null — "
                "check deck paths and OpenSpiel registration.");
        }
        mirror = std::make_unique<LiveMirror>(game, engine_seed,
                                              mcts_seed ^ 0xC3C3C3C3u);
        search = std::make_unique<IsMctsSearch>(
            /*seed=*/mcts_seed ^ 0x3C3C3C3Cu,
            std::make_shared<RiftboundHeuristicEvaluator>(),
            /*uct_c=*/2.0,
            /*max_simulations=*/sims,
            /*root_player_keys=*/model == WorldModel::Sampled);
        if (world_model == WorldModel::Sampled) {
            sampler = std::make_unique<WorldSampler>(
                static_cast<const openspiel::RiftboundGame&>(*game).cardDb());
        }
        const char* env = std::getenv("RIFTBOUND_ISMCTS_CHECK_WORLDS");
        check_worlds = env && *env && std::string(env) != "0";
    }

    /// One simulation's world at the decision `root` is in, for `observer`.
    std::unique_ptr<openspiel::RiftboundState> makeWorld(
        const openspiel::RiftboundState& root, PlayerId observer) {
        std::unique_ptr<openspiel::RiftboundState> world(
            static_cast<openspiel::RiftboundState*>(root.Clone().release()));
        if (world_model == WorldModel::Perfect) return world;
        std::mt19937_64& rng = search->rng();
        const uint64_t draw_seed = rng();
        const uint64_t engine_rng_seed = rng();
        world->editHiddenInformation(
            [&](GameState& g) {
                std::mt19937_64 draw(draw_seed);
                sampler->sample(g, observer, draw);
            },
            engine_rng_seed);
        if (check_worlds) {
            const auto problems = sampler->check(root.engineState(), world->engineState(), observer);
            ++worlds_checked;
            if (!problems.empty()) {
                std::string all;
                for (const auto& p : problems) all += "\n  " + p;
                throw std::logic_error("IsMctsAgent: a sampled world contradicts what " +
                                       std::string(toString(observer)) + " knows:" + all);
            }
        }
        return world;
    }
};

IsMctsAgent::IsMctsAgent(std::string deck1_path,
                         std::string deck2_path,
                         std::string /*registry_path*/,
                         uint64_t    engine_seed,
                         uint64_t    mcts_seed,
                         int         sims,
                         WorldModel  world)
    : impl_(std::make_unique<Impl>(std::move(deck1_path),
                                   std::move(deck2_path),
                                   engine_seed, mcts_seed, sims, world)) {}

IsMctsAgent::~IsMctsAgent() = default;

const SearchSummary& IsMctsAgent::lastSearch() const { return impl_->last; }
void IsMctsAgent::setCheckWorlds(bool on) { impl_->check_worlds = on; }
long IsMctsAgent::worldsChecked() const { return impl_->worlds_checked; }

Intent IsMctsAgent::selectAction(const GameState& state,
                                 const std::vector<Intent>& legal_actions) {
    impl_->last = SearchSummary{};
    if (legal_actions.empty()) {
        return Intent::concede(decidingPlayer(state, legal_actions));
    }
    auto unsearched = [&]() -> Intent {
        const Intent& pick = impl_->mirror->unsearched(legal_actions);
        impl_->last.chosen = static_cast<int>(&pick - legal_actions.data());
        return pick;
    };
    // The mirror is kept in step at every decision, searched or not.
    const openspiel::RiftboundState* os_state =
        impl_->mirror->rootFor(state, legal_actions);
    if (legal_actions.size() == 1) {
        impl_->last.chosen = 0;
        return legal_actions.front();
    }
    // The setup battlefield choice (see the class comment).
    if (state.battlefields.empty()) return unsearched();
    if (!os_state || os_state->IsTerminal()) return unsearched();

    const PlayerId observer = decidingPlayer(state, legal_actions);
    impl_->last = impl_->search->step(
        [&]() { return impl_->makeWorld(*os_state, observer); }, legal_actions);
    return legal_actions[impl_->last.chosen];
}

} // namespace riftbound
