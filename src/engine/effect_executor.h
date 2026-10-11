#pragma once
/// @file effect_executor.h
/// Atomic game operation helpers used by Card objects.
///
/// Card objects call these methods to modify game state (deal damage,
/// draw cards, bounce units, etc.). This is a utility library, not a
/// dispatch mechanism — Card objects are the dispatch layer.

#include "core/card_db.h"
#include "core/events.h"
#include "core/game_state.h"
#include "core/intent.h"

#include <functional>
#include <optional>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace riftbound {

class CardRegistry;
class Card;
class EffectExecutor;

/// Units one effect is about to kill together ("kill all units here", "deal 3
/// to each ..., then kill what has lethal damage"). Declare it right before
/// the loop that kills them: each unit's last-known information is taken while
/// all of them are still on the board, so abilities that look back at the
/// death see the others ("if I didn't die alone") — as the engine does for
/// lethal damage found in one cleanup. Put it in a block of its own with the
/// loop, so it ends with the kills. Listing a unit that then survives is
/// harmless: if it is changed and dies later while the batch is still open,
/// its last-known information is taken afresh.
/// (EffectExecutor::beginDeathBatch / endDeathBatch.)
class DeathBatch {
public:
    DeathBatch(EffectExecutor& executor, std::vector<GameObjectId> units);
    ~DeathBatch();
    DeathBatch(const DeathBatch&) = delete;
    DeathBatch& operator=(const DeathBatch&) = delete;
private:
    EffectExecutor& executor_;
    std::vector<GameObjectId> units_;
};

/// Which parts of a card's cost an effect-driven play still charges
/// (EffectExecutor::playPayingCost).
struct EffectPlayCost {
    bool ignore_energy = false;   // "ignoring its Energy cost"
    bool ignore_power = false;
    int energy_reduction = 0;     // "reducing its Energy cost by [N]"
};

// State-only queries shared by the executor, the action generators and
// Card::enumerateLegalTargets (which has no executor to ask).
/// [A] an opponent must pay to choose `target` (CR 808 Deflect); 0 if
/// `chooser` controls it or Deflect is ignored at its battlefield.
int deflectCostFor(const GameState& state, PlayerId chooser, GameObjectId target);
/// Power of any Domain `player` could pay right now.
int payableAnyPower(const GameState& state, PlayerId player);

class EffectExecutor {
public:
    EffectExecutor(GameState& state, EventBus& events, const CardDB& card_db,
                   const CardRegistry* card_registry = nullptr);

    /// Register the CardRegistry after construction. Lets effects that
    /// re-play a card (Dazzling Aurora, Mirror Image, etc.) reach the
    /// played card's onPlay hook — without the registry, playIgnoringCost
    /// silently skips "As you play me" effects (Baron Nashor's Pit
    /// spawn, etc.). Tests that don't exercise the play path may leave
    /// this null.
    void setCardRegistry(const CardRegistry* registry) { card_registry_ = registry; }

    /// Access the CardDB for card-side code that needs to look up static
    /// card data (e.g. Virtuoso needs to read a chain item's energy_cost
    /// from the def). Returned reference is valid for the executor's life.
    const CardDB& cardDB() const { return card_db_; }
    /// The Card class behind `def`, or null (no registry attached, a token).
    Card* cardFor(CardDefId def) const;

    /// Set the RNG for effects that need randomness (reveal, recycle).
    void setRng(std::mt19937_64* rng) { rng_ = rng; }

    /// Set agent query callback for effects that require player choice (discard).
    using AgentChoiceQuery = std::function<Intent(PlayerId, const std::vector<Intent>&)>;
    void setAgentQuery(AgentChoiceQuery query) { agent_query_ = std::move(query); }

    /// Ask `player` to choose one of `objects` right now, for an effect that
    /// is not resolving from the chain and so cannot suspend (an Equip
    /// attaching). Returns the first object when there is no agent to ask, and
    /// kInvalidId when `objects` is empty.
    GameObjectId chooseObjectNow(PlayerId player, const std::vector<GameObjectId>& objects);

