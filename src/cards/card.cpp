#include "cards/card.h"
#include "core/game_state.h"
#include "core/intent.h"
#include "engine/effect_executor.h"
#include "core/events.h"
#include "cards/card_helpers.h"
#include "engine/resource_payment.h"
#include <algorithm>

namespace riftbound {

// A cost is due exactly once per chain item, on every path.
//
// The end-of-choices marker (finalizingChoices) is also the end of the card's
// cost section: once a run of the callback reaches it after costDueNow() said
// "due", the cost counts as paid. Until then it is still owed — a card that
// asks which unit to kill inside its cost section is re-entered with the cost
// still due, and a payment that fails (the card returns early) is owed again
// later. After it, costDueNow() is false however often a later prompt brings
// the card back in: in the rest of the finalize pass, at resolution after a
// finalize pass that paid, and at resolution when no finalize pass ran at all
// (a card that does not opt in, an item an effect put on the chain).
//
// finalize_checked bit 4: a cost was asked for during the finalize pass.
// finalize_checked bit 16: a cost was asked for while resolving.
// finalize_checked bit 8: that cost was paid while resolving (marker reached).
// WHERE A FINALIZE PASS MAY END (one rule, checked by the engine in
// GameEngine::collectFinalizeChoices — a pass that ends anywhere else is a
// std::logic_error naming the card):
//   1. at this marker; or
//   2. at a return taken because a choice helper said there is nothing to
//      decide and stored that as the answer: a "may" that was declined, not
//      available, or no longer possible (confirmOptional -> 0), no legal mode
//      (pickMode -> -2), nothing to choose (pickTarget / pickTargetPair /
//      selectObject -> kInvalidId, with an empty list; splitDamageAmong
//      with no unit to target). The stored answer
//      takes the card down the same path as it resolves, so nothing is asked
//      then.
// A return for any other reason — a state test of the card's own ("am I still
// in the trash?") — leaves nothing stored: if the test comes out differently
// as the item resolves, the card goes on and its helpers ask then, after
// everyone has had the chance to respond. Such a test belongs after the
// marker, or inside a helper's still_legal.
bool Card::finalizingChoices(const CardContext& ctx) {
    auto& r = ctx.state.chain.resuming;
    if (!r.has_value()) return false;
    if (!r->finalizing) {
        if (r->finalize_checked & 16) r->finalize_checked |= 8;
        return false;
    }
    r->finalize_checked |= 64;   // the end-of-choices marker was reached in this pass
    if (r->finalize_checked & 4) r->finalize_costs_paid = true;
    return true;
}

bool Card::costDueNow(CardContext& ctx) {
    auto& r = ctx.state.chain.resuming;
    if (!r.has_value()) return true;
    if (r->finalizing) {
        if (r->finalize_costs_paid) return false;   // paid earlier in this pass
        if (!(r->finalize_checked & 32)) {
            // First "due" of this run: remember the state, so the engine can
            // tell whether the card went on to pay before asking a question.
            r->finalize_checked |= 32;
            r->cost_due_fingerprint = costStateFingerprint(ctx.state);
        }
        r->finalize_checked |= 4;
        return true;
    }
    if (r->choices_finalized && r->finalize_costs_paid) return false;
    if (r->finalize_checked & 8) return false;
    r->finalize_checked |= 16;
    return true;
}

bool Card::hasLegalChoicesHiddenAt(const GameState& state, PlayerId controller,
                                   const CardDB& db, BattlefieldId /*battlefield*/) const {
    return hasLegalTargets(state, controller, db);
}

void Card::forgetPlayPayments(GameState& state, const Card* card, PlayerId player,
                              GameObjectId object) {
    if (!state.objectExists(object)) return;
    auto& counters = state.getObject(object).card_counters;
    counters.erase(kAdditionalCostPaid);
    // Whether [Accelerate] was decided / paid for a play, and from where the
    // play came (GameEngine::decideAccelerate): per-play flags, dropped as a
    // play begins. (Whether a card on the board gives the unit [Accelerate]
    // is judged for each play, GameEngine::accelerateAvailable.)
    counters.erase(kAccelerateDecided);
    counters.erase(kAccelerateFromHand);
    // "The first time I ... each turn" belongs to the object's life on the
    // board: played again, it is a new object (CR 124.1).
    if (card)
        for (TriggerType t : card->triggerTypes())
            if (card->firstTimeEachTurn(t)) counters.erase(firstTimeKey(t));
    // "Played from facedown" belongs to one play too (GameEngine::
    // executePlayFromHidden stamps it right after this): a card returned and
    // played again the same turn is not bound to that battlefield (CR
    // 811.1.d) and is not "played from face down".
    counters.erase("__played_from_facedown");
    counters.erase("__facedown_play_bf");
    counters.erase("__facedown_play_turn");
    if (!card) return;
    for (const auto& oac : {card->optionalAdditionalCost(),
                            card->optionalAdditionalCost(state, player)})
        if (oac.paid_flag && oac.paid_flag[0] != '\0') counters.erase(oac.paid_flag);
}

uint64_t Card::costStateFingerprint(const GameState& state) {
    uint64_t h = 1469598103934665603ull;
    auto mix = [&h](uint64_t v) { h = (h ^ v) * 1099511628211ull; };
    // What a cost can change, field by field.
    //   PlayerState — digested: the sizes of hand, trash, banishment, Main
    //   Deck and Rune Deck (discard, recycle, banish, draw); XP; score; every
    //   resource pool, each on its own; the Champion Zone. Not digested: the
    //   per-turn tallies (cards_played_this_turn, power_spent_this_turn, ...)
    //   and the continuous-effect flags — they record or follow a change that
    //   is already digested, and are never themselves what is paid.
    //   BattlefieldState — digested: who controls it and how many cards are
    //   facedown there. Not digested: combat / showdown progress and the aura
    //   fields, which no cost pays with.
    //   GameObject — digested: zone, location (which base or battlefield),
    //   controller, exhausted, stunned, buffs, damage, Empowered, what it is
    //   attached to. Not digested: card_counters — cards and the choice
    //   helpers write progress there while choices are being made, and a
    //   digest that moved with them would call bookkeeping a payment; no
    //   printed cost is paid by setting one. Might, keywords and aura fields
    //   are derived from what is digested.
    for (const auto& ps : state.players) {
        mix(ps.hand.size()); mix(ps.trash.size()); mix(ps.banishment.size());
        mix(ps.main_deck.size()); mix(ps.rune_deck.size());
        mix(static_cast<uint64_t>(ps.xp)); mix(static_cast<uint64_t>(ps.score));
        mix(static_cast<uint64_t>(ps.champion_zone.raw()));
        const auto& pool = ps.rune_pool;
        mix(static_cast<uint64_t>(pool.energy));
        mix(static_cast<uint64_t>(pool.universal_power));
        for (int d = 0; d < static_cast<int>(Domain::Count); ++d)
            mix(static_cast<uint64_t>(pool.power[d]));
        mix(static_cast<uint64_t>(pool.unit_energy));
        mix(static_cast<uint64_t>(pool.spell_energy));
        mix(static_cast<uint64_t>(pool.showdown_energy));
        mix(static_cast<uint64_t>(pool.spell_power));
        mix(static_cast<uint64_t>(pool.gear_power));
    }
    for (const auto& bf : state.battlefields) {
        mix(static_cast<uint64_t>(bf.id.raw()));
        mix(bf.controller.has_value() ? static_cast<uint64_t>(*bf.controller) + 1 : 0u);
        mix(bf.facedown.size());
    }
    // Order-independent over the object map.
    uint64_t objects = 0;
    // (Every object: the fingerprint digests cards in every zone.)
    for (const auto& [id, obj] : state.objects) {
        uint64_t o = 1469598103934665603ull;
        auto omix = [&o](uint64_t v) { o = (o ^ v) * 1099511628211ull; };
        omix(static_cast<uint64_t>(id.raw()));
        omix(static_cast<uint64_t>(obj.zone.get()));
        if (!obj.location.has_value()) omix(0u);
        else if (const auto* base = std::get_if<BaseLocation>(&*obj.location))
            omix(1u + static_cast<uint64_t>(base->player));
        else
            omix(1000u + static_cast<uint64_t>((std::get<BattlefieldLocation>(*obj.location).id).raw()));
        omix(static_cast<uint64_t>(obj.controller));
        omix(obj.is_stunned ? 1u : 0u);
        omix(obj.is_exhausted ? 1u : 0u);
        omix(static_cast<uint64_t>(obj.buff_count));
        omix(static_cast<uint64_t>(obj.damage_marked));
        omix(static_cast<uint64_t>(obj.empowered));
        omix(obj.attached_to.has_value() ? static_cast<uint64_t>((*obj.attached_to).raw()) + 1 : 0u);
        objects += o;
    }
    mix(objects);
    return h;
}

bool Card::costAlreadyPaid(const CardContext& ctx) {
    const auto& r = ctx.state.chain.resuming;
    if (!r.has_value()) return false;
    if (r->finalizing) return r->finalize_costs_paid;
    return (r->choices_finalized && r->finalize_costs_paid) || (r->finalize_checked & 8);
}

void Card::nothingToDecide(ChainItem& item) {
    if (item.finalizing) item.finalize_checked |= 128;
}

void Card::mayIsNotTheWholeAbility(CardContext& ctx) {
    if (ctx.state.chain.resuming.has_value()) ctx.state.chain.resuming->may_is_partial = true;
}

void Card::requiredChoiceMissing(ChainItem& item) {
    if (item.finalizing) item.required_choice_missing = true;
}

// ── Choice-helper progress ──────────────────────────────────────────────────
// Each helper keeps its own "waiting for the agent" / "answered" flags on the
// resolving item (resume_data[Card::kChoiceStateSlot]), so the helpers compose
// in any order within one callback. ChainItem::resume_point is still written
// with the documented per-helper values, but only so cards can tell a suspend
// from a fizzle; no helper infers another helper's progress from it.
// (confirmOptional's answer lives in ChainItem::may_answer and it stays off
// resume_data entirely: cards with a "may" gate use resume_data freely.)
namespace {

// Who may read a prompt line that lists `options` by name: both players when
// every option is somewhere both can see (the board, a trash, banishment, the
// chain); only the player being asked when one of them is a card in a hand, a
// deck or facedown (LogEvent::audience).
LogAudience promptAudience(const CardContext& ctx, const std::vector<GameObjectId>& options) {
    for (auto id : options) {
        if (!ctx.state.objectExists(id)) continue;
        const auto& o = ctx.state.getObject(id);
        if (o.is_hidden || o.zone == ZoneType::Hand || o.zone == ZoneType::MainDeck ||
            o.zone == ZoneType::RuneDeck || o.zone == ZoneType::FacedownZone)
            return LogAudience::only(ctx.controller);
    }
    return LogAudience::both();
}

}  // namespace

namespace {
enum : uint32_t {
    kXAwait = 1u << 0,     kXDone = 1u << 1,
    kModeAwait = 1u << 2,  kModeDone = 1u << 3,
    kTgtAwait = 1u << 4,   kTgtDone = 1u << 5,
    kPairAwaitA = 1u << 6, kPairAwaitB = 1u << 7, kPairDone = 1u << 8,
    kCostUnpaid = 1u << 9,   // Card::markCostUnpaid
};
uint32_t choiceState(const ChainItem& ri) {
    return ri.resume_data.size() > Card::kChoiceStateSlot
        ? static_cast<uint32_t>(ri.resume_data[Card::kChoiceStateSlot]) : 0u;
}
void setChoiceState(ChainItem& ri, uint32_t clear, uint32_t set) {
    while (ri.resume_data.size() <= Card::kChoiceStateSlot) ri.resume_data.push_back(0);
    auto& slot = ri.resume_data[Card::kChoiceStateSlot];
    slot = static_cast<int32_t>((static_cast<uint32_t>(slot) & ~clear) | set);
}
}  // namespace

void Card::markCostUnpaid(CardContext& ctx) {
    auto& r = ctx.state.chain.resuming;
    if (r.has_value()) setChoiceState(*r, 0, kCostUnpaid);
}

bool Card::costUnpaid(const CardContext& ctx) {
    const auto& r = ctx.state.chain.resuming;
    return r.has_value() && (choiceState(*r) & kCostUnpaid);
}

bool Card::killAsCost(CardContext& ctx, GameObjectId object) const {
    bool died = false;
    if (ctx.state.objectExists(object) && ctx.state.getObject(object).location.has_value()) {
        // The death itself is what counts, not where the object is afterwards:
        // EffectExecutor::killObject announces a death as a LeftBoardEvent
        // with was_killed, and a replacement ("banish it instead", "return it
        // to hand instead", "it stays") announces something else or nothing.
        boost::signals2::scoped_connection watch = ctx.events.on_left_board.connect(
            [&died, object](const LeftBoardEvent& e) {
                if (e.object == object && e.was_killed) died = true;
            });
        ctx.executor.killObject(object);
    }
    if (!died) markCostUnpaid(ctx);
    return died;
}

int Card::confirmOptional(CardContext& ctx, const std::string& label,
                           const std::function<bool()>& still_legal) {
    auto& ri_opt = ctx.state.chain.resuming;
    if (!ri_opt.has_value()) {
        // Card invoked outside the chain (test/direct invocation). The
        // resumable yes/no path can't be driven here — default to YES if
        // the precondition holds. Tests that want to exercise the
        // "no"-branch must use the chain-driven path via
        // CardTestFixture::driveResumableTrigger (which sets resuming).
        return still_legal() ? 1 : 0;
    }
    auto& ri = *ri_opt;
    // Waiting for the yes/no published below (pickXAmount shares resume_point 1).
    const bool awaiting = ri.may_answer == -1 && ri.resume_point == 1 &&
                          !(choiceState(ri) & kXAwait);

    // Tournament Rules 505: a loop players keep going by their decisions — two
    // "you may" abilities answering each other (Imposing Challenger and
    // Irresistible Faefolk moving each other, finding F8) — runs for a number
    // of iterations and then "that player ... breaks the loop" by "making a
    // different decision" (505.4, 505.7, 505.10). The players name that
    // number at a table; here it is ChainState::kMayLoopIterations for every
    // triggered ability: once its "may" has been accepted that often in one
    // unbroken chain it is not offered again, which is the decline.
    const bool counts_for_loops = ri.is_ability && !ri.is_activated_ability;
    auto mayCount = [&]() -> ChainState::MayCount* {
        for (auto& mc : ctx.state.chain.may_loop_counts)
            if (mc.source == ri.source && mc.trigger == ri.fired_trigger) return &mc;
        return nullptr;
    };
    if (ri.may_answer == -1 && !awaiting && counts_for_loops) {
        if (const auto* mc = mayCount();
            mc && mc->accepted >= ChainState::kMayLoopIterations) {
            RIFT_TRACE(ctx.events, "MAY_NOT_OFFERED: " + label + " (a loop: accepted " +
                                 std::to_string(mc->accepted) +
                                 " times in this chain — Tournament Rules 505.7)");
            ri.resume_point = 2;
            ri.may_answer = 0;
            nothingToDecide(ri);
            return 0;
        }
    }

    if (ri.may_answer == -1 && !awaiting) {
        // Validate-before-prompt. CR principle: never offer a choice that
        // can't legally be acted on. If preconditions failed (e.g. the
        // spell to banish isn't in trash anymore), skip silently.
        // The trace tag distinguishes "skipped because illegal" from
        // "agent declined" — important for V&V.
        if (!still_legal()) {
            RIFT_TRACE(ctx.events, "MAY_NOT_OFFERED: " + label +
                                 " (precondition failed at prompt time)");
            ri.resume_point = 2;  // mark "done, do not re-enter"
            ri.may_answer = 0;
            nothingToDecide(ri);
            return 0;
        }
        // Publish a 2-option choice via Intent::chosen_value (no = 0, yes = 1).
        //
        // The action_vocab encoder reserves a slot per chosen_value, so no
        // and yes encode to DISTINCT OpenSpiel actions — the policy head can
        // express preference and the random agent picks ~50/50. The earlier
        // encoding stuffed a sentinel GameObjectId into chosen_objects which
        // collided with slot 0 (empty chosen_objects) in the encoder; result
        // was OpenSpiel deduping the two intents and never showing "yes" to
        // any agent. Caused 100% MAY_DECLINED on Virtuoso. See
        // action_vocab.h "kNumIntChoices" + comment block.
        Intent no_choice;
        no_choice.type = IntentType::MakeChoice;
        no_choice.player = ctx.controller;
        no_choice.chosen_value = 0;
        no_choice.declines = true;
        no_choice.choice_label = "No — " + label;
        Intent yes_choice;
        yes_choice.type = IntentType::MakeChoice;
        yes_choice.player = ctx.controller;
        yes_choice.chosen_value = 1;
        yes_choice.choice_label = "Yes — " + label;

        RIFT_TRACE(ctx.events, "MAY_PROMPT: " + label +
                             " (P=" + std::string(toString(ctx.controller)) +
                             ", deferred to agent)");
        ctx.executor.requestChoice(ctx.controller,
                                    {no_choice, yes_choice},
                                    "may: " + label);
        ri.resume_point = 1;
        return -1;  // caller returns, chain manager re-enters after agent
    }

    if (awaiting) {
        auto picked = ctx.executor.takeChoice();
        // Yes = chosen_value == 1 (set explicitly by yes_choice above).
        // Anything else (no_choice with chosen_value=0, no recorded choice,
        // legacy intents without chosen_value) reads as a decline.
        bool said_yes = picked.has_value() &&
                        picked->chosen_value.has_value() &&
                        *picked->chosen_value == 1;
        if (!said_yes) {
            RIFT_TRACE(ctx.events, "MAY_DECLINED: " + label);
            ri.resume_point = 2;
            ri.may_answer = 0;
            nothingToDecide(ri);
            return 0;
        }
        // Re-validate at use time. State may have shifted while the
        // agent decided — if the precondition no longer holds, the
        // "yes" can't be honored. CR-correct silent-fail behavior.
        if (!still_legal()) {
            RIFT_TRACE(ctx.events, "MAY_INVALIDATED: " + label +
                                 " (state shifted between agent yes "
                                 "and execution; silent fail)");
            ri.resume_point = 2;
            ri.may_answer = 0;
            nothingToDecide(ri);
            return 0;
        }
        RIFT_TRACE(ctx.events, "MAY_ACCEPTED: " + label);
        ri.resume_point = 2;  // commit; caller proceeds to do the work
        ri.may_answer = 1;
        if (counts_for_loops) {
            if (auto* mc = mayCount()) ++mc->accepted;
            else ctx.state.chain.may_loop_counts.push_back({ri.source, ri.fired_trigger, 1});
        }
        return 1;
    }

    // Already answered (whatever other helpers have done to resume_point
    // since): the card is in its post-confirmation work. Return the answer so
    // the caller re-enters its execute branch on subsequent loop iterations
    // (in case the card itself requests further choices and is re-entered).
    if (ri.may_answer == 0) return 0;
    // Answered at finalization: the effect happens only if it can still be
    // done as the ability resolves. (Once a cost has been paid the
    // precondition, which usually includes affording it, no longer applies.)
    if (ri.may_answer == 1 && ri.choices_finalized && !ri.finalizing) {
        if (!ri.finalize_costs_paid && !still_legal()) {
            RIFT_TRACE(ctx.events, "MAY_INVALIDATED: " + label + " (no longer possible at resolution)");
            ri.may_answer = 0;
            nothingToDecide(ri);
            return 0;
        }
        ri.may_answer = 2;
    }
    return 1;
}

int Card::pickXAmount(CardContext& ctx, const std::string& label,
                       int min, int max) {
    auto& ri_opt = ctx.state.chain.resuming;
    if (!ri_opt.has_value()) {
        // Direct invocation (no chain) — default to max for test/legacy
        // paths. Real-game flow goes through the chain so this branch
        // only hits in test scenarios that bypass driveResumable.
        return max;
    }
    auto& ri = *ri_opt;
    const uint32_t st = choiceState(ri);
    auto store = [&ri](int x) {
        if (ri.resume_data.empty()) ri.resume_data.push_back(0);
        ri.resume_data[0] = x;
        setChoiceState(ri, kXAwait, kXDone);
        ri.resume_point = 2;
    };

    // Already chosen: X is stashed in resume_data[0].
    if (st & kXDone) return ri.resume_data.empty() ? min : ri.resume_data[0];

    if (!(st & kXAwait)) {
        if (max < min) {
            RIFT_TRACE(ctx.events, "X_NOT_OFFERED: " + label +
                                 " (max=" + std::to_string(max) +
                                 " below min=" + std::to_string(min) +
                                 ")");
            store(min);
            return min;
        }
        // Publish (max - min + 1) MakeChoice intents, one per legal X.
        // Encode X via Intent::chosen_value — each X gets its OWN
        // OpenSpiel action slot (action_vocab reserves a kNumIntChoices-
        // wide range for int-coded MakeChoice answers), so MCTS /
        // AlphaZero can express preference between X values.
        std::vector<Intent> options;
        for (int x = min; x <= max; ++x) {
            Intent intent;
            intent.type = IntentType::MakeChoice;
            intent.player = ctx.controller;
            intent.chosen_value = x;
            intent.choice_label = label + ": " + std::to_string(x);
            options.push_back(intent);
        }
        RIFT_TRACE(ctx.events, "X_PROMPT: " + label + " (range " +
                             std::to_string(min) + ".." +
                             std::to_string(max) + ", " +
                             std::to_string(options.size()) +
                             " options) [agent decides]");
        ctx.executor.requestChoice(ctx.controller, std::move(options),
                                    "X: " + label);
        setChoiceState(ri, 0, kXAwait);
        ri.resume_point = 1;
        return -1;
    }

    auto picked = ctx.executor.takeChoice();
    int x = min;
    if (picked.has_value() && picked->chosen_value.has_value()) {
        x = *picked->chosen_value;
    }
    if (x < min) x = min;
    if (x > max) x = max;
    RIFT_TRACE(ctx.events, "X_PICKED: " + label + " = " +
                         std::to_string(x));
    store(x);
    return x;
}

void Card::settleMode(CardContext& ctx, int mode) {
    auto& ri_opt = ctx.state.chain.resuming;
    if (!ri_opt.has_value()) return;
    auto& ri = *ri_opt;
    if (choiceState(ri) & (kModeAwait | kModeDone)) return;
    while (ri.resume_data.size() < 2) ri.resume_data.push_back(0);
    ri.resume_data[1] = mode;
    setChoiceState(ri, kModeAwait, kModeDone);
}

int Card::pickMode(CardContext& ctx, const std::string& label, int num_modes,
                    const std::vector<std::string>& mode_labels,
                    uint32_t legal_modes) {
    auto& ri_opt = ctx.state.chain.resuming;

    // Build the legal-mode index list once. The bitmask filters out
    // modes that don't apply right now (e.g. damage modes when no
    // legal target is present).
    std::vector<int> legal;
    legal.reserve(num_modes);
    for (int m = 0; m < num_modes && m < 32; ++m) {
        if (legal_modes & (1u << m)) legal.push_back(m);
    }

    if (!ri_opt.has_value()) {
        // Direct invocation (no chain). Default to legal[0] or -2.
        if (legal.empty()) return -2;
        return legal.front();
    }
    auto& ri = *ri_opt;

    // resume_point is left at 4 (waiting for the agent) / 5 (answered) for
    // the card to read; the mode itself is kept in resume_data[1].
    const uint32_t st = choiceState(ri);
    auto store = [&ri](int m) {
        while (ri.resume_data.size() < 2) ri.resume_data.push_back(0);
        ri.resume_data[1] = m;
        setChoiceState(ri, kModeAwait, kModeDone);
        ri.resume_point = 5;
    };

    // Already chosen: the mode is in resume_data[1].
    if (st & kModeDone)
        return ri.resume_data.size() >= 2 ? ri.resume_data[1]
                                          : (legal.empty() ? -2 : legal.front());

    if (!(st & kModeAwait)) {
        if (legal.empty()) {
            RIFT_TRACE(ctx.events, "MODE_NOT_OFFERED: " + label +
                                 " (no legal modes after filtering)");
            store(-2);
            nothingToDecide(ri);
            requiredChoiceMissing(ri);   // CR 403.4
            return -2;
        }
        // Always publish a choice, even if only one mode is legal. The
        // agent still records the decision (matches the engine-wide
        // rule that every decision point should surface to the agent
        // for training fidelity — see cost-payment loop, pickTarget).
        std::vector<Intent> options;
        // Built for the MODE_PROMPT line only: not when nobody reads the log.
        const bool logging = ctx.events.logging();
        std::string label_summary;
        for (int m : legal) {
            Intent i;
            i.type = IntentType::MakeChoice;
            i.player = ctx.controller;
            // Encode the mode index via chosen_value — each mode gets its
            // own action_vocab slot. Previously this stuffed (m+1) into
            // chosen_objects as a fake GameObjectId, which collided with
            // real objects whose CardDefId happened to share that slot
            // (e.g. obj=1 = Bounty Hunter), polluting the policy head's
            // view of what each slot represents.
            i.chosen_value = m;
            std::string mode_name = (m < static_cast<int>(mode_labels.size()))
                ? mode_labels[m]
                : ("mode#" + std::to_string(m));
            i.choice_label = mode_name;
            options.push_back(i);
            if (!logging) continue;
            if (!label_summary.empty()) label_summary += "/";
            label_summary += mode_name;
        }
        RIFT_TRACE(ctx.events, "MODE_PROMPT: " + label + " [" + label_summary +
                             "] (agent decides among " +
                             std::to_string(legal.size()) + " modes)");
        ctx.executor.requestChoice(ctx.controller, std::move(options),
                                    "mode: " + label);
        setChoiceState(ri, 0, kModeAwait);
        ri.resume_point = 4;
        return -1;
    }

    {
        auto picked = ctx.executor.takeChoice();
        int m = legal.empty() ? 0 : legal.front();
        if (picked.has_value() && picked->chosen_value.has_value()) {
            m = *picked->chosen_value;
        }
        if (m < 0) m = 0;
        if (m >= num_modes) m = num_modes - 1;
        RIFT_TRACE(ctx.events, "MODE_PICKED: " + label + " = mode#" +
                             std::to_string(m) +
                             (m < static_cast<int>(mode_labels.size())
                                ? " (" + mode_labels[m] + ")" : ""));
        store(m);
        return m;
    }
}

// ── Deflect (CR 809) at choice time ─────────────────────────────────────────
// "Spells and abilities an opponent controls that target [me] cost an amount of
// Power equal to [Deflect Value] more ... for each time they choose [me]." This
// engine makes most choices as the spell or ability resolves, so the mandatory
// additional cost is charged at the moment of the choice: an object whose
// Deflect cost the chooser can't pay isn't offered, and picking one pays it.
namespace {
std::vector<GameObjectId> affordableWithDeflect(CardContext& ctx, const Card& card,
                                                const std::vector<GameObjectId>& in) {
    const int payable = ctx.executor.payableAnyPower(ctx.controller);
    const bool ignores = card.ignoresDeflect();
    std::vector<GameObjectId> out;
    out.reserve(in.size());
    for (auto t : in) {
        // A spell choosing before it is paid for: only what leaves it payable.
        if (!ctx.executor.choiceAllowed(t)) continue;
        if (ignores || ctx.executor.deflectCostFor(ctx.controller, t) <= payable)
            out.push_back(t);
    }
    return out;
}

// Pays the Deflect cost for choosing `t` (if any) and records the choice on
// the resolving chain item.
void commitChoice(CardContext& ctx, const Card& card, GameObjectId t) {
    if (t == kInvalidId || !ctx.state.objectExists(t)) return;
    if (!card.ignoresDeflect()) {
        const int cost = ctx.executor.deflectCostFor(ctx.controller, t);
        if (cost > 0) {
            ctx.executor.payAnyPower(ctx.controller, cost);
            RIFT_TRACE(ctx.events, "DEFLECT: " + std::string(toString(ctx.controller)) +
                                " pays " + std::to_string(cost) + " [A] to choose " +
                                ctx.state.getObject(t).name);
        }
    }
    if (ctx.state.chain.resuming.has_value())
        ctx.state.chain.resuming->chosen_objects.push_back(t);
    // "If you've chosen an enemy unit this turn" (Hungry Wolf). A unit is a
    // unit on the board (CR 141.1.a): a unit card picked out of a hand, deck
    // or trash is a card, not an enemy unit.
    const auto& tobj = ctx.state.getObject(t);
    ctx.state.noteChosen(ctx.controller, t,
        ctx.state.chain.resuming.has_value() ? &*ctx.state.chain.resuming : nullptr);
    // Announced once the item is on the chain when the choice is made before
    // that (GameEngine::chooseTargetsAtFinalize), so abilities that trigger on
    // it still go above the spell.
    const bool held = ctx.state.chain.resuming.has_value() &&
                      ctx.state.chain.resuming->pre_play_choices;
    if (!held && tobj.isUnit() && tobj.location.has_value() && tobj.controller == ctx.controller)
        ctx.events.emit(ObjectStateChangedEvent{t, "chosen_by_controller"});
}
}  // namespace

std::vector<GameObjectId> Card::offerableChoices(
    CardContext& ctx, const std::vector<GameObjectId>& candidates) const {
    return affordableWithDeflect(ctx, *this, candidates);
}

void Card::commitChosenObject(CardContext& ctx, GameObjectId chosen) const {
    commitChoice(ctx, *this, chosen);
}

GameObjectId Card::pickTarget(CardContext& ctx, const std::string& label,
                                const std::vector<GameObjectId>& legal_targets_in) {
    auto& ri_opt = ctx.state.chain.resuming;
    // Once a choice is locked in the list is not consulted again, so
    // filtering here can't un-offer an already-paid-for target.
    const std::vector<GameObjectId> legal_targets =
        (ri_opt.has_value() && !(choiceState(*ri_opt) & kTgtDone))
            ? affordableWithDeflect(ctx, *this, legal_targets_in) : legal_targets_in;

    if (!ri_opt.has_value()) {
        // Direct invocation (test / no chain). Default to first legal
        // target or kInvalidId. Real-game flow goes through the chain
        // so this branch only hits in tests that bypass the resume
        // machinery.
        return legal_targets.empty() ? kInvalidId : legal_targets.front();
    }
    auto& ri = *ri_opt;

    // resume_point is left at 7 (waiting for the agent) / 8 (answered) for
    // the card to read; the picked id is kept in resume_data[2].
    const uint32_t st = choiceState(ri);
    auto store = [&ri](GameObjectId t) {
        while (ri.resume_data.size() < 3) ri.resume_data.push_back(0);
        ri.resume_data[2] = static_cast<int32_t>(t.raw());
        setChoiceState(ri, kTgtAwait, kTgtDone);
        ri.resume_point = 8;
    };

    // Already chosen: the target is in resume_data[2].
    if (st & kTgtDone) {
        if (ri.resume_data.size() < 3)
            return legal_targets.empty() ? kInvalidId : legal_targets.front();
        // Chosen in the finalize pass: checked once as the item resolves. A
        // target that is no longer legal is a mistarget (CR 359.3.e.6).
        if (ri.choices_finalized && !ri.finalizing && !(ri.finalize_checked & 1)) {
            ri.finalize_checked |= 1;
            const auto t = GameObjectId::fromRaw(ri.resume_data[2]);
            if (t != kInvalidId &&
                std::find(legal_targets_in.begin(), legal_targets_in.end(), t) == legal_targets_in.end()) {
                RIFT_TRACE(ctx.events, "TGT_MISTARGET: " + label + " — the chosen target is no longer legal");
                ri.resume_data[2] = static_cast<int32_t>(kInvalidId.raw());
            }
        }
        return GameObjectId::fromRaw(ri.resume_data[2]);
    }

    if (!(st & kTgtAwait)) {
        // The target was already chosen when this item was finalized (CR 355;
        // GameEngine::chooseTargetsAtFinalize). Deflect was paid then. If it is
        // no longer a legal target the instruction is a mistarget and is
        // ignored (CR 359.3.e.6) — the caller sees "no target".
        if (ri.resume_data.size() > 5 && ri.resume_data[5] == 1 && !ri.targets.empty()) {
            ri.resume_data[5] = 0;
            GameObjectId t = ri.targets.front();
            const bool still_legal =
                std::find(legal_targets_in.begin(), legal_targets_in.end(), t) != legal_targets_in.end();
            if (!still_legal) {
                RIFT_TRACE(ctx.events, "TGT_MISTARGET: " + label + " — the chosen target is no longer legal");
                t = kInvalidId;
            }
            store(t);
            return t;
        }
        if (legal_targets.empty()) {
            RIFT_TRACE(ctx.events, "TGT_NOT_OFFERED: " + label +
                                 " (no legal targets at resolve time)");
            store(kInvalidId);
            nothingToDecide(ri);
            requiredChoiceMissing(ri);   // CR 403.4
            return kInvalidId;
        }
        // Always publish a choice, even if only one target is legal —
        // the agent still records the decision. (See pickMode + the
        // cost-payment loop for the same rule. Auto-picking single
        // targets was hiding meaningful decision points like Bounty
        // Hunter's Ganking grant when only one unit was on board.)
        // Publish one MakeChoice per legal target. action_vocab keys
        // MakeChoice slots by chosen_objects[0]'s card_def_id (Phase
        // 5g), so the policy head sees one slot per distinct
        // target-card-type. Two friendly Bounty Hunters alias to the
        // Bounty Hunter slot — known limitation, same as discard /
        // predict picks today.
        std::vector<Intent> options;
        options.reserve(legal_targets.size());
        // Built for the TGT_PROMPT line only: not when nobody reads the log.
        const bool logging = ctx.events.logging();
        std::string label_summary;
        for (auto t : legal_targets) {
            Intent i;
            i.type = IntentType::MakeChoice;
            i.player = ctx.controller;
            i.chosen_objects = { t };
            options.push_back(std::move(i));
            if (!logging) continue;
            if (!label_summary.empty()) label_summary += "/";
            label_summary += ctx.state.objectExists(t)
                ? ctx.state.getObject(t).name
                : std::string("?");
        }
        RIFT_TRACE_FOR(ctx.events, promptAudience(ctx, legal_targets),
                             "TGT_PROMPT: " + label + " [" + label_summary +
                             "] (agent decides among " +
                             std::to_string(legal_targets.size()) +
                             " targets)");
        ctx.executor.requestChoice(ctx.controller, std::move(options),
                                    "target: " + label);
        setChoiceState(ri, 0, kTgtAwait);
        ri.resume_point = 7;
        return kInvalidId;
    }

    auto picked = ctx.executor.takeChoice();
    GameObjectId t = legal_targets.empty()
        ? kInvalidId : legal_targets.front();
    if (picked.has_value() && !picked->chosen_objects.empty()) {
        t = picked->chosen_objects.front();
    }
    // Only for who may know the card when it is hidden (testing phase D11).
    RIFT_TRACE_FOR(ctx.events, audienceNaming(ctx.state, ctx.controller, {t}),
                   "TGT_PICKED: " + label + " = " +
                       (ctx.state.objectExists(t) ? ctx.state.getObject(t).name
                                                  : std::string("?")) +
                       " (id=" + toString(t) + ")");
    commitChoice(ctx, *this, t);
    store(t);
    return t;
}

std::pair<GameObjectId, GameObjectId> Card::pickTargetPair(
    CardContext& ctx, const std::string& label,
    const std::vector<GameObjectId>& legal_a_in,
    const std::function<std::vector<GameObjectId>(GameObjectId)>& legal_b_in_fn) {

    auto& ri_opt = ctx.state.chain.resuming;
    // Deflect (CR 809): see affordableWithDeflect / commitChoice above.
    const bool choosing = ri_opt.has_value();
    const std::vector<GameObjectId> legal_a =
        choosing ? affordableWithDeflect(ctx, *this, legal_a_in) : legal_a_in;
    auto legal_b_fn = [&](GameObjectId a) {
        auto v = legal_b_in_fn(a);
        return choosing ? affordableWithDeflect(ctx, *this, v) : v;
    };
    if (!ri_opt.has_value()) {
        // Direct invocation (test / no chain). Default to first
        // legal pair or kInvalidId-pair.
        if (legal_a.empty()) return {kInvalidId, kInvalidId};
        auto a = legal_a.front();
        auto lb = legal_b_fn(a);
        if (lb.empty()) return {a, kInvalidId};
        return {a, lb.front()};
    }
    auto& ri = *ri_opt;

    // resume_point is left for the card to read at
    //   10 = waiting for the first target
    //   12 = waiting for the second target
    //   13 = done
    // resume_data[3] = picked first target id (int32 cast)
    // resume_data[4] = picked second target id (int32 cast)
    const uint32_t st = choiceState(ri);
    while (ri.resume_data.size() < 5) ri.resume_data.push_back(0);
    auto finish = [&ri](GameObjectId a, GameObjectId b) {
        ri.resume_data[3] = static_cast<int32_t>(a.raw());
        ri.resume_data[4] = static_cast<int32_t>(b.raw());
        if (a == kInvalidId || b == kInvalidId) nothingToDecide(ri);
        setChoiceState(ri, kPairAwaitA | kPairAwaitB, kPairDone);
        ri.resume_point = 13;
        return std::pair<GameObjectId, GameObjectId>{a, b};
    };
    auto publish = [&](const std::vector<GameObjectId>& legal, const char* tag,
                       const char* which, const std::string& request) {
        std::vector<Intent> options;
        options.reserve(legal.size());
        // Built for the prompt's trace line only: not when nobody reads the log.
        const bool logging = ctx.events.logging();
        std::string summary;
        for (auto t : legal) {
            Intent i;
            i.type = IntentType::MakeChoice;
            i.player = ctx.controller;
            i.chosen_objects = { t };
            options.push_back(std::move(i));
            if (!logging) continue;
            if (!summary.empty()) summary += "/";
            summary += ctx.state.objectExists(t)
                ? ctx.state.getObject(t).name : std::string("?");
        }
        RIFT_TRACE_FOR(ctx.events, promptAudience(ctx, legal),
                               std::string(tag) + label + " [" + summary + "] " + which);
        ctx.executor.requestChoice(ctx.controller, std::move(options), request + label);
    };

    if (st & kPairDone) {
        // Chosen in the finalize pass: each target is checked once as the item
        // resolves; one that is no longer legal is unaffected (CR 359.3.e.5).
        if (ri.choices_finalized && !ri.finalizing && !(ri.finalize_checked & 2)) {
            ri.finalize_checked |= 2;
            const auto a = GameObjectId::fromRaw(ri.resume_data[3]);
            const auto b = GameObjectId::fromRaw(ri.resume_data[4]);
            if (b != kInvalidId) {
                const auto lb = legal_b_in_fn(a);
                if (std::find(lb.begin(), lb.end(), b) == lb.end()) {
                    RIFT_TRACE(ctx.events, "TGT_MISTARGET: " + label + " — the second target is no longer legal");
                    ri.resume_data[4] = static_cast<int32_t>(kInvalidId.raw());
                }
            }
            if (a != kInvalidId &&
                std::find(legal_a_in.begin(), legal_a_in.end(), a) == legal_a_in.end()) {
                RIFT_TRACE(ctx.events, "TGT_MISTARGET: " + label + " — the first target is no longer legal");
                ri.resume_data[3] = static_cast<int32_t>(kInvalidId.raw());
            }
        }
        return {GameObjectId::fromRaw(ri.resume_data[3]),
                GameObjectId::fromRaw(ri.resume_data[4])};
    }

    // ── Step 1: publish first target ──
    if (!(st & (kPairAwaitA | kPairAwaitB))) {
        if (legal_a.empty()) {
            RIFT_TRACE(ctx.events, "TGT_PAIR_NOT_OFFERED: " + label +
                                 " (no legal first targets at resolve time)");
            requiredChoiceMissing(ri);   // CR 403.4
            return finish(kInvalidId, kInvalidId);
        }
        // Always publish a choice, even with a single legal first
        // target — the agent records the decision (engine-wide rule).
        publish(legal_a, "TGT_PAIR_PROMPT_A: ", "(agent picks 1st of 2)", "target A: ");
        setChoiceState(ri, 0, kPairAwaitA);
        ri.resume_point = 10;
        return {kInvalidId, kInvalidId};
    }

    if (st & kPairAwaitA) {
        auto picked = ctx.executor.takeChoice();
        GameObjectId a = legal_a.empty() ? kInvalidId : legal_a.front();
        if (picked.has_value() && !picked->chosen_objects.empty()) {
            a = picked->chosen_objects.front();
        }
        RIFT_TRACE(ctx.events, "TGT_PAIR_PICKED_A: " + label + " = " +
                             (ctx.state.objectExists(a) ? ctx.state.getObject(a).name
                                                        : std::string("?")) +
                             " (id=" + toString(a) + ")");
        commitChoice(ctx, *this, a);
        ri.resume_data[3] = static_cast<int32_t>(a.raw());

        // ── Step 2: publish second target, filtered by picked_a ──
        auto legal_b = legal_b_fn(a);
        if (legal_b.empty()) {
            RIFT_TRACE(ctx.events, "TGT_PAIR_NOT_OFFERED: " + label +
                                 " (no legal second targets after A picked)");
            return finish(a, kInvalidId);
        }
        // Always publish a choice, even with a single legal second
        // target — the agent records the decision (engine-wide rule).
        publish(legal_b, "TGT_PAIR_PROMPT_B: ", "(agent picks 2nd of 2)", "target B: ");
        setChoiceState(ri, kPairAwaitA, kPairAwaitB);
        ri.resume_point = 12;
        // Still suspended: the pair is not usable until both halves are in,
        // so a card that only tests for "nothing picked" can't act on the
        // first half now and again when it is re-entered.
        return {kInvalidId, kInvalidId};
    }

    // Waiting for the second target.
    GameObjectId a = GameObjectId::fromRaw(ri.resume_data[3]);
    auto picked = ctx.executor.takeChoice();
    auto legal_b = legal_b_fn(a);
    GameObjectId b = legal_b.empty() ? kInvalidId : legal_b.front();
    if (picked.has_value() && !picked->chosen_objects.empty()) {
        b = picked->chosen_objects.front();
    }
    RIFT_TRACE(ctx.events, "TGT_PAIR_PICKED_B: " + label + " = " +
                         (ctx.state.objectExists(b) ? ctx.state.getObject(b).name
                                                    : std::string("?")) +
                         " (id=" + toString(b) + ")");
    commitChoice(ctx, *this, b);
    return finish(a, b);
}

std::vector<GameObjectId> Card::enumerateLegalTargets(
    const GameState& state, PlayerId controller) const {

    auto reqs = getTargetRequirements();
    if (reqs.count == 0) return {};

    std::vector<GameObjectId> targets;
    // What `controller` could pay in [A] for Deflect: asked once, and only if
    // some candidate has a Deflect cost (it was asked per enemy candidate —
    // plans/06 item 6.5).
    std::optional<int> payable_power;

    for (auto& [id, obj] : state.objects.onBoard()) {
        // Must be on the board (has a location)
        if (!obj.location.has_value()) continue;

        // Type filter
        if (reqs.must_be_unit && !obj.isUnit()) continue;
        if (reqs.must_be_gear && !obj.isGear()) continue;

        // Ownership filter
        if (reqs.must_be_enemy && obj.controller == controller) continue;
        if (reqs.must_be_friendly && obj.controller != controller) continue;

        // Targeting protection (CR 700.x — "can't be chosen by enemy
        // spells and abilities"). Filtered when the candidate is an
        // enemy of `controller`. Friendly targeting still works. Flag
        // is populated by the engine during cleanup/aura-recalc from
        // each card's Card::canBeChosenByEnemy() override.
        if (obj.controller != controller && obj.untargetable_by_enemy) continue;

        // Location filter
        if (reqs.must_be_at_battlefield && !obj.isAtBattlefield()) continue;

        // Stat filter
        if (reqs.max_might > 0 && obj.current_might > reqs.max_might) continue;

        // CR 809: an opponent must be able to pay the Deflect cost to choose it.
        // Only when a choice is being OFFERED: while an item is resolving its
        // targets were already chosen and paid for (the engine re-validates
        // them through this function), and resolve-time picks do their own
        // affordability check in pickTarget.
        if (!state.chain.resuming.has_value() &&
            !ignoresDeflect() && obj.controller != controller) {
            const int deflect = deflectCostFor(state, controller, id);
            if (deflect > 0) {
                if (!payable_power.has_value())
                    payable_power = payableAnyPower(state, controller);
                if (deflect > *payable_power) continue;
            }
        }

        targets.push_back(id);
    }

    return targets;
}

bool Card::hasLegalTargets(const GameState& state,
                            PlayerId controller) const {
    auto reqs = getTargetRequirements();
    if (reqs.count == 0) return true;
    if (reqs.optional) return true;
    // Several targets locked in as the card is played (the generator offers
    // one intent per whole choice of them): there must be such a choice — a
    // spell that needs two units is not playable with one on the board.
    if (reqs.count >= 2 && !needsPlayTimeTarget() && !needsPlayTimeTargetPair()) {
        if (auto sets = enumerateLegalTargetSets(state, controller)) return !sets->empty();
        return static_cast<int>(enumerateLegalTargets(state, controller).size()) >= reqs.count;
    }
    return !enumerateLegalTargets(state, controller).empty();
}

GameObjectId Card::chosenTarget(const CardContext& ctx,
                                const std::vector<GameObjectId>& targets, size_t role) {
    // The slots as chosen are on the resolving chain item; `targets` is what
    // is left of them. (Called outside a resolution, `targets` is all there is.)
    const auto& chosen = ctx.state.chain.resuming.has_value()
        ? ctx.state.chain.resuming->targets : targets;
    if (role >= chosen.size()) return kInvalidId;
    const GameObjectId id = chosen[role];
    if (id == kInvalidId ||
        std::find(targets.begin(), targets.end(), id) == targets.end()) return kInvalidId;
    return id;
}

// Multi-ability default: wrap the legacy single-ability surface into a
// one-element vector when hasActivatedAbility() is true. Cards with N
// activated abilities override this directly. (Phase 6r)
ActivatedAbility Card::empowerAbility() const {
    ActivatedAbility ab;
    ab.cost = empowerSpec().cost;
    ab.is_empower = true;
    // "[Empower] — Kill a friendly unit": the unit to kill is the target.
    if (ab.cost.kill_friendly_unit)
        ab.targets = TargetRequirements{.count = 1, .must_be_unit = true,
                                        .must_be_friendly = true};
    return ab;
}

void Card::appendEmpowerAbilities(std::vector<ActivatedAbility>& out) const {
    const auto spec = empowerSpec();
    if (!spec.valid) return;
    out.push_back(empowerAbility());
    if (spec.has_alt_cost) {
        ActivatedAbility alt = empowerAbility();
        alt.cost = spec.alt_cost;
        out.push_back(alt);
    }
}

int Card::empowerAbilityIndex() const {
    const auto& abs = abilities();
    for (size_t i = 0; i < abs.size(); ++i)
        if (abs[i].is_empower) return static_cast<int>(i);
    return -1;
}

bool Card::canActivateAbilityAt(const GameState& state, PlayerId /*controller*/,
                                GameObjectId source, int ability_index) const {
    const auto& abs = abilities();
    if (ability_index < 0 || ability_index >= static_cast<int>(abs.size())) return true;
    if (!state.objectExists(source)) return false;
    const auto& obj = state.getObject(source);
    const auto& ab = abs[ability_index];
    // CR 827.1.c.1: "Play only if not Empowered". CR 827.1.c.5: an object
    // that may be Empowered multiple times ignores that restriction entirely —
    // even at its maximum, where the activation simply does nothing.
    if (ab.is_empower && maxEmpower() <= 1 && obj.isEmpowered()) return false;
    // "Disempower this" as a cost needs an Empowered status to remove.
    if (ab.cost.disempower_self && !obj.isEmpowered()) return false;
    return true;
}

int Card::activationCostReductionFor(const GameState& state, PlayerId controller,
                                     GameObjectId source, int ability_index) const {
    int r = activationCostReduction(state, controller, ability_index);
    const auto& abs = abilities();
    if (ability_index >= 0 && ability_index < static_cast<int>(abs.size()) &&
        abs[ability_index].is_empower && state.objectExists(source))
        r += state.getObject(source).empower_cost_reduction;
    return r;
}

int Card::activationPowerReductionFor(const GameState& state, PlayerId /*controller*/,
                                      GameObjectId source, int ability_index,
                                      int unused_energy_reduction) const {
    // "[Empower] costs ... cost [1] or [A] less" (Risen Altar): what the
    // Energy side could not use comes off the Power.
    const auto& abs = abilities();
    if (ability_index < 0 || ability_index >= static_cast<int>(abs.size()) ||
        !abs[ability_index].is_empower || !state.objectExists(source))
        return 0;
    return std::max(0, std::min(unused_energy_reduction,
                                state.getObject(source).empower_cost_reduction));
}

// ── Equip (CR 818) ──────────────────────────────────────────────────────────

Card::EquipCost Card::netEquipCost(const GameState& state, PlayerId controller,
                                   GameObjectId gear, GameObjectId unit,
                                   const EquipTerms& terms) const {
    EquipCost cost = equipCost(state, controller, gear, unit);
    cost.energy = std::max(0, cost.energy - std::max(0, terms.energy_discount));
    // CR 821.1.c.3: a cost with no Power is not reduced by "[A] less".
    cost.power = std::max(0, cost.power - std::max(0, terms.power_discount));
    return cost;
}

bool Card::canPayEquip(const GameState& state, PlayerId controller, GameObjectId gear,
                       GameObjectId unit, const EquipTerms& terms) const {
    if (!equipEndsOnBoard(state, gear, unit)) return false;
    const auto& g = state.getObject(gear);
    if (g.controller != controller || state.getObject(unit).controller != controller)
        return false;
    if (g.attached_to.has_value()) {
        // Attached: its Rules Text, the Equip ability included, is Inactive
        // (CR 718.2, 721.2). Weaponmaster lifts that (CR 821.1.c).
        if (!terms.even_if_attached || *g.attached_to == unit) return false;
    }
    const EquipCost cost = netEquipCost(state, controller, gear, unit, terms);
    if (!cost.valid) return false;                       // CR 821.1.c.4
    if (!resource::canPay(state, controller, cost.energy, cost.power,
                          cost.power_any_domain ? Domain::Count : cost.power_domain,
                          SpendFor::Gear))
        return false;
    return canPayEquipExtraCost(state, controller, gear, unit);
}

bool Card::payEquipAndAttach(CardContext& ctx, GameObjectId unit, const EquipTerms& terms) {
    if (!canPayEquip(ctx.state, ctx.controller, ctx.source, unit, terms)) return false;
    const EquipCost cost = netEquipCost(ctx.state, ctx.controller, ctx.source, unit, terms);
    if (!resource::pay(ctx.state, ctx.events, ctx.controller, cost.energy, cost.power,
                       cost.power_any_domain ? Domain::Count : cost.power_domain,
                       SpendFor::Gear))
        return false;
    if (!payEquipExtraCost(ctx, unit)) return false;
    // The extra cost may have removed either end ("Kill a friendly unit").
    if (!equipEndsOnBoard(ctx.state, ctx.source, unit)) return false;
    // EffectExecutor::attachGear takes an attached Equipment off its old
    // bearer first (unattachGear) and checks both ends are on the board.
    if (!ctx.executor.attachGear(ctx.source, unit)) return false;
    onEquipped(ctx, unit);
    return true;
}

bool Card::canPayEquip(const GameState& state, PlayerId controller, GameObjectId gear,
                       GameObjectId unit) const {
    return canPayEquip(state, controller, gear, unit, EquipTerms{});
}
bool Card::payEquipAndAttach(CardContext& ctx, GameObjectId unit) {
    return payEquipAndAttach(ctx, unit, EquipTerms{});
}
bool Card::onEquip(CardContext& ctx, GameObjectId unit) {
    return payEquipAndAttach(ctx, unit, EquipTerms{});
}

// ── Additional costs chosen before payment (CR 356) ─────────────────────────

Card::AdditionalPlayCost Card::additionalPlayCost(const GameState& state, const CardDB& /*db*/,
                                                  PlayerId player, GameObjectId self) const {
    // "you may discard N as an additional cost. If you do, reduce my cost by
    // [M]" declared through optionalAdditionalCost().
    const auto oac = optionalAdditionalCost(state, player);
    if (!oac.valid || oac.discard_cards <= 0 || oac.reduce_energy <= 0) return {};
    const auto& hand = state.player(player).hand;
    int others = static_cast<int>(hand.size());
    if (std::find(hand.begin(), hand.end(), self) != hand.end()) --others;
    AdditionalPlayCost out;
    out.exists = true;
    out.payable = others >= oac.discard_cards;
    out.best.energy_reduction = oac.reduce_energy;
    return out;
}

Card::PlayCostAdjust Card::payAdditionalPlayCost(
    CardContext& ctx, const std::function<bool(const PlayCostAdjust&)>& affordable) {
    const auto oac = optionalAdditionalCost(ctx.state, ctx.controller);
    if (!oac.valid || oac.discard_cards <= 0 || oac.reduce_energy <= 0) return {};
    {
        const auto& h = ctx.state.player(ctx.controller).hand;
        int others = static_cast<int>(h.size());
        if (std::find(h.begin(), h.end(), ctx.source) != h.end()) --others;
        if (others < oac.discard_cards) return {};
    }
    // Optional while the full cost can be paid; otherwise the play was only
    // legal with the discount, so the discard is part of it.
    const bool may_decline = affordable(PlayCostAdjust{});
    if (may_decline &&
        !ctx.executor.confirmNow(ctx.controller,
            "discard " + std::to_string(oac.discard_cards) + " to reduce the cost by [" +
            std::to_string(oac.reduce_energy) + "]"))
        return {};
    // The card being played is not one of the cards discarded for it (an
    // effect-driven play pays while the card is still in its zone).
    auto& hand = ctx.state.player(ctx.controller).hand;
    const auto self_at = std::find(hand.begin(), hand.end(), ctx.source);
    const bool self_in_hand = self_at != hand.end();
    if (self_in_hand) hand.erase(self_at);
    ctx.executor.discardCards(ctx.controller, oac.discard_cards);
    if (self_in_hand) hand.push_back(ctx.source);
    if (oac.paid_flag && oac.paid_flag[0] != '\0' && ctx.state.objectExists(ctx.source))
        ctx.state.getObject(ctx.source).card_counters[oac.paid_flag] = 1;
    PlayCostAdjust adj;
    adj.energy_reduction = oac.reduce_energy;
    return adj;
}

void Card::primeStaticFacts() const {
    auto facts = std::make_shared<StaticFacts>();
    facts->abilities = activatedAbilities();
    for (auto t : triggerTypes()) facts->triggers.set(static_cast<size_t>(t));
    static_facts_ = std::move(facts);
}

const std::vector<ActivatedAbility>& Card::abilities() const {
    if (!static_facts_) primeStaticFacts();
    return static_facts_->abilities;
}

std::vector<ActivatedAbility> Card::activatedAbilities() const {
    if (!hasActivatedAbility()) {
        std::vector<ActivatedAbility> only_empower;
        appendEmpowerAbilities(only_empower);
        return only_empower;
    }
    std::vector<ActivatedAbility> out = { ActivatedAbility{
        .cost = getActivationCost(),
        .targets = getTargetRequirements(),
        .is_action = isActionAbility(),
        .is_reaction = isReactionAbility(),
        // Single-ability cards don't use the multi-ability target-defer
        // hook today; opt in by overriding activatedAbilities() directly.
        .needs_activation_time_target = false,
    } };
    appendEmpowerAbilities(out);
    return out;
}

} // namespace riftbound
