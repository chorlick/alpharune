#include "training/trainer.h"

#include "ml/entity_tokens.h"

#include <nlohmann/json.hpp>

#include <cstring>
#include <fstream>
#include <limits>

namespace riftbound::training {

namespace {

// Stack the entity-token snapshots from a batch of ReplaySamples into
// one set of V2Model input tensors with leading dim B. All tensors are
// CPU-resident; caller moves to device once.
struct StackedInputs {
    torch::Tensor card_ids;        // (B, N) long
    torch::Tensor zone_ids;        // (B, N) long
    torch::Tensor domain_ids;      // (B, N) long
    torch::Tensor stance_ids;      // (B, N) long
    torch::Tensor stats;           // (B, N, kEntityStatsDim) float
    torch::Tensor is_perspective;  // (B, N) long
    torch::Tensor chain_index;     // (B, N) long
    torch::Tensor spatial_node;    // (B, N) long
    torch::Tensor token_mask;      // (B, N) bool
};
StackedInputs stackBatch(const std::vector<ReplaySample>& batch) {
    const int B = static_cast<int>(batch.size());
    const int N = ::riftbound::ml::kMaxEntityTokens;
    const int S = ::riftbound::ml::kEntityStatsDim;

    auto long_opts  = torch::TensorOptions().dtype(torch::kLong);
    auto bool_opts  = torch::TensorOptions().dtype(torch::kBool);
    auto float_opts = torch::TensorOptions().dtype(torch::kFloat);

    StackedInputs s;
    s.card_ids       = torch::empty({B, N}, long_opts);
    s.zone_ids       = torch::empty({B, N}, long_opts);
    s.domain_ids     = torch::empty({B, N}, long_opts);
    s.stance_ids     = torch::empty({B, N}, long_opts);
    s.stats          = torch::empty({B, N, S}, float_opts);
    s.is_perspective = torch::empty({B, N}, long_opts);
    s.chain_index    = torch::empty({B, N}, long_opts);
    s.spatial_node   = torch::empty({B, N}, long_opts);
    s.token_mask     = torch::empty({B, N}, bool_opts);

    auto card_acc       = s.card_ids.accessor<int64_t, 2>();
    auto zone_acc       = s.zone_ids.accessor<int64_t, 2>();
    auto dom_acc        = s.domain_ids.accessor<int64_t, 2>();
    auto sta_acc        = s.stance_ids.accessor<int64_t, 2>();
    auto per_acc        = s.is_perspective.accessor<int64_t, 2>();
    auto chain_acc      = s.chain_index.accessor<int64_t, 2>();
    auto spatial_acc    = s.spatial_node.accessor<int64_t, 2>();
    auto mask_acc       = s.token_mask.accessor<bool, 2>();
    auto stats_acc      = s.stats.accessor<float, 3>();

    for (int i = 0; i < B; ++i) {
        const auto& t = batch[i].tokens;
        for (int n = 0; n < N; ++n) {
            card_acc[i][n]    = t.card_def_id[n];
            zone_acc[i][n]    = t.zone_id[n];
            dom_acc[i][n]     = t.domain_id[n];
            sta_acc[i][n]     = t.stance_id[n];
            per_acc[i][n]     = t.is_perspective[n];
            chain_acc[i][n]   = t.chain_index[n];
            spatial_acc[i][n] = t.spatial_node[n];
            mask_acc[i][n]    = (t.valid_mask[n] != 0);
            for (int k = 0; k < S; ++k) {
                stats_acc[i][n][k] = t.stats[n * S + k];
            }
        }
    }
    return s;
}

}  // namespace

Trainer::Trainer(::riftbound::ml::V2Model model,
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
Trainer::step(const std::vector<ReplaySample>& batch) {
    if (batch.empty()) return {0.0f, 0.0f, 0.0f};

    const auto& cfg = model_->config();
    const int B = static_cast<int>(batch.size());

    // ── Stack entity-token snapshots into batched V2 inputs ──
    auto inp = stackBatch(batch);
    inp.card_ids       = inp.card_ids.to(device_);
    inp.zone_ids       = inp.zone_ids.to(device_);
    inp.domain_ids     = inp.domain_ids.to(device_);
    inp.stance_ids     = inp.stance_ids.to(device_);
    inp.stats          = inp.stats.to(device_);
    inp.is_perspective = inp.is_perspective.to(device_);
    inp.chain_index    = inp.chain_index.to(device_);
    inp.spatial_node   = inp.spatial_node.to(device_);
    inp.token_mask     = inp.token_mask.to(device_);

    // ── Build dense policy target [B, num_action_slots] (mostly zeros) ──
    auto policy_target = torch::zeros({B, cfg.num_action_slots},
                                       torch::TensorOptions().dtype(torch::kFloat32));
    auto legal_mask    = torch::zeros({B, cfg.num_action_slots},
                                       torch::TensorOptions().dtype(torch::kBool));
    {
        auto pt = policy_target.accessor<float, 2>();
        auto lm = legal_mask.accessor<bool, 2>();
        for (int i = 0; i < B; ++i) {
            const auto& la = batch[i].legal_actions;
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

    // ── Build value target [B] ──
    auto value_target = torch::empty({B, 1},
                                      torch::TensorOptions().dtype(torch::kFloat32));
    {
        float* vd = value_target.data_ptr<float>();
        for (int i = 0; i < B; ++i) vd[i] = batch[i].value;
    }
    value_target = value_target.to(device_);

    // ── Forward + loss ──
    model_->train();
    auto out = model_->forward(
        inp.card_ids, inp.zone_ids, inp.domain_ids, inp.stance_ids,
        inp.stats, inp.is_perspective, inp.chain_index,
        inp.spatial_node, inp.token_mask);

    // Mask illegal logits to -inf so log_softmax normalizes over legal
    // actions only. Without this, illegal slots would absorb gradient
    // mass and the policy head would learn to put probability on
    // actions it will never play.
    auto neg_inf = torch::full_like(
        out.flat_policy_logits, -std::numeric_limits<float>::infinity());
    auto masked_logits = torch::where(legal_mask, out.flat_policy_logits, neg_inf);

    auto log_probs = torch::log_softmax(masked_logits, /*dim=*/1);
    auto contributions = policy_target * log_probs;
    contributions = torch::where(
        policy_target > 0, contributions,
        torch::zeros_like(contributions));
    auto policy_loss = -contributions.sum(/*dim=*/1).mean();

    // Phase 6t — entropy regularization. Compute model entropy on the
    // SAME masked log_probs (legal-only). Add -entropy_coef * H to the
    // loss so peaked policies get penalized. When entropy_coef=0
    // (default), this is a no-op. Use during bootstrap to prevent
    // collapse to a single dominant action (observed: 80% EndTurn).
    //
    // Numerical care: illegal slots have log_prob = -inf and prob = 0.
    // `probs * log_probs` evaluates 0 * -inf = NaN, which propagates
    // through autograd even when guarded by torch::where (where
    // evaluates BOTH branches). We replace illegal log_probs with 0
    // BEFORE the multiplication so the bad product never gets formed.
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

    // Phase 6t — freeze_value_head: compute value_loss for reporting
    // but don't back-propagate it. Used during bootstrap to keep the
    // value head from collapsing to "predict zero" on the z-balanced
    // mirror-match data.
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

void Trainer::save(const std::string& path) {
    torch::save(model_, path);
    // Also persist a JSON sidecar of the V2ModelConfig so the
    // path-loading inference path (LibTorchEvaluator) can reconstruct
    // a shape-matching model. Without this, a model trained with
    // non-default hyperparams (e.g. d_model=128 vs the default 512)
    // would fail at torch::load with a tensor-shape mismatch.
    nlohmann::json j;
    j["kind"]                = "v2";
    const auto& c = model_->config();
    j["num_card_defs"]       = c.num_card_defs;
    j["num_zones"]           = c.num_zones;
    j["num_domains"]         = c.num_domains;
    j["num_stances"]         = c.num_stances;
    j["num_spatial"]         = c.num_spatial;
    j["max_entities"]        = c.max_entities;
    j["stats_dim"]           = c.stats_dim;
    j["num_action_types"]    = c.num_action_types;
    j["max_chain_index"]     = c.max_chain_index;
    j["card_embed_dim"]      = c.card_embed_dim;
    j["zone_embed_dim"]      = c.zone_embed_dim;
    j["domain_embed_dim"]    = c.domain_embed_dim;
    j["stance_embed_dim"]    = c.stance_embed_dim;
    j["stats_proj_dim"]      = c.stats_proj_dim;
    j["perspective_dim"]     = c.perspective_dim;
    j["spatial_dim"]         = c.spatial_dim;
    j["d_model"]             = c.d_model;
    j["encoder_layers"]      = c.encoder_layers;
    j["encoder_heads"]       = c.encoder_heads;
    j["encoder_ffn_dim"]     = c.encoder_ffn_dim;
    j["encoder_dropout"]     = c.encoder_dropout;
    j["spatial_attn_layers"] = c.spatial_attn_layers;
    j["spatial_attn_heads"]  = c.spatial_attn_heads;
    j["num_edge_types"]      = c.num_edge_types;
    j["policy_hidden"]       = c.policy_hidden;
    j["value_hidden"]        = c.value_hidden;
    j["num_action_slots"]    = c.num_action_slots;
    std::ofstream(path + ".config.json") << j.dump(2);
}

void Trainer::load(const std::string& path) {
    torch::load(model_, path);
    model_->to(device_);
}

} // namespace riftbound::training
