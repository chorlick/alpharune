#include "engine/trash_replacement.h"
#include "chain_manager.h"

#include "effect_executor.h"

#include "cards/card.h"

#include <algorithm>
#include <cassert>
#include <stdexcept>

namespace riftbound {

ChainManager::ChainManager(GameState& state, EventBus& events,
                           const CardDB& card_db)
    : state_(state), events_(events), card_db_(card_db) {}

// CR 809: choosing an opponent's [Deflect] object costs its Deflect Value in
// [A], a mandatory additional cost of making that choice. A choice whose cost
// cannot be paid was never legal, so it is not made: the object is dropped
// from the item's targets (the instruction that names it then does nothing,
// like a mistarget) instead of being chosen for free.
void ChainManager::payDeflectForChoices(ChainItem& item) {
    if (!executor_) return;
    // "Ignore [Deflect] while paying this spell's cost." (Card::ignoresDeflect)
    // — as Card::pickTarget honours it for a choice made at resolution.
    if (const Card* card = executor_->cardFor(item.card_def_id); card && card->ignoresDeflect())
        return;
    std::vector<GameObjectId> chosen;
    chosen.reserve(item.targets.size());
    for (auto t : item.targets) {
        const int deflect = executor_->deflectCostFor(item.controller, t);
        if (deflect > 0 &&
            !executor_->payAnyPower(item.controller, deflect,
                                    item.is_spell ? SpendFor::Spell : SpendFor::Other)) {
            RIFT_TRACE(events_, "DEFLECT_UNPAID: " + std::string(toString(item.controller)) +
                             " cannot pay " + std::to_string(deflect) + " [A] to choose " +
                             state_.getObject(t).name + " — it is not chosen");
            continue;
        }
        if (deflect > 0)
            RIFT_TRACE(events_, "DEFLECT: " + std::string(toString(item.controller)) + " pays " +
                             std::to_string(deflect) + " [A] to choose " +
                             state_.getObject(t).name);
        chosen.push_back(t);
    }
    item.targets = chosen;
    item.chosen_objects = std::move(chosen);
}

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
    // Objects chosen as this item is played. CR 809: choosing an opponent's
    // [Deflect] object costs its Deflect Value in [A], a mandatory additional
    // cost (resolve-time choices pay in Card::pickTarget instead).
    item.chosen_objects = targets;
    // Each as it is now: one that changes zones to or from a non-board zone
    // before this item resolves is a new object (CR 359.3.e.4; G14).
    for (auto t : targets) state_.stampChosen(item, t);
    payDeflectForChoices(item);
    item.is_spell = true;

    chain.items.push_back(item);
    // Announce the choices only once the item is on the chain, so abilities
    // that trigger on them go above it (and resolve first).
    for (auto t : item.targets) {
        if (!state_.objectExists(t)) continue;
        // "Chosen an enemy unit this turn" and the per-turn count of enemy
        // units / gear chosen (GameState::noteChosen).
        state_.noteChosen(controller, t, &item);
        const auto& tobj = state_.getObject(t);
        if (!tobj.isUnit() || !tobj.location.has_value()) continue;
        if (tobj.controller == controller)
            events_.emit(ObjectStateChangedEvent{t, "chosen_by_controller"});
    }

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
                                      const std::vector<GameObjectId>& targets,
                                      bool is_activated,
                                      int ability_index) {
    auto& chain = state_.chain;
    bool was_empty = !chain.exists();

    ChainItem item;
    item.id = chain.allocateId();
    item.status = ChainItemStatus::Pending;
    item.source = source;
    item.card_def_id = def_id;
    item.controller = controller;
    item.targets = targets;
    // Objects chosen as this item is played. CR 809: choosing an opponent's
    // [Deflect] object costs its Deflect Value in [A], a mandatory additional
    // cost (resolve-time choices pay in Card::pickTarget instead).
    item.chosen_objects = targets;
    // Each as it is now: one that changes zones to or from a non-board zone
    // before this item resolves is a new object (CR 359.3.e.4; G14).
    for (auto t : targets) state_.stampChosen(item, t);
    payDeflectForChoices(item);
    item.is_ability = true;
    item.is_activated_ability = is_activated;
    item.ability_index = ability_index;

    chain.items.push_back(item);
    // Announce the choices only once the item is on the chain, so abilities
    // that trigger on them go above it (and resolve first).
    for (auto t : item.targets) {
        if (!state_.objectExists(t)) continue;
        // "Chosen an enemy unit this turn" and the per-turn count of enemy
        // units / gear chosen (GameState::noteChosen).
        state_.noteChosen(controller, t, &item);
        const auto& tobj = state_.getObject(t);
        if (!tobj.isUnit() || !tobj.location.has_value()) continue;
        if (tobj.controller == controller)
            events_.emit(ObjectStateChangedEvent{t, "chosen_by_controller"});
    }

    // Source stays on board — do NOT move to Chain zone

    // Close state when chain is created
    state_.turn.oc_state = OpenClosedState::Closed;

    if (was_empty) {
        events_.emit(ChainCreatedEvent{item.id, controller});
    }

    return item.id;
}

