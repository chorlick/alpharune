#include "ml/v3_model.h"

#include <string>

namespace riftbound::ml {

V3ModelImpl::V3ModelImpl(const V3ModelConfig& cfg)
    : cfg_(cfg),
      stem_(torch::nn::LinearOptions(cfg.input_dim, cfg.trunk_hidden)),
      policy_hidden_(torch::nn::LinearOptions(cfg.trunk_hidden, cfg.policy_hidden)),
      policy_out_(torch::nn::LinearOptions(cfg.policy_hidden, cfg.num_action_slots)),
      value_hidden_(torch::nn::LinearOptions(cfg.trunk_hidden, cfg.value_hidden)),
      value_out_(torch::nn::LinearOptions(cfg.value_hidden, 1)) {
    register_module("stem", stem_);

    // Build the residual trunk. Each block has two Linear layers; the
    // skip connection adds the block input to the post-second-Linear
    // output, then ReLU. No LayerNorm — V3's whole point is to avoid
    // the V2 mean-pool/LayerNorm collapse failure mode.
    block_a_.reserve(cfg.trunk_blocks);
    block_b_.reserve(cfg.trunk_blocks);
    for (int i = 0; i < cfg.trunk_blocks; ++i) {
        auto a = torch::nn::Linear(
            torch::nn::LinearOptions(cfg.trunk_hidden, cfg.trunk_hidden));
        auto b = torch::nn::Linear(
            torch::nn::LinearOptions(cfg.trunk_hidden, cfg.trunk_hidden));
        register_module("block_a_" + std::to_string(i), a);
        register_module("block_b_" + std::to_string(i), b);
        block_a_.push_back(a);
        block_b_.push_back(b);
    }

    register_module("policy_hidden", policy_hidden_);
    register_module("policy_out",    policy_out_);
    register_module("value_hidden",  value_hidden_);
    register_module("value_out",     value_out_);
}

V3ModelImpl::Output V3ModelImpl::forward(torch::Tensor state_features) {
    // state_features: (B, input_dim) float

    // Stem: project to trunk_hidden, then ReLU.
    auto x = torch::relu(stem_->forward(state_features));   // (B, H)

    // Residual blocks.
    for (size_t i = 0; i < block_a_.size(); ++i) {
        auto h = torch::relu(block_a_[i]->forward(x));      // Linear + ReLU
        h = block_b_[i]->forward(h);                         // Linear (no ReLU yet)
        x = torch::relu(x + h);                              // skip + ReLU
    }

    // Policy head: hidden + ReLU + project to vocab.
    auto p_hidden = torch::relu(policy_hidden_->forward(x));
    auto flat_policy_logits = policy_out_->forward(p_hidden);   // (B, num_action_slots)

    // Value head: hidden + ReLU + project to 1 + tanh.
    auto v_hidden = torch::relu(value_hidden_->forward(x));
    auto v = value_out_->forward(v_hidden);                     // (B, 1)
    auto value = torch::tanh(v);

    return Output{
        /*.flat_policy_logits =*/ flat_policy_logits,
        /*.value              =*/ value,
    };
}

}  // namespace riftbound::ml
