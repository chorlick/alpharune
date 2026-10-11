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

/// The player a decision belongs to. Every Intent the engine offers carries
/// the deciding player, and that is not always the turn player: the opponent
/// decides the second mulligan, chain priority, showdown focus, combat damage
/// and their own triggers' choices. Falls back to the turn player only for an
/// empty list.
inline PlayerId decidingPlayer(const GameState& state,
                               const std::vector<Intent>& legal_actions) {
    if (!legal_actions.empty() &&
        legal_actions.front().player != PlayerId::None) {
        return legal_actions.front().player;
    }
    return state.turn.turn_player;
}

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