    /// Ask `player` a yes/no question right now, for a "may" that is not
    /// resolving from the chain and so cannot suspend (a replacement effect:
    /// "its controller may pay ... instead"). Returns false when there is no
    /// agent to ask.
    bool confirmNow(PlayerId player, const std::string& label);

    /// "You may make new choices for it" (CR 750-755): the item's controller
    /// re-chooses its targets. Costs already paid are unaffected (755.1).
    using RechooseFn = std::function<void(ChainItemId)>;
    void setRechoose(RechooseFn fn) { rechoose_ = std::move(fn); }
    void makeNewChoices(ChainItemId item) { if (rechoose_) rechoose_(item); }

    // ── Atomic game operations ──
    // dealDamage, killObject, bounceToHand, buffUnit, stunUnit*, moveToBase and
    // moveToBattlefield act only on an object that is on the board (it has a
    // location); on anything else they do nothing. readyObject also accepts a
    // legend or a battlefield. `objectExists` alone is not that check.
    void dealDamage(GameObjectId target, int amount, GameObjectId source);
    void killObject(GameObjectId target);
    /// Fill GameObject::last_known from the object's current state (called by
    /// the kill paths just before it dies).
    void snapshotLastKnown(GameObjectId id);
    void drawCards(PlayerId player, int count);
    void bounceToHand(GameObjectId target);
    void giveTemporaryMight(GameObjectId target, int amount, int minimum = 0);
    void giveTemporaryKeyword(GameObjectId target, Keyword kw, int value);
    /// Buff (CR 427): place a Buff counter on `target` if it has none. A unit
    /// that already has one is not buffed (CR 427.1.b.1 / 702.3) unless its
    /// Card::canHaveAnyNumberOfBuffs(). Returns true if a Buff was placed —
    /// "if it was buffed this way" (CR 427.1.c).
    bool buffUnit(GameObjectId target);
    void readyObject(GameObjectId target);
    /// MOVE a unit to its controller's base (CR 421): counts toward "moved N
    /// times this turn", emits UnitMovedEvent ("When I move", "When a unit
    /// moves from here") and is refused when the move is blocked
    /// (moveToBaseBlocked, "can't be moved by enemy spells and abilities").
    /// For "recall it" use recallToBase.
    void moveToBase(GameObjectId target);
    /// RECALL a permanent to its controller's base (CR 457-460): "Send it to
    /// base. This isn't a move." It changes location with everything attached
    /// to it (CR 719.3.a), keeps its damage and statuses (CR 460), triggers no
    /// move abilities (CR 458.1), does not count as a move and cannot be
    /// stopped by anything that restricts movement (CR 458.3). Emits
    /// ObjectStateChangedEvent "recalled". No-op off the board or already in
    /// its controller's base.
    void recallToBase(GameObjectId target);
    /// The whole of "heal it, exhaust it, and recall it" for a unit whose
    /// death is being replaced.
    void healExhaustRecall(GameObjectId unit);
    /// Every replacement of `unit`'s death, in one place for both kill paths
    /// (killObject and GameEngine::killUnit). The ones that apply are
    /// collected first: those the unit carries (the one-shot "next time it
    /// would die this turn, heal it, exhaust it, and recall it instead",
    /// GameObject::pending_death_replacements, "banish it instead"), the card
    /// of the battlefield it is at, and its controller's cards in play
    /// (Card::replacementApplies), in that order and by object id within the
    /// last. When more than one applies, the unit's controller chooses which
    /// is applied (CR 372); one that then does not replace the death (a "may"
    /// declined) drops out and the choice is offered again among the rest.
    /// With nobody to ask, the listed order is used. True = it did not die.
    bool applyDeathReplacements(GameObjectId unit);
    /// Attach `gear` to `unit` (the cost, if any, is already paid): both must
    /// be on the board (CR 716); a gear attached elsewhere comes off first
    /// (its onUnattached runs); its printed rules text goes Inactive (CR
    /// 718.2). Emits "attached" / "equipped". Every attach goes through here.
    /// Returns false, and does nothing, when it is not possible.
    bool attachGear(GameObjectId gear, GameObjectId unit);
    /// Whose damage is marked on `obj` and whether a spell dealt any of it:
    /// forgotten when its damage is healed, and by both damage paths
    /// (dealDamage, GameEngine::applyCombatDamage) when they add damage to an
    /// object that has none — which covers every way damage is removed,
    /// cards that zero `damage_marked` themselves included.
    static void forgetDamageSources(GameObject& obj);
    /// How many Equipment attached to `unit` copy its text to their effect
    /// text (Card::copiesBearerText): the unit has each of its abilities that
    /// many more times. For a unit that has left the board, as it last existed.
    int textCopiesOn(GameObjectId unit) const;
    /// Spend a buff (CR 427.2): remove one Buff counter from `unit` as a cost
    /// or effect of `spender`. Returns false (nothing spent) if it has none.
    /// Emits ObjectStateChangedEvent "buff_spent" -> WhenYouSpendABuff.
    bool spendBuff(GameObjectId unit, PlayerId spender);

