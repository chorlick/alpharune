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
#include <vector>

namespace riftbound {

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

    /// Add a triggered/activated ability to the chain.
    /// Source stays on board (not moved to chain zone).
    ChainItemId addAbility(GameObjectId source, PlayerId controller,
                           CardDefId def_id,
                           const std::vector<GameObjectId>& targets = {});

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

    /// Generate legal actions for a player with priority in Closed State.
    std::vector<Intent> generateClosedStateActions(PlayerId player) const;

private:
    GameState& state_;
    EventBus& events_;
    const CardDB& card_db_;
    AffordCheck can_afford_;
    PayCost pay_cost_;

    /// Step 1: Finalize all pending items in order.
    /// Returns true if any items were finalized.
    bool stepFinalize(
        std::function<void(const ChainItem&)> resolve_permanent
    );

    /// Steps 2-3: Execute (player acts) and Pass (check all passed).
    /// Returns true if an item was added (restart from Finalize).
    bool stepExecuteAndPass(AgentQuery query_agent,
                            ClosedActionsGen gen_closed_actions);

    /// Step 4: Resolve the top item on the chain.
    void stepResolve(std::function<void(const ChainItem&)> resolve_spell);
};

} // namespace riftbound