std::optional<ChainItem> ChainManager::removeItemFrom(GameState& state, ChainItemId id) {
    auto& items = state.chain.items;
    for (auto it = items.begin(); it != items.end(); ++it) {
        if (it->id != id) continue;
        ChainItem out = std::move(*it);
        items.erase(it);
        return out;
    }
    return std::nullopt;
}

void ChainManager::processFEPR(
    AgentQuery query_agent,
    std::function<void(const ChainItem&)> resolve_permanent,
    std::function<void(const ChainItem&)> resolve_spell,
    ClosedActionsGen gen_closed_actions) {

    // The engine's play path, called from the Execute step for a card played
    // in response, ends by running the chain: this loop is already doing that.
    if (playing_card_ || running_) return;
    // An item is resolving: nothing is finalized or resolved and no Cleanup
    // happens until it has finished (CR 321, 354.3, 402.2).
    if (state_.chain.resuming.has_value()) return;

    struct Running {
        bool& flag;
        explicit Running(bool& f) : flag(f) { flag = true; }
        ~Running() { flag = false; }
    } running(running_);

    // Engine bound on a chain that never empties (abilities that keep
    // triggering each other). Every pass finalizes or resolves one item, so
    // this is about a thousand items. Reaching it is an engine error that
    // ends the game as a failure — it is not a quiet end of the chain
    // (finding F1: the chain used to be dropped with a warning and the game
    // played on with a thousand tokens on the board).
    constexpr int kMaxIterations = 2000;
    int iterations = 0;
    bool had_chain = false;

    // HOT FEPR (CR 334): Handle Outstanding Tasks, then Finalize, Execute,
    // Pass, Resolve. The loop ends when the tasks leave nothing on the chain.
    bool emptied_announced = false;
    auto announceEmptied = [&] {
        // CR 331.2 / 340.2: no Chain, so the turn is in an Open State — for
        // the Cleanup that follows the last resolution too (CR 323.5: control
        // of a battlefield without units is lost only in an Open State).
        if (!had_chain || emptied_announced || state_.chain.exists()) return;
        state_.chain.may_loop_counts.clear();   // a new chain is a new count (TR 505)
        state_.turn.oc_state = OpenClosedState::Open;
        events_.emit(ChainEmptiedEvent{});
        emptied_announced = true;
    };
    bool quiet_finalization = false;   // the last pass finalized an item and logged nothing
    while (true) {
        announceEmptied();
        if (handle_tasks_ && !quiet_finalization) handle_tasks_();
        quiet_finalization = false;
        if (state_.game_over || !state_.chain.exists()) break;
        had_chain = true;
        emptied_announced = false;
        if (++iterations > kMaxIterations) {
            const ChainItem& top = state_.chain.items.back();
            // A loop nobody keeps going by a decision and nobody broke: the
            // engine says what the rules make of it (the game is a draw,
            // Tournament Rules 505.6). Anything else is an engine error.
            if (unbroken_loop_ && unbroken_loop_(top)) break;
            throw std::logic_error(
                "ChainManager::processFEPR: the chain did not empty after " +
                std::to_string(kMaxIterations) + " passes; " +
                std::to_string(state_.chain.items.size()) + " item(s) on it, the newest is " +
                (state_.objectExists(top.source) ? state_.getObject(top.source).name.get()
                                                 : std::string("?")) +
                " (chain_item=" + toString(top.id) + ")");
        }
        // CR 331.1: the turn is in a Closed State while a Chain exists,
        // whoever last wrote the state (a showdown that began with an ability
        // pending, the start of a phase).
        state_.turn.oc_state = OpenClosedState::Closed;

        // Step 1: Finalize the oldest pending item (CR 337.1). Permanents
        // resolve as they are finalized (CR 337.2). Then back to the tasks:
        // a Cleanup follows every finalization (CR 319.4).
        if (state_.chain.hasPending()) {
            if (order_pending_) order_pending_();
            const uint64_t before = activity_ ? activity_() : 0;
            const size_t items_before = state_.chain.items.size();
            stepFinalize(resolve_permanent);
            // Nothing chosen, paid, removed or entered: only the item's
            // status changed, and no Cleanup task can have anything to do.
            quiet_finalization = activity_ && activity_() == before &&
                                 state_.chain.items.size() == items_before;
            continue;
        }

        // Steps 2-3: Execute and Pass. The controller of the newest item gets
        // Priority; an item added restarts from the tasks.
        if (stepExecuteAndPass(query_agent, gen_closed_actions)) {
            if (state_.game_over) break;   // a concession
            continue;
        }

        // Step 4: Resolve the newest item; a Cleanup follows (CR 319.5).
        stepResolve(query_agent, resolve_spell);
    }

    announceEmptied();
}

