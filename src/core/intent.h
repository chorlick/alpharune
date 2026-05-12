#pragma once
/// @file intent.h
/// Intent (Command) pattern — every player action is an Intent.
///
/// The engine enumerates legal intents for the active player,
/// the agent selects one, and the engine validates + executes it.
/// This gives us: replay capability, legal action enumeration for ML,
/// and clean separation between decision-making and game logic.

#include "types.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace riftbound {

struct Intent {
    IntentType type = IntentType::Concede;
    PlayerId player = PlayerId::None;

    // ── Payloads (populated based on type) ──

    // PlayCard, PlayReaction, PlayActionCard
    GameObjectId card = kInvalidId;
    std::optional<LocationId> play_location;   // where to play a unit

    // StandardMove — units to move and destination
    std::vector<GameObjectId> units_to_move;
    std::optional<LocationId> move_destination;

    // ActivateAbility, ActivateReactionAbility, ActivateActionAbility
    AbilityId ability = kInvalidId;
    GameObjectId ability_source = kInvalidId;

    // Targeting — spells/abilities that choose game objects
    std::vector<GameObjectId> targets;

    // AssignCombatDamage — ordered damage assignments
    std::vector<DamageAssignment> damage_assignments;

    // MulliganDecision — cards to set aside
    std::vector<GameObjectId> cards_to_mulligan;

    // ChooseBattlefield — for setup
    BattlefieldId chosen_battlefield = kInvalidId;

    // PlayFirstDecision — true = go first, false = go second
    bool choose_first = true;

    // SideboardSwap — pairs of (out, in)
    std::vector<std::pair<GameObjectId, GameObjectId>> sideboard_swaps;

    // Generic choice payload (for prompted choices during resolution)
    std::vector<GameObjectId> chosen_objects;
    std::optional<int> chosen_value;

    // ── Factory methods for readability ──

    static Intent endTurn(PlayerId p) {
        Intent i;
        i.type = IntentType::EndTurn;
        i.player = p;
        return i;
    }

    static Intent passPriority(PlayerId p) {
        Intent i;
        i.type = IntentType::PassPriority;
        i.player = p;
        return i;
    }

    static Intent passFocus(PlayerId p) {
        Intent i;
        i.type = IntentType::PassFocus;
        i.player = p;
        return i;
    }

    static Intent concede(PlayerId p) {
        Intent i;
        i.type = IntentType::Concede;
        i.player = p;
        return i;
    }

    static Intent standardMove(PlayerId p, std::vector<GameObjectId> units,
                                LocationId dest) {
        Intent i;
        i.type = IntentType::StandardMove;
        i.player = p;
        i.units_to_move = std::move(units);
        i.move_destination = dest;
        return i;
    }

    static Intent mulligan(PlayerId p, std::vector<GameObjectId> cards) {
        Intent i;
        i.type = IntentType::MulliganDecision;
        i.player = p;
        i.cards_to_mulligan = std::move(cards);
        return i;
    }

    static Intent chooseBattlefield(PlayerId p, BattlefieldId bf) {
        Intent i;
        i.type = IntentType::ChooseBattlefield;
        i.player = p;
        i.chosen_battlefield = bf;
        return i;
    }

    static Intent assignCombatDamage(PlayerId p,
                                      std::vector<DamageAssignment> assignments) {
        Intent i;
        i.type = IntentType::AssignCombatDamage;
        i.player = p;
        i.damage_assignments = std::move(assignments);
        return i;
    }

    static Intent playFirst(PlayerId p, bool first) {
        Intent i;
        i.type = IntentType::PlayFirstDecision;
        i.player = p;
        i.choose_first = first;
        return i;
    }

    /// Debug description
    std::string describe() const;
};

} // namespace riftbound
