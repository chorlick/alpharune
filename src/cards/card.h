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
#include "core/prompt.h"

#include <bitset>
#include <functional>
#include <memory>
#include <optional>
#include <source_location>
#include <utility>
#include <vector>

namespace riftbound {

// Forward declarations
class GameState;
struct GameObject;
class EventBus;
class EffectExecutor;
class CardRegistry;
struct ChainItem;

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
    // Optional registry handle so a card's effect can dispatch into OTHER
    // cards' logic (Reckoner's Arena re-firing units' conquer effects;
    // Heimerdinger proxying granted abilities). Null in contexts that don't
    // set it; cards MUST null-check. Set by the resolution path.
    const CardRegistry* registry = nullptr;
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
    /// Called when this spell resolves from the chain. `targets` holds the
    /// play-time targets that are still legal, in chosen order with the
    /// illegal ones removed: positions are not roles (see chosenTarget).
    virtual void onResolve(CardContext& /*ctx*/,
                           const std::vector<GameObjectId>& /*targets*/) {}

    // ── Permanent enters board ──
    /// Called when this permanent (unit/gear) enters the board from play.
    virtual void onPlay(CardContext& /*ctx*/) {}
    /// "As you play me, choose / name ..." (CR 355.1): the choice made as the
    /// card is played, before its cost — no chain item, no priority window.
    /// Called by every play path as the play begins (GameEngine::
    /// executePlayCard, executePlayFromHidden; EffectExecutor::playIgnoringCost
    /// / playPayingCost). The card asks with the executor's synchronous
    /// prompts (chooseValueNow, chooseObjectNow, confirmNow).
    virtual void asYouPlayChoices(CardContext& /*ctx*/) {}
    /// "As you look at or reveal me from the top of your deck, ..." — a
    /// replacement applied as the card is seen there (CR 370.1.c.1): no chain
    /// item, inside the effect that looks (EffectExecutor::seenOnTopOfDeck).
    /// `ctx.source` is the card, `ctx.controller` its owner. A card that takes
    /// itself away records it in GameState::left_while_seen.
    virtual bool replacesBeingSeenOnTop() const { return false; }
    virtual void seenOnTopOfDeck(CardContext& /*ctx*/) {}

    // ── Activated abilities (single-ability legacy surface) ──
    /// Called when an activated ability ([E]:, [N]:, etc.) is used.
    /// Legacy single-ability hook. Cards with one activated ability still
    /// override this. Cards with N abilities override the indexed variant
    /// below instead.
    virtual void onActivate(CardContext& /*ctx*/,
                            const std::vector<GameObjectId>& /*targets*/) {}

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

    // ── Empower (Vendetta, CR 827) ──
    /// "[Empower] [Cost]" — "[Cost]: Empower this. Play only if not Empowered."
    /// A card that prints Empower returns its cost here (valid = true). The
    /// DEFAULT activatedAbilities() appends it as the LAST ability (after the
    /// legacy single ability, if any) and the default indexed onActivate
    /// empowers the source for that index — so a plain Empower card overrides
    /// only this (plus applyPassiveAura for its [Empowered] text). Cards that
    /// override activatedAbilities() themselves append `empowerAbility()`.
    struct EmpowerSpec {
        bool valid = false;
        ActivationCost cost;
        // "[Empower] — [1] or [O]": a second way to pay. Each cost is its own
        // Empower ability, so the player picks one by activating it.
        bool has_alt_cost = false;
        ActivationCost alt_cost;
    };
    virtual EmpowerSpec empowerSpec() const { return {}; }
    /// The Empower ability descriptor built from empowerSpec().
    ActivatedAbility empowerAbility() const;
    /// Appends the Empower abilities (one per cost) to `out`.
    void appendEmpowerAbilities(std::vector<ActivatedAbility>& out) const;
    /// Index of the Empower ability within activatedAbilities(), or -1.
    int empowerAbilityIndex() const;
    /// How many Empowered statuses this object may hold (Kayle, Justified: 3).
    virtual int maxEmpower() const { return 1; }
    /// "I can have any number of buffs" (CR 427.1.b.2): lifts the one-Buff
    /// limit EffectExecutor::buffUnit otherwise enforces (CR 427.1.b / 702.3).
    virtual bool canHaveAnyNumberOfBuffs() const { return false; }

    /// Per-ability, per-instance activation gate. Consulted by the action
    /// generator after canActivateAbility. The default implements the Empower
    /// rule ("Play only if not Empowered") and the disempower_self cost
    /// ("Disempower this" requires being Empowered).
    virtual bool canActivateAbilityAt(const GameState& state, PlayerId controller,
                                      GameObjectId source, int ability_index) const;

    /// Instance-aware variant of activationCostReduction (knows the source
    /// object). Default: activationCostReduction(...) plus, for the Empower
    /// ability, the source's aura-computed empower_cost_reduction.
    virtual int activationCostReductionFor(const GameState& state, PlayerId controller,
                                           GameObjectId source, int ability_index) const;

    // ── Flow (Vendetta) ──
    /// "[Flow] [Cost]" — the spell may be played from its owner's trash for
    /// this cost, then banished. valid = false for cards without Flow.
    struct FlowCost {
        bool valid = false;
        int energy = 0;
        int power = 0;
        Domain power_domain = Domain::Fury;
        bool any_domain = false;        // power is [A]
    };
    virtual FlowCost flowCost() const { return {}; }

    /// "This can't be countered." Counter effects leave such a spell on the chain.
    virtual bool cantBeCountered() const { return false; }

    /// "This enters exhausted." (gear normally enters ready, CR 149.1).
    virtual bool entersExhausted() const { return false; }

    /// Play-legality gate independent of cost/targets ("I can't be played on
    /// your first, second, or third turns" — Ol' Poro). Default: playable.
    virtual bool canBePlayed(const GameState& /*state*/, PlayerId /*player*/) const { return true; }

    /// Cost to activate the (single, legacy) ability.
    virtual ActivationCost getActivationCost() const { return {}; }

    /// Whether the (single, legacy) activated ability [Add]s resources (CR
    /// 430.2): it resolves as it is activated (ActivatedAbility::adds_resources).
    virtual bool abilityAddsResources() const { return false; }

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

    /// activatedAbilities(), built once per card object instead of once per
    /// question (the generators ask for every object in play at every
    /// decision, and each answer was a fresh vector — plans/06 item 6.5). The
    /// descriptors are a property of the card class: activatedAbilities()
    /// takes no game state. Engine code reads this; a card that overrides
    /// activatedAbilities() needs to do nothing else.
    const std::vector<ActivatedAbility>& abilities() const;

    /// Builds what abilities() and firesOn() answer from. CardRegistry calls
    /// it as a card is registered; a card that is never registered (a test
    /// that drives a card object directly) builds it on first use.
    void primeStaticFacts() const;

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

    /// Whether this card fires on the given trigger event: is `t` among
    /// triggerTypes(). Answered from a set built once per card object (the
    /// trigger manager asks it of every object in play for every event, and
    /// each triggerTypes() call was a fresh vector — plans/06 item 6.3).
    bool firesOn(TriggerType t) const {
        if (!static_facts_) primeStaticFacts();
        return static_facts_->triggers.test(static_cast<size_t>(t));
    }