void ChainManager::stepFinalize(
    std::function<void(const ChainItem&)> resolve_permanent) {

    size_t i = 0;
    while (i < state_.chain.items.size() &&
           state_.chain.items[i].status != ChainItemStatus::Pending) ++i;
    if (i == state_.chain.items.size()) return;

    state_.chain.items[i].status = ChainItemStatus::Finalized;
    const ChainItemId fid = state_.chain.items[i].id;

    // Choices made at finalization (targets, "may", costs; see
    // setFinalizeChoices). The callback may put triggered abilities on the
    // chain, so the item is found again afterwards.
    if (finalize_choices_ && !state_.chain.items[i].is_permanent)
        finalize_choices_(state_.chain.items[i]);
    bool found = false;
    for (size_t k = 0; k < state_.chain.items.size(); ++k)
        if (state_.chain.items[k].id == fid) { i = k; found = true; break; }
    if (!found) return;

    // CR 403.1.a / 403.4 / 405.2: a triggered ability that is not performed
    // — declined, not paid for, or without a legal option for a required
    // choice — "is removed from the chain": it "ceases to be a Pending Item
    // but never becomes a Finalized Chain Item". No finalized event, no
    // priority; if it was the only item the state is Open again.
    if (state_.chain.items[i].remove_at_finalize) {
        const ChainItem gone = state_.chain.items[i];
        RIFT_TRACE(events_, "REMOVED: " +
                         (state_.objectExists(gone.source) ? state_.getObject(gone.source).name.get()
                                                           : std::string("?")) +
                         " (chain_item=" + toString(gone.id) +
                         ") leaves the chain unfinalized — not performed (CR 403)");
        state_.chain.items.erase(state_.chain.items.begin() + static_cast<ptrdiff_t>(i));
        return;
    }

    {
        const auto& fin = state_.chain.items[i];
        events_.emit(ChainItemFinalizedEvent{fin.id, fin.source, fin.controller});
    }
    for (size_t k = 0; k < state_.chain.items.size(); ++k)
        if (state_.chain.items[k].id == fid) { i = k; found = true; break; }

    // CR 337.2: Permanents resolve immediately on finalize. They leave the
    // chain and become game objects on the board.
    if (i < state_.chain.items.size() && state_.chain.items[i].id == fid &&
        state_.chain.items[i].is_permanent) {
        ChainItem resolved_item = state_.chain.items[i]; // copy before erasing
        state_.chain.items.erase(state_.chain.items.begin() + static_cast<ptrdiff_t>(i));

        resolve_permanent(resolved_item);

        events_.emit(ChainItemResolvedEvent{resolved_item.id,
            resolved_item.source, resolved_item.controller});
    }
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
    // An ability used in response that puts nothing on the chain (a rune's
    // Add, CR 164.2) is not a pass: the player keeps priority and the window
    // goes on, however many of them are used. The bound only stops an
    // activation that keeps being chosen without ever changing anything.
    constexpr int kMaxCostOnlyActivations = 512;
    int cost_only_activations = 0;

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
        } else if (chosen.type == IntentType::PlayReaction && play_card_) {
            // One play path for every card (see setPlayCard).
            const ChainItemId before = state_.chain.next_id;
            playing_card_ = true;
            try {
                play_card_(chosen);
            } catch (...) {
                playing_card_ = false;
                throw;
            }
            playing_card_ = false;
            if (state_.chain.next_id != before)
                return true; // Item added → restart FEPR from Finalize
            // The play did not happen. (An activated ability that adds no
            // item is different: a cost-only ability — a rune's Add — has done
            // its work, and is offered again only while it is still legal.)
            // An option that was offered and could not be executed: the
            // generator and the play path disagree. Not one such play in
            // 2,268 games across every pairing of the constructed lists
            // (batch 1 of the testing phase), so it is an engine error and no
            // longer a play quietly taken off the list and asked again.
            throw std::logic_error(
                "ChainManager: " +
                (state_.objectExists(chosen.card) ? state_.getObject(chosen.card).name.get()
                                                  : std::string("?")) +
                " was offered to " + toString(current) + " at chain priority (decision #" +
                std::to_string(state_.decision_index) +
                ") and playing it put nothing on the chain");
        } else if (chosen.type == IntentType::PlayReaction) {
            // There is one way to play a card: the engine's (setPlayCard). A
            // ChainManager with no play callback cannot play one.
            throw std::logic_error(
                "ChainManager: a card was played in response but no play callback is set "
                "(ChainManager::setPlayCard)");
        }
        else if (chosen.type == IntentType::ActivateReactionAbility ||
                 chosen.type == IntentType::ActivateAbility ||
                 chosen.type == IntentType::ActivateActionAbility) {
            // A [Reaction] ability used in response (CR 164.2 runes, Gold,
            // legends, ...). The engine pays its cost and queues it.
            if (activate_ability_ && activate_ability_(chosen)) {
                return true;   // item added → restart FEPR from Finalize
            }
            // No chain item (cost-only effect or nothing happened): the same
            // player keeps priority and chooses again. This did not use up one
            // of the window's passes.
            if (++cost_only_activations <= kMaxCostOnlyActivations) --pass;
        } else if (chosen.type == IntentType::Concede) {
            // Always accepted (GameEngine::askAgent): the game ends here, as
            // it does for a concession at any other decision.
            if (!state_.game_over) {   // (the engine's askAgent has ended it already)
                state_.game_over = true;
                state_.winner = opponent(current);
                state_.game_over_reason = toString(current) + std::string(" conceded");
                events_.emit(GameOverEvent{state_.winner, state_.game_over_reason});
            }
            return true;   // nothing resolves: the loop sees the game is over
        } else {
            // An option the generator offered at chain priority that nothing
            // here can execute (finding D6: a plain PlayCard was offered, the
            // choice was dropped and the same decision asked again).
            throw std::logic_error(
                std::string("ChainManager: ") + toString(chosen.type) +
                " was chosen at chain priority and cannot be executed there");
        }
    }

    // Engine bound: the window never ended in both players passing (an
    // activation that changes nothing chosen hundreds of times). It used to
    // go on to resolve the item without the passes.
    throw std::logic_error("ChainManager: the priority window of chain item " +
                           toString(top->id) + " did not end in both players passing");
}

