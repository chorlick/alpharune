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
#include "core/card_db.h"
#include "effects/effect_types.h"

#include <functional>
#include <memory>
#include <utility>
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
    // Which trigger fired this onTrigger invocation. Lets a card that
    // registers MULTIPLE trigger types (see Card::triggerTypes) branch on
    // the event that fired. None for spells / activated abilities / cards
    // with a single trigger that don't care.
    TriggerType firing_trigger = TriggerType::None;
};

// ─── Card base class ────────────────────────────────────────────────────────

class Card {
public:
    virtual ~Card() = default;

    // ── Interface contract: every card defines its own data ──
    // The card answers with its CardDef; the engine requests it (it is NOT
    // pushed into the base via a constructor). CardRegistry/CardDB materialize
    // the card table by asking each registered card for def(). Implementations
    // return a reference to a function-local static built once, e.g.:
    //   const CardDef& def() const override {
    //       static const CardDef d = makeDef();
    //       return d;
    //   }
    virtual const CardDef& def() const = 0;

    // Convenience: the card's registry id, derived from its own def().
    CardDefId cardDefId() const { return def().id; }

    // ── Spell resolution (spells only) ──
    /// Called when this spell resolves from the chain.
    virtual void onResolve(CardContext& ctx,
                           const std::vector<GameObjectId>& targets) {}

    // ── Permanent enters board ──
    /// Called when this permanent (unit/gear) enters the board from play.
    virtual void onPlay(CardContext& ctx) {}

    // ── Activated abilities (single-ability legacy surface) ──
    /// Called when an activated ability ([E]:, [N]:, etc.) is used.
    /// Legacy single-ability hook. Cards with one activated ability still
    /// override this. Cards with N abilities override the indexed variant
    /// below instead.
    virtual void onActivate(CardContext& ctx,
                            const std::vector<GameObjectId>& targets) {}

    /// Multi-ability variant. Default impl falls through to the legacy
    /// single-ability `onActivate(ctx, targets)` ignoring the index, so
    /// single-ability cards continue to work without changes. Multi-ability
    /// cards override this AND override `activatedAbilities()` to return
    /// multiple descriptors.
    virtual void onActivate(CardContext& ctx,
                            int /*ability_index*/,
                            const std::vector<GameObjectId>& targets) {
        onActivate(ctx, targets);
    }

    /// Whether this card has an activated ability.
    /// Legacy: returns true on single-ability cards. With multi-ability
    /// the source of truth is `activatedAbilities().size() > 0`; this
    /// remains for backwards-compat dispatch in the default
    /// `activatedAbilities()` impl.
    virtual bool hasActivatedAbility() const { return false; }

    /// Per-card activation legality gate ("Use only if …"). Consulted by the
    /// action generator before offering ANY of this card's activated
    /// abilities. Default: always activatable. Used by Emperor of the Sands
    /// ("if you've played an Equipment this turn"), and a natural home for
    /// Zero Drive's "only if unattached" etc.
    virtual bool canActivateAbility(const GameState& /*state*/,
                                    PlayerId /*controller*/) const { return true; }

    /// State-aware reduction to an activated ability's ENERGY cost (e.g.
    /// Bashful Bloom: "costs [1] less for each friendly [Temporary] unit").
    /// Consulted by the activation affordability check + payment, clamped so
    /// the net energy cost can't go below 0. Mirrors selfCostReduction (which
    /// applies to PLAYING cards). Default 0 = no reduction.
    virtual int activationCostReduction(const GameState& /*state*/,
                                        PlayerId /*controller*/,
                                        int /*ability_index*/) const { return 0; }

    /// Cost to activate the (single, legacy) ability.
    virtual ActivationCost getActivationCost() const { return {}; }

    /// Whether the (single, legacy) activated ability has [Action] timing
    /// (playable in showdowns).
    virtual bool isActionAbility() const { return false; }

