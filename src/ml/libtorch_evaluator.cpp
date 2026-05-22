#include "ml/libtorch_evaluator.h"
#include "ml/libtorch_evaluator_with_model.h"
#include "ml/v2_model.h"
#include "ml/v3_model.h"
#include "ml/entity_tokens.h"
#include "ml/feature_extractor.h"

#include "openspiel/riftbound_game.h"
#include "openspiel/riftbound_state.h"

#include "open_spiel/spiel.h"

#include <nlohmann/json.hpp>

#include <torch/torch.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace riftbound::ml {

namespace {

// Pull entity tokens out of an open_spiel::State by casting to
// RiftboundState. Throws if the cast fails (programmer error — this
// evaluator is Riftbound-only).
EntityTokens extractTokensFor(const ::open_spiel::State& state, int perspective) {
    const auto& rb_state = dynamic_cast<const ::riftbound::openspiel::RiftboundState&>(state);
    const auto& rb_game  = dynamic_cast<const ::riftbound::openspiel::RiftboundGame&>(*state.GetGame());
    auto pid = (perspective == 0) ? ::riftbound::PlayerId::Player1
                                  : ::riftbound::PlayerId::Player2;
    return extractEntityTokens(rb_state.engineState(), pid, rb_game.cardDb());
}

// Pack one EntityTokens into a batched set of tensors of leading dim B=1.
// Returned tensors live on CPU; caller may .to(device) if needed.
struct V2InputTensors {
    torch::Tensor card_ids;
    torch::Tensor zone_ids;
    torch::Tensor domain_ids;
    torch::Tensor stance_ids;
    torch::Tensor stats;
    torch::Tensor is_perspective;
    torch::Tensor chain_index;
    torch::Tensor spatial_node;
    torch::Tensor token_mask;
};
V2InputTensors packOne(const EntityTokens& t) {
    // from_blob is non-owning — we clone() so the tensor outlives `t`.
    auto long_opts = torch::TensorOptions().dtype(torch::kLong);
    auto bool_opts = torch::TensorOptions().dtype(torch::kBool);
    auto float_opts = torch::TensorOptions().dtype(torch::kFloat);

    const int N = static_cast<int>(t.card_def_id.size());
    auto from_int32 = [&](const std::vector<int32_t>& v) {
        // (1, N) int64 — cast through int32 buffer to int64.
        return torch::from_blob(const_cast<int32_t*>(v.data()), {1, N},
                                torch::TensorOptions().dtype(torch::kInt32))
                    .to(torch::kLong)
                    .clone();
    };
    V2InputTensors out;
    out.card_ids       = from_int32(t.card_def_id);
    out.zone_ids       = from_int32(t.zone_id);
    out.domain_ids     = from_int32(t.domain_id);
    out.stance_ids     = from_int32(t.stance_id);
    out.is_perspective = from_int32(t.is_perspective);
    out.chain_index    = from_int32(t.chain_index);
    out.spatial_node   = from_int32(t.spatial_node);
    // Stats: (1, N, kEntityStatsDim) float.
    out.stats = torch::from_blob(const_cast<float*>(t.stats.data()),
                                  {1, N, ::riftbound::ml::kEntityStatsDim},
                                  float_opts)
                    .clone();
    // valid_mask -> bool tensor.
    auto mask_int = from_int32(t.valid_mask);  // long (1, N), 0/1
    out.token_mask = mask_int.to(torch::kBool);
    return out;
}

class LibTorchEvaluator : public ::open_spiel::algorithms::Evaluator {
public:
    LibTorchEvaluator(const std::string& checkpoint_path,
                       const V2ModelConfig& cfg)
        : cfg_(cfg), model_(cfg) {
        if (!checkpoint_path.empty()) {
            // Force-load to CPU. Checkpoints saved on CUDA (training box)
            // would otherwise try to materialize CUDA tensors on a
            // CPU-only build and crash in aten::empty_strided.
            torch::load(model_, checkpoint_path, torch::kCPU);
        }
        model_->eval();
    }

    explicit LibTorchEvaluator(V2Model shared)
        : cfg_(shared->config()), model_(std::move(shared)) {
        model_->eval();
    }

    std::vector<double> Evaluate(const ::open_spiel::State& state) override {
        if (state.IsTerminal()) {
            return state.Returns();
        }
        auto v_current = forwardValue(state);
        const auto cp = state.CurrentPlayer();
        std::vector<double> per_player(2, 0.0);
        if (cp == 0)      { per_player[0] = v_current;  per_player[1] = -v_current; }
        else if (cp == 1) { per_player[0] = -v_current; per_player[1] =  v_current; }
        return per_player;
    }

