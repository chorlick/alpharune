/// @file test_choice_helpers.cpp
/// The shared card helpers: Card::confirmOptional / pickXAmount / pickMode /
/// pickTarget / pickTargetPair composing in any order (plans/03 item 3.1,
/// plans/07 item 7.32), the chosen-object bookkeeping (7.29, 7.30), Power
/// paid through card_helpers.h (3.23), splitDamageAmong (3.24), and the
/// fixture's real-engine entry points (7.3).

#include "tests/cards/card_test_fixture.h"
#include "cards/card_helpers.h"

#include <algorithm>
#include <array>
#include <string>
#include <vector>

using namespace riftbound;
using namespace riftbound::test;

namespace {

constexpr CardDefId kKeepersVerdict = 760;
constexpr CardDefId kAltarOfMemories = 490;
constexpr CardDefId kBulletTime = 263;
constexpr CardDefId kZenithBlade = 258;
constexpr CardDefId kSentinelAdept = 331;
constexpr CardDefId kArmedAssailant = 324;
constexpr CardDefId kRecurveBow = 339;       // SimpleEquipGear(Domain::Fury)

// ─── A card that calls the helpers in a scripted order ──────────────────────
// 'C' confirmOptional, 'X' pickXAmount(0..3), 'M' pickMode(3 modes),
// 'T' pickTarget(targets), 'P' pickTargetPair(targets, the rest).
class ProbeCard : public SpellCard {
public:
    std::string order;
    std::vector<GameObjectId> targets;

    // What the helpers returned on the pass that got past all of them.
    int confirm = -9, x = -9, mode = -9;
    GameObjectId target = kInvalidId;
    std::pair<GameObjectId, GameObjectId> pair{kInvalidId, kInvalidId};
    int completions = 0;      // times the body after the helpers ran
    int pair_half_seen = 0;   // times pickTargetPair returned {a, invalid} while waiting

    const CardDef& def() const override { return def_; }

    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        for (char h : order) {
            const bool waiting_before = ctx.executor.hasPendingChoice();
            ASSERT_FALSE(waiting_before);
            switch (h) {
            case 'C': confirm = confirmOptional(ctx, "probe may", [] { return true; }); break;
            case 'X': x = pickXAmount(ctx, "probe X", 0, 3); break;
            case 'M': mode = pickMode(ctx, "probe mode", 3, {"a", "b", "c"}); break;
            case 'T': target = pickTarget(ctx, "probe target", targets); break;
            case 'P':
                pair = pickTargetPair(ctx, "probe pair", targets, [this](GameObjectId a) {
                    std::vector<GameObjectId> rest;
                    for (auto t : targets) if (t != a) rest.push_back(t);
                    return rest;
                });
                if (ctx.executor.hasPendingChoice() && pair.first != kInvalidId) ++pair_half_seen;
                break;
            }
            if (ctx.executor.hasPendingChoice()) return;   // suspended for the agent
        }
        ++completions;
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1;
        d.name = "Probe";
        d.card_type = CardType::Spell;
        return d;
    }();
};

class ChoiceHelperTest : public CardTestFixture {
protected:
    /// Resolve `card` the way ChainManager::stepResolve does: re-enter after
    /// every published choice. `picker` answers; the prompt labels are logged.
    void resolveProbe(ProbeCard& card, ChainItem item,
                      const std::function<Intent(const std::string&, const std::vector<Intent>&)>& picker,
                      EffectExecutor& exec) {
        item.id = state.chain.allocateId();
        item.controller = P1;
        item.is_spell = true;
        state.chain.resuming = item;
        CardContext ctx{state, events, exec, P1, kInvalidId};
        card.onResolve(ctx, {});
        int guard = 0;
        while (exec.hasPendingChoice()) {
            ASSERT_LT(++guard, 16) << "resume loop overflow";
            auto pending = exec.consumePendingChoice();
            labels.push_back(pending.label);
            exec.recordChoice(picker(pending.label, pending.legal));
            card.onResolve(ctx, {});
        }
        resolved = *state.chain.resuming;
        state.chain.resuming.reset();
    }

    static Intent byValue(const std::vector<Intent>& legal, int v) {
        for (const auto& i : legal)
            if (i.chosen_value.has_value() && *i.chosen_value == v) return i;
        ADD_FAILURE() << "no option with value " << v;
        return legal.front();
    }
    static Intent byObject(const std::vector<Intent>& legal, GameObjectId id) {
        for (const auto& i : legal)
            if (!i.chosen_objects.empty() && i.chosen_objects.front() == id) return i;
        ADD_FAILURE() << "object " << id << " was not offered";
        return legal.front();
    }

    std::vector<std::string> labels;
    ChainItem resolved;
};

// ─── 3.1 — the helpers compose in any order ─────────────────────────────────

