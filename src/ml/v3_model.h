#pragma once
/// @file v3_model.h
/// V3 model — MLP + residual stack on flat state features.
///
/// Rationale: V2 (entity-token transformer + spatial fusion + mean-pool
/// aggregation) was found to collapse to a CONSTANT predictor across
/// drastically different game states (probe_v2_model: PERSPECTIVE_WIRING=
/// BROKEN since at least iter_4 of multiple training cycles spanning
/// May 2026). Mean-pool of LayerNorm'd token vectors mathematically
/// converges toward a constant under training pressure — see Phase 6v
/// research notes.
///
/// V3 matches OpenSpiel's reference AlphaZero design for card games:
///   * Fixed-size flat observation tensor (no variable-length seq).
///   * Pure MLP trunk with residual blocks (gradient flow protected by
///     skips, no aggressive per-layer LayerNorm).
///   * Direct policy + value heads from the trunk output.
///
/// V2 remains in-tree as a learning artifact / future research base.
/// Switch between them via `training.model.kind = "v2" | "v3"`.
///
/// Inputs:
///   state_features : FloatTensor (B, kStateFeatureDim) — flat
///                    observation from extractStateFeatures().
///
/// Outputs:
///   flat_policy_logits : (B, num_action_slots) — bare logits, callers
///                        apply legal-mask + cross-entropy downstream.
///   value              : (B, 1) — tanh in [-1, +1].

#include "ml/feature_extractor.h"

#include <torch/torch.h>

#include <vector>

namespace riftbound::ml {

struct V3ModelConfig {
    /// Input dim — flat state features. Default tracks kStateFeatureDim;
    /// configurable so a sidecar can override if the feature layout
    /// changes between checkpoints.
    int input_dim = kStateFeatureDim;

    /// Hidden width of the trunk. 512 matches OpenSpiel's nn_width
    /// default and our V2 d_model, so checkpoints are comparable in
    /// param-count order.
    int trunk_hidden = 512;

    /// Number of residual blocks in the trunk. Each block: Linear →
    /// ReLU → Linear, plus skip + ReLU. 6 blocks ≈ OpenSpiel's
    /// "nn_depth=6" default for the MLP variant.
    int trunk_blocks = 6;

    /// Policy head width.
    int policy_hidden = 256;

    /// Value head width.
    int value_hidden = 128;

    /// Flat policy vocab size. MUST be overridden from the runtime
    /// action vocab (action_vocab.h kVocabSize). Default below is a
    /// floor for the smoke test.
    int num_action_slots = 7141;
};

class V3ModelImpl : public torch::nn::Module {
public:
    explicit V3ModelImpl(const V3ModelConfig& cfg = {});

    /// Output struct — same shape as V2's Output where applicable so
    /// trainer/evaluator code can swap models with minimal branching.
    /// V2-specific pointer-head fields (action_type_logits, source_logits,
    /// target_logits, dest_node_logits) are NOT produced by V3.
    struct Output {
        torch::Tensor flat_policy_logits;   // (B, num_action_slots)
        torch::Tensor value;                // (B, 1)
    };

    /// Forward — flat state-feature input, no token packing required.
    Output forward(torch::Tensor state_features);  // (B, input_dim) float

    const V3ModelConfig& config() const { return cfg_; }

private:
    V3ModelConfig cfg_;

    // Stem: input projection to trunk_hidden.
    torch::nn::Linear stem_{nullptr};

    // Residual blocks. Each block has two Linear layers; stored in
    // vectors with register_module per-element so state-dict keys are
    // deterministic and readable across checkpoints.
    std::vector<torch::nn::Linear> block_a_;   // first Linear of each block
    std::vector<torch::nn::Linear> block_b_;   // second Linear of each block

    // Policy head.
    torch::nn::Linear policy_hidden_{nullptr};
    torch::nn::Linear policy_out_{nullptr};

    // Value head.
    torch::nn::Linear value_hidden_{nullptr};
    torch::nn::Linear value_out_{nullptr};
};
TORCH_MODULE(V3Model);

}  // namespace riftbound::ml
