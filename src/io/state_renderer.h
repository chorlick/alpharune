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

/// Whose private information a board render may show: the contents of a
/// player's hand and the names of that player's facedown cards. Counts are
/// always shown. The default shows both players (replays, spectating).
struct RenderVisibility {
    bool p1 = true;
    bool p2 = true;

    bool shows(PlayerId player) const {
        return player == PlayerId::Player1 ? p1
             : player == PlayerId::Player2 ? p2 : false;
    }
};

class StateRenderer {
public:
    explicit StateRenderer(const CardDB& db) : db_(db) {}

    /// Render the full board state for both players.
    std::string render(const GameState& state) const;

    /// As above, but a player whose private information `visibility` does
    /// not show has their hand replaced by a count and their facedown cards
    /// left unnamed.
    std::string render(const GameState& state,
                       const RenderVisibility& visibility) const;

    /// Render a single game object inline: "Name(NM,exh,Asl2)"
    std::string renderObject(const GameState& state, GameObjectId id) const;

    /// Render a list of legal actions in compact format.
    std::string renderActions(const GameState& state,
                               const std::vector<Intent>& actions) const;

    /// Render the chosen action with card names resolved from state.
    std::string renderChosenAction(const GameState& state,
                                    const Intent& intent) const;

    /// Print-friendly label for a battlefield. Returns the card name,
    /// with a `(from Px)` suffix when another battlefield on the
    /// board has the same name (so a mirror-match where both players
    /// brought the same BF doesn't show two indistinguishable
    /// "Vilemaw's Lair" entries). `contributed_by` is the disambiguator
    /// — that's the player who supplied the BF in deckbuild, fixed
    /// for the life of the game, NOT the runtime controller (which
    /// changes when a player Scores it).
    static std::string battlefieldLabel(const GameState& state, BattlefieldId bf_id);

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

    std::string renderPlayerSection(const GameState& state, PlayerId player,
                                    bool show_private) const;
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
    /// Pad to `width` like padRight, but never cut: a row that lists game
    /// objects overflows the box border rather than dropping one.
    std::string padRightKeep(const std::string& s, int width) const;
    std::string centerPad(const std::string& s, int width) const;
    std::string horizontalLine(int width, char fill = '-') const;
    std::string boxLine(const std::string& content, int width) const;
};

} // namespace riftbound