    ::open_spiel::ActionsAndProbs
    Prior(const ::open_spiel::State& state) override {
        auto legal = state.LegalActions();
        if (legal.empty()) return {};

        auto logits = forwardPolicy(state);
        double max_logit = -std::numeric_limits<double>::infinity();
        std::vector<double> legal_logits;
        legal_logits.reserve(legal.size());
        for (auto a : legal) {
            double l = (a >= 0 && a < static_cast<int64_t>(logits.size()))
                        ? logits[a] : 0.0;
            legal_logits.push_back(l);
            if (l > max_logit) max_logit = l;
        }
        double sum = 0.0;
        for (auto& l : legal_logits) {
            l = std::exp(l - max_logit);
            sum += l;
        }
        if (sum <= 0.0) sum = 1.0;

        ::open_spiel::ActionsAndProbs out;
        out.reserve(legal.size());
        for (size_t i = 0; i < legal.size(); ++i) {
            out.emplace_back(legal[i], legal_logits[i] / sum);
        }
        return out;
    }

private:
    V2ModelImpl::Output forwardOnce(const ::open_spiel::State& state) {
        auto cp = state.CurrentPlayer();
        if (cp < 0) cp = 0;
        auto tokens = extractTokensFor(state, cp);
        auto in = packOne(tokens);
        // packOne emits CPU tensors; move them to the model's device so
        // the embedding/matmul kernels match. Without this, a model
        // moved to CUDA (by Trainer::Trainer in training mode) crashes
        // in EmbeddingImpl::forward with a device-mismatch error on
        // the very first self-play forward pass.
        auto device = model_->parameters().begin()->device();
        if (in.card_ids.device() != device) {
            in.card_ids       = in.card_ids.to(device);
            in.zone_ids       = in.zone_ids.to(device);
            in.domain_ids     = in.domain_ids.to(device);
            in.stance_ids     = in.stance_ids.to(device);
            in.stats          = in.stats.to(device);
            in.is_perspective = in.is_perspective.to(device);
            in.chain_index    = in.chain_index.to(device);
            in.spatial_node   = in.spatial_node.to(device);
            in.token_mask     = in.token_mask.to(device);
        }
        torch::NoGradGuard no_grad;
        return model_->forward(in.card_ids, in.zone_ids, in.domain_ids,
                                in.stance_ids, in.stats, in.is_perspective,
                                in.chain_index, in.spatial_node, in.token_mask);
    }

    double forwardValue(const ::open_spiel::State& state) {
        return forwardOnce(state).value.item<double>();
    }

    std::vector<double> forwardPolicy(const ::open_spiel::State& state) {
        auto out = forwardOnce(state);
        // Pull to CPU + float64 before the raw memcpy. If the model is
        // on CUDA, flat_policy_logits lives on the GPU; memcpy on a
        // device pointer would segfault.
        auto flat = out.flat_policy_logits.contiguous().view({-1})
                        .to(torch::kCPU).to(torch::kFloat64);
        std::vector<double> v(static_cast<size_t>(cfg_.num_action_slots));
        std::memcpy(v.data(), flat.data_ptr<double>(),
                    v.size() * sizeof(double));
        return v;
    }

    V2ModelConfig cfg_;
    V2Model model_;
};

} // namespace

