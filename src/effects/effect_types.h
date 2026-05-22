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
    WhenIWinCombat,     // "When I win a combat," — surviving combatant on winning side

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
    AtStartOfMain,      // "At the start of your Main Phase," — used by
                        // delayed-ability cards like Iascylla. Fires once
                        // per main-phase entry per delayed ability instance.

    // Activated abilities
    Activated,          // "[E]:", "[N]:", "[N], [E]:", etc.

    // Passive
    Passive,            // always active (auras, static abilities)

    // Other event triggers
    WhenYouPlayASpell,  // "When you play a spell,"
    WhenYouPlayAUnit,   // "When you play a unit,"
    WhenAFriendlyUnitDies, // "When a friendly unit dies,"
    WhenAFriendlyUnitMovesToFB, // "When a friendly unit moves to a battlefield,"
    WhenYouDiscard,     // "When you discard,"
    WhenYouChooseAFriendlyUnit, // "When you choose a friendly unit,"
                                // Fires when an intent's targets include a
                                // friendly unit. Currently emitted from
                                // TriggerManager::onCardPlayed (covers spell/
                                // permanent plays). Activated-ability target
                                // selection not yet covered.
    WhenYouStun,        // "When you stun an enemy," — fires for cards
                        // controlled by the stunner when EffectExecutor::
                        // stunUnit successfully applies a stun to a unit
                        // owned by the opponent. Used by Vex Mocking.
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
    int xp_cost = 0;            // "Spend N XP" (deducts from controller's PlayerState.xp)
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

// ─── Activated ability descriptor (multi-ability per Card) ──────────────────
//
// Cards with one activated ability can still use the legacy single-ability
// virtuals on Card; the default Card::activatedAbilities() impl wraps those
// into a one-element vector. Cards with N activated abilities override
// Card::activatedAbilities() to return N descriptors and override
// Card::onActivate(ctx, ability_index, targets) to dispatch on the index.
struct ActivatedAbility {
    ActivationCost cost;
    TargetRequirements targets;
    bool is_action = false;
    bool is_reaction = false;
    // Phase 6q+ — if true, the action generator does NOT enumerate one
    // intent per legal target; instead it emits ONE intent with empty
    // targets and the card's onActivate uses pickTarget() at resolve
    // time. Fixes the same vocab-collision class we fixed for Play.
    bool needs_activation_time_target = false;
};

} // namespace riftbound
