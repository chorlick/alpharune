#pragma once
/// @file types.h
/// Core type definitions, enums, and type aliases for the Riftbound engine.
/// This is the foundational header — almost everything includes this.

#include <cstdint>
#include <limits>
#include <string>
#include <variant>
#include <vector>

namespace riftbound {

// ─── ID types ────────────────────────────────────────────────────────────────
// Strong typedefs via tagged integers for type safety.
// Using uint32_t for all IDs — 4 billion is plenty.

using CardDefId   = uint32_t;  // Index into CardDB (from registry.json card_id)
using GameObjectId = uint32_t; // Runtime game object instance
using AbilityId   = uint32_t;  // Ability instance
using ChainItemId = uint32_t;  // Chain item instance
using ZoneId      = uint32_t;  // Zone instance
using BattlefieldId = uint32_t; // Battlefield slot

constexpr uint32_t kInvalidId = std::numeric_limits<uint32_t>::max();

// ─── Player ──────────────────────────────────────────────────────────────────

enum class PlayerId : uint8_t {
    None = 0,
    Player1 = 1,
    Player2 = 2,
};

inline PlayerId opponent(PlayerId p) {
    return p == PlayerId::Player1 ? PlayerId::Player2 : PlayerId::Player1;
}

inline int playerIndex(PlayerId p) {
    return static_cast<int>(p) - 1; // 0-based for array indexing
}

// ─── Card classification ─────────────────────────────────────────────────────

enum class CardType : uint8_t {
    Unit,
    Spell,
    Gear,
    Rune,
    Battlefield,
    Legend,
};

enum class SuperType : uint8_t {
    None,
    Champion,
    Signature,
    Token,
};

enum class Domain : uint8_t {
    Fury   = 0,  // Red  [R]
    Calm   = 1,  // Green [G]
    Mind   = 2,  // Blue  [B]
    Body   = 3,  // Orange [O]
    Chaos  = 4,  // Purple [P]
    Order  = 5,  // Yellow [Y]
    Count  = 6,
};

enum class Rarity : uint8_t {
    Common,
    Uncommon,
    Rare,
    Epic,
    Showcase,
};

// ─── Keywords ────────────────────────────────────────────────────────────────
// Bit positions for the keyword bitset.

enum class Keyword : uint32_t {
    Accelerate   = 0,
    Action       = 1,
    Ambush       = 2,
    Assault      = 3,
    Backline     = 4,
    Deathknell   = 5,
    Deflect      = 6,
    Equip        = 7,
    Ganking      = 8,
    Hidden       = 9,
    Hunt         = 10,
    Legion       = 11,
    Level        = 12,
    Predict      = 13,
    QuickDraw    = 14,
    Reaction     = 15,
    Repeat       = 16,
    Shield       = 17,
    Tank         = 18,
    Temporary    = 19,
    Unique       = 20,
    Vision       = 21,
    Weaponmaster = 22,
    Count        = 23,
};

// Fixed-size keyword bitset
struct KeywordSet {
    uint32_t bits = 0;

    bool has(Keyword kw) const { return (bits >> static_cast<uint32_t>(kw)) & 1; }
    void set(Keyword kw) { bits |= (1u << static_cast<uint32_t>(kw)); }
    void clear(Keyword kw) { bits &= ~(1u << static_cast<uint32_t>(kw)); }
    void reset() { bits = 0; }

