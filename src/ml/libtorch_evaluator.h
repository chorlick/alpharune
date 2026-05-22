#pragma once
/// @file libtorch_evaluator.h
/// OpenSpiel MCTS Evaluator backed by a LibTorch V2 model.
///
/// Plugs the entity-token / transformer / spatial-fusion / pointer-head
/// V2 architecture (see `ml/v2_model.h`) into
/// `open_spiel::algorithms::MCTSBot` in place of `RandomRolloutEvaluator`.
/// Each MCTS leaf evaluation becomes one forward pass over the V2 model
/// instead of a full random rollout — typically much cheaper at high
/// sim counts and the foundation for AlphaZero-style training where
/// the value/policy signal improves with each retraining iteration.
///
/// Header-only declarations to keep LibTorch's heavy include surface
/// (`torch/torch.h`) out of consumers — only the .cpp depends on torch.
/// Consumers see std::shared_ptr<open_spiel::algorithms::Evaluator>.

#include "open_spiel/algorithms/mcts.h"

#include <memory>
#include <string>

namespace riftbound::ml {

struct V2ModelConfig;  // from v2_model.h

/// Construct a LibTorch-backed Evaluator.
///
/// If `checkpoint_path` is empty, a random-initialized V2 model is
/// created using the default `V2ModelConfig` (or `cfg` if supplied).
/// Useful for validating the integration end-to-end before any
/// training has been done — random-weight model + MCTSBot still beats
/// random agent purely through MCTS structural bias.
///
/// If `checkpoint_path` is non-empty, the model is loaded from that
/// file via `torch::load`. The on-disk format must match a model
/// trained with the same `V2ModelConfig` (all dim knobs). Mismatches
/// fail loudly at load time.
///
/// Pass `cfg = nullptr` to use the default config. The pointer is only
/// read during construction; the caller owns the storage.
std::shared_ptr<::open_spiel::algorithms::Evaluator>
makeLibTorchEvaluator(const std::string& checkpoint_path = "",
                       const V2ModelConfig* cfg = nullptr);

// The torch-aware overload that shares an in-memory model lives in
// libtorch_evaluator_with_model.h — separate header to keep heavy
// torch includes out of consumers that only need the path-loading
// API above.

} // namespace riftbound::ml
