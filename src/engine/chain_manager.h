#pragma once
/// @file chain_manager.h
/// Chain resolution manager — implements the FEPR loop.
///
/// FEPR = Finalize → Execute → Pass → Resolve
/// This is the core mechanic enabling spells, reactions, triggered abilities,
/// and activated abilities. Items stack on the chain and resolve LIFO.
///
/// Rules references: CR 335-340, 309, 312, 337.1.c, 354, 359

#include "core/card_db.h"
#include "core/events.h"
#include "core/game_state.h"
#include "core/intent.h"

#include <functional>
#include <string>
#include <vector>

namespace riftbound {

class EffectExecutor;

/// Callback to query an agent for an action.
/// Given a player and their legal actions, returns their chosen action.
using AgentQuery = std::function<Intent(PlayerId, const std::vector<Intent>&)>;

class ChainManager {
public:
    ChainManager(GameState& state, EventBus& events, const CardDB& card_db);

    /// Add a spell to the chain as a Pending Item.
    /// Returns the chain item ID.
    ChainItemId addSpell(GameObjectId spell_obj, PlayerId controller,
                         const std::vector<GameObjectId>& targets);

    /// Add a permanent (unit/gear) to the chain as a Pending Item.
    /// Permanents resolve immediately on finalize (CR 337.1.c).
    ChainItemId addPermanent(GameObjectId card_obj, PlayerId controller);

    /// Add a triggered/activated ability to the chain. Source stays on
    /// board (not moved to chain zone). `is_activated` distinguishes
    /// "player chose to activate (e.g. [E]:)" from "event fired" — set
    /// it true from the ActivateAbility intent handler in the engine,
    /// false (default) from TriggerManager. Drives whether stepResolve
    /// dispatches through Card::onActivate or Card::onTrigger.
    ChainItemId addAbility(GameObjectId source, PlayerId controller,
                           CardDefId def_id,
                           const std::vector<GameObjectId>& targets = {},
                           bool is_activated = false,
                           int ability_index = 0);

    /// Callback to generate legal closed-state actions for a player.
    /// Injected from GameEngine so targeting and affordability are handled.
    using ClosedActionsGen = std::function<std::vector<Intent>(PlayerId)>;

    /// Run the FEPR loop until the chain is empty.
    /// query_agent is called whenever a player with priority must act.
    /// resolve_permanent is called when a permanent finalizes (bypasses chain).
    /// resolve_spell is called when a spell resolves.
    /// gen_closed_actions generates legal actions during Closed State.
    void processFEPR(
        AgentQuery query_agent,
        std::function<void(const ChainItem&)> resolve_permanent,
        std::function<void(const ChainItem&)> resolve_spell,
        ClosedActionsGen gen_closed_actions = nullptr
    );

    /// Check if the chain currently exists.
    bool chainExists() const { return state_.chain.exists(); }

    /// Set affordability check callback (injected from GameEngine).
    using AffordCheck = std::function<bool(PlayerId, GameObjectId)>;
    void setAffordCheck(AffordCheck check) { can_afford_ = std::move(check); }

    /// Set cost payment callback (injected from GameEngine).
    using PayCost = std::function<bool(PlayerId, GameObjectId)>;
    void setPayCost(PayCost pay) { pay_cost_ = std::move(pay); }

    /// Activating an ability while the chain is open ([Reaction] abilities:
    /// a rune's Add, Gold, legends). Pays the cost and adds the item; returns
    /// true if an item was added.
    /// Called for each item as it is finalized, before anyone gets priority:
    /// the engine makes the choices the rules place at finalization (CR 355 —
    /// targets are chosen as the card or ability is played, and Deflect is
    /// paid then).
    using FinalizeChoices = std::function<void(ChainItem&)>;
    void setFinalizeChoices(FinalizeChoices fn) { finalize_choices_ = std::move(fn); }
    using ActivateAbility = std::function<bool(const Intent&)>;
    void setActivateAbility(ActivateAbility fn) { activate_ability_ = std::move(fn); }

    /// Playing a card while the chain is open (a [Reaction] spell, an
    /// [Ambush] / hidden permanent): the engine's own play path, so a card
    /// played in response is played exactly like one played in an Open State
    /// (Card::onPlay, additional costs, Repeat, per-turn counters,
    /// "played from somewhere other than your hand"). That path ends by
    /// running the chain; while this callback runs, a nested processFEPR
    /// returns at once and the loop already in progress carries on.
    /// The state was edited from outside while a player was being asked
    /// (god mode): whatever made a play fail earlier in this priority window
    /// may no longer hold, so the plays dropped from it are offered again —
    /// as GameEngine::recomputePendingLegalActions, which rebuilds the list
    /// for the edited state, already does.
    void forgetDroppedPlays() { dropped_plays_.clear(); }
    using PlayCard = std::function<void(const Intent&)>;
    void setPlayCard(PlayCard fn) { play_card_ = std::move(fn); }

    /// Inject the EffectExecutor so stepResolve can detect mid-resolution
    /// pending choices published by Card::onResolve / onTrigger via
    /// `requestChoice`. Required before processFEPR runs.
    void setEffectExecutor(EffectExecutor* exec) { executor_ = exec; }

    /// Generate legal actions for a player with priority in Closed State.
    std::vector<Intent> generateClosedStateActions(PlayerId player) const;

private:
    GameState& state_;
    EventBus& events_;
    const CardDB& card_db_;
    AffordCheck can_afford_;
    PayCost pay_cost_;
    ActivateAbility activate_ability_;
    PlayCard play_card_;
    // Plays chosen in the current priority window that did not happen (see
    // stepExecuteAndPass). Not game state: it lives for one window.
    std::vector<Intent> dropped_plays_;
    bool playing_card_ = false;   // inside play_card_: nested processFEPR is a no-op
    FinalizeChoices finalize_choices_;
    EffectExecutor* executor_ = nullptr;

    /// Step 1: Finalize all pending items in order. (processFEPR reads the
    /// chain itself afterwards — exists / hasPending — so there is nothing
    /// to return.)
    void stepFinalize(
        std::function<void(const ChainItem&)> resolve_permanent
    );

    /// Steps 2-3: Execute (player acts) and Pass (check all passed).
    /// Returns true if an item was added (restart from Finalize).
    bool stepExecuteAndPass(AgentQuery query_agent,
                            ClosedActionsGen gen_closed_actions);

    /// Pays the Deflect cost (CR 809) of each object `item` chooses as it is
    /// played; an object whose cost cannot be paid is removed from the item's
    /// targets — it was not a legal choice.
    void payDeflectForChoices(ChainItem& item);

    /// Runs the item held in `state_.chain.resuming` until it stops asking
    /// for choices (see stepResolve).
    void runResumable(const AgentQuery& query_agent,
                      const std::function<void(const ChainItem&)>& resolve_spell);

    /// Gives up on a chain that will not empty (safety bound in processFEPR):
    /// discards what is on it, reopens the state and emits ChainEmptiedEvent.
    void abandonChain(const std::string& why);

    /// Step 4: Resolve the top item on the chain. Drives a resumable loop:
    /// while the resolving Card publishes a pending choice via
    /// `EffectExecutor::requestChoice`, query the agent + record the
    /// choice + re-invoke resolve_spell so the Card's next switch case
    /// runs. Single-shot resolution (the common case) loops once.
    void stepResolve(AgentQuery query_agent,
                     std::function<void(const ChainItem&)> resolve_spell);
};

} // namespace riftbound
