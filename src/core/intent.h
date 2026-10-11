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

    // Where this play originated. Default Hand for normal plays from
    // the player's hand. Set to Trash for Fizz / Thrill-of-the-Hunt
    // style plays from the trash zone, Banishment for re-cast effects,
    // ChampionZone for champion plays, Hidden for facedown reveals,
    // ChainZone for chain-time plays (Reactions). Consulted at
    // payCardCost time to apply Rek'Sai-style auto-Accelerate when
    // the source is anything other than Hand AND a friendly Rek'Sai
    // (or similar) is on board.
    enum class PlaySource : uint8_t {
        Hand = 0,
        Trash,
        Banishment,
        ChampionZone,
        Hidden,
        ChainZone,
        MainDeck,   // only staged for a card an effect plays off a deck; no intent carries it
    };
    PlaySource play_source = PlaySource::Hand;

    // StandardMove — units to move and destination
    std::vector<GameObjectId> units_to_move;
    std::optional<LocationId> move_destination;

    // ActivateAbility, ActivateReactionAbility, ActivateActionAbility
    AbilityId ability = kInvalidId;
    GameObjectId ability_source = kInvalidId;
    // Phase 6r — multi-ability per Card. Identifies which of the
    // ability_source's activated abilities is being invoked when the
    // card has more than one. 0 = first/only ability (back-compat
    // default for the existing 51 single-ability cards).
    int ability_index = 0;

    // Targeting — spells/abilities that choose game objects
    std::vector<GameObjectId> targets;

    // AssignCombatDamage — ordered damage assignments
    std::vector<DamageAssignment> damage_assignments;

    // MulliganDecision — cards to set aside
    std::vector<GameObjectId> cards_to_mulligan;

    // HideCard: the battlefield slot. ChooseBattlefield (setup): NOT a slot
    // but the position of the chosen card in the player's battlefield list
    // (GameEngine::setupBattlefields makes it with fromRaw and reads it back
    // with raw(); no battlefield exists yet). One field for both because the
    // action slot and the history token encode it as one number (plans/05
    // item 5.5: a field of its own would change both).
    BattlefieldId chosen_battlefield = kInvalidId;

    // PlayFirstDecision — true = go first, false = go second
    bool choose_first = true;

    // SideboardSwap — pairs of (out, in)
    std::vector<std::pair<GameObjectId, GameObjectId>> sideboard_swaps;

    // Generic choice payload (for prompted choices during resolution)
    std::vector<GameObjectId> chosen_objects;
    std::optional<int> chosen_value;
    // MakeChoice — a card named rather than pointed at ("name a spell"): there
    // need be no object with this definition anywhere in the game.
    CardDefId chosen_card_def = kInvalidId;

    // Alternative play cost (Jhin, Meticulous Killer): when true, the payment
    // path charges Card::alternativePlayCost(...) instead of the printed cost.
    bool use_alt_play_cost = false;
    // PlayCard — the unit is played to `play_location` by paying that
    // battlefield's additional cost (BattlefieldState::paid_play_power; Dragon
    // Roost). Only offered where the unit could not otherwise be played.
    bool pay_location_cost = false;
    // Aura-granted activated ability (Forge/Gardens/Heimerdinger): when set,
    // this ActivateAbility invokes ability_source's GRANTED ability whose logic
    // lives on card def `granted_ability_def` (kInvalidId = the source's own
    // ability).
    CardDefId granted_ability_def = kInvalidId;

    // Display-only human-readable label for an int-coded MakeChoice
    // option (yes/no, mode index, X amount). The Intent itself only
    // carries the answer (chosen_value), which renders as a bare number
    // ("=0", "=1") with no indication of meaning. The CARD — which knows
    // the semantics — stamps the label here (e.g. "Yes", "Deal 2 damage",
    // "X = 3") so the UI / trace can show what each option means. The
    // engine never inspects this; it only forwards and renders it, so no
    // card-specific knowledge leaks into the engine. Excluded from
    // operator== (purely cosmetic — must not affect chosen-index lookup
    // or action-vocab encoding).
    std::string choice_label;

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

    /// Full structural equality. Used by serializers to locate the chosen
    /// action's index within the legal-action list — a 4-field comparison
    /// would conflate "twin" intents (same type/card/targets/ability_source
    /// but different destinations, chosen objects, damage assignments, etc.)
    /// and bias the recorded chosen_idx toward earlier indices.
    bool operator==(const Intent& o) const {
        return type == o.type
            && player == o.player
            && card == o.card
            && play_location == o.play_location
            && units_to_move == o.units_to_move
            && move_destination == o.move_destination
            && ability == o.ability
            && ability_source == o.ability_source
            && ability_index == o.ability_index
            && play_source == o.play_source
            && targets == o.targets
            && damage_assignments == o.damage_assignments
            && cards_to_mulligan == o.cards_to_mulligan
            && chosen_battlefield == o.chosen_battlefield
            && choose_first == o.choose_first
            && sideboard_swaps == o.sideboard_swaps
            && chosen_objects == o.chosen_objects
            && chosen_value == o.chosen_value
            && chosen_card_def == o.chosen_card_def
            && use_alt_play_cost == o.use_alt_play_cost
            && pay_location_cost == o.pay_location_cost
            && granted_ability_def == o.granted_ability_def;
    }
};

} // namespace riftbound
