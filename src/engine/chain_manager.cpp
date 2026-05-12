#include "chain_manager.h"

#include <algorithm>
#include <cassert>

namespace riftbound {

ChainManager::ChainManager(GameState& state, EventBus& events,
                           const CardDB& card_db)
    : state_(state), events_(events), card_db_(card_db) {}

ChainItemId ChainManager::addSpell(GameObjectId spell_obj, PlayerId controller,
                                    const std::vector<GameObjectId>& targets) {
    auto& chain = state_.chain;
    bool was_empty = !chain.exists();

    ChainItem item;
    item.id = chain.allocateId();
    item.status = ChainItemStatus::Pending;
    item.source = spell_obj;
    item.card_def_id = state_.getObject(spell_obj).card_def_id;
    item.controller = controller;
    item.targets = targets;
    item.is_spell = true;

    chain.items.push_back(item);

    // Move spell object to chain zone
    auto& obj = state_.getObject(spell_obj);
    obj.zone = ZoneType::Chain;

    // Close the state (CR 354)
    state_.turn.oc_state = OpenClosedState::Closed;

    if (was_empty) {
        events_.emit(ChainCreatedEvent{item.id, controller});
    }

    return item.id;
}

ChainItemId ChainManager::addPermanent(GameObjectId card_obj,
                                         PlayerId controller) {
    auto& chain = state_.chain;
    bool was_empty = !chain.exists();

    ChainItem item;
    item.id = chain.allocateId();
    item.status = ChainItemStatus::Pending;
    item.source = card_obj;
    item.card_def_id = state_.getObject(card_obj).card_def_id;
    item.controller = controller;
    item.is_permanent = true;

    chain.items.push_back(item);

    // Card goes to Chain zone temporarily (CR 354: step 1)
    auto& obj = state_.getObject(card_obj);
    obj.zone = ZoneType::Chain;

    if (was_empty) {
        events_.emit(ChainCreatedEvent{item.id, controller});
    }

    return item.id;
}

ChainItemId ChainManager::addAbility(GameObjectId source, PlayerId controller,
                                      CardDefId def_id,
                                      const std::vector<GameObjectId>& targets) {
    auto& chain = state_.chain;
    bool was_empty = !chain.exists();

    ChainItem item;
    item.id = chain.allocateId();
    item.status = ChainItemStatus::Pending;
    item.source = source;
    item.card_def_id = def_id;
    item.controller = controller;
    item.targets = targets;
    item.is_ability = true;

    chain.items.push_back(item);

    // Source stays on board — do NOT move to Chain zone

    // Close state when chain is created
    state_.turn.oc_state = OpenClosedState::Closed;

    if (was_empty) {
        events_.emit(ChainCreatedEvent{item.id, controller});
    }

    return item.id;
}

void ChainManager::processFEPR(
    AgentQuery query_agent,
    std::function<void(const ChainItem&)> resolve_permanent,
    std::function<void(const ChainItem&)> resolve_spell,
    ClosedActionsGen gen_closed_actions) {

    constexpr int kMaxIterations = 100; // safety
    int iterations = 0;

    while (state_.chain.exists() && iterations < kMaxIterations) {
        iterations++;

        // Step 1: Finalize
        // Permanents resolve immediately here (CR 337.1.c).
        // If finalize removed all items, chain is empty → done.
        bool finalized_something = stepFinalize(resolve_permanent);
        if (!state_.chain.exists()) {
            // Chain emptied during finalization
            state_.turn.oc_state = OpenClosedState::Open;
            events_.emit(ChainEmptiedEvent{});
            return;
        }

        // If there are still pending items, loop back to finalize
        if (state_.chain.hasPending()) continue;

        // Steps 2-3: Execute and Pass
        // Controller of newest item gets Priority.
        // If someone adds an item → restart from Finalize.
        bool item_added = stepExecuteAndPass(query_agent, gen_closed_actions);
        if (item_added) continue; // restart FEPR

        // Step 4: Resolve top item
        stepResolve(resolve_spell);

        // After resolve, check chain state:
        if (!state_.chain.exists()) {
            state_.turn.oc_state = OpenClosedState::Open;
            events_.emit(ChainEmptiedEvent{});
            return;
        }
        // If pending items exist (from triggers), loop back to Finalize
        // If no pending, loop back to Execute (newest item controller gets priority)
    }
}

bool ChainManager::stepFinalize(
    std::function<void(const ChainItem&)> resolve_permanent) {

    bool finalized_any = false;

    // Finalize pending items in order (oldest pending first)
    // Use index loop since vector may be modified
    for (size_t i = 0; i < state_.chain.items.size(); ++i) {
        auto& item = state_.chain.items[i];
        if (item.status != ChainItemStatus::Pending) continue;

        item.status = ChainItemStatus::Finalized;
        finalized_any = true;

        events_.emit(ChainItemFinalizedEvent{item.id, item.source,
                                              item.controller});

        // CR 337.1.c: Permanents resolve immediately on finalize.
        // They leave the chain and become game objects on the board.
        if (item.is_permanent) {
            ChainItem resolved_item = item; // copy before erasing
            state_.chain.items.erase(state_.chain.items.begin() +
                                      static_cast<ptrdiff_t>(i));
            --i; // adjust index

            resolve_permanent(resolved_item);

            events_.emit(ChainItemResolvedEvent{resolved_item.id,
                resolved_item.source, resolved_item.controller});
        }
    }

    return finalized_any;
}

bool ChainManager::stepExecuteAndPass(AgentQuery query_agent,
                                      ClosedActionsGen gen_closed_actions) {
    // Controller of newest finalized item gets Priority first (CR 337.1.b.3)
    auto* top = state_.chain.newest();
    assert(top && "stepExecuteAndPass called with empty chain");

    PlayerId first_priority = top->controller;
    state_.turn.players_passed_priority.clear();

    PlayerId current = first_priority;

    constexpr int kMaxPriorityPasses = 10; // safety (2 players × some margin)

    for (int pass = 0; pass < kMaxPriorityPasses; ++pass) {
        state_.turn.priority_holder = current;
        events_.emit(PriorityGrantedEvent{current, false});

        // Generate legal actions — use injected generator if available
        // (handles targeting + affordability), fallback to our own
        auto actions = gen_closed_actions
            ? gen_closed_actions(current)
            : generateClosedStateActions(current);

        // Query agent
        auto chosen = query_agent(current, actions);

        if (chosen.type == IntentType::PassPriority) {
            state_.turn.players_passed_priority.insert(current);

            // Step 3: Check if ALL players have passed
            bool all_passed = true;
            for (auto pid : {PlayerId::Player1, PlayerId::Player2}) {
                if (state_.turn.players_passed_priority.find(pid) ==
                    state_.turn.players_passed_priority.end()) {
                    all_passed = false;
                    break;
                }
            }

            if (all_passed) {
                return false; // All passed → proceed to Resolve
            }

            // Pass to next player in turn order
            current = opponent(current);
        } else if (chosen.type == IntentType::PlayReaction) {
            auto& card = state_.getObject(chosen.card);
            auto& ps = state_.player(current);

            if (card.is_hidden) {
                // Playing from facedown — remove from BF facedown zone, no cost
                for (auto& bf : state_.battlefields) {
                    auto fit = std::find(bf.facedown.begin(), bf.facedown.end(),
                                          chosen.card);
                    if (fit != bf.facedown.end()) {
                        bf.facedown.erase(fit);
                        break;
                    }
                }
                card.is_hidden = false;
                card.hidden_at = kInvalidId;
            } else {
                // Normal reaction from hand
                auto it = std::find(ps.hand.begin(), ps.hand.end(), chosen.card);
                if (it != ps.hand.end()) ps.hand.erase(it);

                // Pay cost
                if (pay_cost_) {
                    pay_cost_(current, chosen.card);
                }
            }

            // Track play
            ps.cards_played_this_turn++;
            events_.emit(CardPlayedEvent{chosen.card, current,
                card.card_type, ps.cards_played_this_turn});

            // Add to chain with targets
            addSpell(chosen.card, current, chosen.targets);

            return true; // Item added → restart FEPR from Finalize
        }
        // Other intent types (ActivateReactionAbility etc.) are Phase 3+
    }

    return false;
}

void ChainManager::stepResolve(
    std::function<void(const ChainItem&)> resolve_spell) {

    assert(!state_.chain.items.empty() && "stepResolve called with empty chain");

    // Resolve newest (top) item — LIFO
    ChainItem resolved = state_.chain.items.back();
    state_.chain.items.pop_back();

    if (resolved.is_spell) {
        resolve_spell(resolved);

        // Spell goes to controller's trash after resolving (CR 359.3)
        if (state_.objectExists(resolved.source)) {
            auto& spell_obj = state_.getObject(resolved.source);
            spell_obj.zone = ZoneType::Trash;
            spell_obj.location = std::nullopt;
            state_.player(resolved.controller).trash.push_back(resolved.source);

            events_.emit(SpellResolvedEvent{resolved.source, resolved.controller});
            events_.emit(LeftBoardEvent{resolved.source, resolved.controller,
                CardType::Spell, BaseLocation{resolved.controller},
                ZoneType::Trash, false});
        }
    } else if (resolved.is_ability) {
        // Triggered/activated ability — execute effects but source stays on board
        resolve_spell(resolved); // reuse spell resolver (calls executeScript)
    }

    events_.emit(ChainItemResolvedEvent{resolved.id, resolved.source,
                                         resolved.controller});
}

std::vector<Intent> ChainManager::generateClosedStateActions(
    PlayerId player) const {

    std::vector<Intent> actions;

    // Always can pass priority
    actions.push_back(Intent::passPriority(player));

    // Can play Reaction spells from hand (CR 309.1.a)
    auto& ps = state_.player(player);
    for (auto card_id : ps.hand) {
        auto& card = state_.getObject(card_id);
        if (!card.isSpell()) continue;
        if (!card.keywords.has(Keyword::Reaction)) continue;

        // Check affordability via injected callback from GameEngine
        if (can_afford_ && !can_afford_(player, card_id)) continue;

        Intent play;
        play.type = IntentType::PlayReaction;
        play.player = player;
        play.card = card_id;
        // Targets will be filled by the agent from legal targets
        actions.push_back(play);
    }

    return actions;
}

} // namespace riftbound