// Runs the item in `state_.chain.resuming` to completion. Resumable cards
// (discard, predict, the Card::pick* / confirmOptional helpers) publish a
// pending choice via `EffectExecutor::requestChoice` and set
// `resuming->resume_point`; we then query the agent, record the choice for the
// Card to read on re-entry, and re-call resolve_spell until no choice is left.
void ChainManager::runResumable(
    const AgentQuery& query_agent,
    const std::function<void(const ChainItem&)>& resolve_spell) {
    if (!state_.chain.resuming.has_value()) return;
    if (!state_.chain.resuming->is_spell && !state_.chain.resuming->is_ability) return;

    // A card may ask as many questions as its text needs (one per unit on a
    // crowded board). The bound only stops a card that never stops asking;
    // reaching it is an engine error, not a quiet end of the resolution (the
    // same rule as GameEngine::runFinalizePass).
    constexpr int kMaxResumeIterations = 1024;
    for (int iter = 0; ; ++iter) {
        if (iter >= kMaxResumeIterations) {
            const auto& ri = *state_.chain.resuming;
            throw std::logic_error(
                "ChainManager::runResumable: " +
                (state_.objectExists(ri.source) ? state_.getObject(ri.source).name
                                                : std::string("?")) +
                " asked more than " + std::to_string(kMaxResumeIterations) +
                " choices while resolving");
        }

        // The engine's resolveSpell dispatches through Card::onResolve /
        // onTrigger / onActivate based on is_spell / is_ability.
        resolve_spell(state_.chain.resuming.value());

        if (!executor_ || !executor_->hasPendingChoice()) return;

        auto pending = executor_->consumePendingChoice();
        // Surface the labeled choice request in the trace BEFORE the
        // agent picks. Pairs with the on_decision-callback-driven CHOSE
        // logging downstream; gives a reader the WHY of a MakeChoice
        // decision (e.g. "discard 1 (Lunar Boon)") instead of just the
        // WHAT (e.g. "pick=[Hard Bargain(id=12)]"). Cards that don't pass
        // a label fall back to a generic line.
        RIFT_TRACE(events_, 
            std::string("CHOICE-REQUEST: ") +
            (pending.label.empty() ? "MakeChoice" : pending.label) +
            " [" + std::to_string(pending.legal.size()) + " options] (" +
            toString(pending.player) + ")");
        // The prompt's own text heads its decision line (plans/09 9.PP1).
        const EffectExecutor::Asking named(*executor_, pending.askedPrompt(
            pending.label.empty() &&
                    state_.chain.resuming.has_value() &&
                    state_.objectExists(state_.chain.resuming->source)
                ? state_.getObject(state_.chain.resuming->source).name + ": a choice"
                : std::string("a choice")));
        Intent choice = query_agent(pending.player, pending.legal);
        executor_->recordChoice(std::move(choice));
        // Loop back — resolve_spell re-invokes the Card, which reads the
        // recorded choice via `executor.takeChoice()` and continues past
        // its resume_point.
    }
}

