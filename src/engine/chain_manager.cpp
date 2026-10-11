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
        if (deflect > 0 && !executor_->payAnyPower(item.controller, deflect)) {
            events_.logTrace("DEFLECT_UNPAID: " + std::string(toString(item.controller)) +
                             " cannot pay " + std::to_string(deflect) + " [A] to choose " +
                             state_.getObject(t).name + " — it is not chosen");
            continue;
        }
        if (deflect > 0)
            events_.logTrace("DEFLECT: " + std::string(toString(item.controller)) + " pays " +
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

void ChainManager::processFEPR(
    AgentQuery query_agent,
    std::function<void(const ChainItem&)> resolve_permanent,
    std::function<void(const ChainItem&)> resolve_spell,
    ClosedActionsGen gen_closed_actions) {

    // Safety bound on a chain that never empties (abilities that keep
    // triggering each other). Every pass finalizes, adds or resolves an item,
    // so an ordinary chain is nowhere near it.
    constexpr int kMaxIterations = 1000;
    int iterations = 0;

    // The engine's play path, called from the Execute step for a card played
    // in response, ends by running the chain: this loop is already doing that.
    if (playing_card_) return;

    while (state_.chain.exists()) {
        if (++iterations > kMaxIterations) {
            abandonChain("FEPR loop did not empty the chain");
            return;
        }

        // Step 1: Finalize
        // Permanents resolve immediately here (CR 337.1.c).
        // If finalize removed all items, chain is empty → done.
        stepFinalize(resolve_permanent);
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
        stepResolve(query_agent, resolve_spell);

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

void ChainManager::stepFinalize(
    std::function<void(const ChainItem&)> resolve_permanent) {

    // Finalize pending items in order (oldest pending first)
    // Use index loop since vector may be modified
    for (size_t i = 0; i < state_.chain.items.size(); ++i) {
        auto& item = state_.chain.items[i];
        if (item.status != ChainItemStatus::Pending) continue;

        item.status = ChainItemStatus::Finalized;

        // Choices made at finalization (targets; see setFinalizeChoices). The
        // callback may put triggered abilities on the chain, so re-take the
        // reference afterwards.
        if (finalize_choices_ && !item.is_permanent) {
            const ChainItemId fid = item.id;
            finalize_choices_(item);
            bool found = false;
            for (size_t k = 0; k < state_.chain.items.size(); ++k)
                if (state_.chain.items[k].id == fid) { i = k; found = true; break; }
            if (!found) continue;
        }

        {
            const auto& fin = state_.chain.items[i];
            events_.emit(ChainItemFinalizedEvent{fin.id, fin.source, fin.controller});
        }

        // CR 337.1.c: Permanents resolve immediately on finalize.
        // They leave the chain and become game objects on the board.
        if (state_.chain.items[i].is_permanent) {
            ChainItem resolved_item = state_.chain.items[i]; // copy before erasing
            state_.chain.items.erase(state_.chain.items.begin() +
                                      static_cast<ptrdiff_t>(i));
            --i; // adjust index

            resolve_permanent(resolved_item);

            events_.emit(ChainItemResolvedEvent{resolved_item.id,
                resolved_item.source, resolved_item.controller});
        }
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
    // Plays that were chosen in this window and did not happen. They are not
    // offered again: choosing one changes nothing, so asking again with the
    // same options would only repeat it.
    std::vector<Intent> failed_plays;
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
        if (!failed_plays.empty())
            actions.erase(std::remove_if(actions.begin(), actions.end(),
                [&](const Intent& a) {
                    return std::find(failed_plays.begin(), failed_plays.end(), a) !=
                           failed_plays.end();
                }), actions.end());

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
            // The play did not happen (its cost could not be paid after all):
            // the same player keeps priority and chooses again, without it.
            // (An activated ability that adds no item is different: a
            // cost-only ability — a rune's Add — has done its work, and is
            // offered again only while it is still legal.)
            failed_plays.push_back(chosen);
        } else if (chosen.type == IntentType::PlayReaction) {
            // No engine attached (a ChainManager driven on its own): the bare
            // mechanics — leave the zone, pay the printed cost, go on the chain.
            auto& card = state_.getObject(chosen.card);
            auto& ps = state_.player(current);

            // Where a permanent played here will land: the facedown card's own
            // battlefield, or the location the intent names ([Ambush]).
            LocationId permanent_dest = chosen.play_location.value_or(
                LocationId{BaseLocation{current}});
            if (card.is_hidden && card.hidden_at != kInvalidId)
                permanent_dest = BattlefieldLocation{card.hidden_at};

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
                // As GameEngine::executePlayFromHidden records it (read by
                // the hidden spell's own card).
                if (card.hidden_at != kInvalidId) {
                    card.card_counters["__facedown_play_bf"] =
                        static_cast<int>(card.hidden_at) + 1;
                    card.card_counters["__facedown_play_turn"] = state_.turn.turn_number;
                }
                card.is_hidden = false;
                card.hidden_at = kInvalidId;
            } else {
                // Normal reaction from hand — or the Chosen Champion from its
                // zone (CR 420.1.a), when it is a [Reaction] unit.
                auto it = std::find(ps.hand.begin(), ps.hand.end(), chosen.card);
                if (it != ps.hand.end()) ps.hand.erase(it);
                else if (ps.champion_zone == chosen.card) ps.champion_zone = kInvalidId;

                // Pay cost
                if (pay_cost_) {
                    pay_cost_(current, chosen.card);
                }
            }

            // Track play
            ps.cards_played_this_turn++;
            int energy_spent = (card.card_def_id != kInvalidId)
                ? card_db_.get(card.card_def_id).energy_cost : 0;
            // A permanent's location is chosen before the play is announced,
            // so "when you play a unit here" abilities see where it is going.
            if (!card.isSpell()) card.location = permanent_dest;
            events_.emit(CardPlayedEvent{chosen.card, current,
                card.card_type, ps.cards_played_this_turn, energy_spent});

            // Add to chain. A unit or gear played with Reaction timing
            // ([Ambush] — CR 822.1.b, a [Hidden] permanent, "play me as a
            // Reaction") is still a permanent: it resolves onto the board, it
            // is not a spell.
            if (card.isSpell()) {
                addSpell(chosen.card, current, chosen.targets);
            } else {
                card.location = permanent_dest;
                addPermanent(chosen.card, current);
            }

            return true; // Item added → restart FEPR from Finalize
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
        }
    }

    return false;
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
        events_.logTrace(
            std::string("CHOICE-REQUEST: ") +
            (pending.label.empty() ? "MakeChoice" : pending.label) +
            " [" + std::to_string(pending.legal.size()) + " options] (" +
            toString(pending.player) + ")");
        Intent choice = query_agent(pending.player, pending.legal);
        executor_->recordChoice(std::move(choice));
        // Loop back — resolve_spell re-invokes the Card, which reads the
        // recorded choice via `executor.takeChoice()` and continues past
        // its resume_point.
    }
}

