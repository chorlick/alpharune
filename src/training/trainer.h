#pragma once
/// @file trainer.h
/// AlphaZero-style trainer for the V2 entity-token / transformer model.
///
/// One `step()` call:
///   1. Stacks a batch of ReplaySamples' entity-token snapshots into
///      V2Model's batched tensor inputs.
///   2. Forward pass → V2 Output (flat_policy_logits + value used).
///   3. Masks illegal logits to -inf using each sample's legal_actions.
///   4. Loss = policy cross-entropy + value MSE.
///   5. Backward + AdamW step + gradient clipping.
///
/// Owns the optimizer. Doesn't save the optimizer state — restart on
/// resume rebuilds it (acceptable for AlphaZero where the model's
/// running average is what matters).
///
/// NOTE: pointer-head supervision (action_type / source / target / dest)
/// is deferred until the action-vocab decomposition + aux losses land.
/// In V2's current training surface, the flat policy head carries the
/// gradient signal and the trunk learns end-to-end through it.

#include "ml/v2_model.h"
#include "training/replay_buffer.h"

#include <torch/torch.h>

#include <memory>
#include <string>
#include <tuple>

namespace riftbound::training {

struct TrainerConfig {
    double learning_rate = 1e-3;
    double weight_decay  = 1e-4;
    double grad_clip     = 1.0;
    // Phase 6t — entropy regularization. Adds `-entropy_coef * H(policy)`
    // to the policy loss. Penalizes peaked policies, preserves
    // exploration. Disabled by default; bootstrap should set this
    // ~0.01-0.05 to prevent the EndTurn-collapse degenerate optimum.
    double entropy_coef = 0.0;
    // Phase 6t — skip value-head gradient during bootstrap. When true,
    // value loss is computed for reporting but not back-propagated.
    // Bootstrap value head collapses to "predict zero everywhere"
    // because mirror-match z values are ~50/50; skipping its gradient
    // keeps the value head at initialization (random predictions, but
    // not actively wrong) so MCTS doesn't get misled by a degenerate
    // pessimist value head.
    bool   freeze_value_head = false;
};

class Trainer {
public:
    Trainer(::riftbound::ml::V2Model model,
            const TrainerConfig& cfg,
            torch::Device device = torch::kCPU);

    /// Run one optimizer step on a batch.
    /// Returns (policy_loss, value_loss, total_loss).
    std::tuple<float, float, float>
    step(const std::vector<ReplaySample>& batch);

    /// Save model state to `path`. Use `torch::load` (or our own
    /// LibTorchEvaluator path constructor) to read back.
    void save(const std::string& path);

    /// Load model state from `path`. Mismatched config → loud failure.
    void load(const std::string& path);

    ::riftbound::ml::V2Model& model() { return model_; }

    /// Mutate per-iter training behavior. Called by the iter loop in
    /// config_driver to switch between bootstrap config (entropy
    /// regularization on, value head frozen) and post-bootstrap config
    /// (both off).
    void setEntropyCoef(double c) { cfg_.entropy_coef = c; }
    void setFreezeValueHead(bool freeze) { cfg_.freeze_value_head = freeze; }

private:
    ::riftbound::ml::V2Model model_;
    TrainerConfig cfg_;
    torch::Device device_;
    std::unique_ptr<torch::optim::AdamW> optimizer_;
};

} // namespace riftbound::training
