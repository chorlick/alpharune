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

namespace riftbound {

class EffectExecutor; // forward declaration

class TriggerManager {
public:
    TriggerManager(GameState& state, EventBus& events,
                   const CardDB& card_db, ChainManager& chain,
                   const CardRegistry& card_registry);

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

    // ── Delayed abilities ──

    /// Check and fire any delayed abilities matching a trigger type.
    void checkDelayedAbilities(TriggerType trigger, PlayerId relevant_player);

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

    /// Fires WhenIDie/Deathknell for the dying unit.
    /// Also fires WhenAFriendlyUnitDies for other objects the controller owns.
    void onUnitDied(const UnitDiedEvent& e);

    /// Fires WhenIConquer, WhenIHold, WhenIConquerOrHold for units at the BF.
    /// Also fires WhenYouConquerHere, WhenYouHoldHere for the battlefield card.
    void onScore(const ScoreEvent& e);

    /// Fires WhenIMove, WhenIMoveToFB for the moving unit.
    void onUnitMoved(const UnitMovedEvent& e);

    /// Fires AtEndOfTurn, AtStartOfBeginning for all objects on board.
    void onPhaseChanged(const PhaseChangedEvent& e);

    // ── Helpers ──

    /// Add a triggered ability to the chain for a game object.
    void fireTrigger(GameObjectId source, PlayerId controller);
};

} // namespace riftbound