TEST_F(ChoiceHelperTest, EveryOrderOfTheHelpersAsksEachOnceAndKeepsEachAnswer) {
    EffectExecutor exec(state, events, card_db);
    auto u1 = addUnit(P1, kInvalidId, 2, 0);
    auto u2 = addUnit(P1, kInvalidId, 2, 0);
    auto u3 = addUnit(P1, kInvalidId, 2, 0);

    // In every order: the "may" is answered Yes (1), the mode 2, the X 3, the
    // single target u2, the pair (u3, u1).
    for (std::string order : {"TC", "CT", "TM", "MT", "TX", "XT", "MC", "CM", "PC", "CP",
                              "PM", "MP", "TMC", "CMT", "MCT", "TCM", "PMC", "CMP", "XMT",
                              "TMX", "MXP", "PXM"}) {
        SCOPED_TRACE("order " + order);
        ProbeCard card;
        card.order = order;
        card.targets = {u1, u2, u3};
        labels.clear();
        int pair_step = 0;
        resolveProbe(card, ChainItem{}, [&](const std::string& label, const std::vector<Intent>& legal) {
            if (label.rfind("may:", 0) == 0) return byValue(legal, 1);
            if (label.rfind("mode:", 0) == 0) return byValue(legal, 2);
            if (label.rfind("X:", 0) == 0) return byValue(legal, 3);
            if (label.rfind("target A:", 0) == 0) { ++pair_step; return byObject(legal, u3); }
            if (label.rfind("target B:", 0) == 0) { ++pair_step; return byObject(legal, u1); }
            return byObject(legal, u2);
        }, exec);

        const size_t expected_prompts = order.size() + (order.find('P') != std::string::npos ? 1 : 0);
        EXPECT_EQ(labels.size(), expected_prompts) << "every helper asks, exactly once";
        EXPECT_EQ(card.completions, 1) << "the effect runs once, after the last answer";
        if (order.find('C') != std::string::npos) { EXPECT_EQ(card.confirm, 1); }
        if (order.find('M') != std::string::npos) { EXPECT_EQ(card.mode, 2); }
        if (order.find('X') != std::string::npos) { EXPECT_EQ(card.x, 3); }
        if (order.find('T') != std::string::npos) { EXPECT_EQ(card.target, u2); }
        if (order.find('P') != std::string::npos) {
            EXPECT_EQ(card.pair.first, u3);
            EXPECT_EQ(card.pair.second, u1);
            EXPECT_EQ(pair_step, 2);
        }
    }
}

TEST_F(ChoiceHelperTest, ConfirmOptionalAfterPickTargetCanBeDeclined) {
    // Zenith Blade's shape: a target, then "you may". The "may" used to come
    // back 1 without being asked because pickTarget had moved resume_point on.
    EffectExecutor exec(state, events, card_db);
    auto u1 = addUnit(P1, kInvalidId, 2, 0);
    ProbeCard card;
    card.order = "TC";
    card.targets = {u1};
    resolveProbe(card, ChainItem{}, [&](const std::string& label, const std::vector<Intent>& legal) {
        return label.rfind("may:", 0) == 0 ? byValue(legal, 0) : byObject(legal, u1);
    }, exec);
    EXPECT_EQ(card.target, u1);
    EXPECT_EQ(card.confirm, 0) << "the agent said no";
    ASSERT_EQ(labels.size(), 2u);
}

TEST_F(ChoiceHelperTest, PickXAmountAfterAPlayTimeTargetKeepsX) {
    // The engine pre-sizes resume_data to 6 for a target chosen at play
    // (resume_data[5] = 1). X used to be appended at index 6 and read back
    // from index 0, i.e. always 0.
    EffectExecutor exec(state, events, card_db);
    auto u1 = addUnit(P1, kInvalidId, 2, 0);
    ProbeCard card;
    card.order = "TX";
    card.targets = {u1};
    ChainItem item;
    item.targets = {u1};
    item.resume_data = {0, 0, 0, 0, 0, 1};
    resolveProbe(card, item, [&](const std::string&, const std::vector<Intent>& legal) {
        return byValue(legal, 2);
    }, exec);
    ASSERT_EQ(labels.size(), 1u) << "only X is asked: the target was chosen at play";
    EXPECT_EQ(card.target, u1);
    EXPECT_EQ(card.x, 2);
    EXPECT_EQ(card.completions, 1);
    EXPECT_EQ(resolved.resume_data[0], 2);
}

TEST_F(ChoiceHelperTest, HelpersLeaveTheDocumentedResumePoints) {
    // Cards tell a suspend from a fizzle by reading resume_point after a helper.
    EffectExecutor exec(state, events, card_db);
    auto u1 = addUnit(P1, kInvalidId, 2, 0);
    auto u2 = addUnit(P1, kInvalidId, 2, 0);
    std::vector<int> seen;
    auto record = [&](const std::string&, const std::vector<Intent>& legal) {
        seen.push_back(state.chain.resuming->resume_point);
        return legal.back();
    };
    for (auto [order, waiting, done] : std::vector<std::tuple<std::string, std::vector<int>, int>>{
             {"C", {1}, 2}, {"X", {1}, 2}, {"M", {4}, 5}, {"T", {7}, 8}, {"P", {10, 12}, 13}}) {
        SCOPED_TRACE(order);
        ProbeCard card;
        card.order = order;
        card.targets = {u1, u2};
        seen.clear();
        resolveProbe(card, ChainItem{}, record, exec);
        EXPECT_EQ(seen, waiting);
        EXPECT_EQ(resolved.resume_point, done);
    }
    // Nothing to choose: straight to "done", no prompt.
    ProbeCard none;
    none.order = "T";
    labels.clear();
    resolveProbe(none, ChainItem{}, record, exec);
    EXPECT_TRUE(labels.empty());
    EXPECT_EQ(none.target, kInvalidId);
    EXPECT_EQ(resolved.resume_point, 8);
}

