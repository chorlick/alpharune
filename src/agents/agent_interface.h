#pragma once
/// @file agent_interface.h
/// Abstract interface for game-playing agents.
///
/// An agent receives the current game state and a list of legal actions,
/// and returns the action it chooses. This interface is implemented by:
///   - RandomAgent (uniform random selection)
///   - Future: scripted agents, MCTS, neural network policies

#include "core/game_state.h"
#include "core/intent.h"

#include <vector>

namespace riftbound {

class AgentInterface {
public:
    virtual ~AgentInterface() = default;

    /// Select an action from the list of legal actions given the current state.
    virtual Intent selectAction(
        const GameState& state,
        const std::vector<Intent>& legal_actions
    ) = 0;
};

} // namespace riftbound