    // ── Units that die at the same time ──
    /// Call with every unit about to die together (lethal damage found in one
    /// cleanup, "kill all units here"), before the first kill: takes each
    /// one's last-known information while all of them are still on the board.
    /// Follow the kills with endDeathBatch(same ids).
    void beginDeathBatch(const std::vector<GameObjectId>& units);
    void endDeathBatch(const std::vector<GameObjectId>& units);

    /// Announce that `card` was recycled into its owner's Main Deck by a path
    /// that placed it itself (a recycle-self cost, the mulligan): fires "When
    /// you recycle one or more cards". recycleCards announces its own.
    void announceRecycledToMainDeck(GameObjectId card);
    /// "Draw" a card an effect is holding — already taken out of the deck
    /// with deck::take / takeTop(..., Place::Held) — ("look at the top two,
    /// draw one"): it goes to `player`'s hand and counts as a draw
    /// (draws_this_turn, CardsDrawnEvent -> "When you draw"). `seen_by_all`:
    /// it was revealed, so the other player knows which card it is.
    void drawHeldCard(PlayerId player, GameObjectId card, bool seen_by_all);
    /// Unattach a single gear from the unit it's equipped to. Removes it from
    /// the unit's attachment list, refunds the unit's might bonus, and clears
    /// the gear's attachment state. Per CR 719.5 the detached gear stays at
    /// its current location (on board, unattached). No-op if not attached.
    void unattachGear(GameObjectId gear);
    /// Move a unit to the given battlefield. Updates `location` + `zone`,
    /// counts the move (`moves_this_turn`), brings attached gear along (CR
    /// 719.3) and emits UnitMovedEvent. No-op if it is already there. Caller is responsible for any contested-status
    /// re-evaluation downstream (cleanup recomputes BF contested flags).
    /// `from_play` is false: this is a card-effect-driven move, not a play.
    void moveToBattlefield(GameObjectId target, BattlefieldId bf);
    void stunUnit(GameObjectId target);
    /// Stun + attribute to the controller of `stunner_source`. Use this from
    /// card effects so the resulting UnitStunnedEvent identifies the stunner
    /// for WhenYouStun triggers (e.g. Vex Mocking).
    void stunUnitBy(GameObjectId target, GameObjectId stunner_source);
    void discardCards(PlayerId player, int count);
    /// Make a specific player discard (for opponent targeting: "they discard 1").
    void opponentDiscards(PlayerId opponent, int count);
    /// Commit a single-card discard chosen by the agent earlier (e.g. via
    /// requestChoice/takeChoice during resumable resolution). Moves the card
    /// from hand → trash, sets `has_discarded_this_turn`, emits
    /// CardsDiscardedEvent. No-op if `card_id` isn't in player's hand.
    void applyDiscard(PlayerId player, GameObjectId card_id);
    void recycleCards(PlayerId player, const std::vector<GameObjectId>& cards);
    void banishObject(GameObjectId target);