    /// Whether the (single, legacy) activated ability has [Reaction] timing
    /// (CR 376/398-406) — playable in Closed State as priority response.
    /// Override on cards with `[Reaction] [E]:` or `[E]: [Reaction]` ability
    /// text. Default false; opt-in per card.
    virtual bool isReactionAbility() const { return false; }

    /// Multi-ability surface (Phase 6r — multi-ability per Card).
    /// Returns one ActivatedAbility descriptor per distinct activated
    /// ability on this card. Default impl wraps the legacy single-ability
    /// virtuals (hasActivatedAbility / getActivationCost / etc) into a
    /// one-element vector, so existing cards keep working unchanged.
    ///
    /// Cards with N > 1 activated abilities override this directly,
    /// returning N descriptors. They also override
    /// `onActivate(ctx, ability_index, targets)` to dispatch on the index.
    virtual std::vector<ActivatedAbility> activatedAbilities() const;

    // ── Triggers ──
    /// What event type triggers this card's ability.
    virtual TriggerType triggerType() const { return TriggerType::None; }

    /// Cards with MORE THAN ONE trigger (e.g. "When you play me ... When I
    /// hold ...") override this to return all of them; the trigger manager
    /// fires the ability for each matching event and sets
    /// CardContext::firing_trigger so onTrigger can branch. The default
    /// wraps the single triggerType() for back-compat.
    virtual std::vector<TriggerType> triggerTypes() const {
        auto t = triggerType();
        if (t == TriggerType::None) return {};
        return {t};
    }

    /// Whether this card fires on the given trigger event.
    bool firesOn(TriggerType t) const {
        for (auto x : triggerTypes()) if (x == t) return true;
        return false;
    }

    /// Called when a matching trigger fires. For multi-trigger cards,
    /// inspect ctx.firing_trigger to tell which event fired.
    virtual void onTrigger(CardContext& ctx,
                           const std::vector<GameObjectId>& targets) {}

    /// Optional-trigger confirmation helper (CR principle: "you may [X]").
    ///
    /// Cards with optional triggers ("you may banish it", "you may exhaust
    /// me to draw 1", "you may discard 1 and exhaust me to play a token")
    /// call this at the top of their `onTrigger` / `onResolve`. The flow:
    ///
    ///   0. First invocation: `still_legal()` evaluated. If false → return
    ///      0 (effect skipped — preconditions not met). If true → publish
    ///      a yes/no choice through the executor and return -1 (the chain
    ///      manager's resume loop will re-enter the card after the agent
    ///      decides).
    ///   1. Second invocation (post-choice): consume the agent's pick. If
    ///      "no" → return 0. If "yes" → RE-EVALUATE `still_legal()`
    ///      (state may have shifted while the agent was deciding) and
    ///      return 1 if still legal, else 0.
    ///   2. Further invocations: returns 1 (already confirmed).
    ///
    /// Reserves resume_points 0, 1, 2 on the chain item. Cards needing
    /// additional internal state should use resume_point >= 3 (or use
    /// `card_counters` for non-step-tracking).
    ///
    /// `still_legal` is a closure the card supplies. It captures `ctx`
    /// (or whatever state it needs) and returns true iff the effect can
    /// legally execute right now. Called twice — once before prompting,
    /// once after the agent's "yes" — so the effect can never fire on a
    /// stale precondition.
    ///
    /// Returns:
    ///   -1 = waiting for agent choice (caller MUST return immediately)
    ///    0 = effect skipped (agent declined, or precondition invalid)
    ///    1 = effect confirmed and re-validated — caller proceeds
    int confirmOptional(CardContext& ctx, const std::string& label,
                        const std::function<bool()>& still_legal);