// If `<checkpoint_path>.config.json` exists (written by Trainer::save),
// parse it into a V2ModelConfig so the deserialized model has the
// matching shape. Caller-supplied `cfg` overrides the sidecar.
namespace {
bool loadConfigSidecar(const std::string& checkpoint_path,
                       V2ModelConfig& out) {
    auto side = checkpoint_path + ".config.json";
    if (checkpoint_path.empty() || !std::filesystem::exists(side)) {
        return false;
    }
    nlohmann::json j;
    std::ifstream(side) >> j;
    auto get_int = [&](const char* k, int& dst) {
        if (j.contains(k)) dst = j[k].get<int>();
    };
    auto get_double = [&](const char* k, double& dst) {
        if (j.contains(k)) dst = j[k].get<double>();
    };
    get_int("num_card_defs",       out.num_card_defs);
    get_int("num_zones",           out.num_zones);
    get_int("num_domains",         out.num_domains);
    get_int("num_stances",         out.num_stances);
    get_int("num_spatial",         out.num_spatial);
    get_int("max_entities",        out.max_entities);
    get_int("stats_dim",           out.stats_dim);
    get_int("num_action_types",    out.num_action_types);
    get_int("max_chain_index",     out.max_chain_index);
    get_int("card_embed_dim",      out.card_embed_dim);
    get_int("zone_embed_dim",      out.zone_embed_dim);
    get_int("domain_embed_dim",    out.domain_embed_dim);
    get_int("stance_embed_dim",    out.stance_embed_dim);
    get_int("stats_proj_dim",      out.stats_proj_dim);
    get_int("perspective_dim",     out.perspective_dim);
    get_int("spatial_dim",         out.spatial_dim);
    get_int("d_model",             out.d_model);
    get_int("encoder_layers",      out.encoder_layers);
    get_int("encoder_heads",       out.encoder_heads);
    get_int("encoder_ffn_dim",     out.encoder_ffn_dim);
    get_double("encoder_dropout",  out.encoder_dropout);
    get_int("spatial_attn_layers", out.spatial_attn_layers);
    get_int("spatial_attn_heads",  out.spatial_attn_heads);
    get_int("num_edge_types",      out.num_edge_types);
    get_int("policy_hidden",       out.policy_hidden);
    get_int("value_hidden",        out.value_hidden);
    get_int("num_action_slots",    out.num_action_slots);
    return true;
}
}  // namespace

namespace {
// Read the "kind" field from a sidecar JSON. Returns "v2" if no
// sidecar or no kind field — preserves back-compat with pre-Phase-6v
// checkpoints.
std::string readCheckpointKind(const std::string& checkpoint_path) {
    auto side = checkpoint_path + ".config.json";
    if (checkpoint_path.empty() || !std::filesystem::exists(side)) return "v2";
    nlohmann::json j;
    std::ifstream(side) >> j;
    if (j.contains("kind")) return j["kind"].get<std::string>();
    return "v2";
}

// Load a V3 sidecar (mirror of loadConfigSidecar but for V3 fields).
bool loadV3ConfigSidecar(const std::string& checkpoint_path,
                          V3ModelConfig& out) {
    auto side = checkpoint_path + ".config.json";
    if (checkpoint_path.empty() || !std::filesystem::exists(side)) return false;
    nlohmann::json j;
    std::ifstream(side) >> j;
    auto gi = [&](const char* k, int& dst) { if (j.contains(k)) dst = j[k].get<int>(); };
    gi("input_dim",        out.input_dim);
    gi("trunk_hidden",     out.trunk_hidden);
    gi("trunk_blocks",     out.trunk_blocks);
    gi("policy_hidden",    out.policy_hidden);
    gi("value_hidden",     out.value_hidden);
    gi("num_action_slots", out.num_action_slots);
    return true;
}
}  // namespace

// makeLibTorchEvaluator moved below the V3 evaluator definition so it
// can reference LibTorchEvaluatorV3. See dispatch block at end of file.

std::shared_ptr<::open_spiel::algorithms::Evaluator>
makeEvaluatorFromModel(V2Model model) {
    return std::make_shared<LibTorchEvaluator>(std::move(model));
}

// ── V3 (flat-feature MLP) evaluator ─────────────────────────────────────
//
// Phase 6v. Same Evaluator interface as the V2 path above, but feeds
// the flat state-feature vector (kStateFeatureDim floats) into a V3
// model. Used by self_play when cfg.model.kind == "v3".

namespace {

// Build a (1, kStateFeatureDim) input tensor from an OpenSpiel state.
torch::Tensor extractV3InputFor(const ::open_spiel::State& state,
                                 int perspective) {
    const auto& rb_state =
        dynamic_cast<const ::riftbound::openspiel::RiftboundState&>(state);
    const auto& rb_game =
        dynamic_cast<const ::riftbound::openspiel::RiftboundGame&>(*state.GetGame());
    auto pid = (perspective == 0) ? ::riftbound::PlayerId::Player1
                                  : ::riftbound::PlayerId::Player2;
    auto feats = extractStateFeatures(rb_state.engineState(), pid,
                                       rb_game.cardDb());
    // from_blob is non-owning; clone so the tensor outlives `feats`.
    auto t = torch::from_blob(feats.data(),
                              {1, static_cast<long>(feats.size())},
                              torch::TensorOptions().dtype(torch::kFloat))
                  .clone();
    return t;
}

class LibTorchEvaluatorV3 : public ::open_spiel::algorithms::Evaluator {
public:
    LibTorchEvaluatorV3(const std::string& checkpoint_path,
                         const V3ModelConfig& cfg)
        : cfg_(cfg), model_(cfg) {
        if (!checkpoint_path.empty()) {
            // Force-load to CPU (matches V2 evaluator). Caller may
            // .to(device) afterward via model().
            torch::load(model_, checkpoint_path, torch::kCPU);
        }
        model_->eval();
    }

