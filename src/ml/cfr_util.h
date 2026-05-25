#pragma once
/// @file cfr_util.h
/// Utilities for Counterfactual Regret Minimization (CFR) — the
/// foundation for Deep CFR training (Phase 6w+).
///
/// Background: AlphaZero-style training (V2, V3) trains the policy
/// head on MCTS visit distributions. Because always-legal actions
/// (EndTurn, PassPriority) get a baseline share of MCTS visits, the
/// model imitates that and the bias self-reinforces — see
/// docs/v2-model-failure-analysis.md and observed V3 Pass-collapse
/// at iter_10 (20% vs random).
///
/// CFR sidesteps this by training on REGRET (the gap to the
/// counterfactual best action) instead of empirical visit counts.
/// Regret matching transforms cumulative regrets into the strategy:
///
///     policy(a) = max(R(a), 0) / sum_a' max(R(a'), 0)
///
/// If all cumulative regrets are <= 0, the policy falls back to
/// uniform over legal actions (no action has been "regretted"
/// positively, so no preference). This is Zinkevich et al. 2007's
/// canonical regret matching.
///
/// References:
///   * Zinkevich et al., "Regret Minimization in Games with Incomplete
///     Information" (NeurIPS 2007): https://martin.zinkevich.org/publications/regretpoker.pdf
///   * Brown & Sandholm, "Deep CFR" (ICML 2019): https://arxiv.org/abs/1811.00164
///   * LabML annotated CFR implementation: https://nn.labml.ai/cfr/index.html

#include <cstddef>
#include <cstdint>
#include <vector>

namespace riftbound::ml {

/// Phase 6w — info-set ID for CFR. Stable FNV-1a 64-bit hash over the
/// masked observation-feature vector, mixed with the perspective
/// player id so same engine state from P1 vs P2 view produces distinct
/// ids (a per-player info-set requirement of CFR).
///
/// Pure function — no engine/state coupling. Caller computes the
/// feature vector (typically via extractStateFeatures) and passes
/// it in. RiftboundState::InfoSetId is a thin wrapper.
///
/// Collision properties: 64-bit FNV-1a on a ~4623-float input gives
/// <1% collision at 2^32 distinct entries (birthday-paradox floor).
/// Test coverage: see tests/test_cfr_util.cpp.
uint64_t computeInfoSetId(const std::vector<float>& features, int player);

/// Apply regret matching to cumulative regrets to derive an action
/// distribution.
///
/// Inputs:
///   cumulative_regrets — one entry per legal action at the info-set.
///                        Negative entries are clamped to 0 internally.
///
/// Output:
///   probability vector, same length as input, sums to 1.0 (within
///   floating-point precision). If all regrets are <= 0, returns
///   uniform-over-legal (1/N each).
///
/// Pure function — no shared state. Caller manages cumulative regret
/// storage (typically per info-set, accumulated across CFR iters).
std::vector<double>
regretMatching(const std::vector<double>& cumulative_regrets);

/// In-place variant — writes the result into `out`. Avoids an
/// allocation in inner loops. `out` is resized to match
/// `cumulative_regrets.size()`.
void regretMatching(const std::vector<double>& cumulative_regrets,
                     std::vector<double>& out);

} // namespace riftbound::ml