// ─── 7.32 — a half-chosen pair is not handed to the card ────────────────────

TEST_F(ChoiceHelperTest, PickTargetPairReturnsNothingWhileWaitingForTheSecondPick) {
    EffectExecutor exec(state, events, card_db);
    auto u1 = addUnit(P1, kInvalidId, 2, 0);
    auto u2 = addUnit(P1, kInvalidId, 2, 0);
    ProbeCard card;
    card.order = "P";
    card.targets = {u1, u2};
    resolveProbe(card, ChainItem{}, [&](const std::string&, const std::vector<Intent>& legal) {
        return legal.front();
    }, exec);
    EXPECT_EQ(card.pair_half_seen, 0)
        << "{a, invalid} while suspended makes a card act on `a` now and again on re-entry";
    EXPECT_EQ(card.pair.first, u1);
    EXPECT_EQ(card.pair.second, u2);
    EXPECT_EQ(card.completions, 1);
}

TEST_F(ChoiceHelperTest, PickTargetPairWithNoSecondTargetIsAFinishedPartialPair) {
    EffectExecutor exec(state, events, card_db);
    auto u1 = addUnit(P1, kInvalidId, 2, 0);
    ProbeCard card;
    card.order = "P";
    card.targets = {u1};
    resolveProbe(card, ChainItem{}, [&](const std::string&, const std::vector<Intent>& legal) {
        return legal.front();
    }, exec);
    EXPECT_EQ(card.pair.first, u1);
    EXPECT_EQ(card.pair.second, kInvalidId);
    EXPECT_EQ(resolved.resume_point, 13) << "done, not waiting";
    EXPECT_EQ(card.completions, 1);
}

// ─── 7.29 / 7.30 — chosen-object bookkeeping ────────────────────────────────

TEST_F(ChoiceHelperTest, PickingAnEnemyUnitCardOutOfAHandIsNotChoosingAnEnemyUnit) {
    // CR 141.1.a: a unit is a unit on the board. Insightful Investigator picks
    // a card from the opponent's hand through pickTarget.
    EffectExecutor exec(state, events, card_db);
    auto in_hand = addToHand(P2, kArmedAssailant);
    ASSERT_TRUE(state.getObject(in_hand).isUnit());
    ProbeCard card;
    card.order = "T";
    card.targets = {in_hand};
    resolveProbe(card, ChainItem{}, [&](const std::string&, const std::vector<Intent>& legal) {
        return legal.front();
    }, exec);
    EXPECT_EQ(card.target, in_hand);
    EXPECT_FALSE(state.player(P1).chose_enemy_unit_this_turn);

    auto on_board = addUnit(P2, kInvalidId, 2, 0);
    ProbeCard card2;
    card2.order = "T";
    card2.targets = {on_board};
    resolveProbe(card2, ChainItem{}, [&](const std::string&, const std::vector<Intent>& legal) {
        return legal.front();
    }, exec);
    EXPECT_TRUE(state.player(P1).chose_enemy_unit_this_turn);
}

TEST_F(ChoiceHelperTest, HandBuiltChoicesCanChargeDeflectAndRecordTheChoice) {
    // offerableChoices / commitChosenObject: what pickTarget does, for cards
    // that publish their own objChoice options.
    EffectExecutor exec(state, events, card_db);
    auto plain = addUnit(P2, kInvalidId, 2, 0);
    auto deflector = addUnit(P2, kInvalidId, 2, 0);
    state.getObject(deflector).keywords.set(Keyword::Deflect);
    state.getObject(deflector).deflect_value = 1;
    ProbeCard card;
    ChainItem item;
    item.id = state.chain.allocateId();
    item.controller = P1;
    state.chain.resuming = item;
    CardContext ctx{state, events, exec, P1, kInvalidId};

    auto offered = card.offerableChoices(ctx, {plain, deflector});
    EXPECT_EQ(offered, std::vector<GameObjectId>{plain}) << "no [A] to pay Deflect with";

    auto rune = addRune(P1, Domain::Fury);
    offered = card.offerableChoices(ctx, {plain, deflector});
    EXPECT_EQ(offered.size(), 2u);

    card.commitChosenObject(ctx, deflector);
    EXPECT_FALSE(state.getObject(rune).location.has_value()) << "the rune was recycled for Deflect";
    EXPECT_EQ(state.chain.resuming->chosen_objects, std::vector<GameObjectId>{deflector});
    EXPECT_TRUE(state.player(P1).chose_enemy_unit_this_turn);
    state.chain.resuming.reset();
}

