#pragma once
/// @file random_agent.h
/// Agent that picks uniformly at random from legal actions.
/// Used for initial corpus generation and engine testing.

#include "agent_interface.h"

#include <random>

namespace riftbound {

class RandomAgent : public AgentInterface {
public:
    explicit RandomAgent(uint64_t seed = 0)
        : rng_(seed == 0 ? std::random_device{}() : seed) {}

    Intent selectAction(
        const GameState& state,
        const std::vector<Intent>& legal_actions
    ) override {
        if (legal_actions.empty()) {
            return Intent::concede(state.turn.turn_player);
        }
        std::uniform_int_distribution<size_t> dist(0, legal_actions.size() - 1);
        return legal_actions[dist(rng_)];
    }

private:
    std::mt19937_64 rng_;
};

} // namespace riftbound
