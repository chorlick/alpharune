#pragma once
/// @file game_state.h
/// Complete game state container.
///
/// GameState holds ALL mutable state for a single game in progress.
/// It is the single source of truth — every subsystem reads from and
/// writes to this structure.

#include "effects/effect_types.h"
#include "game_object.h"
#include "types.h"

#include <algorithm>
#include <cassert>
#include <map>
#include <optional>
#include <set>
#include <unordered_map>
#include <vector>

namespace riftbound {

// ─── Rune Pool ───────────────────────────────────────────────────────────────

struct RunePool {
    int energy = 0;
    int power[static_cast<int>(Domain::Count)] = {};  // per-domain
    int universal_power = 0;  // [A] — any domain

    void clear() {
        energy = 0;
        universal_power = 0;
        for (int i = 0; i < static_cast<int>(Domain::Count); ++i)
            power[i] = 0;
    }

    int totalPower() const {
        int sum = universal_power;
        for (int i = 0; i < static_cast<int>(Domain::Count); ++i)
            sum += power[i];
        return sum;
    }
};

// ─── Player State ────────────────────────────────────────────────────────────

struct PlayerState {
    PlayerId id = PlayerId::None;
    int score = 0;
    int xp = 0;

    RunePool rune_pool;

    // Zone contents (game object IDs)
    std::vector<GameObjectId> main_deck;    // ordered (top = back)
    std::vector<GameObjectId> rune_deck;    // ordered (top = back)
    std::vector<GameObjectId> hand;
    std::vector<GameObjectId> trash;
    std::vector<GameObjectId> banishment;

    // Special zones (single card or empty)
    GameObjectId champion_zone = kInvalidId;
    GameObjectId legend_zone = kInvalidId;

    // Per-turn tracking
    int cards_played_this_turn = 0;
    bool has_discarded_this_turn = false;
    std::set<BattlefieldId> battlefields_scored_this_turn;
    bool burned_out = false;
    bool is_first_turn = true;  // for first-turn adjustments
    bool cant_play_cards_this_turn = false;  // Brynhir Thundersong lockout, resets at turn end

    // Cost modifiers (Phase 5)
    struct CostModifier {
        GameObjectId source = kInvalidId;  // card granting the reduction
        int energy_reduction = 0;          // reduce energy cost by this
        int min_cost = 0;                  // minimum cost after reduction (0 = no min)
        bool this_turn_only = true;        // expires at end of turn
        bool next_spell_only = false;      // expires after next spell played
        bool next_unit_only = false;       // expires after next unit played
    };
    std::vector<CostModifier> cost_modifiers;

    // Additional turns queue (Phase 5)
    std::vector<PlayerId> additional_turns;

    void resetTurnTracking() {
        cards_played_this_turn = 0;
        has_discarded_this_turn = false;
        battlefields_scored_this_turn.clear();
        cant_play_cards_this_turn = false;
        // Expire this-turn cost modifiers
        cost_modifiers.erase(
            std::remove_if(cost_modifiers.begin(), cost_modifiers.end(),
                [](const CostModifier& m) { return m.this_turn_only; }),
            cost_modifiers.end());
    }
};

// ─── Battlefield State ───────────────────────────────────────────────────────

struct BattlefieldState {
    BattlefieldId id = kInvalidId;
    GameObjectId card_object_id = kInvalidId;  // the battlefield card itself

    std::optional<PlayerId> controller;
    bool is_contested = false;
    PlayerId contested_by = PlayerId::None;
    PlayerId contributed_by = PlayerId::None;  // who brought this BF to the game

    // Facedown sub-zone
    std::vector<GameObjectId> facedown;
    int facedown_max_occupancy = 1;

    // Staged events
    bool showdown_staged = false;
    bool combat_staged = false;

    // Active combat/showdown
    bool showdown_in_progress = false;
    bool combat_in_progress = false;
    std::optional<PlayerId> attacker;
    std::optional<PlayerId> defender;
    CombatPhase combat_phase = CombatPhase::None;

    // Token tracking (CR 438, 439)
    bool is_token = false;
    std::optional<GameObjectId> replaced_card;  // original in banishment
    bool was_replaced = false;

    /// Get all unit IDs at this battlefield belonging to a player.
    std::vector<GameObjectId> unitsControlledBy(
        PlayerId player,
        const std::unordered_map<GameObjectId, GameObject>& objects) const;

    /// Check if any player has units here.
    bool hasUnitsFrom(
        PlayerId player,
        const std::unordered_map<GameObjectId, GameObject>& objects) const;
};

// ─── Turn State ──────────────────────────────────────────────────────────────

struct TurnState {
    PlayerId turn_player = PlayerId::Player1;
    PlayerId starting_player = PlayerId::Player1;  // who won the coin toss
    TurnPhase phase = TurnPhase::Setup;
    NeutralShowdownState ns_state = NeutralShowdownState::Neutral;
    OpenClosedState oc_state = OpenClosedState::Open;

    std::optional<PlayerId> priority_holder;
    std::optional<PlayerId> focus_holder;

    int turn_number = 0;
    bool is_additional_turn = false;

    // Per-turn decision counters (reset each turn)
    int turn_decisions_p1 = 0;
    int turn_decisions_p2 = 0;