    KeywordSet operator|(const KeywordSet& o) const { return {bits | o.bits}; }
    KeywordSet operator&(const KeywordSet& o) const { return {bits & o.bits}; }
    bool operator==(const KeywordSet& o) const { return bits == o.bits; }
};

// ─── Zones ───────────────────────────────────────────────────────────────────

enum class ZoneType : uint8_t {
    MainDeck,
    RuneDeck,
    Hand,
    Base,
    Trash,
    Banishment,
    ChampionZone,
    LegendZone,
    BattlefieldZone,
    Chain,
    FacedownZone,
};

enum class PrivacyLevel : uint8_t {
    Secret,    // No player can see
    Private,   // Only owner/controller can see
    Public,    // All players can see
};

// ─── Locations ───────────────────────────────────────────────────────────────
// A Location is where a unit/permanent physically resides on the board.
// Either a player's Base or a Battlefield.

struct BaseLocation {
    PlayerId player;
    bool operator==(const BaseLocation& o) const { return player == o.player; }
};

struct BattlefieldLocation {
    BattlefieldId id;
    bool operator==(const BattlefieldLocation& o) const { return id == o.id; }
};

using LocationId = std::variant<BaseLocation, BattlefieldLocation>;

// ─── Turn structure ──────────────────────────────────────────────────────────

enum class TurnPhase : uint8_t {
    // Setup (pre-game)
    Setup,
    Mulligan,

    // Start of turn
    AwakenPhase,
    BeginningStep,
    ScoringStep,
    ChannelPhase,
    DrawPhase,

    // Main
    MainPhase,

    // End of turn
    EndingStep,
    ExpirationStep,

    // Terminal
    GameOver,
};

enum class NeutralShowdownState : uint8_t {
    Neutral,
    Showdown,
};

enum class OpenClosedState : uint8_t {
    Open,
    Closed,
};

// ─── Combat ──────────────────────────────────────────────────────────────────

enum class CombatDesignation : uint8_t {
    None,
    Attacker,
    Defender,
};

enum class CombatPhase : uint8_t {
    None,
    ShowdownStep,    // Step 1: Combat showdown
    DamageStep,      // Step 2: Combat damage
    ResolutionStep,  // Step 3: Resolution
};

// ─── Scoring ─────────────────────────────────────────────────────────────────

enum class ScoreMethod : uint8_t {
    Conquer,  // Gain control of a battlefield
    Hold,     // Maintain control during Beginning Phase
    Direct,   // Card effect granting points (bypasses Winning Point restrictions)
};

// ─── Intents ─────────────────────────────────────────────────────────────────
// Every player action is an Intent. The engine enumerates legal intents,
// the agent picks one, and the engine executes it.

enum class IntentType : uint8_t {
    // Main Phase — Neutral Open
    PlayCard,
    StandardMove,
    ActivateAbility,
    HideCard,
    EndTurn,

    // Chain responses — Closed State
    PlayReaction,
    ActivateReactionAbility,
    PassPriority,

    // Showdown — Open
    PlayActionCard,
    ActivateActionAbility,
    PassFocus,

    // Prompted actions
    AssignCombatDamage,
    MulliganDecision,
    ChooseBattlefield,
    MakeChoice,
    SideboardSwap,
    PlayFirstDecision,

    // Triggered ability responses
    PlaceOptionalTrigger,
    DeclineOptionalTrigger,
    PayTriggeredCost,
    DeclineTriggeredCost,

    // Special
    Concede,
};

// Damage assignment for combat
struct DamageAssignment {
    GameObjectId target_unit;
    int damage;
};

// ─── Mode of Play ────────────────────────────────────────────────────────────

enum class MatchFormat : uint8_t {
    BestOf1,  // Duel
    BestOf3,  // Match
};

struct ModeOfPlay {
    int num_players = 2;
    int victory_score = 8;
    int battlefield_count = 2;
    int battlefields_per_player = 1;
    bool second_player_extra_channel = true;
    MatchFormat format = MatchFormat::BestOf3;
};

// ─── String conversions (for logging/rendering) ─────────────────────────────

const char* toString(CardType t);
const char* toString(SuperType t);
const char* toString(Domain d);
const char* toString(TurnPhase p);
const char* toString(PlayerId p);
const char* toString(IntentType t);
const char* toString(CombatDesignation d);
const char* toString(ScoreMethod m);

} // namespace riftbound