void ChainManager::stepResolve(
    AgentQuery query_agent,
    std::function<void(const ChainItem&)> resolve_spell) {

    assert(!state_.chain.items.empty() && "stepResolve called with empty chain");

    // Pop the newest (top) item — LIFO. Counter spells (Abandon, Wind Wall,
    // Repulse, Not So Fast, Lilting Lullaby) rely on `items.back()` being
    // the previous item on the chain (their counter target) — keep that
    // invariant by popping FIRST. The popped item is held in
    // `state_.chain.resuming` so a resumable Card can read/write its
    // resume_point across iterations.
    ChainItem resolved = state_.chain.items.back();
    state_.chain.items.pop_back();
    state_.chain.resuming = resolved;

    // Single-shot cards (the common case) resolve in one call; resumable
    // cards publish choices and are re-entered (runResumable).
    runResumable(query_agent, resolve_spell);

    // Repeat (CR 820): if `repeats_paid > 0`, execute the item's instructions
    // that many extra times. Each execution starts the Card afresh (case 0)
    // and is driven through the same resumable loop, so a repeated effect
    // that asks for a choice gets its answer and finishes before the item is
    // disposed of. Its choices may differ from the first execution's
    // (CR 820.2.a) and were made with them, as the item was finalized (CR
    // 820.2): they are loaded from ChainItem::repeat_choices below. An item
    // nobody finalized has none and asks as the repeat executes.
    if (state_.chain.resuming.has_value()) {
        const int repeats = state_.chain.resuming->repeats_paid;
        for (int r = 0; r < repeats && state_.chain.resuming.has_value(); ++r) {
            auto& ri = *state_.chain.resuming;
            ri.resume_point = 0;
            ri.resume_data.clear();
            ri.may_answer = -1;
            ri.chosen_item = kInvalidId;
            ri.chosen_stamps.clear();
            ri.choices_finalized = false;
            ri.finalize_costs_paid = false;
            ri.finalize_checked = 0;
            // The choices this execution was given as the item was finalized
            // (CR 820.2; ChainItem::repeat_choices).
            if (static_cast<size_t>(r) < ri.repeat_choices.size()) {
                const auto chosen = ri.repeat_choices[static_cast<size_t>(r)];   // copy
                ri.targets = chosen.targets;
                ri.chosen_objects = chosen.chosen_objects;
                ri.chosen_stamps = chosen.chosen_stamps;
                ri.chosen_item = chosen.chosen_item;
                ri.resume_point = chosen.resume_point;
                ri.resume_data = chosen.resume_data;
                ri.may_answer = chosen.may_answer;
                ri.choices_finalized = chosen.choices_finalized;
                ri.finalize_costs_paid = chosen.finalize_costs_paid;
            }
            runResumable(query_agent, resolve_spell);
        }
    }

    // Read the (possibly-mutated) resolving item back out and dispose.
    resolved = std::move(state_.chain.resuming.value());
    state_.chain.resuming.reset();

    // "Play this spell again" (Dancing Grenade, Death from Below): the spell's
    // own effect put the card back on the chain as a new play. The card is on
    // the chain for that play now — it is not filed anywhere as this
    // resolution ends; the new play files it when IT resolves (and carries
    // any "banish / recycle as it leaves the chain" along).
    bool replayed = false;
    if (resolved.is_spell)
        for (const auto& ci : state_.chain.items)
            if (ci.is_spell && ci.source == resolved.source) replayed = true;

    if (replayed && state_.objectExists(resolved.source)) {
        RIFT_TRACE(events_, "CHAIN: " + state_.getObject(resolved.source).name +
                         " was played again while it resolved — it stays on the chain");
        events_.emit(SpellResolvedEvent{resolved.source, resolved.controller});
    } else if (resolved.is_spell && state_.objectExists(resolved.source)) {
        auto& spell_obj = state_.getObject(resolved.source);
        // A card always goes to its OWNER's trash / banishment / deck, whoever
        // controlled the spell (objects built by hand in tests may have none).
        const PlayerId home = spell_obj.owner != PlayerId::None ? spell_obj.owner
                                                                 : resolved.controller;
        auto rit = spell_obj.card_counters.find("__recycle_on_leave_chain");
        if (spell_obj.zone != ZoneType::Chain) {
            // The spell's own effect already moved it ("Banish this."): it is
            // where that put it and is not filed a second time.
            spell_obj.banish_on_leave_chain = false;
            if (rit != spell_obj.card_counters.end()) rit->second = 0;
            events_.emit(SpellResolvedEvent{resolved.source, resolved.controller});
        } else if (rit != spell_obj.card_counters.end() && rit->second != 0 && executor_) {
            // "Then recycle it" (Fizz, Trickster; Kai'Sa, Evolutionary): a
            // leaving-the-chain replacement like "then banish it" below. The
            // spell goes from the chain straight to the bottom of its owner's
            // deck — it is never put in the trash.
            rit->second = 0;
            spell_obj.location = std::nullopt;
            events_.emit(SpellResolvedEvent{resolved.source, resolved.controller});
            RIFT_TRACE(events_, "RECYCLE: " + spell_obj.name +
                                " leaves the chain to the bottom of its owner's deck (\"Then "
                                "recycle it\")");
            executor_->recycleCards(home, {resolved.source});
        } else {
            // The spell goes to its owner's trash after resolving (CR 359.3).
            // [Flow] / "then banish this": banished instead of trashed as it
            // leaves the chain (a replacement effect, Vendetta rules update).
            const bool to_banish = spell_obj.banish_on_leave_chain;
            spell_obj.banish_on_leave_chain = false;
            const ZoneType dest = to_banish ? ZoneType::Banishment : ZoneType::Trash;
            spell_obj.zone = dest;
            spell_obj.location = std::nullopt;
            if (to_banish) {
                state_.player(home).banishment.push_back(resolved.source);
                state_.player(home).cards_banished_this_turn++;
            } else {
                state_.player(home).trash.push_back(resolved.source);
                settleInTrash(state_, events_, resolved.source);
            }

            events_.emit(SpellResolvedEvent{resolved.source, resolved.controller});
            events_.emit(LeftBoardEvent{resolved.source, resolved.controller,
                CardType::Spell, BaseLocation{resolved.controller},
                dest, false});
            if (to_banish)
                events_.emit(ObjectStateChangedEvent{resolved.source, "banished_owned"});
        }
    }
    // Triggered/activated abilities leave their source on the board — no
    // zone changes here. The chain item itself is gone (popped above).

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
