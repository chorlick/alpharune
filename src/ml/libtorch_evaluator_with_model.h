#pragma once
/// @file libtorch_evaluator_with_model.h
/// Torch-aware factory that builds an Evaluator sharing an existing
/// in-memory V2Model (no checkpoint round-trip through disk).
///
/// Used by the training driver to keep the evaluator pointing at the
/// SAME model instance the trainer is mutating. After each
/// optimizer step, evaluator forward passes pick up the updated
/// weights automatically.
///
/// Pulls torch headers — only include from code that already does
/// (trainer.cpp, self_play.cpp). External consumers stick with the
/// path-based makeLibTorchEvaluator() in libtorch_evaluator.h.

#include "ml/libtorch_evaluator.h"
#include "ml/v2_model.h"
#include "ml/v3_model.h"

namespace riftbound::ml {

std::shared_ptr<::open_spiel::algorithms::Evaluator>
makeEvaluatorFromModel(V2Model model);

/// Phase 6v — V3 (flat-feature MLP) evaluator factory. Same semantics
/// as the V2 overload: caller passes in the in-memory model and the
/// returned Evaluator keeps a reference to it, so weight updates from
/// the trainer are picked up automatically by the next forward pass.
std::shared_ptr<::open_spiel::algorithms::Evaluator>
makeEvaluatorFromModel(V3Model model);

} // namespace riftbound::ml
