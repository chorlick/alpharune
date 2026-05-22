#pragma once
/// @file trainer_v3.h
/// AlphaZero-style trainer for the V3 MLP+residual model.
///
/// Mirrors `Trainer` (V2's entity-token trainer) in API surface so
/// `config_driver` can swap between them based on `cfg.model.kind`.
/// Differences from V2 trainer:
///   * Input is the flat 4623-dim observation tensor stored in
///     `ReplaySample::flat_state` (not the entity-token snapshot).
///   * Model is `V3Model`, which has only flat_policy_logits + value
///     outputs (no pointer heads).
///
/// One `step()` call:
///   1. Stacks a batch of ReplaySamples' flat_state vectors into a
///      (B, input_dim) input tensor.
///   2. Forward pass → V3 Output (flat_policy_logits + value).
///   3. Masks illegal logits to -inf using each sample's legal_actions.
///   4. Loss = policy cross-entropy + value MSE (+ optional entropy
///      regularization).
///   5. Backward + AdamW step + gradient clipping.
///
/// See trainer.h for the V2 equivalent and TrainerConfig docs.

#include "ml/v3_model.h"
#include "training/replay_buffer.h"
#include "training/trainer.h"   // for TrainerConfig — reused as-is

#include <torch/torch.h>

#include <memory>
#include <string>
#include <tuple>

namespace riftbound::training {

class TrainerV3 {
public:
    TrainerV3(::riftbound::ml::V3Model model,
              const TrainerConfig& cfg,
              torch::Device device = torch::kCPU);

    /// Run one optimizer step on a batch.
    /// Returns (policy_loss, value_loss, total_loss).
    std::tuple<float, float, float>
    step(const std::vector<ReplaySample>& batch);

    /// Save / load V3 model state via torch::save / torch::load.
    void save(const std::string& path);
    void load(const std::string& path);

    ::riftbound::ml::V3Model& model() { return model_; }

    // Same knobs as V2 Trainer — config_driver flips these per-iter
    // during the bootstrap → model-driven transition.
    void setEntropyCoef(double c)        { cfg_.entropy_coef = c; }
    void setFreezeValueHead(bool freeze) { cfg_.freeze_value_head = freeze; }

private:
    ::riftbound::ml::V3Model model_;
    TrainerConfig cfg_;
    torch::Device device_;
    std::unique_ptr<torch::optim::AdamW> optimizer_;
};

} // namespace riftbound::training