    // ── Vendetta game actions ──
    /// Empower (CR 442): give `target` an Empowered status if it has room
    /// (binary unless its Card::maxEmpower() allows stacking). Returns true if
    /// a status was added; emits ObjectStateChangedEvent "empowered".
    bool empower(GameObjectId target);
    /// Disempower (CR 443): remove one Empowered status. Returns false (and
    /// does nothing) if the object wasn't Empowered.
    bool disempower(GameObjectId target);
    /// Burn X (CR 441): move the top `count` cards of `player`'s Main Deck to
    /// their trash, burning out if the deck runs dry. Returns the burned
    /// cards (in burn order); emits ObjectStateChangedEvent "burned" per card.
    std::vector<GameObjectId> burn(PlayerId player, int count);
    /// One Burn Out (CR 432.2): recycle `player`'s trash into their Main Deck
    /// (possibly nothing) and give the opponent 1 point. Returns true if the
    /// deck now has cards and the game is still going.
    bool burnOut(PlayerId player);
    /// Burn out repeatedly until the deck has a card or the game ends (CR 432.3).
    bool burnOutUntilDeckHasCards(PlayerId player);
    void healObject(GameObjectId target);
    void exhaustObject(GameObjectId target);
    void channelRunes(PlayerId player, int count, bool enter_exhausted = false);

    // Phase 6q+ — Add abilities (CR 429). Add floating energy / power to
    // a player's rune pool. Floating resources are spent before runes,
    // and the entire pool clears at end of turn (CR 317.2.d already
    // handled by expirationStep::emptyRunePools).
    //
    // Examples that use this:
    //   - Jhin, Murderous Artist [584]: "When I move, [Add] [1][A]."
    //   - Hextech Anomaly [405]: "Pay any amount of [A] to Add that
    //     much Energy."
    //   - Various "Add" rune-pool effects.
    //
    // Per CR 429.2, Add abilities resolve immediately (not via chain).
    // These helpers are called directly from Card::onTrigger / onActivate
    // bodies; no chain bookkeeping required.
    void addFloatingEnergy(PlayerId player, int amount);
    /// Energy that can only pay for units and activated abilities of units.
    void addUnitOnlyEnergy(PlayerId player, int amount);
    void addFloatingPower(PlayerId player, Domain domain, int amount);
    /// Universal power ([A]). Counted separately from domain-specific power
    /// and spendable for any cost.
    void addFloatingUniversalPower(PlayerId player, int amount);

    // ── Paying inside an effect ──
    /// Power of any Domain ([A]) `player` could pay now: floating power of
    /// every kind plus runes in base (CR 164.2.b — recycle, ready or exhausted).
    int payableAnyPower(PlayerId player) const;
    /// Pay `amount` [A]: floating power first, then recycle runes (exhausted
    /// ones first). Pays nothing and returns false if it can't be covered.
    bool payAnyPower(PlayerId player, int amount);

    // ── Scoring by an effect (CR 471 "score N points" outside Conquer/Hold) ──
    /// Give `player` `amount` points from an effect: honours "can't gain/score
    /// points" locks and ends the game at once if this reaches the Victory
    /// Score with more points than every opponent. Returns points gained.
    int scorePoints(PlayerId player, int amount);
    /// Victory Score in effect right now (mode score plus modifiers).
    int effectiveVictoryScore() const;

    // ── Deflect (CR: "Opponents must pay [A] to choose me") ──
    /// [A] an opponent must pay to choose `target` with a spell or ability.
    /// 0 when `chooser` controls it, it has no Deflect, or Deflect is ignored
    /// at its battlefield.
    int deflectCostFor(PlayerId chooser, GameObjectId target) const;

    /// Ask the engine to recompute continuous effects now (auras, Might) —
    /// for an effect that changes what feeds them and then reads the result
    /// in the same resolution (Lacerate: disempower, then check Might).
    void setAuraRecalc(std::function<void()> fn) { aura_recalc_ = std::move(fn); }
    void recalculateContinuousEffects() { if (aura_recalc_) aura_recalc_(); }

    // Phase 6q+ — take control of a unit (CR 416 + "take control of").
    // Mutates GameObject::controller. If `until_end_of_turn` is true,
    // saves the original controller and reverts in expirationStep.
    // If false, control change is permanent (until something else
    // changes it). Used by Akshan, Mischievous + Conscription.
    void takeControl(GameObjectId target, PlayerId new_controller,
                      bool until_end_of_turn = false);

    // Take control of `target` only until `source` leaves the board (Akshan,
    // Mischievous: "You control it until I leave the board"). Records the
    // controlling source + the controller to restore; GameEngine's cleanup
    // reverts control once `source` is no longer in play. Snapshots the
    // restore-controller once (earliest owner wins if stacked).
    void takeControlUntilSourceLeaves(GameObjectId target,
                                      PlayerId new_controller,
                                      GameObjectId source);