    // Priority tracking for chain resolution
    std::set<PlayerId> players_passed_priority;
    std::set<PlayerId> players_passed_focus;

    bool isNeutralOpen() const {
        return ns_state == NeutralShowdownState::Neutral &&
               oc_state == OpenClosedState::Open;
    }

    bool isShowdownOpen() const {
        return ns_state == NeutralShowdownState::Showdown &&
               oc_state == OpenClosedState::Open;
    }

    bool isClosedState() const {
        return oc_state == OpenClosedState::Closed;
    }
};

// ─── Chain State ─────────────────────────────────────────────────────────────

enum class ChainItemStatus : uint8_t { Pending, Finalized };

struct ChainItem {
    ChainItemId id = kInvalidId;
    ChainItemStatus status = ChainItemStatus::Pending;
    GameObjectId source = kInvalidId;      // the spell/ability game object
    CardDefId card_def_id = kInvalidId;    // for looking up effects
    PlayerId controller = PlayerId::None;

    // Targeting
    std::vector<GameObjectId> targets;

    // Classification
    bool is_spell = false;        // true for spells (go to trash on resolve)
    bool is_permanent = false;    // true for units/gear (resolve on finalize, CR 337.1.c)
    bool is_ability = false;      // true for activated/triggered abilities

};

struct ChainState {
    std::vector<ChainItem> items;
    ChainItemId next_id = 1;

    bool exists() const { return !items.empty(); }

    ChainItemId allocateId() { return next_id++; }

    /// Get the newest (top) item on the chain (last added = resolves first).
    ChainItem* newest() {
        return items.empty() ? nullptr : &items.back();
    }

    const ChainItem* newest() const {
        return items.empty() ? nullptr : &items.back();
    }

    /// Check if any items are still pending.
    bool hasPending() const {
        for (auto& item : items)
            if (item.status == ChainItemStatus::Pending) return true;
        return false;
    }
};

// ─── Delayed Abilities (CR 389-392) ─────────────────────────────────────────

struct DelayedAbility {
    GameObjectId source = kInvalidId;     // card that created this
    CardDefId card_def_id = kInvalidId;   // for looking up the Card object
    PlayerId controller = PlayerId::None;
    TriggerType trigger = TriggerType::None; // what event fires it
    int expires_on_turn = -1;             // -1 = end of current turn
    bool fired = false;                   // one-shot: fire once, then remove
};

// ─── Complete Game State ─────────────────────────────────────────────────────

struct GameState {
    // Configuration
    ModeOfPlay mode;

    // Players (indexed by playerIndex())
    PlayerState players[2];

    // Board
    std::vector<BattlefieldState> battlefields;

    // All game objects (the object pool)
    std::unordered_map<GameObjectId, GameObject> objects;
    GameObjectId next_object_id = 1;

    // Turn
    TurnState turn;

    // Chain
    ChainState chain;

    // Delayed abilities (Phase 5, CR 389-392)
    std::vector<DelayedAbility> delayed_abilities;

    // Game result
    bool game_over = false;
    PlayerId winner = PlayerId::None;
    std::string game_over_reason;

    // Decision counter (for training data indexing)
    int decision_index = 0;

    // Sequence of opaque action IDs applied to this game, in order.
    // Engine-agnostic: the producer (e.g., OpenSpiel wrapper) defines the
    // encoding. Lives here so that a future memcpy-based Clone() carries the
    // full apply-history with the rest of the state — no out-of-band copy.
    // Read-only from the engine's perspective today; written by the harness
    // that applies actions (currently RiftboundState::DoApplyAction).
    std::vector<int64_t> action_history;

    // ── Object management ──

    GameObjectId createObject() {
        GameObjectId id = next_object_id++;
        objects[id] = GameObject{};
        objects[id].id = id;
        return id;
    }

    GameObject& getObject(GameObjectId id) {
        auto it = objects.find(id);
        assert(it != objects.end() && "Invalid game object ID");
        return it->second;
    }

    const GameObject& getObject(GameObjectId id) const {
        auto it = objects.find(id);
        assert(it != objects.end() && "Invalid game object ID");
        return it->second;
    }

    bool objectExists(GameObjectId id) const {
        return objects.find(id) != objects.end();
    }

    // ── Player access ──

    PlayerState& player(PlayerId id) {
        return players[playerIndex(id)];
    }

    const PlayerState& player(PlayerId id) const {
        return players[playerIndex(id)];
    }

    // ── Board queries ──

    /// Find all game objects at a specific location.
    std::vector<GameObjectId> objectsAt(LocationId loc) const;

    /// Find all units at a location controlled by a player.
    std::vector<GameObjectId> unitsAt(LocationId loc, PlayerId controller) const;

    /// Find all units on the board controlled by a player (base + battlefields).
    std::vector<GameObjectId> allUnitsControlledBy(PlayerId controller) const;

    /// Find all objects in a player's base.
    std::vector<GameObjectId> objectsInBase(PlayerId player) const;

    /// Find all runes in a player's base (channeled runes).
    std::vector<GameObjectId> runesInBase(PlayerId player) const;

    /// Get the number of battlefields currently on the board.
    size_t totalBattlefields() const { return battlefields.size(); }
};

} // namespace riftbound