// ─── 3.23 — Power paid by card helpers is the engine's Power ────────────────

TEST_F(ChoiceHelperTest, PayOnePowerRecyclesARuneWithTheEnginesBookkeeping) {
    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, kInvalidId};
    auto ready = addRune(P1, Domain::Fury);
    auto spent = addRune(P1, Domain::Fury, /*exhausted=*/true);
    addRune(P1, Domain::Calm, /*exhausted=*/true);
    std::vector<GameObjectId> left;
    auto conn = events.on_left_board.connect([&](const LeftBoardEvent& e) {
        if (e.card_type == CardType::Rune) left.push_back(e.object);
    });

    ASSERT_TRUE(payableOnePower(state, P1, Domain::Fury));
    ASSERT_TRUE(payOnePower(ctx, P1, Domain::Fury));
    EXPECT_EQ(left, std::vector<GameObjectId>{spent}) << "the exhausted Fury rune, with its event";
    EXPECT_EQ(state.player(P1).power_spent_this_turn, 1) << "Sivir, Mercenary counts it";
    EXPECT_FALSE(state.getObject(spent).is_exhausted) << "a recycled rune is not left exhausted";
    EXPECT_EQ(state.getObject(spent).zone, ZoneType::RuneDeck);
    EXPECT_TRUE(state.getObject(ready).location.has_value());

    ASSERT_TRUE(payOnePower(ctx, P1, std::nullopt));   // [A]
    EXPECT_EQ(state.player(P1).power_spent_this_turn, 2);
    EXPECT_EQ(left.size(), 2u);

    // Floating Power pays first and is not a recycled rune.
    state.player(P1).rune_pool.power[static_cast<int>(Domain::Fury)] = 1;
    ASSERT_TRUE(payOnePower(ctx, P1, Domain::Fury));
    EXPECT_EQ(state.player(P1).power_spent_this_turn, 2);
    EXPECT_EQ(left.size(), 2u);
    conn.disconnect();
}

TEST_F(ChoiceHelperTest, EquipCostsCountAsPowerSpentAndAreAllOrNothing) {
    // "Sivir, Mercenary (464) still reads 0 after two Equip costs."
    EffectExecutor exec(state, events, card_db);
    auto unit = addUnit(P1, kInvalidId, 2);
    auto make_bow = [&] {
        auto id = state.createObject();
        auto& g = state.getObject(id);
        g.owner = g.controller = P1;
        g.card_def_id = kRecurveBow;
        g.card_type = CardType::Gear;
        g.name = card_db.get(kRecurveBow).name;
        g.tags = card_db.get(kRecurveBow).tags;
        g.zone = ZoneType::Base;
        g.location = BaseLocation{P1};
        return id;
    };
    auto bow1 = make_bow();
    auto bow2 = make_bow();
    Card* bow = card_registry.get(kRecurveBow);
    ASSERT_NE(bow, nullptr);
    ASSERT_TRUE(bow->hasEquipAbility());

    EXPECT_FALSE(equipThroughEngine(bow1, unit)) << "no Fury power: nothing paid, nothing attached";
    EXPECT_FALSE(state.getObject(bow1).attached_to.has_value());

    addRune(P1, Domain::Fury);
    addRune(P1, Domain::Fury);
    EXPECT_TRUE(equipThroughEngine(bow1, unit));
    EXPECT_TRUE(equipThroughEngine(bow2, unit));
    EXPECT_EQ(state.player(P1).power_spent_this_turn, 2);
    EXPECT_EQ(state.getObject(unit).attachments.size(), 2u);

    // A unit that has left the board can't be equipped, whatever objectExists says.
    auto bow3 = make_bow();
    addRune(P1, Domain::Fury);
    auto gone = addUnit(P1, kInvalidId, 2);
    state.getObject(gone).location = std::nullopt;
    state.getObject(gone).zone = ZoneType::Trash;
    EXPECT_FALSE(equipThroughEngine(bow3, gone));
    EXPECT_EQ(state.player(P1).power_spent_this_turn, 2) << "and nothing was paid";
}

// ─── 3.24 — one shared split-damage helper ──────────────────────────────────

