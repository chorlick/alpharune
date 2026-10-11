#pragma once
/// @file types.h
/// Core type definitions, enums, and type aliases for the Riftbound engine.
/// This is the foundational header — almost everything includes this.

#include <compare>
#include <concepts>
#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include <variant>
#include <vector>

namespace riftbound {

// ─── ID types ────────────────────────────────────────────────────────────────
// Every kind of id is a type of its own (plans/05 item 5.5). Mixing two kinds
// (a card-template id where a runtime object id is expected: `cardDefId()` vs
// `ctx.source`), or handing a plain integer to something that takes an id,
// does not compile; tests/test_strong_ids.cpp and the
// `riftbound_strong_id_case_*` ctests keep it that way.
//
// An id is one uint32_t: trivially copyable, the same size and the same
// numeric value as when ids were plain integers, ordered and hashed by that
// value (so map order, option order and everything written to a log, a replay
// or a feature are what they were).
//
// To and from a plain integer, always by name:
//   id.raw()              the number (for a log line, an int slot, an index)
//   X::fromRaw(n)         the id that number names (from an int slot)
// Every id kind has exactly ONE "none" value, kInvalidId, which is also what
// a default-constructed id is. `id == kInvalidId` / `id.valid()` test for it.

/// "No id", for every id kind (and UINT32_MAX in a plain integer slot that
/// holds an id's raw value, e.g. `resume_data`).
struct InvalidId {
    constexpr operator uint32_t() const noexcept {
        return std::numeric_limits<uint32_t>::max();
    }
};
inline constexpr InvalidId kInvalidId{};

template <class Tag>
class StrongId {
public:
    /// Default: none.
    constexpr StrongId() noexcept = default;
    constexpr StrongId(InvalidId) noexcept {}

    /// A kind the SOURCE names by number (a card id: `d.id = 235`,
    /// `registry.get(235)`) converts from an integer LITERAL and from nothing
    /// else: the constructor is consteval, so a value only known at run time
    /// (an int variable, a counter, a resume slot) does not compile, and
    /// neither does 0 or UINT32_MAX (there is one "none", kInvalidId).
    consteval StrongId(int literal) requires Tag::kNamedByLiteral
        : v_(static_cast<uint32_t>(literal)) {
        if (literal <= 0) throw "an id literal must be positive: 'none' is kInvalidId";
    }

    /// The id a plain integer names. The integer is taken as stored: -1 in a
    /// signed slot is kInvalidId.
    template <std::integral I>
    static constexpr StrongId fromRaw(I raw) noexcept {
        StrongId s;
        s.v_ = static_cast<uint32_t>(raw);
        return s;
    }
    /// The number, for a log line, an integer slot or an index.
    constexpr uint32_t raw() const noexcept { return v_; }
    /// Not kInvalidId.
    constexpr bool valid() const noexcept {
        return v_ != std::numeric_limits<uint32_t>::max();
    }

    friend constexpr bool operator==(StrongId, StrongId) noexcept = default;
    friend constexpr auto operator<=>(StrongId, StrongId) noexcept = default;
    friend constexpr bool operator==(StrongId a, InvalidId) noexcept { return !a.valid(); }

    /// Prints the number, as a plain integer id did.
    template <class Stream>
        requires requires(Stream& os, uint32_t n) { os << n; }
    friend Stream& operator<<(Stream& os, StrongId id) {
        os << id.v_;
        return os;
    }

private:
    uint32_t v_ = std::numeric_limits<uint32_t>::max();
};

/// The number as text: what std::to_string gave for a plain integer id.
template <class Tag>
inline std::string toString(StrongId<Tag> id) { return std::to_string(id.raw()); }

/// JSON (nlohmann, found by ADL): an id is its number.
template <class Json, class Tag>
    requires requires(Json& j, uint32_t n) { j = n; }
void to_json(Json& j, StrongId<Tag> id) { j = id.raw(); }
template <class Json, class Tag>
    requires requires(const Json& j) { j.template get<uint32_t>(); }
void from_json(const Json& j, StrongId<Tag>& id) {
    id = StrongId<Tag>::fromRaw(j.template get<uint32_t>());
}

struct CardDefIdTag { static constexpr bool kNamedByLiteral = true; };
struct BattlefieldIdTag { static constexpr bool kNamedByLiteral = false; };
struct AbilityIdTag { static constexpr bool kNamedByLiteral = false; };
struct ChainItemIdTag { static constexpr bool kNamedByLiteral = false; };

using CardDefId   = StrongId<CardDefIdTag>;  // Index into CardDB (the card class's def().id)
using GameObjectId = uint32_t; // Runtime game object instance
using AbilityId   = StrongId<AbilityIdTag>;    // Ability instance (Intent::ability)
using ChainItemId = StrongId<ChainItemIdTag>;  // Chain item instance
using BattlefieldId = StrongId<BattlefieldIdTag>; // Battlefield slot (an index into GameState::battlefields)

// ─── Player ──────────────────────────────────────────────────────────────────

enum class PlayerId : uint8_t {
    None = 0,
    Player1 = 1,
    Player2 = 2,
};

/// The other player. `None` has no opponent and stays `None`.
inline PlayerId opponent(PlayerId p) {
    if (p == PlayerId::Player1) return PlayerId::Player2;
    if (p == PlayerId::Player2) return PlayerId::Player1;
    return PlayerId::None;
}

/// Throws std::logic_error. Out of line so the checks below stay one compare
/// on the hot path.
[[noreturn]] void throwInvalidPlayerId(PlayerId p);

/// 0-based index for the per-player arrays. Checked in every build type: an
/// index of -1 for `None` would otherwise read or write outside the array.
inline int playerIndex(PlayerId p) {
    if (p != PlayerId::Player1 && p != PlayerId::Player2) [[unlikely]]
        throwInvalidPlayerId(p);
    return static_cast<int>(p) - 1;
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
    Empower      = 23,  // Vendetta: activated "[Cost]: Empower this" (CR 827)
    Flow         = 24,  // Vendetta: spell may be played from trash for its Flow cost, then banished
    Count        = 25,
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
    constexpr BattlefieldLocation() noexcept = default;
    constexpr BattlefieldLocation(BattlefieldId slot) noexcept : id(slot) {}
    constexpr BattlefieldLocation(InvalidId) noexcept {}
    /// `BattlefieldLocation{0}`: the slot named by its number, spelled out
    /// with the type. A literal only (consteval), and only here: a
    /// BattlefieldId itself takes no number (`BattlefieldId::fromRaw(n)`).
    explicit consteval BattlefieldLocation(int slot_literal)
        : id(BattlefieldId::fromRaw(slot_literal)) {
        if (slot_literal < 0) throw "a battlefield slot is 0 or more";
    }
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

    bool operator==(const DamageAssignment& o) const {
        return target_unit == o.target_unit && damage == o.damage;
    }
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
const char* toString(Keyword k);

} // namespace riftbound

template <class Tag>
struct std::hash<riftbound::StrongId<Tag>> {
    // The hash of the number: an unordered container keyed by an id iterates
    // in the order it did when ids were plain integers.
    std::size_t operator()(riftbound::StrongId<Tag> id) const noexcept {
        return std::hash<uint32_t>{}(id.raw());
    }
};
