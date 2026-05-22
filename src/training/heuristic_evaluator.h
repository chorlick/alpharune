#pragma once
/// @file heuristic_evaluator.h
/// MCTS Evaluator that uses a hand-coded heuristic value function instead
/// of either random rollouts or a model forward pass. Designed for
/// bootstrap training: gives MCTS a real value signal even when the
/// model's value head hasn't learned anything yet (or has collapsed to
/// the degenerate "predict zero" optimum).
///
/// Why this matters for bootstrap:
///   When the model is fresh and producing meaningless value estimates
///   AND random rollouts on long, complex games return noisy ~0 values,
///   MCTS has no signal to distinguish good moves from bad moves. The
///   policy prior dominates the search, and any small bias in the prior
///   (slot-0 preference, init noise) gets amplified into a degenerate
///   policy (observed: 80% EndTurn at iter 6 of the rengar bootstrap).
///
///   A heuristic value function — even a simple one like score
///   differential — gives MCTS something to optimize. Actions that
///   genuinely advance the score look BETTER than EndTurn-spam,
///   pushing the visit distribution away from the degenerate
///   attractor.
///
/// Heuristic semantics:
///   value(state, player) = clamp((player_score - opp_score) / 8.0, -1, +1)
///   value is a tanh-like surrogate — bounded [-1, +1], zero at parity,
///   approaches +1 as you near winning (score 8 = victory in 1v1).
///   The acting player's perspective is used (OpenSpiel Evaluator::Evaluate
///   convention: returns one value per player).
///
/// Prior semantics:
///   Uniform over legal actions. MCTS doesn't need a strong prior when
///   the heuristic value is informative — exploration via UCT does the
///   work. A flat prior also prevents the prior from leaking the
///   slot-0 bias into the visit distribution.

#include "open_spiel/algorithms/mcts.h"
#include "open_spiel/spiel.h"

#include <vector>

namespace riftbound::training {

class HeuristicValueEvaluator : public ::open_spiel::algorithms::Evaluator {
public:
    HeuristicValueEvaluator() = default;
    ~HeuristicValueEvaluator() override = default;

    /// Returns [value_for_p0, value_for_p1] from score differential.
    std::vector<double> Evaluate(const ::open_spiel::State& state) override;

    /// Returns uniform-over-legal prior. Prevents prior leakage of
    /// slot-0 bias into MCTS visit distribution.
    ::open_spiel::ActionsAndProbs Prior(
        const ::open_spiel::State& state) override;
};

} // namespace riftbound::training