    /// Called when a matching trigger fires. For multi-trigger cards,
    /// inspect ctx.firing_trigger to tell which event fired. `targets` is
    /// ALWAYS EMPTY: the engine hands a triggered ability no targets
    /// (GameEngine::resolveSpell passes an empty list whatever the chain item
    /// holds). The event's subject is ChainItem::triggering_subject; a target
    /// is picked with pickTarget / pickTargetPair (kFinalizeTrigger).
    virtual void onTrigger(CardContext& /*ctx*/,
                           const std::vector<GameObjectId>& /*targets*/) {}

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
    ///   2. Further invocations: return the answer already given (1 or 0).
    ///
    /// Leaves resume_point at 1 (waiting) / 2 (answered); the answer itself is
    /// ChainItem::may_answer, so this composes with the pick* helpers in any
    /// order. ONE "may" per callback: a second confirmOptional in the same
    /// callback returns the first one's answer. A second yes/no is a
    /// two-mode pickMode; a second clause that is its own "When ..." sentence
    /// is its own trigger (Card::triggerTypes).
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
                        const std::function<bool()>& still_legal,
                        std::source_location where = std::source_location::current());

    // ── Choices at finalization (CR 355, 383.3) ──
    // Targets, modes, "you may" and "[cost] to [effect]" payments of a spell
    // or ability are decided when it is played / put on the chain, before
    // anyone can respond — not as it resolves. A card opts a callback in by
    // returning its bit here, and splits that callback in two:
    //
    //     int go = confirmOptional(ctx, ...);          // choices
    //     if (go <= 0) return;
    //     auto t = pickTarget(ctx, ...);
    //     if (costDueNow(ctx)) payEnergy(ctx, 1);      // costs
    //     if (finalizingChoices(ctx)) return;          // ── end of choices ──
    //     ... the effect ...
    //
    // The engine runs the callback once at finalization (it stops at the
    // marker) and again at resolution, where the helpers return the stored
    // answers. Nothing before the marker may change game state other than
    // through the helpers and the cost payment.
    enum : unsigned { kFinalizeTrigger = 1, kFinalizeResolve = 2, kFinalizeActivate = 4 };
    virtual unsigned choicesAtFinalization() const { return 0; }
    /// True while the engine is collecting this item's finalization choices.
    /// The marker also ends the card's cost section on every path: a cost
    /// that costDueNow() said was due counts as paid once a run reaches it.
    static bool finalizingChoices(const CardContext& ctx);
    /// For a choice helper: it has just stored "nothing to decide" as its
    /// answer on `item` (see the rule at Card::finalizingChoices in card.cpp).
    static void nothingToDecide(ChainItem& item);
    /// For a choice helper, in a finalize pass: a REQUIRED choice (a target,
    /// a mode) has no legal option. CR 403.4: "If there are not enough options
    /// to make legal choices for a Triggered Ability that has been put on the
    /// chain, remove it from the Chain now." The engine removes a triggered
    /// ability whose pass recorded this (GameEngine::runFinalizePass); for a
    /// spell or an activated ability it means nothing. Not for a choice the
    /// text makes optional ("up to", "any number of", a "may" pick).
    static void requiredChoiceMissing(ChainItem& item);
    /// For a triggered ability that asks an optional part of its effect with
    /// confirmOptional in its finalize pass ("heal your units here, then move
    /// UP TO ONE enemy unit"): that "may" is not the first part of the effect,
    /// so declining it is not declining the ability (CR 383.3.a: only a "you
    /// may" that is the first part of the effect decides "whether or not to
    /// perform the Triggered Ability"). Call it before confirmOptional; the
    /// engine then keeps the ability on the chain when the answer is no.
    static void mayIsNotTheWholeAbility(CardContext& ctx);
    /// Whether the cost is still to be paid in this run. Due exactly once per
    /// chain item: false after it was paid at finalization, and false after a
    /// run reached the marker with it paid — also when no finalize pass ran
    /// and a later prompt re-enters the card.
    static bool costDueNow(CardContext& ctx);
    /// A cheap digest of what a cost payment changes (pools, zone sizes, XP,
    /// each object's zone / exhaustion / buffs / damage). The engine compares
    /// it across a finalize-pass run to catch a card that paid a cost and
    /// then asked for a choice before its marker.
    static uint64_t costStateFingerprint(const GameState& state);

    /// For the play gate of a spell that chooses in its finalize pass
    /// (kFinalizeResolve): whether it could choose `object`, a unit its
    /// controller has on the board. Such a spell is paid for after those
    /// choices (CR 355 -> 356), so one that is too dear on its own is still
    /// offered when choosing `object` makes it payable ("your spells that
    /// choose me cost [1] or [A] less"). The engine cannot know a card's pick
    /// list, so every finalize-pass spell declares this (card_helpers.h has
    /// friendlyUnitChoice for the common shapes); a registry test fails for
    /// one that leaves it Undeclared. Must cope with an `object` that does
    /// not exist (the registry test asks with kInvalidId).
    enum class FinalizeChoice { Undeclared, No, Yes };
    virtual FinalizeChoice mayChooseAtFinalization(const GameState& /*state*/,
                                                   PlayerId /*controller*/,
                                                   GameObjectId /*object*/) const {
        return FinalizeChoice::Undeclared;
    }

    /// Whether this spell, played from facedown at `battlefield`, has the
    /// choices it needs there (CR 811.1.d: a hidden card's choices are made
    /// from that battlefield, and with none it cannot be played). Asked for a
    /// spell that declares no play-time targets and chooses in its finalize
    /// pass. Default: hasLegalTargets(); a card whose pick list is tied to the
    /// hidden battlefield (facedownPlayBattlefield) overrides this to match.
    virtual bool hasLegalChoicesHiddenAt(const GameState& state, PlayerId controller,
                                         const CardDB& db, BattlefieldId battlefield) const;
    /// True at resolution when the cost was paid at finalization: checks like
    /// "can I afford it" / "am I ready to exhaust" no longer apply.
    static bool costAlreadyPaid(const CardContext& ctx);

    // ── A cost that could not be paid ("[kill X] to [effect]") ──
    // A cost is attempted once. When the attempt fails — the kill was replaced,
    // so nothing died (CR 370.1.b) — the ability has not been paid for: it does
    // nothing, and it must not try again when it resolves. The failure is kept
    // on the chain item, so the finalize pass and the resolution agree:
    //
    //     bool paid = true;
    //     if (costDueNow(ctx) && !costUnpaid(ctx)) paid = killAsCost(ctx, unit);
    //     if (finalizingChoices(ctx)) return;      // still reached
    //     if (!paid || costUnpaid(ctx)) return;    // nothing was paid: no effect
    //
    /// Kill `object` (a unit or a gear on the board) as a cost. True iff it
    /// DIED: a death that a replacement turned into something else — it
    /// stayed, was recalled, or was banished "instead" — is not a kill,
    /// wherever the object ends up (a banished token and a dead token are in
    /// the same place; this tells them apart). On false the failure is
    /// recorded (costUnpaid).
    bool killAsCost(CardContext& ctx, GameObjectId object) const;
    /// Record on the resolving chain item that its cost could not be paid.
    static void markCostUnpaid(CardContext& ctx);
    /// Whether an earlier run of this callback recorded that.
    static bool costUnpaid(const CardContext& ctx);

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
    /// On re-entry, returns the previously-chosen X stashed in
    /// `resume_data[0]` so the caller can re-invoke this helper from a
    /// later phase of its own resume state machine and get the same X
    /// back without re-prompting.
    ///
    /// Leaves resume_point at 1 (waiting) / 2 (answered); uses resume_data[0].
    int pickXAmount(CardContext& ctx, const std::string& label,
                    int min, int max, std::source_location where = std::source_location::current());

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
    /// Leaves resume_point at 4 (waiting) / 5 (answered); uses
    /// resume_data[1]. Composes with the other helpers in any order (see
    /// kChoiceStateSlot).
    ///
    /// Trace tags: MODE_PROMPT, MODE_PICKED, MODE_NOT_OFFERED.
    int pickMode(CardContext& ctx, const std::string& label, int num_modes,
                 const std::vector<std::string>& mode_labels = {},
                 uint32_t legal_modes = 0xFFFFFFFFu,
                 std::source_location where = std::source_location::current(),
                 PromptKind kind = PromptKind::Mode);
    /// Record `mode` as this callback's pickMode answer without asking: for a
    /// helper that finds there is nothing to choose now but must not have the
    /// question asked later, when there might be (pickEffectPlayLocation at
    /// finalization with no battlefield controlled). Does nothing once a mode
    /// has been asked for or answered.
    void settleMode(CardContext& ctx, int mode);

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
    /// Leaves resume_point at 7 (waiting) / 8 (answered); uses
    /// resume_data[2]. Composes with the other helpers in any order (see
    /// kChoiceStateSlot). ONE pickTarget per callback: a second call returns
    /// the first pick. Two objects are a pickTargetPair.
    ///
    /// Trace tags: TGT_PROMPT, TGT_PICKED, TGT_FORCED, TGT_NOT_OFFERED.
    GameObjectId pickTarget(CardContext& ctx, const std::string& label,
                             const std::vector<GameObjectId>& legal_targets,
                             std::source_location where = std::source_location::current());
    /// pickTarget for a card picked out of a zone that is not public — a
    /// hand, a deck, cards looked at, Banishment — which is not a target (CR
    /// 355.10.a): the same prompt, slots, resume points and return values as
    /// pickTarget (it IS the callback's pickTarget: use one or the other),
    /// but the prompt is a PromptKind::ChooseObject that says "pick:", no
    /// Deflect is filtered for or paid, and the card is not recorded as
    /// chosen (commitChoice). Asked as the item resolves (testing phase R13).
    GameObjectId pickUnseenCard(CardContext& ctx, const std::string& label,
                                const std::vector<GameObjectId>& options,
                                std::source_location where = std::source_location::current());

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
    ///   {kInvalidId, kInvalidId} on suspend — for the first AND for the
    ///     second pick (caller MUST return immediately — chain manager
    ///     re-enters after agent picks) OR on fizzle (no legal first
    ///     target). Distinguish via ctx.state.chain.resuming->resume_point
    ///     (10 / 12 = waiting, 13 = done).
    ///   {a, b} on full completion.
    ///   {a, kInvalidId} if second pick has no legal targets after
    ///     first was chosen — partial fizzle (b never picked).
    ///     Caller can still fire b-independent riders.
    ///
    /// Leaves resume_point at 10 / 12 (waiting) / 13 (done); uses
    /// resume_data[3..4]. Composes with the other helpers in any order
    /// (see kChoiceStateSlot).
    ///
    /// Trace tags: TGT_PAIR_PROMPT_A, TGT_PAIR_PICKED_A,
    /// TGT_PAIR_PROMPT_B, TGT_PAIR_PICKED_B, TGT_PAIR_NOT_OFFERED.
    std::pair<GameObjectId, GameObjectId> pickTargetPair(
        CardContext& ctx, const std::string& label,
        const std::vector<GameObjectId>& legal_a,
        const std::function<std::vector<GameObjectId>(GameObjectId picked_a)>& legal_b_fn,
        std::source_location where = std::source_location::current());

    /// resume_data slot holding the pick* helpers' per-helper progress flags
    /// (waiting / answered). Because each helper tracks its own progress here
    /// rather than reading the shared resume_point, they may be called in any
    /// order. Slots 0..5 are the helpers' answers and the engine's
    /// "target chosen at finalization" flag (slot 5). A card that drives
    /// resume_data by hand AND calls a pick* helper must leave slots 0..6 to
    /// the helpers (or finish with resume_data before the first helper call).
    static constexpr size_t kChoiceStateSlot = 6;