TEST_F(ChoiceHelperTest, SplitDamageAmongLetsTheControllerDivideTheDamage) {
    EffectExecutor exec(state, events, card_db);
    auto src = addUnit(P1, kInvalidId, 3, 0);
    auto e1 = addUnit(P2, kInvalidId, 5, 0);
    auto e2 = addUnit(P2, kInvalidId, 5, 0);
    ChainItem item;
    item.id = state.chain.allocateId();
    item.controller = P1;
    state.chain.resuming = item;
    CardContext ctx{state, events, exec, P1, src};

    std::vector<GameObjectId> damaged;
    // The targets first (CR 355.14.b): e2, e1. Each gets 1; the third point
    // goes to e2. (Before plans/09 item 9.M4 every point was its own pick and
    // the units were not chosen as targets up front.)
    std::vector<GameObjectId> answers = {e2, e1, e2};
    size_t n = 0;
    bool done = splitDamageAmong(ctx, "test", 3, src, {e1, e2}, damaged);
    while (!done) {
        ASSERT_TRUE(exec.hasPendingChoice());
        auto pending = exec.consumePendingChoice();
        ASSERT_LT(n, answers.size());
        exec.recordChoice(byObject(pending.legal, answers[n++]));
        done = splitDamageAmong(ctx, "test", 3, src, {e1, e2}, damaged);
    }
    state.chain.resuming.reset();
    EXPECT_EQ(n, 3u);
    EXPECT_EQ(state.getObject(e1).damage_marked, 1);
    EXPECT_EQ(state.getObject(e2).damage_marked, 2);
    EXPECT_EQ(damaged.size(), 2u);
    EXPECT_TRUE(state.player(P1).chose_enemy_unit_this_turn);
}

// ─── 7.3 — the fixture's real engine path ───────────────────────────────────

class EnginePathTest : public CardTestFixture {
protected:
    GameObjectId addEquipment(PlayerId owner) {
        auto id = state.createObject();
        auto& g = state.getObject(id);
        g.owner = g.controller = owner;
        g.card_type = CardType::Gear;
        g.name = "TestGear";
        g.tags = {"Equipment"};
        g.zone = ZoneType::Base;
        g.location = BaseLocation{owner};
        withEquipmentCard(id);   // CR 821.1.c.4: see the fixture
        return id;
    }
};

TEST_F(EnginePathTest, PlayedUnitsTriggerIsFinalizedAndResolvedByTheEngine) {
    // Sentinel Adept: "[Weaponmaster] (When you play me, you may [Equip] one
    // of your Equipment to me for [A] less, even if it's already attached.)"
    // Played from hand, cost paid; the trigger goes through TriggerManager and
    // the chain, and the agent — not the test — supplies the "may" and the
    // Equipment. (The test Equipment has no card behind it, so no Equip cost.)
    auto adept = addToHand(P1, kSentinelAdept);
    auto gear_a = addEquipment(P1);
    auto gear_b = addEquipment(P1);

    EnginePathAgent agent(EnginePathAgent::choosing(gear_b, EnginePathAgent::May::Yes));
    playThroughEngine(adept, &agent);

    ASSERT_TRUE(state.getObject(adept).location.has_value()) << "the unit was played";
    EXPECT_FALSE(inHand(P1, adept));
    ASSERT_GE(agent.prompts.size(), 2u) << "asked the may, then the Equipment";
    EXPECT_TRUE(agent.wasOffered(gear_a));
    EXPECT_EQ(state.getObject(gear_b).attached_to, std::optional<GameObjectId>{adept});
    EXPECT_FALSE(state.getObject(gear_a).attached_to.has_value());
    EXPECT_FALSE(state.chain.exists());
}

TEST_F(EnginePathTest, AnOptionalTriggerCanBeDeclinedThroughTheEngine) {
    auto adept = addToHand(P1, kSentinelAdept);
    auto gear = addEquipment(P1);
    EnginePathAgent agent(EnginePathAgent::script({0}));   // "No"
    playThroughEngine(adept, &agent);
    ASSERT_TRUE(state.getObject(adept).location.has_value());
    EXPECT_EQ(agent.prompts.size(), 1u);
    EXPECT_FALSE(state.getObject(gear).attached_to.has_value());
}

TEST_F(EnginePathTest, WeaponmasterIsAMayWithAChoiceOfEquipment) {
    // CR 821.1.c: "you may choose a card you control with the Equipment tag".
    // The base used to attach the first Equipment found without asking.
    auto gear_a = addEquipment(P1);
    auto gear_b = addEquipment(P1);

    auto declined = addToHand(P1, kArmedAssailant);
    EnginePathAgent no(EnginePathAgent::script({0}));
    playThroughEngine(declined, &no);
    ASSERT_TRUE(state.getObject(declined).location.has_value());
    EXPECT_TRUE(state.getObject(declined).attachments.empty()) << "the may was declined";

    auto wm = addToHand(P1, kArmedAssailant);
    EnginePathAgent yes(EnginePathAgent::choosing(gear_b, EnginePathAgent::May::Yes));
    playThroughEngine(wm, &yes);
    EXPECT_TRUE(yes.wasOffered(gear_a));
    EXPECT_EQ(state.getObject(gear_b).attached_to, std::optional<GameObjectId>{wm});
    EXPECT_FALSE(state.getObject(gear_a).attached_to.has_value());
}