    // ── Token creation ──
    /// Create a token game object and place it on the board.
    /// A token is PLAYED (CR 350.2): TokenCreatedEvent drives "When you play
    /// a unit / a gear / a token unit" and the token's own keyword abilities.
    /// `location_choices`: when the instruction let the controller choose
    /// where the token goes, every place it could have gone — an additional
    /// copy (Card::... "play that token and an additional copy of it") then
    /// has the same choice; empty = the instruction named `location`, and a
    /// copy goes there too. `token_card`: the registered token card this token
    /// is (Gold 326, Shadow Clone 955) — set before the play is announced, so
    /// what triggers on it sees the token with its abilities, and carried by
    /// an additional copy. `enters_exhausted`: the instruction says "play a
    /// ... token exhausted" (a gear otherwise enters ready, CR 149.1) — it is
    /// exhausted when its play is announced, unless "your tokens enter ready"
    /// replaces how it enters (CR 184.1). Returns the token asked for; lastTokensCreated()
    /// also lists any additional copies.
    GameObjectId createToken(PlayerId controller, CardType type,
                             const std::string& name, int might,
                             const std::vector<std::string>& tags,
                             KeywordSet keywords, LocationId location,
                             bool enter_ready = false,
                             const std::vector<LocationId>& location_choices = {},
                             CardDefId token_card = kInvalidId,
                             bool enters_exhausted = false);
    /// Every token the most recent createToken call put on the board: the one
    /// asked for first, then additional copies.
    const std::vector<GameObjectId>& lastTokensCreated() const { return last_tokens_; }
    /// For a helper that asks where a token goes and then hands the answer to
    /// a card's own createToken call (playTokensChoosingLocations): the
    /// places the controller was choosing among, used as `location_choices`
    /// by createToken calls that pass none, until cleared.
    void setTokenLocationChoices(std::vector<LocationId> choices) {
        token_location_choices_ = std::move(choices);
    }
    void clearTokenLocationChoices() { token_location_choices_.clear(); }
    /// Ask `player` to choose one of `locations` right now (see
    /// chooseObjectNow). Returns the first when there is no agent to ask.
    LocationId chooseLocationNow(PlayerId player, const std::vector<LocationId>& locations,
                                 const std::string& label);

    /// Add a battlefield-token slot to the board. Creates a fresh
    /// GameObject for the BF card (card_type=Battlefield, super_type=Token,
    /// no card_def_id) and a new BattlefieldState appended to
    /// state.battlefields. Returns the new BattlefieldId. Used by Baron
    /// Nashor for its Baron Pit spawn. `accepts_any_inbound` carries the
    /// "Units can move here from anywhere" rule onto the BattlefieldState.
    BattlefieldId addBattlefieldToken(const std::string& name,
                                       bool accepts_any_inbound);

    /// Replace an existing BF in place with a named token BF (CR 438).
    /// The original BF card moves to its owner's banishment and the
    /// BattlefieldState's `card_object_id` swaps to a fresh token card
    /// with `super_type=Token`. The original is recorded in
    /// `replaced_card`/`was_replaced` so swap-back works on score.
    /// Used by Green Father to install the Brush BF token.
    /// `token_def`: the card whose abilities the token battlefield has (the
    /// Brush card for a Brush token), so they work like any battlefield's.
    void replaceBattlefieldWithToken(BattlefieldId target_bf,
                                      const std::string& token_name,
                                      PlayerId controller,
                                      CardDefId token_def = kInvalidId);

    // ── Copy effects ──
    /// Make a token become a copy of another unit (base traits only, not buffs/auras).
    void copyUnit(GameObjectId token, GameObjectId source);

