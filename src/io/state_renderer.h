#pragma once
/// @file state_renderer.h
/// ASCII game state renderer — produces human-readable board state text
/// matching the physical playmat layout.

#include "core/card_db.h"
#include "core/game_state.h"
#include "core/intent.h"

#include <string>
#include <vector>

namespace riftbound {

class StateRenderer {
public:
    explicit StateRenderer(const CardDB& db) : db_(db) {}

    /// Render the full board state for both players.
    std::string render(const GameState& state) const;

    /// Render a single game object inline: "Name(NM,exh,Asl2)"
    std::string renderObject(const GameState& state, GameObjectId id) const;

    /// Render a list of legal actions in compact format.
    std::string renderActions(const GameState& state,
                               const std::vector<Intent>& actions) const;

    /// Render the chosen action with card names resolved from state.
    std::string renderChosenAction(const GameState& state,
                                    const Intent& intent) const;

    /// Render a full decision block (header + actions + chosen).
    /// Returns empty string if the decision is trivial (1 legal action).
    std::string renderDecision(const GameState& state,
                                const std::vector<Intent>& actions,
                                const Intent& chosen) const;

    // Configuration
    int max_width = 80;
    bool show_hand = false;        // show hand contents (debug only)
    bool show_legal_actions = true;

private:
    const CardDB& db_;

    std::string renderPlayerSection(const GameState& state, PlayerId player) const;
    std::string renderPlayerSummary(const GameState& state, PlayerId player) const;
    std::string renderBase(const GameState& state, PlayerId player) const;
    std::string renderBattlefield(const GameState& state, const BattlefieldState& bf) const;
    std::string renderChain(const GameState& state) const;
    std::string renderHandLine(const GameState& state, PlayerId player) const;
    std::string renderTrashLine(const GameState& state, PlayerId player) const;
    std::string renderBanishmentLine(const GameState& state, PlayerId player) const;
    std::string renderRunes(const GameState& state, PlayerId player) const;

    std::string keywordAbbrev(const GameObject& obj) const;
    std::string padRight(const std::string& s, int width) const;
    std::string centerPad(const std::string& s, int width) const;
    std::string horizontalLine(int width, char fill = '-') const;
    std::string boxLine(const std::string& content, int width) const;
};

} // namespace riftbound