TEST_F(EnginePathTest, WeaponmasterDoesNotEquipAUnitThatLeftTheBoard) {
    // The play trigger is finalized for a unit on the board — the agent
    // accepts the may and chooses the Equipment (CR 383.3) — and the unit is
    // killed in response: no Equipment follows it to the trash.
    // (Was a direct fireTriggerAs call with no agent, plans/09 item 9.N6.)
    auto gear = addEquipment(P1);
    auto wm = addUnit(P1, kArmedAssailant, 2);

    EnginePathAgent agent(EnginePathAgent::choosing(gear, EnginePathAgent::May::Yes));
    GameState* live = nullptr;
    bool removed = false;
    // The opponent's first decision is priority on the finalized ability.
    PickByPredicateAgent opponent([&](int, const std::vector<Intent>& legal) {
        if (!removed && live) {
            removed = true;
            EXPECT_FALSE(live->getObject(gear).attached_to.has_value())
                << "attached as the ability resolves, not before";
            auto& unit = live->getObject(wm);
            unit.location = std::nullopt;
            unit.zone = ZoneType::Trash;
            live->player(P1).trash.push_back(wm);
        }
        for (const auto& i : legal)
            if (i.type == IntentType::PassPriority) return i;
        return legal.empty() ? Intent{} : legal.front();
    });
    withEngine([&](GameEngine& e) {
        live = &e.mutableState();
        e.testHook_executor().queueTriggeredAbility(wm, P1, TriggerType::WhenYouPlayMe);
        e.testHook_runChain();
        live = nullptr;
    }, &agent, &opponent);

    ASSERT_TRUE(removed) << "the opponent never got priority on the ability";
    ASSERT_EQ(agent.prompts.size(), 2u) << "the may was accepted and the Equipment chosen";
    EXPECT_TRUE(agent.wasOffered(gear));
    EXPECT_FALSE(state.getObject(wm).location.has_value());
    EXPECT_FALSE(state.getObject(gear).attached_to.has_value());
    EXPECT_TRUE(state.getObject(gear).location.has_value());
    EXPECT_TRUE(state.getObject(wm).attachments.empty());
    EXPECT_FALSE(state.chain.exists());
}

TEST_F(EnginePathTest, SpellTargetThenModeIsAskedThroughTheEngine) {
    // Keeper's Verdict: "Choose an enemy unit at a battlefield. Its owner
    // places it on the top or bottom of their Main Deck." The target is chosen
    // at play, then top / bottom: that used to come back 0 (top) unasked.
    // Card text: "Its OWNER places it" — the owner answers, not the caster.
    auto spell = addToHand(P1, kKeepersVerdict);
    auto victim = addUnit(P2, kInvalidId, 3, 0);
    auto other = addUnit(P2, kInvalidId, 3, 0);
    addToDeck(P2, kInvalidId);
    addToDeck(P2, kInvalidId);

    // Prompts: the caster picks the target; the owner picks top (0) / bottom (1).
    EnginePathAgent agent(EnginePathAgent::choosing(victim));
    EnginePathAgent owner(EnginePathAgent::script({1}));
    playThroughEngine(spell, &agent, std::nullopt, &owner);

    ASSERT_EQ(agent.prompts.size(), 1u);
    ASSERT_EQ(owner.prompts.size(), 1u);
    EXPECT_FALSE(state.getObject(victim).location.has_value());
    ASSERT_EQ(deckSize(P2), 3);
    EXPECT_EQ(state.player(P2).main_deck.front(), victim) << "bottom of the deck, as chosen";
    EXPECT_TRUE(state.getObject(other).location.has_value());
}

TEST_F(EnginePathTest, SpellTargetThenXIsAskedThroughTheEngine) {
    // Bullet Time: "Pay any amount of [A] to deal that much damage to all
    // enemy units at a battlefield." pickTarget then pickXAmount: X used to
    // come back 0 unasked.
    auto spell = addToHand(P1, kBulletTime);
    auto e1 = addUnit(P2, kInvalidId, 5, 0);
    auto e2 = addUnit(P2, kInvalidId, 5, 0);
    auto elsewhere = addUnit(P2, kInvalidId, 5, 1);
    addRune(P1, Domain::Body, /*exhausted=*/true);
    addRune(P1, Domain::Body, /*exhausted=*/true);

    EnginePathAgent agent([&](const std::vector<Intent>& legal) {
        for (const auto& i : legal)
            if (!i.chosen_objects.empty() && i.chosen_objects.front() == e1) return i;
        for (const auto& i : legal)
            if (i.chosen_value.has_value() && *i.chosen_value == 2) return i;
        return legal.front();
    });
    playThroughEngine(spell, &agent);

    ASSERT_EQ(agent.prompts.size(), 2u) << "the battlefield's unit, then X";
    EXPECT_EQ(state.getObject(e1).damage_marked, 2);
    EXPECT_EQ(state.getObject(e2).damage_marked, 2);
    EXPECT_EQ(state.getObject(elsewhere).damage_marked, 0);
}

