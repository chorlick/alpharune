#pragma once
/// @file trigger_manager.h
/// Triggered ability manager — subscribes to events and fires matching triggers.
///
/// When a game event occurs (unit played, unit dies, combat starts, etc.),
/// the TriggerManager scans all game objects for abilities whose trigger
/// type matches the event. Matching abilities are added to the chain
/// as Pending Items and resolved through FEPR.

#include "cards/card_registry.h"
#include "core/card_db.h"
#include "core/events.h"
#include "core/game_state.h"
#include "engine/chain_manager.h"

#include <boost/signals2/connection.hpp>
#include <vector>

namespace riftbound {

class EffectExecutor; // forward declaration

class TriggerManager {
public:
    TriggerManager(GameState& state, EventBus& events,
                   const CardDB& card_db, ChainManager& chain,
                   const CardRegistry& card_registry);

    /// Disconnects all event subscriptions installed by subscribe().
    /// Required for sessions that replace the live engine on the same
    /// EventBus (play-server restart loop) — otherwise dangling
    /// subscribers from the prior TriggerManager fire on the new
    /// engine's events and dereference freed memory.
    ~TriggerManager();

    /// Set the effect executor (needed for equipped trigger CardContext).
    void setEffectExecutor(EffectExecutor* exec) { effect_executor_ = exec; }

    /// Connect to all event bus signals. Call once after construction.
    void subscribe();

private:
    GameState& state_;
    EventBus& events_;
    const CardDB& card_db_;
    ChainManager& chain_;
    const CardRegistry& card_registry_;
    EffectExecutor* effect_executor_ = nullptr;
    std::vector<boost::signals2::connection> connections_;

    // ── Delayed abilities ──

    /// Check and fire any delayed abilities matching a trigger type.
    void checkDelayedAbilities(TriggerType trigger, PlayerId relevant_player,
                                GameObjectId event_object = kInvalidId);

    // ── Equipment triggers ──

    /// Check attached gear for equipped triggers matching an event on a unit.
    void fireEquippedTriggers(GameObjectId unit, PlayerId controller,
                               TriggerType trigger);

    // ── Event handlers ──

    /// Fires WhenYouPlayMe triggers for the card that was just played.
    /// Also fires WhenYouPlayAUnit/WhenYouPlayASpell for other objects.
    void onCardPlayed(const CardPlayedEvent& e);

    /// Fires WhenYouPlayThis for gear entering the board.
    void onEnteredBoard(const EnteredBoardEvent& e);

    /// Fires WhenIAttack, WhenIDefend, WhenIAttackOrDefend for combat units.
    void onCombatStarted(const CombatStartedEvent& e);

    /// Fires WhenIWinCombat for each surviving unit at the BF controlled
    /// by the combat winner. No-op when there's no decisive winner.
    void onCombatEnded(const CombatEndedEvent& e);

    /// Fires WhenIDie/Deathknell for the dying unit.
    /// Also fires WhenAFriendlyUnitDies for other objects the controller owns.
    void onUnitDied(const UnitDiedEvent& e);

    /// Fires WhenIConquer, WhenIHold, WhenIConquerOrHold for units at the BF.
    /// Also fires WhenYouConquerHere, WhenYouHoldHere for the battlefield card.
    void onScore(const ScoreEvent& e);

    /// Fires WhenIMove, WhenIMoveToFB for the moving unit.
    void onUnitMoved(const UnitMovedEvent& e);

    /// Fires AtEndOfTurn, AtStartOfBeginning, AtStartOfMain for all objects on board.
    void onPhaseChanged(const PhaseChangedEvent& e);

    /// Fires WhenYouStun for cards controlled by the stunner when an
    /// enemy unit is stunned. Stunned-unit id stored in
    /// card_counters["__stunned_unit_id"] for the trigger's onTrigger.
    void onUnitStunned(const UnitStunnedEvent& e);

    /// Phase-2 trigger events.
    void onUnitReadied(const UnitReadiedEvent& e);          // WhenIAmReadied
    void onCardHidden(const CardHiddenEvent& e);            // WhenYouHideACard
    void onPlayedFromFacedown(const PlayedFromFacedownEvent& e); // WhenYouPlayFromFacedown
    void onUnitReturnedToHand(const UnitReturnedToHandEvent& e); // WhenAUnitReturnsToHandHere
    void onShowdownStarted(const ShowdownStartedEvent& e);  // WhenAShowdownBeginsHere
    void onCardRevealed(const CardRevealedEvent& e);        // WhenIRevealedFromTop

    // ── Helpers ──

    /// Add a triggered ability to the chain for a game object.
    /// `triggering_spell_energy_spent` snapshots the total energy spent
    /// on the spell that caused this trigger (relevant for
    /// WhenYouPlaySpell triggers like Virtuoso / Forgotten Library).
    /// 0 = not a play-triggered ability (default).
    void fireTrigger(GameObjectId source, PlayerId controller,
                     int triggering_spell_energy_spent = 0,
                     TriggerType which = TriggerType::None);

    /// Fire `t` on any battlefield CARD (state_.battlefields[].card_object_id)
    /// that declares it, attributed to `player`. BF card objects have no
    /// location/controller so the normal on-board scans skip them; this covers
    /// their phase triggers (AtStartOfBeginning/AtEndOfTurn/AtStartOfMain) and
    /// "When you play a unit here" style triggers. `at_battlefield` (when not
    /// kInvalidId) restricts firing to the BF whose id matches — used so a
    /// "play a unit here" trigger only fires for the BF the unit landed at.
    void fireBattlefieldTriggers(TriggerType t, PlayerId player,
                                  BattlefieldId at_battlefield = kInvalidId);

    /// Fire `t` on any legend in the champion zone that declares it. Pass
    /// `relevant_player = PlayerId::None` to sweep both players (the
    /// per-handler logic can still scope semantics by passing only the
    /// relevant player's id — e.g. scoring events pass the scoring player).
    void fireLegendTrigger(TriggerType t, PlayerId relevant_player,
                           int triggering_spell_energy_spent = 0);
};

} // namespace riftbound