// A chain that cannot be brought to an end is given up: what is left on it
// leaves without resolving, the state opens and the game goes on. Cards still
// in the Chain zone go to their owner's trash.
void ChainManager::abandonChain(const std::string& why) {
    events_.logWarn("ChainManager: " + why + " — abandoning " +
                    std::to_string(state_.chain.items.size()) + " chain item(s)");
    for (const auto& item : state_.chain.items) {
        if (!item.is_spell && !item.is_permanent) continue;
        if (!state_.objectExists(item.source)) continue;
        auto& obj = state_.getObject(item.source);
        if (obj.zone != ZoneType::Chain) continue;
        obj.zone = ZoneType::Trash;
        obj.location = std::nullopt;
        const PlayerId home = obj.owner != PlayerId::None ? obj.owner : item.controller;
        if (!obj.isToken()) {
            state_.player(home).trash.push_back(item.source);
            settleInTrash(state_, events_, item.source);
        }
    }
    state_.chain.items.clear();
    state_.chain.resuming.reset();
    if (executor_) {
        if (executor_->hasPendingChoice()) executor_->consumePendingChoice();
        executor_->takeChoice();
    }
    state_.turn.oc_state = OpenClosedState::Open;
    events_.emit(ChainEmptiedEvent{});
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
    // (CR 820.2.a); they are asked here, as the repeat executes, rather than
    // in the Make-Relevant-Choices step (the simplification noted on
    // ChainItem::repeats_paid). Play-time `targets` are reused.
    if (state_.chain.resuming.has_value()) {
        const int repeats = state_.chain.resuming->repeats_paid;
        for (int r = 0; r < repeats && state_.chain.resuming.has_value(); ++r) {
            auto& ri = *state_.chain.resuming;
            ri.resume_point = 0;
            ri.resume_data.clear();
            ri.may_answer = -1;
            ri.choices_finalized = false;
            ri.finalize_costs_paid = false;
            ri.finalize_checked = 0;
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
        events_.logTrace("CHAIN: " + state_.getObject(resolved.source).name +
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