TEST_F(EnginePathTest, SpellMayAfterTargetCanBeDeclinedThroughTheEngine) {
    // Zenith Blade: "Stun an enemy unit at a battlefield. You may move a
    // friendly unit to that enemy unit's battlefield." The move was forced.
    auto spell = addToHand(P1, kZenithBlade);
    auto enemy = addUnit(P2, kInvalidId, 3, 0);
    auto friendly = addUnit(P1, kInvalidId, 3);   // at base

    EnginePathAgent agent([&](const std::vector<Intent>& legal) {
        for (const auto& i : legal)
            if (!i.chosen_objects.empty() && i.chosen_objects.front() == enemy) return i;
        for (const auto& i : legal)
            if (i.chosen_value.has_value() && *i.chosen_value == 0) return i;   // No
        return legal.front();
    });
    playThroughEngine(spell, &agent);

    ASSERT_EQ(agent.prompts.size(), 2u);
    EXPECT_TRUE(state.getObject(enemy).is_stunned);
    EXPECT_TRUE(state.getObject(friendly).isAtBase()) << "declined: nothing moves";
}

TEST_F(EnginePathTest, DeathTriggerWithMayTargetAndModeResolvesThroughTheEngine) {
    // Altar of Memories: "When a friendly unit dies, you may exhaust me to
    // draw 1, then put a card from your hand on the top or bottom of your
    // Main Deck." confirmOptional, pickTarget, pickMode — "bottom" could not
    // be chosen.
    auto altar = state.createObject();
    {
        auto& g = state.getObject(altar);
        g.owner = g.controller = P1;
        g.card_def_id = kAltarOfMemories;
        g.card_type = CardType::Gear;
        g.name = card_db.get(kAltarOfMemories).name;
        g.zone = ZoneType::Base;
        g.location = BaseLocation{P1};
    }
    auto doomed = addUnit(P1, kInvalidId, 1);
    auto keep = addToHand(P1, kInvalidId);
    auto bury = addToHand(P1, kInvalidId);
    addToDeck(P1, kInvalidId);
    addToDeck(P1, kInvalidId);

    EnginePathAgent agent([&](const std::vector<Intent>& legal) {
        for (const auto& i : legal)
            if (!i.chosen_objects.empty() && i.chosen_objects.front() == bury) return i;
        // yes (1) to the may; bottom (1) for the mode.
        for (const auto& i : legal)
            if (i.chosen_value.has_value() && *i.chosen_value == 1) return i;
        return legal.front();
    });
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) { exec.killObject(doomed); },
                         &agent);

    ASSERT_EQ(agent.prompts.size(), 3u) << "the may, the hand card, top/bottom";
    EXPECT_TRUE(state.getObject(altar).is_exhausted);
    EXPECT_TRUE(inHand(P1, keep));
    EXPECT_FALSE(inHand(P1, bury));
    EXPECT_EQ(state.player(P1).main_deck.front(), bury) << "bottom of the deck, as chosen";
}

// ─── A helper that follows another helper suspends: effects run once ────────
// Since the helpers compose in any order, the second helper of a callback
// really asks, and the callback is re-entered from the top with the answer.
// Whatever the card does between the two helpers must not happen twice.

TEST_F(EnginePathTest, ThrillOfTheHuntBanishesItsUnitOnce) {
    // "Banish a friendly unit, then its owner plays it to any battlefield,
    // ignoring its cost." pickTarget, banish, pickMode: the banish ran again
    // when the spell was re-entered with the battlefield.
    constexpr CardDefId kThrillOfTheHunt = 745;
    auto spell = addToHand(P1, kThrillOfTheHunt);
    auto unit = addUnit(P1, kArmedAssailant, 3);   // at base
    int banished_events = 0;
    auto conn = events.on_object_state_changed.connect([&](const ObjectStateChangedEvent& e) {
        if (e.object == unit && e.what_changed == "banished_owned") ++banished_events;
    });

    // Prompts: the unit, then the battlefield (mode 1 = battlefield #1).
    EnginePathAgent agent([&](const std::vector<Intent>& legal) {
        for (const auto& i : legal)
            if (!i.chosen_objects.empty() && i.chosen_objects.front() == unit) return i;
        for (const auto& i : legal)
            if (i.chosen_value.has_value() && *i.chosen_value == 1) return i;
        return legal.front();
    });
    playThroughEngine(spell, &agent);
    conn.disconnect();

    ASSERT_EQ(agent.prompts.size(), 2u);
    EXPECT_EQ(banished_events, 1) << "banished once, not once per pass";
    EXPECT_EQ(state.player(P1).cards_banished_this_turn, 1);
    ASSERT_TRUE(state.getObject(unit).battlefieldId().has_value());
    EXPECT_EQ(*state.getObject(unit).battlefieldId(), BattlefieldId::fromRaw(1)) << "played to the chosen battlefield";
    const auto& bz = state.player(P1).banishment;
    EXPECT_EQ(std::count(bz.begin(), bz.end(), unit), 0) << "it is on the board, not in banishment";
}

