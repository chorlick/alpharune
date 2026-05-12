#pragma once
/// @file effect_types.h
/// Structured representation of card effects, parsed from ability_text.
///
/// An EffectScript is the parsed form of a card's ability_text. It contains:
///   - A trigger (when/how the effect activates)
///   - A sequence of effect steps (what happens)
///
/// The parser (effect_parser.h) produces these from natural language.
/// The executor (effect_executor.h) runs them against game state.

#include "core/types.h"

#include <optional>
#include <string>
#include <vector>

namespace riftbound {

// ─── Effect types ───────────────────────────────────────────────────────────

enum class EffectType : uint8_t {
    None,

    // P0 — core effects (~80% of cards)
    DealDamage,         // "Deal N to TARGET"
    Draw,               // "Draw N"
    Kill,               // "Kill TARGET"
    GiveMight,          // "Give TARGET +N [M] this turn"
    GiveKeyword,        // "Give TARGET [Keyword N] this turn"
    Buff,               // "Buff TARGET" (+1 might buff)
    Ready,              // "Ready TARGET"
    Bounce,             // "Return TARGET to owner's hand"
    Move,               // "Move TARGET"

    // P1 — secondary effects
    Stun,               // "Stun TARGET"
    Discard,            // "Discard N"
    Recycle,            // "Recycle TARGET"
    LookAtTop,          // "Look at the top N cards"
    RevealFromTop,      // "Reveal the top N cards"
    RevealUntil,        // "Reveal cards...until you reveal a CONDITION"
    Counter,            // "Counter a spell"
    Banish,             // "Banish TARGET"
    PlayIgnoringCost,   // "Play it, ignoring its cost"
    Channel,            // "Channel N runes"
    Heal,               // "Heal TARGET"
    Exhaust,            // "Exhaust TARGET"
    ChooseTarget,       // "Choose TARGET" — establishes targeting, not an effect
    GainXP,             // "Gain N XP"
    SpendXP,            // "Spend N XP"
};

// ─── Trigger types ──────────────────────────────────────────────────────────

enum class TriggerType : uint8_t {
    None,               // spell — resolves from chain, no trigger wrapper

    // Play triggers
    WhenYouPlayMe,      // "When you play me,"
    WhenYouPlayThis,    // "When you play this," (gear)
    AsYouPlayMe,        // "As you play me,"

    // Combat triggers
    WhenIAttack,        // "When I attack,"
    WhenIDefend,        // "When I defend,"
    WhenIAttackOrDefend,// "When I attack or defend,"

    // Board triggers
    WhenIDie,           // "When I die,"
    WhenIMoveToFB,      // "When I move to a battlefield,"
    WhenIMove,          // "When I move,"
    WhenIConquer,       // "When I conquer,"
    WhenIHold,          // "When I hold,"
    WhenIConquerOrHold, // "When I conquer or hold,"

    // Battlefield triggers
    WhenYouConquerHere, // "When you conquer here,"
    WhenYouHoldHere,    // "When you hold here,"
    WhenYouScoreHere,   // "When you score here,"
    WhenYouDefendHere,  // "When you defend here,"

    // Phase triggers
    AtEndOfTurn,        // "At the end of your turn,"
    AtStartOfBeginning, // "At the start of your Beginning Phase,"

    // Activated abilities
    Activated,          // "[E]:", "[N]:", "[N], [E]:", etc.

    // Passive
    Passive,            // always active (auras, static abilities)

    // Other event triggers
    WhenYouPlayASpell,  // "When you play a spell,"
    WhenYouPlayAUnit,   // "When you play a unit,"
    WhenAFriendlyUnitDies, // "When a friendly unit dies,"
    WhenYouDiscard,     // "When you discard,"
};

// ─── Conditions ─────────────────────────────────────────────────────────────

enum class EffectCondition : uint8_t {
    None,
    IfKills,            // "If this kills it,"
    IfYouDo,            // "If you do,"
    IfUnit,             // "If it's a unit,"
    IfSpell,            // "If it's a spell,"
    IfGear,             // "If it's a gear,"
    IfCantDo,           // "If you can't,"
};

// ─── Target specification ───────────────────────────────────────────────────

struct TargetSpec {
    // Type filter
    bool unit = false;
    bool gear = false;
    bool spell = false;
    bool rune = false;
    bool any_card = false;      // "a card"

    // Ownership filter
    bool friendly = false;
    bool enemy = false;

    // Location filter
    bool at_battlefield = false;
    bool at_base = false;
    bool here = false;          // "here" = at this battlefield

    // Self-reference
    bool self = false;          // "me" / "I" / "it" (the source card)

    // Context references (from previous steps)
    bool it = false;            // result of previous step
    bool the_rest = false;      // non-matched results from reveal

    // Stat filter
    int max_might = 0;          // "with N [M] or less" (0 = no filter)

    // Quantity
    bool optional = false;      // "you may" / "up to"
    int count = 1;              // "up to 2 units"
    bool all = false;           // "all units" / "all gear"

    bool hasFilter() const {
        return unit || gear || spell || rune || any_card ||
               friendly || enemy || at_battlefield || at_base ||
               here || self || it || the_rest || max_might > 0;
    }
};

// ─── Effect step ────────────────────────────────────────────────────────────

struct EffectStep {
    EffectType type = EffectType::None;

    int value = 0;              // amount: damage, cards to draw, might bonus, etc.
    TargetSpec target;

    // For GiveKeyword
    Keyword keyword = Keyword::Assault;
    int keyword_value = 0;      // e.g., Assault 3

    // For conditionals
    EffectCondition condition = EffectCondition::None;

    // For optional effects
    bool optional = false;      // "you may"

    // For RevealUntil
    CardType reveal_condition = CardType::Unit; // stop when this type found
};

// ─── Activation cost (for [E]: abilities) ───────────────────────────────────

struct ActivationCost {
    bool exhaust = false;       // [E]
    int energy = 0;             // [N]
    int power = 0;              // [domain]
    Domain power_domain = Domain::Fury;
    bool recycle_self = false;  // [C] (recycle this card)
    bool discard = false;       // "Discard 1,"
    int discard_count = 0;
};

// ─── Complete effect script ─────────────────────────────────────────────────

struct EffectScript {
    TriggerType trigger = TriggerType::None;
    std::vector<EffectStep> steps;

    // Timing keywords (from [Action] / [Reaction])
    bool is_action = false;
    bool is_reaction = false;

    // For activated abilities
    ActivationCost activation_cost;

    // Condition gates (must be met for effects to fire)
    bool requires_legion = false;   // cards_played_this_turn > 0
    bool requires_level = false;    // xp >= level_threshold
    int level_threshold = 0;

    // Whether parsing produced any meaningful effects
    bool parsed = false;

    bool hasEffects() const { return !steps.empty(); }
    bool hasTrigger() const { return trigger != TriggerType::None; }
};

// ─── Target requirements (for legal action generation) ──────────────────────

struct TargetRequirements {
    int count = 0;
    bool must_be_unit = false;
    bool must_be_gear = false;
    bool must_be_enemy = false;
    bool must_be_friendly = false;
    bool must_be_at_battlefield = false;
    int max_might = 0;
    bool optional = false;
};

} // namespace riftbound