    /// Variable-X cost helper (CR principle: "pay any amount of [X]").
    ///
    /// Cards like Bullet Time ("Pay any amount of [A] to deal that much
    /// damage…") and Hextech Anomaly ("[E]: [Reaction] — Pay any amount
    /// of [A] to [Add] that much Energy") need the agent to declare X at
    /// activation/play time. This helper handles the full resumable
    /// loop: publishes [min..max] choices, the agent picks one, returns
    /// the chosen X.
    ///
    /// Returns:
    ///   -1 = waiting for agent choice (caller MUST return immediately)
    ///   value in [min, max] = chosen X amount
    ///
    /// On re-entry (resume_point ≥ 2), returns the previously-chosen X
    /// stashed in `resume_data[0]` so the caller can re-invoke this
    /// helper from a later phase of its own resume state machine and
    /// get the same X back without re-prompting.
    ///
    /// Reserves resume_points 0, 1, 2 and resume_data[0].
    int pickXAmount(CardContext& ctx, const std::string& label,
                    int min, int max);

    /// Modal-spell mode selection helper.
    ///
    /// Cards with "Choose one — A / B / C ..." text need the agent to
    /// select a mode at resolve time. This helper publishes
    /// `num_modes` MakeChoice options, the agent picks one, the helper
    /// returns the selected index in [0, num_modes).
    ///
    /// `mode_labels` is an OPTIONAL parallel vector of human-readable
    /// labels (e.g. {"Draw 1", "Deal 2 to BF unit"}). Used in the
    /// MODE_PROMPT trace line; pass {} when trivially numbered.
    ///
    /// `legal_modes` is a bitmask: bit i set = mode i is legal in the
    /// current state. Pass `(1u << num_modes) - 1` (or rely on the
    /// default) for "all modes legal". If filtering produces 0 legal
    /// modes the helper returns -2 (caller silently skips).
    ///
    /// Returns:
    ///   -1 = waiting for agent choice (caller MUST return immediately)
    ///   -2 = no legal mode (caller silently skips)
    ///   value in [0, num_modes) = chosen mode index
    ///
    /// Reserves resume_points 3, 4, 5 and resume_data[1] so it can
    /// coexist with `pickXAmount` (which uses 0/1/2 + data[0]) in the
    /// same Card's onResolve.
    ///
    /// Trace tags: MODE_PROMPT, MODE_PICKED, MODE_NOT_OFFERED.
    int pickMode(CardContext& ctx, const std::string& label, int num_modes,
                 const std::vector<std::string>& mode_labels = {},
                 uint32_t legal_modes = 0xFFFFFFFFu);

    /// Resolve-time target picker — counterpart to pickMode for spells
    /// that defer target selection from play-time to resolve-time.
    ///
    /// Phase 6q (target-decoupling). The action vocab keys Play slots
    /// by card_def_id only — so every (card, target) variant of the
    /// same play collapses to one slot, and the policy head can't
    /// learn target preference through the Play action head. Solution:
    /// emit Play with NO target (one slot per card), then mid-resolve
    /// the card calls pickTarget which publishes one MakeChoice per
    /// legal target. MakeChoice is keyed by the target's card_def_id
    /// (Phase 5g), so per-target preference IS learnable through that
    /// head.
    ///
    /// `legal_targets` is the live list of legal targets in the CURRENT
    /// state (the card recomputes via Card::enumerateLegalTargets right
    /// before calling). Re-enumerating at resolve time is CR-friendly:
    /// targets selected at play time can become illegal by the time the
    /// spell resolves (chain ordering, bounce, kill), and CR says
    /// illegal targets fizzle. Picking at resolve time means we never
    /// see fizzled selections — the agent picks among targets that
    /// ARE legal at resolution.
    ///
    /// Returns:
    ///   kInvalidId (= -1) on first call if there are no legal targets
    ///      (caller silently skips — effect fizzles).
    ///   kInvalidId on suspend (caller MUST return immediately — chain
    ///      manager re-enters after agent decides; you tell suspend
    ///      apart from "no targets" by checking ri.resume_point: see
    ///      pickMode for the same convention).
    ///   chosen GameObjectId on second / re-entry call.
    ///
    /// Reserves resume_points 6, 7, 8 and resume_data[2] so it can
    /// coexist with pickXAmount (0/1/2 + data[0]) and pickMode
    /// (3/4/5 + data[1]) inside the same Card's onResolve.
    ///
    /// Trace tags: TGT_PROMPT, TGT_PICKED, TGT_FORCED, TGT_NOT_OFFERED.
    GameObjectId pickTarget(CardContext& ctx, const std::string& label,
                             const std::vector<GameObjectId>& legal_targets);

