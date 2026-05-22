#include "training/heuristic_evaluator.h"

#include "core/game_state.h"
#include "openspiel/riftbound_state.h"

#include <algorithm>

namespace riftbound::training {

std::vector<double> HeuristicValueEvaluator::Evaluate(
    const ::open_spiel::State& state) {
    // Terminal states defer to engine outcome. OpenSpiel's MCTSBot
    // doesn't actually call Evaluate on terminal states (it calls
    // Returns()), but defending against future changes is cheap.
    if (state.IsTerminal()) {
        return state.Returns();
    }

    // Pull underlying engine state. The cast assumes the OpenSpiel
    // state is always RiftboundState — true for our match/training
    // runners. If a future caller passes a different OpenSpiel game,
    // this throws std::bad_cast which is the right failure mode.
    const auto& rb = dynamic_cast<const ::riftbound::openspiel::RiftboundState&>(state);
    const auto& gs = rb.engineState();
    int p1_score = gs.player(::riftbound::PlayerId::Player1).score;
    int p2_score = gs.player(::riftbound::PlayerId::Player2).score;

    // Score-differential / kVictoryScore. CR victory = 8 points in 1v1,
    // so dividing by 8 maps the differential into [-1, +1] for the
    // normal range. Clamp defends against future scoring exotica.
    constexpr double kVictoryScore = 8.0;
    double diff = (static_cast<double>(p1_score) -
                   static_cast<double>(p2_score)) / kVictoryScore;
    diff = std::clamp(diff, -1.0, 1.0);

    // Zero-sum: P0 gets +diff, P1 gets -diff. (P0 == Player1 in our
    // OpenSpiel indexing; see RiftboundGame::NumPlayers.)
    return {diff, -diff};
}

::open_spiel::ActionsAndProbs HeuristicValueEvaluator::Prior(
    const ::open_spiel::State& state) {
    // Uniform over legal actions. MCTS UCT exploration + the heuristic
    // value signal do the actual work; the prior is intentionally flat
    // to avoid any source of action-id bias.
    auto legal = state.LegalActions();
    ::open_spiel::ActionsAndProbs out;
    out.reserve(legal.size());
    if (legal.empty()) return out;
    const double p = 1.0 / static_cast<double>(legal.size());
    for (auto a : legal) {
        out.emplace_back(a, p);
    }
    return out;
}

} // namespace riftbound::training