private:
    GameObjectId pickOneObject(CardContext& ctx, const std::string& label,
                               const std::vector<GameObjectId>& options, bool as_target,
                               std::source_location where);
public:

    // ── Choosing a spell or ability on the chain (CR 355.9.a.2) ──
    /// "Counter a spell", "Choose a spell", "an enemy spell or ability that
    /// chooses ...": the item is a target, chosen as this spell or ability is
    /// played / finalized (CR 355.5), so a card opts into choices at
    /// finalization and asks with this before its marker. `candidates` are the
    /// finalized items it may choose now (chainItemChoices in card_helpers.h
    /// builds them: never this item itself, CR 355.9.c). Each option names the
    /// item's source object and carries the item's id as its value.
    /// Returns nullopt while the agent decides (return at once); kInvalidId
    /// when there is nothing to choose (stored: nothingToDecide, and
    /// requiredChoiceMissing) and, as the item resolves, when the chosen item
    /// is no longer on the chain or no longer among `candidates` — a
    /// mistarget (CR 359.3.e.5): the instruction does nothing, and nothing
    /// else is taken instead. One per callback; the answer is
    /// ChainItem::chosen_item. The item is acted on wherever it lies on the
    /// chain: ChainManager::removeItem (EffectExecutor::removeChainItem) is
    /// the one way a card takes it off.
    std::optional<ChainItemId> pickChainItem(CardContext& ctx, const std::string& label,
                                             const std::vector<ChainItemId>& candidates,
                                             std::source_location where = std::source_location::current());

    // ── A cost owed after the choices (CR 403.2 -> 405.1) ──
    /// "You may pay [N] to <choose a unit>": the item's choices come before
    /// its cost, and choosing a unit with [Deflect] adds to what must be paid
    /// (CR 809). Called before the choice helpers, it records the cost on the
    /// resolving item, so a unit is offered only if its Deflect and this cost
    /// can be paid together, and a Deflect is paid from what the cost cannot
    /// use first (testing phase F5). The card still pays the cost itself
    /// (costDueNow) after its choices.
    static void owesAfterChoices(CardContext& ctx, int energy, int power = 0,
                                 uint32_t power_mask = ~0u);
    /// `candidates` minus those `ctx.controller` could not choose and still
    /// pay [energy] / [power] afterwards (their Deflect counted): what a "may
    /// pay [N] to choose" offers, and its "may" is offered only if this is
    /// not empty.
    std::vector<GameObjectId> choosablePaying(const CardContext& ctx,
                                              const std::vector<GameObjectId>& candidates,
                                              int energy, int power = 0,
                                              uint32_t power_mask = ~0u) const;

    // ── Choosing an object with hand-built options ──
    // A card that asks for an object through its own MakeChoice options
    // (objChoice / takenObject in card_helpers.h) instead of pickTarget owes
    // the same bookkeeping pickTarget does:
    //
    //     for (auto id : offerableChoices(ctx, candidates))     // when publishing
    //         opts.push_back(objChoice(ctx.controller, id));
    //     ...
    //     GameObjectId id = takenObject(ctx);                   // on re-entry
    //     commitChosenObject(ctx, id);                          // exactly once
    //
    /// `candidates` minus the objects whose Deflect cost (CR 809) the
    /// controller can't pay.
    std::vector<GameObjectId> offerableChoices(
        CardContext& ctx, const std::vector<GameObjectId>& candidates) const;
    /// Pays Deflect for choosing `chosen`, records it on the resolving chain
    /// item, sets "chose an enemy unit this turn" and emits the
    /// "chosen_by_controller" event. Only for objects ctx.controller chose;
    /// not for a pick the opponent makes among their own objects.
    void commitChosenObject(CardContext& ctx, GameObjectId chosen) const;

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

    /// What the engine calls for an activated ability: also given the object
    /// whose ability it is, for targets defined relative to it ("a unit
    /// here", "an attacker at my battlefield"). Default ignores `source`.
    virtual std::vector<GameObjectId> enumerateLegalTargets(
        const GameState& state, PlayerId controller,
        int ability_index, GameObjectId /*source*/) const {
        return enumerateLegalTargets(state, controller, ability_index);
    }

    /// An intervening condition on a triggered ability ("When a BUFFED
    /// friendly unit dies", "..., if you ..."): checked when the event
    /// happens; false means the ability does not trigger at all. `subject` is
    /// the event's object (the unit that died, the card that was played),
    /// kInvalidId when the event has none.
    virtual bool triggerCondition(const GameState& /*state*/, GameObjectId /*source*/,
                                  PlayerId /*controller*/, TriggerType /*trigger*/,
                                  GameObjectId /*subject*/) const {
        return true;
    }

    /// The event a trigger condition is judged on (TriggerManager::fireTrigger,
    /// queueEquippedTrigger): the triggerCondition arguments, plus the
    /// PRINTED card of the event's object when it has one — what a condition
    /// on a printed value of the card played reads ("a spell that costs [5]
    /// or more", "a card with Power cost [A][A] or more", CR 383.1).
    struct TriggerEvent {
        GameObjectId source = kInvalidId;
        PlayerId controller = PlayerId::None;
        TriggerType trigger = TriggerType::None;
        GameObjectId subject = kInvalidId;
        const CardDef* subject_card = nullptr;   // null: no subject, or a token with no card
    };
    /// What the engine asks. Default: triggerCondition(state, source,
    /// controller, trigger, subject). A card whose condition reads the
    /// subject's printed card overrides this one instead.
    virtual bool triggerConditionOn(const GameState& state, const TriggerEvent& event) const {
        return triggerCondition(state, event.source, event.controller, event.trigger,
                                event.subject);
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

    /// What the engine calls: also given the card table, for a requirement on
    /// a target's PRINTED values ("a spell that costs no more than [4]") —
    /// those live in the CardDef, not on the GameObject or the chain item.
    /// Default: the two-argument version. A card whose requirement is on a
    /// printed value overrides this one, and the two-argument one with the
    /// part of the requirement the state alone shows.
    virtual bool hasLegalTargets(const GameState& state, PlayerId controller,
                                  const CardDB& /*db*/) const {
        return hasLegalTargets(state, controller);
    }

    /// The object chosen for target slot `role` when a spell or ability with
    /// several play-time targets was put on the chain, or kInvalidId when that
    /// slot was not filled or its target is no longer legal.
    ///
    /// THE CONTRACT for multi-target cards: `targets`, as handed to onResolve
    /// / onActivate, holds only the targets that are STILL LEGAL at resolution
    /// (CR 359.3.e.5), in their chosen order but with the illegal ones
    /// removed — so its positions are not roles. A card whose slots mean
    /// different things ("kill this one, pump that one") reads each slot
    /// through this helper; a card that treats its targets alike iterates
    /// `targets`.
    static GameObjectId chosenTarget(const CardContext& ctx,
                                     const std::vector<GameObjectId>& targets,
                                     size_t role);

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
        const GameState& /*state*/, PlayerId /*player*/) const { return {}; }

    /// When true, the play-from-hand action generator emits play intents ONLY
    /// for the locations returned by getPlayLocations() — it suppresses the
    /// default base play and the default controlled/keyword BF plays. This is
    /// the NARROWING counterpart to getPlayLocations()'s widening use: a card
    /// that "can be played only to <subset>" returns that subset (possibly
    /// empty = unplayable). Default false preserves normal rules.
    /// (Perched Grimwyrm 338: "play me only to a battlefield you conquered
    /// this turn".)
    virtual bool restrictsPlayLocations() const { return false; }

    /// Battlefields this unit may be played to beyond the ones its controller
    /// controls (CR 355.2), as printed on the card: "You may play me to an
    /// open battlefield" / "... to an occupied enemy battlefield". A bit set
    /// of the values below; the unit-play generator adds those battlefields
    /// to the default locations. (A grant to OTHER units is a PlayerState
    /// flag set from applyPassiveAura, e.g. grant_friendly_units_open_bf.)
    enum : unsigned {
        kPlayToOpenBattlefield = 1,           // no controller
        kPlayToOccupiedEnemyBattlefield = 2,  // enemy-controlled, enemy units there
                                              // (Maduli reads "occupied enemy
                                              // battlefield" otherwise: judge
                                              // question 17, testing phase R14)
        kPlayToAnyBattlefield = 4,
        kPlayToLoneEnemyBattlefield = 8,      // exactly one unit there, an enemy
    };
    virtual unsigned extraPlayLocations() const { return 0; }

    /// CR 811.1.d.2 / .4: a spell played from facedown chooses each target at
    /// the battlefield it was hidden at, "unless it explicitly restricts
    /// targeting in such a way that makes this impossible" — judged target by
    /// target. Return true for a target slot whose own printed restriction
    /// rules that battlefield out ("another unit ... at a different
    /// location": slot 1 of Smoke and Mirrors); that slot is then chosen
    /// freely. Slots are positions in the play-time target list.
    virtual bool hiddenTargetFreeOfBattlefield(int /*slot*/) const { return false; }

    /// When true, this unit's [Ambush] may be played to a battlefield that has
    /// ENEMY units even if the controller has no units there (the base Ambush
    /// keyword restricts placement to BFs where you already have units). The
    /// Ambush action generator consults this per-card relaxation.
    /// (Rengar, Trophy Hunter 682.)
    virtual bool ambushToEnemyBattlefields() const { return false; }

    /// Passive aura contribution. Called during `recalculateAuras` ONCE
    /// per (card, controller) that has at least one instance in play (on
    /// board, or in the Legend Zone) — not once per instance, and without
    /// saying which instance. The hook therefore applies the effect of EVERY
    /// instance `controller` has in play: loop over the instances (or use
    /// `forEachSelfOnce` in card_helpers.h), check each one's own condition
    /// ("here", "while I'm at a battlefield", ...) and let copies stack.
    /// A battlefield card is called once per battlefield slot holding it,
    /// with that slot's controller (PlayerId::None if uncontrolled).
    /// The card can mutate game state to broadcast its passive effect to
    /// other cards or to per-player counters (e.g., Karthus bumping
    /// `PlayerState::deathknell_double_count`); a `CostModifier` it pushes
    /// is tagged `from_aura` by the engine and rebuilt every pass. This is
    /// the ONLY way a card has a continuous effect: the engine does not
    /// read aura wording out of `ability_text`.
    ///
    /// Counters mutated here MUST be reset to zero/default at the start
    /// of each `recalculateAuras` pass, since this hook re-applies
    /// them every cleanup cycle. The engine resets the counters it
    /// knows about (e.g., deathknell_double_count) before calling
    /// `applyPassiveAura` on each on-board card.
    virtual void applyPassiveAura(GameState& /*state*/, PlayerId /*controller*/) const {}

    /// A continuous effect whose CONDITION is a unit's Might ("While I'm
    /// [Mighty], I have ..."). Might is only known once every card's
    /// applyPassiveAura has run and been folded, so such an effect cannot be
    /// decided in that hook without reading the previous pass's numbers.
    /// A card that returns true from hasMightDependentAura() gets this second
    /// hook in the same pass, after the fold: every object's current_might is
    /// then this pass's Might from all ordinary auras, and none of the effects
    /// applied by this hook (by this or any other card) are in it yet — so an
    /// effect never counts toward the Might test that grants it. The engine
    /// folds again afterwards. Same calling contract as applyPassiveAura (once
    /// per (card, controller), every instance).
    virtual bool hasMightDependentAura() const { return false; }
    virtual void applyMightDependentAura(GameState& /*state*/, PlayerId /*controller*/) const {}
    /// True for a card whose continuous effect is given to cards in a Main
    /// Deck, a Rune Deck, a Trash or Banishment ("Your Equipment everywhere
    /// have [Quick-Draw]"). While one is in play the Cleanup walks every
    /// object, not only those outside the piles (GameEngine::cleanup). A card
    /// whose hook writes to a pile object without saying so makes the checked
    /// build (-DRIFTBOUND_CHECK_BOARD_INDEX=ON) throw, naming the object.
    virtual bool continuousEffectReachesPiles() const { return false; }

    /// "Any amount of your damage is enough to kill enemy units." (CR
    /// 142.4.c, Elder Dragon). While a unit that says so is on its
    /// controller's board, any damage that player has marked on an enemy
    /// unit is lethal (GameEngine::anyDamageIsLethalFor).
    virtual bool anyDamageIsLethal() const { return false; }

    // ── Replacement effects ──
    /// Whether this card has a replacement effect (intercepts killUnit etc.).
    virtual bool hasReplacementEffect() const { return false; }

    /// Apply the replacement. Returns true if the replacement was applied
    /// (the original action should be skipped). Asked of the dying unit's
    /// controller's cards in play (ctx.source = that card), and of the
    /// battlefield card where the unit is dying (ctx.source = the
    /// battlefield's card object, ctx.controller = the dying unit's
    /// controller).
    virtual bool applyReplacement(CardContext& /*ctx*/,
                                  GameObjectId /*dying_unit*/) { return false; }

    /// Whether this card's replacement could apply to `dying_unit`'s death —
    /// its conditions, without asking a "may" or taking a payment (`self` is
    /// this card's object, `controller` the dying unit's controller). When
    /// more than one replacement applies, that controller chooses the order
    /// (CR 372; EffectExecutor::applyDeathReplacements), so the engine needs
    /// to know which do before it applies any. Every card with
    /// hasReplacementEffect() overrides replacementAppliesTo and answers Yes
    /// or No; the default, NotDefined, applies to nothing and is what a test
    /// over the registry looks for. Its applyReplacement starts by asking
    /// replacementApplies, so the two cannot drift.
    enum class Applies : uint8_t { NotDefined, No, Yes };
    virtual Applies replacementAppliesTo(const GameState& /*state*/, GameObjectId /*self*/,
                                         PlayerId /*controller*/,
                                         GameObjectId /*dying_unit*/) const {
        return Applies::NotDefined;
    }
    bool replacementApplies(const GameState& state, GameObjectId self, PlayerId controller,
                            GameObjectId dying_unit) const {
        return replacementAppliesTo(state, self, controller, dying_unit) == Applies::Yes;
    }

    // ── Equipment / Equip (CR 716-725, 818) ──
    /// Whether this gear has an equip ability.
    virtual bool hasEquipAbility() const { return false; }

    /// The cost of this gear's [Equip] ability (CR 818.1.c), as data the
    /// engine charges: `[Equip] [1][R]` is {energy 1, power 1, Fury}. `unit`
    /// is the unit being equipped (kInvalidId when only "could any Equip be
    /// paid" is asked), for text that alters the cost by the choice (CR
    /// 818.1.c.4 — Hextech Gauntlets). valid = false: no Equip cost, so it
    /// can't be paid (CR 821.1.c.4).
    struct EquipCost {
        bool valid = false;
        int energy = 0;
        int power = 0;
        Domain power_domain = Domain::Fury;
        bool power_any_domain = false;   // the Power is [A]
    };
    virtual EquipCost equipCost(const GameState& /*state*/, PlayerId /*controller*/,
                                GameObjectId /*gear*/, GameObjectId /*unit*/) const {
        return {};
    }
    /// The non-resource part of an Equip cost (CR 818.1.c.3: "Kill a friendly
    /// unit", "Recycle 2 cards from your trash"). canPay is asked before
    /// anything is paid; pay runs after the Energy / Power and returns false
    /// if it could not be paid after all (the gear is then not attached).
    virtual bool canPayEquipExtraCost(const GameState& /*state*/, PlayerId /*controller*/,
                                      GameObjectId /*gear*/, GameObjectId /*unit*/) const {
        return true;
    }
    virtual bool payEquipExtraCost(CardContext& /*ctx*/, GameObjectId /*unit*/) { return true; }
    /// Called once this gear has been attached to `unit` by paying its Equip
    /// cost (the Equip ability, or Weaponmaster).
    virtual void onEquipped(CardContext& /*ctx*/, GameObjectId /*unit*/) {}

    /// How an Equip cost is being paid.
    struct EquipTerms {
        int energy_discount = 0;        // "the first gear ability each turn costs [1] less"
        int power_discount = 0;         // Weaponmaster: "for [A] less" (CR 821.1.c)
        bool even_if_attached = false;  // Weaponmaster: "even if it's already attached"
    };
    /// This gear's Equip cost for `unit` after `terms`' discounts.
    EquipCost netEquipCost(const GameState& state, PlayerId controller, GameObjectId gear,
                           GameObjectId unit, const EquipTerms& terms) const;
    /// Whether `controller` could pay this gear's Equip cost to attach `gear`
    /// to `unit` right now. An Equipment that is attached cannot activate its
    /// own Equip (its Rules Text is Inactive — CR 718.2, 721.2) unless the
    /// terms say so. The single gate for the Equip generator, the Equip
    /// execution and Weaponmaster.
    bool canPayEquip(const GameState& state, PlayerId controller, GameObjectId gear,
                     GameObjectId unit, const EquipTerms& terms) const;
    bool canPayEquip(const GameState& state, PlayerId controller, GameObjectId gear,
                     GameObjectId unit) const;
    /// Pay this gear's Equip cost and attach ctx.source to `unit`. All or
    /// nothing; returns true if it was attached.
    bool payEquipAndAttach(CardContext& ctx, GameObjectId unit, const EquipTerms& terms);
    bool payEquipAndAttach(CardContext& ctx, GameObjectId unit);

    /// The two halves of an Equip activation (CR 818, 377.3): its cost is
    /// paid as it is activated (payEquip: the Energy / Power and the extra
    /// cost, all or nothing), the gear is attached as it resolves
    /// (attachByEquip: both still on the board; onEquipped after).
    bool payEquip(CardContext& ctx, GameObjectId unit, const EquipTerms& terms);
    bool attachByEquip(CardContext& ctx, GameObjectId unit);

    /// This gear's Equip on `unit` at its printed terms (no discount): pay
    /// equipCost() and attach. The engine's own Equip path is
    /// payEquipAndAttach with GameEngine::equipTerms; an Equip intent always
    /// names the unit.
    bool onEquip(CardContext& ctx, GameObjectId unit);

    /// A triggered ability of this card whose effect [Add]s resources
    /// (CR 430.2, 401.2): it is finalized and resolves as soon as it triggers
    /// — no chain item waiting, no priority, also in the middle of another
    /// effect (Undertitan "As I'm revealed ..., [Add] [2]"; Jhin "When I move,
    /// [Add] ..."; a battlefield's "each [Add] [1]").
    virtual bool triggeredAbilityAdds(TriggerType /*trigger*/) const { return false; }

    /// Trigger type that fires on the equipped UNIT (from effect_text).
    /// e.g., "When I conquer" in effect_text → WhenIConquer on the unit.
    virtual TriggerType equippedTriggerType() const { return TriggerType::None; }

    /// A reflexive trigger of this card (CR 387-389): text after "Do this:",
    /// "Do the following ...", "Do one of the following ...". The ability the
    /// phrase sits in puts it on the chain, when the part before the phrase
    /// has been done, with EffectExecutor::queueReflexiveTrigger(source,
    /// controller, index, data); `index` says which reflexive trigger of the
    /// card it is and `data` is what the first part hands over. It is then a
    /// chain item of its own: its "may", costs and targets are chosen as it
    /// is finalized (CR 388.4) — the engine always runs the finalize pass for
    /// it, so this callback follows the choices-at-finalization contract
    /// (choice helpers, then `if (finalizingChoices(ctx)) return;`, then the
    /// effect) — and players get priority before it resolves. ctx.source is
    /// the card's object; the triggering subject and move of the ability that
    /// created it are carried over.
    virtual void onReflexiveTrigger(CardContext& /*ctx*/, int /*index*/,
                                    const std::vector<int32_t>& /*data*/) {}
    /// True for a card that implements onReflexiveTrigger. A card whose text
    /// has one of the phrases ("do this", "do the following", "do one of the
    /// following") and whose phrase is NOT a reflexive trigger says why in
    /// notReflexiveBecause() instead. A test over the registry requires one
    /// of the two for every such card, so a new one cannot be missed.
    virtual bool hasReflexiveTrigger() const { return false; }
    virtual const char* notReflexiveBecause() const { return nullptr; }

    /// Called when the equipped unit's trigger fires. Gear handles its own
    /// logic. `targets` is always empty, as for onTrigger.
    virtual void onEquippedTrigger(CardContext& /*ctx*/,
                                   GameObjectId /*unit*/,
                                   const std::vector<GameObjectId>& /*targets*/) {}

    /// Keywords granted to the equipped unit (from effect_text).
    virtual KeywordSet equippedKeywords() const { return {}; }
    /// Anything else the effect text gives the equipped unit ("I am a Mech."):
    /// applied by every aura pass with the other equipment effects, BEFORE the
    /// card hooks (`applyPassiveAura`), so an effect that reads it ("Your
    /// Mechs have ...") sees it in the same pass whatever the hook order.
    virtual void applyToBearer(GameState& /*state*/, GameObjectId /*gear*/,
                               GameObjectId /*unit*/) const {}

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
        int discard_cards = 0;          // discard N from hand as the additional cost (Brazen Buccaneer)
        int reduce_energy = 0;          // if paid, reduce this card's energy cost by N (applied before payment)
    };
    virtual OptionalAdditionalCost optionalAdditionalCost() const { return {}; }
    /// What the engine asks as the card is played: also given the state, for a
    /// cost that may only be paid under a condition ("If you've played a spell
    /// this turn, you may pay [P] as an additional cost to play me"). Return
    /// {} when the condition does not hold. Default: the stateless version.
    virtual OptionalAdditionalCost optionalAdditionalCost(const GameState& /*state*/,
                                                          PlayerId /*player*/) const {
        return optionalAdditionalCost();
    }

    // ── Additional costs chosen before the cost is paid (CR 356) ──
    // "As you play me, you may discard 1 as an additional cost. If you do,
    // reduce my cost by [2]." / "As an additional cost to play me, kill a
    // friendly unit." / "you may spend a buff ... If you do, ignore this
    // spell's cost." These are chosen and paid as the card is played, BEFORE
    // its Energy / Power cost is determined, so what they take off the cost
    // counts toward whether the card can be played at all.
    /// What paying such a cost takes off the card's cost.
    struct PlayCostAdjust {
        int energy_reduction = 0;
        int power_reduction = 0;
        bool ignore_cost = false;      // "ignore this spell's cost"
    };
    /// The card's additional cost as the play gate sees it.
    struct AdditionalPlayCost {
        bool exists = false;       // the card prints one that applies right now
        bool mandatory = false;    // "As an additional cost to play me, ..." (not "you may")
        bool payable = false;      // it can be paid right now
        PlayCostAdjust best;       // the most paying it can take off the cost
    };
    /// Asked by the play gate (GameEngine::canAfford). `self` is the card
    /// being played, still in the zone it is played from. A mandatory cost
    /// that is not payable makes the card unplayable; an optional one is
    /// counted toward affordability at its `best`.
    /// Default: derived from optionalAdditionalCost() when that is a discard
    /// with reduce_energy (Brazen Buccaneer).
    virtual AdditionalPlayCost additionalPlayCost(const GameState& state, const CardDB& db,
                                                  PlayerId player, GameObjectId self) const;
    /// Called by the engine as the card is played, after it left its zone and
    /// before its cost is paid: make the choices (ctx.executor.confirmNow /
    /// chooseObjectNow / discardCards), pay the additional cost, and return
    /// what it takes off. `affordable(adj)` says whether the card's cost could
    /// be paid with `adj` applied — an optional cost may be declined only
    /// while `affordable` holds without it (the play was offered on the
    /// strength of it). Only called when additionalPlayCost(...).exists and
    /// .payable.
    virtual PlayCostAdjust payAdditionalPlayCost(
        CardContext& ctx, const std::function<bool(const PlayCostAdjust&)>& affordable);

    /// A non-resource additional cost of a spell ("As an additional cost to
    /// play this, kill a friendly unit": CR 356.2.a.1, 356.7), paid after its
    /// Energy and Power (CR 357.2: "In addition, pay any non-standard Cost ...
    /// in any order"). The engine calls it on every play path once the
    /// resources are paid, so a replacement of that cost which asks for a
    /// payment of its own (The Boss, Altar of Blood) is paid from what is left
    /// and cannot leave the spell unpaid (testing phase F6). A death replaced
    /// is still a cost paid (CR 357.2.a). The card records what it paid on its
    /// own object (`card_counters`, a per-play flag: forgetPlayPayments).
    virtual void payNonResourceCost(CardContext& /*ctx*/) {}

    /// The counter a card sets on itself (to 1) when its additional cost was
    /// paid for THIS play, for its play trigger to read ("When you play me,
    /// if you paid the additional cost, ..."). One key for every card, so
    /// that one place can forget it: forgetPlayPayments.
    static constexpr const char* kAdditionalCostPaid = "__additional_cost_paid";
    /// [Accelerate] for this play: 1 decided and not paid, 2 paid with the cost
    /// (the unit enters ready). Written by GameEngine::decideAccelerate, read
    /// as the unit enters. A per-play flag: forgetPlayPayments drops it.
    static constexpr const char* kAccelerateDecided = "__accelerate";
    /// 1 if this play is from a hand, 2 if from anywhere else (what a grant
    /// of [Accelerate] reads: Card::grantsAccelerateTo). Per play.
    static constexpr const char* kAccelerateFromHand = "__accelerate_from_hand";
    /// On a spell: 1 if this play of it is not from a hand (the trash, face
    /// down, by an effect from a deck or Banishment), 0 if it is. Written by
    /// every spell play path as the spell is played; read when it resolves,
    /// which is when "When you play a card from anywhere other than your hand"
    /// triggers (CR 420.4.a; TriggerManager::onSpellResolved). A countered
    /// spell triggers nothing (420.4.a.1).
    static constexpr const char* kPlayedNotFromHand = "__played_not_from_hand";
    /// Forget what was paid for an earlier play of `object`: kAdditionalCostPaid
    /// and the paid_flag of its optionalAdditionalCost(). The engine calls
    /// this as every play of a card begins — whatever the play path and
    /// whether or not the cost can be paid this time — so a flag left behind
    /// by a play whose trigger never resolved (countered) cannot make a
    /// later, unpaid play look paid.
    static void forgetPlayPayments(GameState& state, const Card* card, PlayerId player,
                                   GameObjectId object);

    /// "Optional additional costs you pay cost [1] or [A] less." (Ezreal,
    /// Prodigy): how many such discounts `self`, on its controller's board,
    /// gives. Applies to Accelerate, Repeat and "you may pay" costs.
    virtual int optionalCostDiscount(const GameState& /*state*/, GameObjectId /*self*/) const {
        return 0;
    }

    // ── Repeat (CR 820) ──
    /// One printed Repeat cost. A card with several ("[Repeat] — [1] / [A] /
    /// [1][A]") returns each; each may be paid once (CR 820.1.c.3) and each
    /// payment repeats the spell's effect once more.
    struct RepeatCost {
        int energy = 0;
        int power = 0;
        Domain power_domain = Domain::Count;   // Domain::Count = [A]
        int discard_cards = 0;                 // "[Repeat] — Discard 1"
    };
    virtual std::vector<RepeatCost> repeatCosts() const { return {}; }

    // ── Play from trash ──
    /// "You may play me from your trash for <cost>." (Undying Legion). Asked
    /// of a card while it is in its owner's trash; valid = true keeps a
    /// standing play-from-trash permission at that cost (like [Flow], without
    /// the banish).
    struct TrashPlayCost {
        bool valid = false;
        int energy = 0;
        int power = 0;
        Domain power_domain = Domain::Fury;
        bool any_domain = false;
    };
    virtual TrashPlayCost trashPlayCost(const GameState& /*state*/, PlayerId /*owner*/,
                                        GameObjectId /*self*/) const { return {}; }

    // ── Hide cost (CR 811.1.b) ──
    /// What hiding a card costs `controller`. The engine starts from [A]
    /// (waived by PlayerState::hide_ignoring_costs_this_turn) and asks every
    /// card the player has in play — board and Legend Zone — to adjust it.
    struct HideCost {
        bool waived = false;         // nothing to pay
        int power = 1;               // [A]
        bool energy_instead = false; // may pay `energy` Energy instead of the [A]
        int energy = 0;
    };
    virtual void adjustHideCost(const GameState& /*state*/, PlayerId /*controller*/,
                                HideCost& /*cost*/) const {}

    /// Power-side counterpart of activationCostReductionFor ("[Empower] costs
    /// ... cost [1] or [A] less"): `unused_energy_reduction` is how much of the
    /// Energy reduction found no Energy to take off. Default: for the Empower
    /// ability, that much Power, up to the source's empower_cost_reduction.
    virtual int activationPowerReductionFor(const GameState& state, PlayerId controller,
                                            GameObjectId source, int ability_index,
                                            int unused_energy_reduction) const;

    // ── Alternative play cost ──
    /// "You may play me for <alt cost> [instead of my printed cost]" when a
    /// condition holds (Jhin, Meticulous Killer: "play me for [B]" if you've
    /// spent [4]+ on a spell this turn). When .valid, the action generator emits
    /// an extra play option (Intent::use_alt_play_cost) and the payment path
    /// charges this cost in place of the printed one.
    struct AltPlayCost {
        bool valid = false;
        int energy = 0;
        int power = 0;
        Domain power_domain = Domain::Fury;
        bool any_domain = false;        // power may be any domain ([A])
    };
    virtual AltPlayCost alternativePlayCost(const GameState& /*state*/,
                                            PlayerId /*player*/) const { return {}; }

    /// LeBlanc, Everywhere at Once: "your [Temporary] effects at my battlefield
    /// don't trigger." When true, the [Temporary] ability (CR 816: "kill this
    /// at the start of its controller's Beginning Phase") of a friendly unit
    /// sharing this card's battlefield + controller does not trigger. Nothing
    /// else of that unit is affected (TriggerManager::temporaryTriggerSuppressed).
    virtual bool suppressesTemporaryTriggersHere() const { return false; }

    /// "Your hold effects for holding here trigger an additional time." (Blue
    /// Sentinel; Red Brambleback for conquers.) Asked of every card `scorer`
    /// has on the board when they score battlefield `bf` (`hold`: by holding,
    /// else by conquering); `self` is the instance asked. Each +1 makes
    /// TriggerManager::onScore queue every hold / conquer ability of that
    /// player for this score once more.
    virtual int extraScoreTriggers(const GameState& /*state*/, GameObjectId /*self*/,
                                   PlayerId /*scorer*/, BattlefieldId /*bf*/,
                                   bool /*hold*/) const { return 0; }

    /// "When ONE OR MORE enemy units die, ..." (Battle Mistress): true makes
    /// `trigger` fire once for a set of events that happen together instead of
    /// once per event — while an ability of this source for that trigger is
    /// still waiting to be finalized, a further event does not trigger it.
    virtual bool triggersOncePerBatch(TriggerType /*trigger*/) const { return false; }

    /// "The first time I <event> each turn": true makes `trigger` trigger only
    /// at the first event of the turn that meets its condition (after
    /// triggerCondition), whether or not that ability then resolved
    /// (TriggerManager::fireTrigger stamps the object; CR 383.1). For an event
    /// of the object itself. A "first time" that counts a player's events
    /// ("a friendly unit dies") reads the turn's count in triggerCondition
    /// instead, since the card need not have been there for the first one.
    virtual bool firstTimeEachTurn(TriggerType /*trigger*/) const { return false; }
    /// The card_counters key of that stamp (turn + 1).
    static std::string firstTimeKey(TriggerType trigger) {
        return "__first_time_" + std::to_string(static_cast<int>(trigger));
    }

    /// An Equipment whose effect text is a copy of its bearer's text ("As this
    /// is attached to a unit, copy that unit's text to this Equipment's effect
    /// text" — Svellsongur, CR 479.1.d). Effect text speaks for the bearer, so
    /// the bearer has each of its abilities once more per such Equipment:
    /// EffectExecutor::textCopiesOn counts them, TriggerManager::fireTrigger
    /// queues the bearer's own triggered abilities that many more times, and
    /// GameEngine::recalculateAuras applies its continuous effects that many
    /// more times.
    virtual bool copiesBearerText() const { return false; }

    /// "When my Might becomes N or more, ..." (Renekton, Brute): N. The engine
    /// fires TriggerType::WhenMyMightReachesThreshold on the unit each time
    /// its Might goes from below N to N or more. 0 = no such ability.
    virtual int mightThreshold() const { return 0; }

    // ── Condition gates ──
    // [Legion] / [Level N] are conditions a card checks itself, where its
    // text applies them (onTrigger / onActivate / applyPassiveAura /
    // selfCostReduction): the engine has no gate to ask.

    // ── Self-cost reduction (e.g., Noxus Hopeful "Legion: I cost 2 less") ──
    /// Returns the amount by which this card's own energy cost should be
    /// reduced, given the current game state. Engine consults this in
    /// canAfford() and payCardCost() when the card is being played.
    /// Default 0 = no reduction.
    virtual int selfCostReduction(const GameState& /*state*/,
                                  PlayerId /*player*/) const { return 0; }

    /// Power-side counterpart ("I cost [2][Y] less if ..." — Keeper of Law).
    /// Reduces this card's own POWER cost, clamped at 0.
    virtual int selfPowerCostReduction(const GameState& /*state*/,
                                       PlayerId /*player*/) const { return 0; }

    /// Called when this gear stops being attached to `unit` for any reason
    /// (re-equipped elsewhere, the unit or the gear left the board). Lets
    /// "for as long as this is attached" effects end (Shady Spectacles).
    virtual void onUnattached(CardContext& /*ctx*/, GameObjectId /*unit*/) {}

    /// Kinds of hostile effect a card may replace on itself (Gangplank, Naval:
    /// "If a spell or ability that chooses me would stun me, give me -[M], or
    /// return me to hand, give me +3 [M] this turn instead").
    enum class HostileEffect : uint8_t { Stun, ReduceMight, ReturnToHand };
    /// Return true if this card replaced the effect (the caller then skips it).
    /// Consulted only for effects of a spell or ability that chose `self`.
    virtual bool replaceHostileEffect(CardContext& /*ctx*/, GameObjectId /*self*/,
                                      HostileEffect /*kind*/) { return false; }

    /// "Ignore [Deflect] while paying this spell's cost." (Decree of Insight)
    virtual bool ignoresDeflect() const { return false; }

    // ── Reaction-to-attack play (e.g., Rengar, Pouncing) ──
    /// Whether this card can be played as a [Reaction] to a battlefield
    /// where the controller is currently attacking. Action generator
    /// emits play-to-attacking-BF intents during showdown/closed state.
    virtual bool playableAsReactionToAttack() const { return false; }

    /// Whether this PERMANENT card itself has [Reaction] (CR 813.1.c.1: "This
    /// can be played during Closed States on any player's turn") — Shen,
    /// Kinkou; Rengar, Pouncing; Janna, Savior; Sumpworks Map. Such a card
    /// also carries Keyword::Reaction on its def. A permanent that only has
    /// a [Reaction] ABILITY (the Seals, Gold) does not — that is
    /// ActivatedAbility::is_reaction — and neither does an [Ambush] unit
    /// (CR 822.1.b); LedgerTest.Defs_APermanentHasReactionOnlyWhenTheCardItselfDoes
    /// holds the two in step. When true the engine offers the card at
    /// Reaction and Action timing, to wherever it may normally be played
    /// (CR 813.3.a). Spells use Keyword::Reaction; [Quick-Draw] gear needs no
    /// override (CR 819.1.b).
    /// Why a virtual beside the def keyword: the keyword on an OBJECT can be
    /// given or taken by effects and is also what marks a spell, a hidden
    /// card and [Quick-Draw] gear; the play generators ask this for "the
    /// card's own printed timing as a permanent", and the test above keeps
    /// the def from saying anything else.
    virtual bool hasReactionTiming() const { return false; }

    /// For a spell whose several targets are locked in as it is played
    /// (getTargetRequirements().count >= 2, no needsPlayTimeTargetPair): every
    /// legal choice of targets, each in the order onResolve reads them.
    /// Default (nullopt): the engine builds them from enumerateLegalTargets —
    /// for "up to N" (optional) every non-empty set of at most N distinct
    /// targets, otherwise every ordered choice of exactly N distinct targets.
    /// Override when the slots have different requirements ("a friendly unit,
    /// then an enemy unit"; "units at the same location").
    virtual std::optional<std::vector<std::vector<GameObjectId>>>
    enumerateLegalTargetSets(const GameState& /*state*/, PlayerId /*controller*/) const {
        return std::nullopt;
    }

    // ── Targeting protection ──
    /// Whether this card may be the target of enemy spells and abilities.
    /// Default true; cards like Baron Nashor override to false. The engine
    /// populates GameObject::untargetable_by_enemy from this during
    /// cleanup/aura recalc, and Card::enumerateLegalTargets reads that
    /// flag to filter out untargetable enemy candidates.
    virtual bool canBeChosenByEnemy() const { return true; }

    /// "I can't be readied." (Maduli the Gatekeeper). False keeps the
    /// permanent exhausted through the Awaken Phase and through every ready
    /// effect (EffectExecutor::readyObject). Entering the board ready is not
    /// being readied.
    virtual bool canBeReadied() const { return true; }

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

    /// A continuous effect that gives a unit being played [Accelerate] ("Friendly
    /// units played from anywhere other than a player's hand have Accelerate").
    /// Asked of each card `self`'s controller has on the board as the unit's
    /// cost is paid; the unit's controller then gets the usual optional [1][C].
    virtual bool grantsAccelerateTo(const GameState& /*state*/, GameObjectId /*self*/,
                                    const GameObject& /*played*/,
                                    bool /*played_from_hand*/) const {
        return false;
    }

    /// Change to the points needed to win while this card is a battlefield in
    /// play ("Increase the points needed to win the game by 1").
    virtual int victoryScoreModifier() const { return 0; }

    /// What the engine calls: also given the unit being played, already at its
    /// destination ("If you play me to a battlefield, I enter ready").
    virtual bool entersReadyOnPlay(const GameState& state, PlayerId controller,
                                    const GameObject& /*self*/) const {
        return entersReadyOnPlay(state, controller);
    }


private:
    // Facts about the card class that need no game state, asked very often.
    // Built by primeStaticFacts(): at registration for every card of a
    // registry (so before any game thread reads them), lazily for a card
    // object used on its own. A copy of a card shares them.
    struct StaticFacts {
        std::vector<ActivatedAbility> abilities;
        std::bitset<256> triggers;   // TriggerType is a uint8_t
    };
    mutable std::shared_ptr<const StaticFacts> static_facts_;
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

    /// "Units can't move from here to base." Read by GameEngine (at setup and
    /// whenever the card in a battlefield slot changes) into
    /// BattlefieldState::blocks_move_to_base.
    virtual bool blocksMoveToBase() const { return false; }

    /// "Units can't be played here." Read the same way into
    /// BattlefieldState::blocks_unit_play.
    virtual bool blocksUnitPlay() const { return false; }
};

class RuneCard : public Card {};

} // namespace riftbound