TEST_F(EnginePathTest, ZenithBladeStunsOnce) {
    // "Stun an enemy unit at a battlefield. You may move a friendly unit to
    // that enemy unit's battlefield." pickTarget, stun, confirmOptional: the
    // stun ran again on re-entry. A second stun of a stunned unit is a no-op,
    // so the witness is a unit that REPLACES being stunned — Gangplank, Naval,
    // Empowered: "+3 [M] this turn instead" — which would get +6.
    constexpr CardDefId kGangplankNaval = 873;
    auto spell = addToHand(P1, kZenithBlade);
    auto gangplank = addUnit(P2, kGangplankNaval, 4, 0);
    state.getObject(gangplank).empowered = 1;
    auto friendly = addUnit(P1, kInvalidId, 3);   // at base: there is something to move

    EnginePathAgent agent([&](const std::vector<Intent>& legal) {
        for (const auto& i : legal)
            if (!i.chosen_objects.empty() && i.chosen_objects.front() == gangplank) return i;
        for (const auto& i : legal)
            if (i.chosen_value.has_value() && *i.chosen_value == 1) return i;   // Yes
        return legal.front();
    });
    playThroughEngine(spell, &agent);

    ASSERT_EQ(agent.prompts.size(), 2u) << "the enemy unit, then the may";
    EXPECT_FALSE(state.getObject(gangplank).is_stunned) << "replaced";
    EXPECT_EQ(state.getObject(gangplank).temp_might_bonus, 3) << "the stun was attempted once";
    ASSERT_TRUE(state.getObject(friendly).battlefieldId().has_value());
    EXPECT_EQ(*state.getObject(friendly).battlefieldId(), BattlefieldId::fromRaw(0)) << "and the move still happens";
}

// ─── 7.1 — target-less triggers, seen from the engine ───────────────────────
// Every card below prints a triggered ability that chooses something. Fired
// through the engine with a legal choice on the board, the controller must be
// asked (by the engine at finalization, or by the card's pickTarget —
// whichever mechanism 7.1 settles on). Today none of them asks: onTrigger
// returns on the empty `targets` the engine passes. Disabled until the cards
// are fixed; a card stream enables its rows by fixing the card.
struct TargetedTrigger {
    CardDefId def;
    TriggerType trigger;
    CardType type;
    const char* name;
};

class DISABLED_TargetedTriggerTest : public CardTestFixture,
                                     public ::testing::WithParamInterface<TargetedTrigger> {};

TEST_P(DISABLED_TargetedTriggerTest, TheControllerIsAskedToChoose) {
    const auto p = GetParam();
    // A board with something to choose for every card in the list: friendly
    // and enemy units here (battlefield 0) and at base, damaged and small
    // ones, a friendly gear attached and one loose, a gear in the trash.
    auto source = state.createObject();
    {
        auto& o = state.getObject(source);
        const auto& def = card_db.get(p.def);
        o.owner = o.controller = P1;
        o.card_def_id = p.def;
        o.card_type = p.type;
        o.name = def.name;
        o.keywords = def.keywords;
        o.tags = def.tags;
        o.base_might = o.current_might = def.might;
        if (p.type == CardType::Battlefield) {
            o.zone = ZoneType::BattlefieldZone;
            state.battlefields[0].card_object_id = source;
            state.battlefields[0].controller = P1;
        } else if (p.type == CardType::Gear) {
            o.zone = ZoneType::Base;
            o.location = BaseLocation{P1};
        } else {
            o.zone = ZoneType::BattlefieldZone;
            o.location = BattlefieldLocation{0};
        }
    }
    state.player(P1).cards_played_this_turn = 2;   // Legion
    auto ally_here = addUnit(P1, kInvalidId, 2, 0);
    addUnit(P1, kInvalidId, 2);
    state.getObject(ally_here).is_exhausted = true;
    addUnit(P2, kInvalidId, 2, 0);
    addUnit(P2, kInvalidId, 2);
    auto gear = state.createObject();
    {
        auto& g = state.getObject(gear);
        g.owner = g.controller = P1;
        g.card_type = CardType::Gear;
        g.name = "TestGear";
        g.tags = {"Equipment"};
        g.zone = ZoneType::Base;
        g.location = BaseLocation{P1};
        g.is_exhausted = true;
    }
    auto trashed = state.createObject();
    {
        auto& g = state.getObject(trashed);
        g.owner = g.controller = P1;
        g.card_type = CardType::Gear;
        g.name = "TrashedGear";
        g.zone = ZoneType::Trash;
        state.player(P1).trash.push_back(trashed);
    }

    EnginePathAgent agent;
    fireTriggerThroughEngine(source, p.trigger, &agent);
    EXPECT_FALSE(agent.prompts.empty())
        << p.name << " (" << p.def << "): its trigger chooses something, but the "
        << "controller was never asked — the printed effect did not happen";
}

// Every card that was listed here has been fixed by its card stream (the
// last row, Vi, Peacekeeper 738, by cards-c7); each has an engine-path test
// in its stream's test file. The probe stays for a future offender.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(DISABLED_TargetedTriggerTest);

}  // namespace