    /// Paired-target picker for dual-target spells (Challenge,
    /// Star-Crossed, Deathgrip, Singularity, …). Publishes TWO
    /// sequential MakeChoice prompts — one for each target — and
    /// returns the picked pair.
    ///
    /// Picks happen in order: first `legal_a` (any filter the card
    /// applies), then `legal_b_fn(picked_a)` (computed per-pick so
    /// the second filter can exclude the first pick, or apply
    /// different friendly/enemy rules — Challenge filters first to
    /// friendly, second to enemy; Deathgrip filters BOTH to friendly
    /// but excludes the killed unit from the recipient list, etc.).
    ///
    /// Returns:
    ///   {kInvalidId, kInvalidId} on suspend (caller MUST return
    ///     immediately — chain manager re-enters after agent picks)
    ///     OR on fizzle (no legal first target). Distinguish via
    ///     ctx.state.chain.resuming->resume_point.
    ///   {a, b} on full completion.
    ///   {a, kInvalidId} if second pick has no legal targets after
    ///     first was chosen — partial fizzle (b never picked).
    ///     Caller can still fire b-independent riders.
    ///
    /// Reserves resume_points 9..14 and resume_data[3..4] so it can
    /// coexist with confirmOptional (0/1/2), pickXAmount (also
    /// 0/1/2 + data[0]), pickMode (3/4/5 + data[1]), pickTarget
    /// (6/7/8 + data[2]) inside the same Card.
    ///
    /// Trace tags: TGT_PAIR_PROMPT_A, TGT_PAIR_PICKED_A,
    /// TGT_PAIR_PROMPT_B, TGT_PAIR_PICKED_B, TGT_PAIR_NOT_OFFERED.
    std::pair<GameObjectId, GameObjectId> pickTargetPair(
        CardContext& ctx, const std::string& label,
        const std::vector<GameObjectId>& legal_a,
        const std::function<std::vector<GameObjectId>(GameObjectId picked_a)>& legal_b_fn);

    // ── Targeting ──
    /// Target requirements for legal action generation.
    virtual TargetRequirements getTargetRequirements() const { return {}; }

    /// Enumerate legal targets given current game state.
    /// Default implementation uses getTargetRequirements() with standard filtering.
    /// Override for cards with unusual targeting.
    virtual std::vector<GameObjectId> enumerateLegalTargets(
        const GameState& state, PlayerId controller) const;

    /// Multi-ability variant — used by generateActivateAbilityActions to
    /// enumerate per-ability legal targets. Default impl ignores
    /// ability_index and falls through to the single-arg version, so
    /// single-ability cards work without changes. Multi-ability cards
    /// override this to dispatch on the index. (Phase 6r)
    virtual std::vector<GameObjectId> enumerateLegalTargets(
        const GameState& state, PlayerId controller,
        int /*ability_index*/) const {
        return enumerateLegalTargets(state, controller);
    }

