#include "training/trainer_v3.h"

#include "ml/feature_extractor.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <limits>

namespace riftbound::training {

namespace {

// Stack the flat_state vectors from a batch of ReplaySamples into a
// (B, input_dim) tensor. CPU-resident; caller moves to device once.
torch::Tensor stackFlatStates(const std::vector<ReplaySample>& batch,
                               int input_dim) {
    const int B = static_cast<int>(batch.size());
    auto float_opts = torch::TensorOptions().dtype(torch::kFloat);

    auto out = torch::empty({B, input_dim}, float_opts);
    auto out_acc = out.accessor<float, 2>();
    for (int i = 0; i < B; ++i) {
        const auto& fs = batch[i].flat_state;
        // Defensive: pad zeros if the stored flat_state is shorter
        // than the model's input_dim (e.g. a checkpoint trained on an
        // older feature layout). Truncate if longer.
        const int n = std::min<int>(input_dim, static_cast<int>(fs.size()));
        for (int j = 0; j < n; ++j)              out_acc[i][j] = fs[j];
        for (int j = n; j < input_dim; ++j)      out_acc[i][j] = 0.0f;
    }
    return out;
}

}  // namespace

TrainerV3::TrainerV3(::riftbound::ml::V3Model model,
                      const TrainerConfig& cfg,
                      torch::Device device)
    : model_(std::move(model)), cfg_(cfg), device_(device) {
    model_->to(device_);
    optimizer_ = std::make_unique<torch::optim::AdamW>(
        model_->parameters(),
        torch::optim::AdamWOptions(cfg_.learning_rate)
            .weight_decay(cfg_.weight_decay));
}

std::tuple<float, float, float>
TrainerV3::step(const std::vector<ReplaySample>& batch) {
    if (batch.empty()) return {0.0f, 0.0f, 0.0f};

    const auto& cfg = model_->config();
    const int B = static_cast<int>(batch.size());

    // ── Stack flat-state vectors into (B, input_dim) ──
    auto x = stackFlatStates(batch, cfg.input_dim).to(device_);

    // ── Build dense policy target [B, num_action_slots] (mostly zeros) ──
    auto policy_target = torch::zeros({B, cfg.num_action_slots},
                                       torch::TensorOptions().dtype(torch::kFloat32));
    auto legal_mask    = torch::zeros({B, cfg.num_action_slots},
                                       torch::TensorOptions().dtype(torch::kBool));
    {
        auto pt = policy_target.accessor<float, 2>();
        auto lm = legal_mask.accessor<bool, 2>();
        for (int i = 0; i < B; ++i) {
            const auto& la  = batch[i].legal_actions;
            const auto& pol = batch[i].policy_over_legal;
            for (size_t j = 0; j < la.size(); ++j) {
                int32_t a = la[j];
                if (a < 0 || a >= cfg.num_action_slots) continue;
                lm[i][a] = true;
                if (j < pol.size()) pt[i][a] = pol[j];
            }
        }
    }
    policy_target = policy_target.to(device_);
    legal_mask    = legal_mask.to(device_);

    // ── Build value target [B, 1] ──
    auto value_target = torch::empty({B, 1},
                                      torch::TensorOptions().dtype(torch::kFloat32));
    {
        float* vd = value_target.data_ptr<float>();
        for (int i = 0; i < B; ++i) vd[i] = batch[i].value;
    }
    value_target = value_target.to(device_);

    // ── Forward ──
    model_->train();
    auto out = model_->forward(x);

    // Mask illegal logits to -inf so log_softmax normalizes over legal
    // actions only.
    auto neg_inf = torch::full_like(
        out.flat_policy_logits, -std::numeric_limits<float>::infinity());
    auto masked_logits = torch::where(legal_mask, out.flat_policy_logits, neg_inf);

    auto log_probs = torch::log_softmax(masked_logits, /*dim=*/1);
    auto contributions = policy_target * log_probs;
    contributions = torch::where(
        policy_target > 0, contributions,
        torch::zeros_like(contributions));
    auto policy_loss = -contributions.sum(/*dim=*/1).mean();

    // Entropy regularization — matches V2 Trainer semantics.
    torch::Tensor entropy_term;
    if (cfg_.entropy_coef != 0.0) {
        auto safe_log_probs = log_probs.masked_fill(~legal_mask, 0.0);
        auto probs = torch::exp(log_probs).masked_fill(~legal_mask, 0.0);
        auto model_entropy = -(probs * safe_log_probs).sum(/*dim=*/1).mean();
        entropy_term = -static_cast<float>(cfg_.entropy_coef) * model_entropy;
    } else {
        entropy_term = torch::zeros({}, masked_logits.options());
    }

    auto value_loss = torch::mse_loss(out.value, value_target);

    // freeze_value_head: report value_loss but don't backprop it.
    auto value_term = cfg_.freeze_value_head
        ? value_loss.detach()
        : value_loss;

    auto total_loss = policy_loss + entropy_term + value_term;

    // ── Backward + step ──
    optimizer_->zero_grad();
    total_loss.backward();
    if (cfg_.grad_clip > 0.0) {
        torch::nn::utils::clip_grad_norm_(model_->parameters(), cfg_.grad_clip);
    }
    optimizer_->step();

    return {policy_loss.item<float>(),
            value_loss.item<float>(),
            total_loss.item<float>()};
}

void TrainerV3::save(const std::string& path) {
    torch::save(model_, path);
    // Sidecar JSON — same role as V2's: lets the path-loading evaluator
    // reconstruct a shape-matching model without re-reading the training
    // config. "kind=v3" is the discriminator the factory + probe use to
    // pick the right model class.
    nlohmann::json j;
    j["kind"]             = "v3";
    const auto& c = model_->config();
    j["input_dim"]        = c.input_dim;
    j["trunk_hidden"]     = c.trunk_hidden;
    j["trunk_blocks"]     = c.trunk_blocks;
    j["policy_hidden"]    = c.policy_hidden;
    j["value_hidden"]     = c.value_hidden;
    j["num_action_slots"] = c.num_action_slots;
    std::ofstream(path + ".config.json") << j.dump(2);
}

void TrainerV3::load(const std::string& path) {
    torch::load(model_, path);
}

} // namespace riftbound::training