    explicit LibTorchEvaluatorV3(V3Model shared)
        : cfg_(shared->config()), model_(std::move(shared)) {
        model_->eval();
    }

    std::vector<double> Evaluate(const ::open_spiel::State& state) override {
        if (state.IsTerminal()) {
            return state.Returns();
        }
        auto v_current = forwardValue(state);
        const auto cp = state.CurrentPlayer();
        std::vector<double> per_player(2, 0.0);
        if (cp == 0)      { per_player[0] = v_current;  per_player[1] = -v_current; }
        else if (cp == 1) { per_player[0] = -v_current; per_player[1] =  v_current; }
        return per_player;
    }

    ::open_spiel::ActionsAndProbs
    Prior(const ::open_spiel::State& state) override {
        auto legal = state.LegalActions();
        if (legal.empty()) return {};
        auto logits = forwardPolicy(state);
        double max_logit = -std::numeric_limits<double>::infinity();
        std::vector<double> legal_logits;
        legal_logits.reserve(legal.size());
        for (auto a : legal) {
            double l = (a >= 0 && a < static_cast<int64_t>(logits.size()))
                        ? logits[a] : 0.0;
            legal_logits.push_back(l);
            if (l > max_logit) max_logit = l;
        }
        double sum = 0.0;
        for (auto& l : legal_logits) {
            l = std::exp(l - max_logit);
            sum += l;
        }
        if (sum <= 0.0) sum = 1.0;

        ::open_spiel::ActionsAndProbs out;
        out.reserve(legal.size());
        for (size_t i = 0; i < legal.size(); ++i) {
            out.emplace_back(legal[i], legal_logits[i] / sum);
        }
        return out;
    }

private:
    V3ModelImpl::Output forwardOnce(const ::open_spiel::State& state) {
        auto cp = state.CurrentPlayer();
        if (cp < 0) cp = 0;
        auto x = extractV3InputFor(state, cp);
        auto device = model_->parameters().begin()->device();
        if (x.device() != device) x = x.to(device);
        torch::NoGradGuard no_grad;
        return model_->forward(x);
    }

    double forwardValue(const ::open_spiel::State& state) {
        return forwardOnce(state).value.item<double>();
    }

    std::vector<double> forwardPolicy(const ::open_spiel::State& state) {
        auto out = forwardOnce(state);
        auto flat = out.flat_policy_logits.contiguous().view({-1})
                        .to(torch::kCPU).to(torch::kFloat64);
        std::vector<double> v(static_cast<size_t>(cfg_.num_action_slots));
        std::memcpy(v.data(), flat.data_ptr<double>(),
                    v.size() * sizeof(double));
        return v;
    }

    V3ModelConfig cfg_;
    V3Model model_;
};

}  // namespace

std::shared_ptr<::open_spiel::algorithms::Evaluator>
makeEvaluatorFromModel(V3Model model) {
    return std::make_shared<LibTorchEvaluatorV3>(std::move(model));
}

// Phase 6v — V2/V3-aware path-loading factory. Reads the sidecar's
// "kind" field to pick the right evaluator class. Lives at the bottom
// of the file so both LibTorchEvaluator and LibTorchEvaluatorV3 are
// visible.
std::shared_ptr<::open_spiel::algorithms::Evaluator>
makeLibTorchEvaluator(const std::string& checkpoint_path,
                       const V2ModelConfig* cfg) {
    if (readCheckpointKind(checkpoint_path) == "v3") {
        V3ModelConfig v3_cfg;
        loadV3ConfigSidecar(checkpoint_path, v3_cfg);
        return std::make_shared<LibTorchEvaluatorV3>(checkpoint_path, v3_cfg);
    }
    V2ModelConfig effective_cfg = cfg ? *cfg : V2ModelConfig{};
    if (!cfg) {
        loadConfigSidecar(checkpoint_path, effective_cfg);
    }
    return std::make_shared<LibTorchEvaluator>(checkpoint_path, effective_cfg);
}

} // namespace riftbound::ml