    /// Universal legality gate for play / activate intents.
    ///
    /// Returns true if this card can legally be played (or its activated
    /// ability used) RIGHT NOW from `controller`'s perspective.
    ///
    /// Default impl preserves the historical engine behavior: if
    /// getTargetRequirements().count > 0 and !optional, require at least one
    /// legal board target. Otherwise return true.
    ///
    /// Override on cards whose target lives outside the board-object pool —
    /// most notably counter spells, which target a chain item and cannot be
    /// legally played when there's nothing on the chain to counter (per CR
    /// 700.x — a card with a target requirement is not playable if the
    /// requirement cannot be satisfied at play time). The engine calls this
    /// from generateSpellActions / generateActivateAbilityActions before
    /// emitting any intent.
    virtual bool hasLegalTargets(const GameState& state,
                                  PlayerId controller) const;

    /// Whether THIS card defers play-time target selection to
    /// resolve-time via pickTarget. When true, generateSpellActions
    /// emits ONE Play intent per card (no per-target variants), and
    /// the card's onResolve uses Card::pickTarget to publish a follow-up
    /// MakeChoice for the agent. When false (default), the historical
    /// behavior runs: action gen emits one Play per (card, target)
    /// pair, and the chain item carries the target in `targets`.
    ///
    /// Phase 6q. Existing cards continue to work because the default
    /// is false; opt in by overriding to true AND rewriting onResolve
    /// to consume the target via pickTarget(...) instead of
    /// targets[0]. See src/cards/manual/deck_cards.cpp's
    /// (post-Phase-6q) MFrigidTouch / MGrimResolve / MStarCrossed as
    /// reference migrations.
    virtual bool needsPlayTimeTarget() const { return false; }

    /// Same shape as needsPlayTimeTarget but for two-target spells.
    /// When true, generateSpellActions emits ONE Play intent per
    /// card (no per-target-pair Cartesian product), and the card's
    /// onResolve uses pickTargetPair to publish both target choices
    /// as sequential MakeChoice intents. Replaces the
    /// O(friendly × enemy) action-vocab blowup for cards like
    /// Challenge.
    virtual bool needsPlayTimeTargetPair() const { return false; }

    // ── Play-to-location ──
    /// Custom play locations (for "play me to an open battlefield" etc.).
    /// Empty = normal play rules (base or controlled BF).
    virtual std::vector<LocationId> getPlayLocations(
        const GameState& state, PlayerId player) const { return {}; }

    /// Passive aura contribution. Called once per on-board instance of
    /// this card during `recalculateAuras`. The card can mutate
    /// game state to broadcast its passive effect to other cards or to
    /// per-player counters (e.g., Karthus bumping
    /// `PlayerState::deathknell_double_count`). This is the engine-
    /// generic counterpart to the per-card aura text-matching in
    /// `recalculateAuras` — overriding this is how a Card communicates
    /// "I have an ongoing passive effect" without the engine needing
    /// to know what the card does.
    ///
    /// Counters mutated here MUST be reset to zero/default at the start
    /// of each `recalculateAuras` pass, since this hook re-applies
    /// them every cleanup cycle. The engine resets the counters it
    /// knows about (e.g., deathknell_double_count) before calling
    /// `applyPassiveAura` on each on-board card.
    virtual void applyPassiveAura(GameState& state, PlayerId controller) const {}

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

    /// Whether THIS gear defers equip-time target selection to onEquip
    /// via pickTarget. When true, generateMainPhaseActions emits ONE
    /// equip intent per gear (no per-target enumeration), and the
    /// engine's equip-execution path calls `onEquip(ctx, kInvalidId)`
    /// — the Card uses `pickTarget` inside `onEquip` to resolve the
    /// target at activation time. Mirrors `needsPlayTimeTarget` /
    /// `needs_activation_time_target` and fixes the same action-vocab
    /// collision class for equip intents. Default false (legacy
    /// per-target enumeration).
    virtual bool needsEquipTimeTarget() const { return false; }

    /// Trigger type that fires on the equipped UNIT (from effect_text).
    /// e.g., "When I conquer" in effect_text → WhenIConquer on the unit.
    virtual TriggerType equippedTriggerType() const { return TriggerType::None; }

