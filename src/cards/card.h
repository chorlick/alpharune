#pragma once
/// @file card.h
/// Card object hierarchy — replaces effect parser with explicit C++ card classes.
///
/// Every card in the game is represented by a Card subclass that implements
/// its behavior via virtual method overrides. The CardRegistry maps
/// CardDefId → Card* for dispatch.
///
/// Card objects are stateless — they describe behavior, not runtime state.
/// Runtime state lives in GameObject. Card objects use EffectExecutor's
/// atomic helpers (dealDamage, drawCards, etc.) to modify game state.

#include "core/types.h"
#include "effects/effect_types.h"

#include <functional>
#include <memory>
#include <vector>

namespace riftbound {

// Forward declarations
class GameState;
class EventBus;
class EffectExecutor;

/// Context passed to all Card virtual methods.
/// Provides access to game services and identifies the card instance.
struct CardContext {
    GameState& state;
    EventBus& events;
    EffectExecutor& executor;
    PlayerId controller;
    GameObjectId source;  // the runtime GameObject producing this effect
};

// ─── Card base class ────────────────────────────────────────────────────────

class Card {
public:
    virtual ~Card() = default;

    CardDefId cardDefId() const { return card_def_id_; }

    // ── Spell resolution (spells only) ──
    /// Called when this spell resolves from the chain.
    virtual void onResolve(CardContext& ctx,
                           const std::vector<GameObjectId>& targets) {}

    // ── Permanent enters board ──
    /// Called when this permanent (unit/gear) enters the board from play.
    virtual void onPlay(CardContext& ctx) {}

    // ── Activated abilities ──
    /// Called when an activated ability ([E]:, [N]:, etc.) is used.
    virtual void onActivate(CardContext& ctx,
                            const std::vector<GameObjectId>& targets) {}

    /// Whether this card has an activated ability.
    virtual bool hasActivatedAbility() const { return false; }

    /// Cost to activate the ability.
    virtual ActivationCost getActivationCost() const { return {}; }

    /// Whether the activated ability has [Action] timing (playable in showdowns).
    virtual bool isActionAbility() const { return false; }

    // ── Triggers ──
    /// What event type triggers this card's ability.
    virtual TriggerType triggerType() const { return TriggerType::None; }

    /// Called when the matching trigger fires.
    virtual void onTrigger(CardContext& ctx,
                           const std::vector<GameObjectId>& targets) {}

    // ── Targeting ──
    /// Target requirements for legal action generation.
    virtual TargetRequirements getTargetRequirements() const { return {}; }

    /// Enumerate legal targets given current game state.
    /// Default implementation uses getTargetRequirements() with standard filtering.
    /// Override for cards with unusual targeting.
    virtual std::vector<GameObjectId> enumerateLegalTargets(
        const GameState& state, PlayerId controller) const;

    // ── Play-to-location ──
    /// Custom play locations (for "play me to an open battlefield" etc.).
    /// Empty = normal play rules (base or controlled BF).
    virtual std::vector<LocationId> getPlayLocations(
        const GameState& state, PlayerId player) const { return {}; }

    // ── Replacement effects ──
    /// Whether this card has a replacement effect (intercepts killUnit etc.).
    virtual bool hasReplacementEffect() const { return false; }

    /// Apply the replacement. Returns true if the replacement was applied
    /// (the original action should be skipped).
    virtual bool applyReplacement(CardContext& ctx,
                                  GameObjectId dying_unit) { return false; }

    // ── Equipment / Equip (CR 716-725, 818) ──
    /// Whether this gear has an equip ability.
    virtual bool hasEquipAbility() const { return false; }

    /// Pay the equip cost and attach to unit. Card handles its own cost logic.
    /// Returns true if equip succeeded (cost paid, attached).
    virtual bool onEquip(CardContext& ctx, GameObjectId unit) { return false; }

    /// Trigger type that fires on the equipped UNIT (from effect_text).
    /// e.g., "When I conquer" in effect_text → WhenIConquer on the unit.
    virtual TriggerType equippedTriggerType() const { return TriggerType::None; }

    /// Called when the equipped unit's trigger fires. Gear handles its own logic.
    virtual void onEquippedTrigger(CardContext& ctx,
                                   GameObjectId unit,
                                   const std::vector<GameObjectId>& targets) {}

    /// Keywords granted to the equipped unit (from effect_text).
    virtual KeywordSet equippedKeywords() const { return {}; }

    /// Assault/Shield/Deflect values granted to equipped unit.
    virtual int equippedAssault() const { return 0; }
    virtual int equippedShield() const { return 0; }
    virtual int equippedDeflect() const { return 0; }

    // ── Condition gates ──
    /// Whether this card's effects require Legion (cards_played >= 2).
    virtual bool requiresLegion() const { return false; }

    /// Whether this card's effects require Level (xp >= threshold).
    virtual bool requiresLevel() const { return false; }

    /// XP threshold for Level requirement.
    virtual int levelThreshold() const { return 0; }

    // ── Self-cost reduction (e.g., Noxus Hopeful "Legion: I cost 2 less") ──
    /// Returns the amount by which this card's own energy cost should be
    /// reduced, given the current game state. Engine consults this in
    /// canAfford() and payCardCost() when the card is being played.
    /// Default 0 = no reduction.
    virtual int selfCostReduction(const GameState& /*state*/,
                                  PlayerId /*player*/) const { return 0; }

    // ── Reaction-to-attack play (e.g., Rengar, Pouncing) ──
    /// Whether this card can be played as a [Reaction] to a battlefield
    /// where the controller is currently attacking. Action generator
    /// emits play-to-attacking-BF intents during showdown/closed state.
    virtual bool playableAsReactionToAttack() const { return false; }

protected:
    CardDefId card_def_id_;

    explicit Card(CardDefId id) : card_def_id_(id) {}
};

// ─── Subclasses ─────────────────────────────────────────────────────────────

class UnitCard : public Card {
public:
    explicit UnitCard(CardDefId id) : Card(id) {}
};

class SpellCard : public Card {
public:
    explicit SpellCard(CardDefId id) : Card(id) {}
};

class GearCard : public Card {
public:
    explicit GearCard(CardDefId id) : Card(id) {}
};

class LegendCard : public Card {
public:
    explicit LegendCard(CardDefId id) : Card(id) {}
};

class BattlefieldCard : public Card {
public:
    explicit BattlefieldCard(CardDefId id) : Card(id) {}
};

class RuneCard : public Card {
public:
    explicit RuneCard(CardDefId id) : Card(id) {}
};

} // namespace riftbound