    // ── Composite operations ──
    /// "If you would reveal cards from a deck, look at the top card first. You
    /// may recycle it." (Void Hatchling, PlayerState::has_reveal_peek.) Called
    /// by revealUntil / revealAndChoose; a card that reveals from `player`'s
    /// deck by hand calls it once, before it takes the cards. Does nothing
    /// without the effect.
    void lookBeforeReveal(PlayerId player);
    std::pair<GameObjectId, std::vector<GameObjectId>>
        revealUntil(PlayerId player, CardType condition);
    /// Play `card` from its current zone onto the board without paying
    /// its costs. The card is taken out of whichever hand, trash, banishment,
    /// Champion Zone or Main Deck lists it. `location` is where the unit lands: if std::nullopt
    /// (default), the controller's base — back-compat for callers that
    /// don't need to surface a location choice. Per CR 355.2.a, when
    /// effects play a unit for free the controller still chooses base
    /// or any battlefield they control; callers should prompt the
    /// agent and pass the result here (see Dazzling Aurora).
    void playIgnoringCost(PlayerId player, GameObjectId card,
                           std::optional<LocationId> location = std::nullopt);

    using EffectPlayCost = ::riftbound::EffectPlayCost;
    /// Whether `player` could pay for `card` under `cost` right now.
    bool canPlayPayingCost(PlayerId player, GameObjectId card,
                           EffectPlayCost cost = {}) const;
    /// Play `card` by an effect that does NOT ignore all of its costs
    /// (CR 356.1.b.2): the rest is paid through the normal cost path (cost
    /// modifiers, floating pool, rune choices). Payment is optional (CR 445.2):
    /// if it can't or won't be paid the card is not played and this returns
    /// false — the card is left where it was. On success it is taken out of
    /// the zone it was played from (see playIgnoringCost), so callers should
    /// not remove it themselves before they know the cost was paid.
    bool playPayingCost(PlayerId player, GameObjectId card,
                        std::optional<LocationId> location = std::nullopt,
                        EffectPlayCost cost = {});
    /// The engine's cost path, injected so effect-driven plays pay like any
    /// other play.
    using CanAffordFn = std::function<bool(PlayerId, GameObjectId)>;
    using PayCostFn = std::function<bool(PlayerId, GameObjectId)>;
    /// Puts a spell played by an effect onto the chain so it resolves like any
    /// other spell (injected by the engine: the executor has no chain access).
    using QueueSpellFn = std::function<void(PlayerId, GameObjectId)>;
    void setQueueSpell(QueueSpellFn fn) { queue_spell_ = std::move(fn); }

    /// Put `source`'s triggered ability for `trigger` on the chain as if the
    /// event had happened ("activate the conquer effects of units here").
    /// Goes through TriggerManager::fireTrigger, so the usual suppression and
    /// trigger conditions apply. No-op without an engine (bare executor).
    using QueueTriggerFn = std::function<void(GameObjectId, PlayerId, TriggerType, GameObjectId)>;
    void setQueueTrigger(QueueTriggerFn fn) { queue_trigger_ = std::move(fn); }
    bool canQueueTriggers() const { return static_cast<bool>(queue_trigger_); }
    /// Put a reflexive trigger of `source`'s card on the chain (CR 389.1: "a
    /// new ability is created and added to the chain as a Pending Item") —
    /// called by the ability that contains it, when the part before "do
    /// this" has been done. It resolves through Card::onReflexiveTrigger(ctx,
    /// index, data). Returns false when nothing can put it there (an executor
    /// driven on its own, outside a game).
    using QueueReflexiveFn =
        std::function<void(GameObjectId, PlayerId, int, std::vector<int32_t>)>;
    void setQueueReflexive(QueueReflexiveFn fn) { queue_reflexive_ = std::move(fn); }
    bool queueReflexiveTrigger(GameObjectId source, PlayerId controller, int index,
                               std::vector<int32_t> data = {}) {
        if (!queue_reflexive_) return false;
        queue_reflexive_(source, controller, index, std::move(data));
        return true;
    }
    void queueTriggeredAbility(GameObjectId source, PlayerId controller, TriggerType trigger,
                               GameObjectId subject = kInvalidId) {
        if (queue_trigger_) queue_trigger_(source, controller, trigger, subject);
    }
    void setPlayCostFns(CanAffordFn can, PayCostFn pay) {
        can_afford_ = std::move(can);
        pay_cost_ = std::move(pay);
    }

    /// Predict N: look at top N cards, agent chooses which to recycle (put back on bottom).
    void predict(PlayerId player, int count);