    /// Called when the equipped unit's trigger fires. Gear handles its own logic.
    virtual void onEquippedTrigger(CardContext& ctx,
                                   GameObjectId unit,
                                   const std::vector<GameObjectId>& targets) {}

    /// Keywords granted to the equipped unit (from effect_text).
    virtual KeywordSet equippedKeywords() const { return {}; }

    /// Skyfall of Areion: "My hold effects are also conquer effects, and vice
    /// versa." When an attached gear returns true, the engine cross-fires the
    /// equipped unit's (and its other gear's) WhenIHold triggers on a Conquer
    /// score and WhenIConquer triggers on a Hold score. Generic property the
    /// trigger dispatcher honors; no card-specific logic in the engine.
    virtual bool crossesHoldConquerTriggers() const { return false; }

    /// Assault/Shield/Deflect values granted to equipped unit.
    virtual int equippedAssault() const { return 0; }
    virtual int equippedShield() const { return 0; }
    virtual int equippedDeflect() const { return 0; }

    // ── Optional additional cost at play time ──
    /// "You may pay X as an additional cost to play me." Returned by cards
    /// (Akshan [O][O], Nami [G]) whose play-time triggers are gated on whether
    /// the optional cost was paid. The engine offers the agent a yes/no during
    /// the play action (after the base cost, before WhenYouPlayMe fires) and,
    /// if accepted, pays it and sets card_counters[paid_flag] = 1 — which the
    /// card's onPlay / WhenYouPlayMe reads. Modeled on the Accelerate flow.
    struct OptionalAdditionalCost {
        bool valid = false;
        int energy = 0;
        int power = 0;
        Domain power_domain = Domain::Fury;
        bool any_domain = false;        // power may be any domain ([A])
        const char* paid_flag = "";     // card_counters key set when paid
    };
    virtual OptionalAdditionalCost optionalAdditionalCost() const { return {}; }

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

    // ── Targeting protection ──
    /// Whether this card may be the target of enemy spells and abilities.
    /// Default true; cards like Baron Nashor override to false. The engine
    /// populates GameObject::untargetable_by_enemy from this during
    /// cleanup/aura recalc, and Card::enumerateLegalTargets reads that
    /// flag to filter out untargetable enemy candidates.
    virtual bool canBeChosenByEnemy() const { return true; }

    // ── Entry state ───────────────────────────────────────────────────
    /// Whether this unit should enter the board READY (not exhausted) on
    /// play, without paying an Accelerate cost. Default false; resolves
    /// to the engine's normal "Units enter exhausted" (CR 143.4) rule.
    /// Used by Daisy! ("I enter ready") and similar self-ready cards.
    /// Consulted in GameEngine::resolvePermanent after Accelerate/Ambush
    /// branches.
    virtual bool entersReadyOnPlay() const { return false; }

    /// State-aware variant for conditional "enter ready" effects. Default
    /// impl falls through to the no-arg version. Used by Monch
    /// ("If an opponent controls a stunned unit, I enter ready").
    virtual bool entersReadyOnPlay(const GameState& /*state*/,
                                    PlayerId /*controller*/) const {
        return entersReadyOnPlay();
    }

};

// ─── Subclasses ─────────────────────────────────────────────────────────────
// Each type-base is abstract: concrete cards implement def(). They carry no
// id/CardDef — a card defines itself via its def() override.

class UnitCard : public Card {};

class SpellCard : public Card {};

class GearCard : public Card {};

class LegendCard : public Card {};

class BattlefieldCard : public Card {
public:
    /// Turn gate for scoring this battlefield (Forgotten Monument [523]:
    /// "Players can't score here until their third turn" → 3). A player may
    /// only score here once their PlayerState::turns_taken >= this value.
    /// 0 (default) = no restriction. Read once by GameEngine::setupBattlefields
    /// into BattlefieldState::min_turn_to_score.
    virtual int minTurnToScore() const { return 0; }
};

class RuneCard : public Card {};

} // namespace riftbound
