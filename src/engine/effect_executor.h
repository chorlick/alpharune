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

    /// Set the RNG for effects that need randomness (reveal, recycle).
    void setRng(std::mt19937_64* rng) { rng_ = rng; }

    /// Set agent query callback for effects that require player choice (discard).
    using AgentChoiceQuery = std::function<Intent(PlayerId, const std::vector<Intent>&)>;
    void setAgentQuery(AgentChoiceQuery query) { agent_query_ = std::move(query); }

    // ── Atomic game operations ──
    void dealDamage(GameObjectId target, int amount, GameObjectId source);
    void killObject(GameObjectId target);
    void drawCards(PlayerId player, int count);
    void bounceToHand(GameObjectId target);
    void giveTemporaryMight(GameObjectId target, int amount, int minimum = 0);
    void giveTemporaryKeyword(GameObjectId target, Keyword kw, int value);
    void buffUnit(GameObjectId target);
    void readyObject(GameObjectId target);
    void moveToBase(GameObjectId target);
    /// Unattach a single gear from the unit it's equipped to. Removes it from
    /// the unit's attachment list, refunds the unit's might bonus, and clears
    /// the gear's attachment state. Per CR 719.5 the detached gear stays at
    /// its current location (on board, unattached). No-op if not attached.
    void unattachGear(GameObjectId gear);
    /// Move a unit to the given battlefield. Updates `location` + `zone`,
    /// emits UnitMovedEvent. Caller is responsible for any contested-status
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
    void addFloatingPower(PlayerId player, Domain domain, int amount);
    /// Universal power ([A]). Counted separately from domain-specific power
    /// and spendable for any cost.
    void addFloatingUniversalPower(PlayerId player, int amount);

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
    GameObjectId createToken(PlayerId controller, CardType type,
                             const std::string& name, int might,
                             const std::vector<std::string>& tags,
                             KeywordSet keywords, LocationId location,
                             bool enter_ready = false);

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
    void replaceBattlefieldWithToken(BattlefieldId target_bf,
                                      const std::string& token_name,
                                      PlayerId controller);

    // ── Copy effects ──
    /// Make a token become a copy of another unit (base traits only, not buffs/auras).
    void copyUnit(GameObjectId token, GameObjectId source);

    // ── Composite operations ──
    std::pair<GameObjectId, std::vector<GameObjectId>>
        revealUntil(PlayerId player, CardType condition);
    /// Play `card` from its current zone onto the board without paying
    /// its costs. `location` is where the unit lands: if std::nullopt
    /// (default), the controller's base — back-compat for callers that
    /// don't need to surface a location choice. Per CR 355.2.a, when
    /// effects play a unit for free the controller still chooses base
    /// or any battlefield they control; callers should prompt the
    /// agent and pass the result here (see Dazzling Aurora).
    void playIgnoringCost(PlayerId player, GameObjectId card,
                           std::optional<LocationId> location = std::nullopt);

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
    const CardRegistry* card_registry_ = nullptr;  // optional; needed for playIgnoringCost onPlay dispatch
    std::mt19937_64* rng_ = nullptr;
    AgentChoiceQuery agent_query_;
    PendingChoice pending_;
    std::optional<Intent> last_applied_choice_;
};

} // namespace riftbound