    /// Reveal top N and let agent choose: draw matching cards or recycle them.
    /// Returns list of cards the agent chose to draw/play.
    std::vector<GameObjectId> revealAndChoose(PlayerId player, int count);

    // ── Phase C-1 commit 6 — pending-choice mechanism (in progress) ──
    //
    // The traditional path (discardCards / predict / revealAndChoose above)
    // calls `agent_query_` synchronously and blocks the engine's worker
    // thread on a condvar inside StepDriver. That's what prevents Clone()
    // from being a memcpy (the worker thread can't be cloned).
    //
    // The new path: a card's onResolve calls requestChoice() to publish
    // pending legal-actions, then immediately returns. The chain manager
    // detects the pending choice, re-pushes the ChainItem with a saved
    // `resume_point`, and bubbles `StepKind::NeedDecision` up the stack.
    // When `applyChoice` is called: consume the pending choice, apply it,
    // re-call Card::onResolve which advances from its saved resume_point.
    //
    // This API is in-place but UNUSED today — cards still call the
    // blocking discardCards/predict/revealAndChoose. Refactoring them is
    // mechanical; CLAUDE.md "Phase C-1 commit 6 — resume pattern"
    // documents the per-card template.
    struct PendingChoice {
        bool active = false;
        PlayerId player = PlayerId::None;
        std::vector<Intent> legal;        // the choice set
        std::string label;                // free-text context for trace logs
                                          // (e.g. "discard 1 (Lunar Boon)").
                                          // Empty = no label printed; trace
                                          // falls back to a generic
                                          // "MakeChoice pick=[...]" line.
    };
    void requestChoice(PlayerId p, std::vector<Intent> legal,
                        std::string label = "") {
        pending_ = PendingChoice{true, p, std::move(legal), std::move(label)};
    }
    bool hasPendingChoice() const { return pending_.active; }

    /// While a spell's choices are made before its cost is paid (CR 355 ->
    /// 356; GameEngine::executePlaySpell), only objects the spell can still be
    /// paid for with may be chosen. Card::pickTarget and the other choice
    /// helpers drop the rest (Card::offerableChoices).
    void setChoiceFilter(std::function<bool(GameObjectId)> f) { choice_filter_ = std::move(f); }
    bool choiceAllowed(GameObjectId id) const { return !choice_filter_ || choice_filter_(id); }
    PendingChoice consumePendingChoice() {
        PendingChoice tmp = std::move(pending_);
        pending_ = PendingChoice{};
        return tmp;
    }

    // The chain manager stashes the agent's answer here between
    // `consumePendingChoice` and the next Card::onResolve re-entry. The
    // resuming Card reads it in its case 1 branch via `takeChoice`, then
    // applies card-specific semantics (move discarded card to trash,
    // recycle predicted card to bottom, etc.). One-shot only — `takeChoice`
    // clears the slot.
    void recordChoice(Intent i) { last_applied_choice_ = std::move(i); }
    std::optional<Intent> takeChoice() {
        auto out = std::move(last_applied_choice_);
        last_applied_choice_.reset();
        return out;
    }
    bool hasRecordedChoice() const { return last_applied_choice_.has_value(); }

private:
    GameState& state_;
    EventBus& events_;
    const CardDB& card_db_;
    std::function<void()> aura_recalc_;
    std::function<bool(GameObjectId)> choice_filter_;
    CanAffordFn can_afford_;
    QueueSpellFn queue_spell_;
    QueueTriggerFn queue_trigger_;
    QueueReflexiveFn queue_reflexive_;
    RechooseFn rechoose_;
    PayCostFn pay_cost_;
    const CardRegistry* card_registry_ = nullptr;  // optional; needed for playIgnoringCost onPlay dispatch
    std::mt19937_64* rng_ = nullptr;
    AgentChoiceQuery agent_query_;
    PendingChoice pending_;
    std::optional<Intent> last_applied_choice_;
    // Scratch result of the createToken call in progress / just finished; not
    // game state (nothing reads it across a decision).
    std::vector<GameObjectId> last_tokens_;
    std::vector<LocationId> token_location_choices_;
    int token_depth_ = 0;
};

} // namespace riftbound
