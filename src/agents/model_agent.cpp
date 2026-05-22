#include "agents/model_agent.h"

#include "ml/feature_extractor.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <mutex>
#include <unordered_map>

namespace riftbound {

// ─── ModelSession ───────────────────────────────────────────────────────────

ModelSession::ModelSession(const std::string& path)
    : env(ORT_LOGGING_LEVEL_WARNING, "RiftboundAgent"),
      session(env, path.c_str(), Ort::SessionOptions{}) {

    auto input_info = session.GetInputTypeInfo(0);
    auto tensor_info = input_info.GetTensorTypeAndShapeInfo();
    auto shape = tensor_info.GetShape();
    state_dim = shape.size() > 1 ? shape[1] : shape[0];

    if (session.GetInputCount() >= 2) {
        auto act_info = session.GetInputTypeInfo(1);
        auto act_shape = act_info.GetTensorTypeAndShapeInfo().GetShape();
        if (act_shape.size() >= 2) {
            max_actions = act_shape[1];
            action_dim = act_shape.size() > 2 ? act_shape[2] : 25;
        }
    }
}

namespace {

// Process-wide cache: one ModelSession per unique model path. ONNX Runtime
// sessions are thread-safe for inference, so all ModelAgents on all threads
// share one session per path. Loaded lazily on first request.
std::mutex g_session_mu;
std::unordered_map<std::string, std::shared_ptr<ModelSession>> g_session_cache;

std::shared_ptr<ModelSession> getOrLoadSession(const std::string& path) {
    std::lock_guard<std::mutex> lock(g_session_mu);
    auto it = g_session_cache.find(path);
    if (it != g_session_cache.end()) return it->second;

    auto sess = std::make_shared<ModelSession>(path);
    g_session_cache.emplace(path, sess);

    std::cout << "ModelAgent loaded: " << path
              << " (state_dim=" << sess->state_dim
              << ", action_dim=" << sess->action_dim
              << ", max_actions=" << sess->max_actions << ")\n";
    return sess;
}

} // namespace

// ─── ModelAgent ─────────────────────────────────────────────────────────────

ModelAgent::ModelAgent(const std::string& model_path, const CardDB& card_db,
                       double temperature, uint64_t rng_seed)
    : card_db_(card_db),
      model_(getOrLoadSession(model_path)),
      temperature_(temperature),
      rng_(rng_seed != 0 ? rng_seed : std::random_device{}()) {}

ModelAgent::~ModelAgent() = default;

// ─── Action selection ───────────────────────────────────────────────────────

Intent ModelAgent::selectAction(const GameState& state,
                                 const std::vector<Intent>& legal) {
    if (legal.empty()) return Intent::endTurn(state.turn.turn_player);
    if (legal.size() == 1) return legal[0];

    PlayerId perspective = legal[0].player;

    const int64_t state_dim = model_->state_dim;
    const int64_t action_dim = model_->action_dim;
    const int64_t max_actions = model_->max_actions;

    // Extract natural-sized features, then pad/truncate to whatever dims the
    // loaded model expects (state_dim may exceed kStateFeatureDim if the model
    // reserves slots for future features).
    auto state_features = ml::extractStateFeatures(state, perspective, card_db_);
    state_features.resize(state_dim, 0.0f);

    auto memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

    std::vector<int64_t> state_shape = {1, state_dim};
    auto state_tensor = Ort::Value::CreateTensor<float>(
        memory_info, state_features.data(), state_features.size(),
        state_shape.data(), state_shape.size());

    std::vector<float> action_feats(max_actions * action_dim, 0.0f);
    std::vector<float> action_mask(max_actions, 0.0f);

    int num_legal = std::min(static_cast<int>(legal.size()),
                              static_cast<int>(max_actions));
    for (int i = 0; i < num_legal; ++i) {
        auto af = ml::featurizeAction(legal[i], state);
        af.resize(action_dim, 0.0f);
        for (int j = 0; j < static_cast<int>(action_dim); ++j) {
            action_feats[i * action_dim + j] = af[j];
        }
        action_mask[i] = 1.0f;
    }

    std::vector<int64_t> action_shape = {1, max_actions, action_dim};
    auto action_tensor = Ort::Value::CreateTensor<float>(
        memory_info, action_feats.data(), action_feats.size(),
        action_shape.data(), action_shape.size());

    std::vector<int64_t> mask_shape = {1, max_actions};
    auto mask_tensor = Ort::Value::CreateTensor<float>(
        memory_info, action_mask.data(), action_mask.size(),
        mask_shape.data(), mask_shape.size());

    const char* input_names[] = {"state_features", "action_features", "action_mask"};
    const char* output_names[] = {"action_scores", "value"};
    std::vector<Ort::Value> inputs;
    inputs.push_back(std::move(state_tensor));
    inputs.push_back(std::move(action_tensor));
    inputs.push_back(std::move(mask_tensor));

    auto outputs = model_->session.Run(Ort::RunOptions{nullptr},
                                       input_names, inputs.data(), 3,
                                       output_names, 2);

    auto* scores = outputs[0].GetTensorMutableData<float>();

    if (temperature_ <= 0.0) {
        // Argmax: deterministic, used for evaluation
        int best_idx = 0;
        float best_score = -1e9f;
        for (int i = 0; i < num_legal; ++i) {
            if (scores[i] > best_score) {
                best_score = scores[i];
                best_idx = i;
            }
        }
        return legal[best_idx];
    }

    // Temperature sampling: softmax(scores / T) over the legal-action slice,
    // then sample. Used during self-play data generation for exploration.
    const float inv_t = 1.0f / static_cast<float>(temperature_);
    float max_score = scores[0];
    for (int i = 1; i < num_legal; ++i)
        if (scores[i] > max_score) max_score = scores[i];

    std::vector<float> probs(num_legal);
    float sum = 0.0f;
    for (int i = 0; i < num_legal; ++i) {
        probs[i] = std::exp((scores[i] - max_score) * inv_t);
        sum += probs[i];
    }
    if (!(sum > 0.0f)) {
        // Numerically degenerate. Fall back to uniform.
        std::uniform_int_distribution<int> uni(0, num_legal - 1);
        return legal[uni(rng_)];
    }

    std::uniform_real_distribution<float> dist(0.0f, sum);
    float r = dist(rng_);
    float cum = 0.0f;
    for (int i = 0; i < num_legal; ++i) {
        cum += probs[i];
        if (r <= cum) return legal[i];
    }
    return legal[num_legal - 1];
}

} // namespace riftbound
