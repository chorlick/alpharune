#pragma once
/// @file model_agent.h
/// ML-powered agent that uses an ONNX model for action selection.
///
/// Supports two model versions:
/// - V1: predicts action TYPE (14 classes). Picks best type, random among same-type actions.
/// - V2: scores each specific legal action. Featurizes [state + action] per action, picks highest.

#include "agents/agent_interface.h"
#include "core/card_db.h"
#include "core/game_state.h"

#include <onnxruntime_cxx_api.h>
#include <memory>
#include <random>
#include <string>
#include <vector>

namespace riftbound {

/// Shared ONNX session resource. One instance per unique model path, owned
/// by the process-wide cache in model_agent.cpp. Ort::Session::Run() is
/// thread-safe, so many ModelAgents on many threads can share one of these.
struct ModelSession {
    Ort::Env env;
    Ort::Session session;
    int64_t state_dim = 0;
    int64_t action_dim = 25;
    int64_t max_actions = 64;

    ModelSession(const std::string& path);
};

class ModelAgent : public AgentInterface {
public:
    /// @param temperature Sampling temperature. 0 = argmax (deterministic,
    ///   for evaluation). >0 = sample from softmax(scores/T) over legal
    ///   actions (for self-play data generation). Typical T=1.0.
    /// @param rng_seed Seed for the sampling RNG. Ignored when temperature=0.
    ModelAgent(const std::string& model_path, const CardDB& card_db,
               double temperature = 0.0, uint64_t rng_seed = 0);
    ~ModelAgent() override;

    Intent selectAction(const GameState& state,
                        const std::vector<Intent>& legal) override;

private:
    const CardDB& card_db_;
    std::shared_ptr<ModelSession> model_;  // cached, shared across threads

    // Sampling
    double temperature_ = 0.0;
    std::mt19937_64 rng_;
};

} // namespace riftbound
