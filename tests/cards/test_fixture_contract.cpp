/// @file test_fixture_contract.cpp
/// What the shared card test fixture promises (tests/cards/card_test_fixture.h),
/// pinned so a test author can rely on it:
///   - a unit built without a card id is not a token (plan item 9.G11);
///   - hand-set battlefield control needs a unit to survive a cleanup (9.G11);
///   - EnginePathAgent's defaults and pickers never answer a prompt the test
///     did not announce in a way that changes the game (9.D8, 9.H5);
///   - every engine-path helper ends with the engine's cleanup (9.F1).

#include "card_test_fixture.h"

#include <gtest/gtest-spi.h>

namespace riftbound::test::fixture_contract {

class FixtureContractTest : public CardTestFixture {};

// ── 9.G11: a unit with no card id ───────────────────────────────────────────

// The report read: "the engine marks every unit with card_def_id == kInvalidId
// as a token". It does not: such a unit is an ordinary non-token unit. Killed,
// it goes to its owner's trash and is listed there; a token would go to
// banishment and be listed nowhere (CR 183.1).
TEST_F(FixtureContractTest, AUnitWithoutACardIdIsNotAToken) {
    auto unit = addUnit(P2, kInvalidId, 2, 0);
    EXPECT_FALSE(state.getObject(unit).isToken());

    resolveThroughEngine([&](EffectExecutor& exec, GameState&) { exec.killObject(unit); });
    EXPECT_FALSE(state.getObject(unit).isToken()) << "the engine did not mark it either";
    EXPECT_EQ(state.getObject(unit).zone, ZoneType::Trash);
    EXPECT_TRUE(inTrash(P2, unit));
}

// The same unit, lethally damaged, is killed by the cleanup every engine-path
// helper runs (9.F1) and goes to the trash too.
TEST_F(FixtureContractTest, ALethallyDamagedUnitDoesNotOutliveAnEnginePathHelper) {
    auto unit = addUnit(P2, kInvalidId, 2);
    auto source = addUnit(P1, kInvalidId, 1);
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) {
        exec.dealDamage(unit, 2, source);
    });
    EXPECT_TRUE(inTrash(P2, unit));
}

// ── 9.G11: control set by hand ──────────────────────────────────────────────

TEST_F(FixtureContractTest, HandSetControlNeedsAUnitToSurviveACleanup) {
    state.battlefields[0].controller = P1;      // nobody there
    state.battlefields[1].controller = P1;
    addUnit(P1, kInvalidId, 2, 1);              // a unit of P1's there
    withEngine([](GameEngine& e) { e.testHook_cleanup(); });
    EXPECT_FALSE(state.battlefields[0].controller.has_value())
        << "control with no unit is lost at cleanup";
    ASSERT_TRUE(state.battlefields[1].controller.has_value());
    EXPECT_EQ(*state.battlefields[1].controller, P1);
}

// ── 9.D8: Predict with no picker keeps the card ─────────────────────────────

// withEngine sets the turn player only while the game has not started
// (phase Setup); afterwards the state's turn player stands. A turn player a
// test NAMES that differs from the one standing would be ignored silently, so
// it is refused (testing phase B8).
TEST_F(FixtureContractTest, WithEngineRefusesATurnPlayerItWouldIgnore) {
    withEngine([](GameEngine&) {}, nullptr, nullptr, P1);
    EXPECT_EQ(state.turn.turn_player, P1);
    EXPECT_THROW(withEngine([](GameEngine&) {}, nullptr, nullptr, P2), std::logic_error);
    EXPECT_NO_THROW(withEngine([](GameEngine&) {}, nullptr, nullptr, P1)) << "the same one";
    EXPECT_NO_THROW(withEngine([](GameEngine&) {})) << "none named";
    // (The helpers that play a card pass its controller only as a default, so
    // P2 may play a card on P1's turn: the rest of the suite does that.)
}

TEST_F(FixtureContractTest, PredictWithNoPickerKeepsTheCard) {
    addToDeck(P1, kInvalidId);
    auto top = addToDeck(P1, kInvalidId);
    EnginePathAgent agent;                      // no picker
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) { exec.predict(P1, 1); }, &agent);
    EXPECT_EQ(agent.prompts.size(), 1u);
    EXPECT_EQ(agent.predict_kept_by_default, 1);
    EXPECT_EQ(state.player(P1).main_deck.back(), top) << "still on top";
}

TEST_F(FixtureContractTest, PredictRecyclesOnlyWhenThePickerSaysSo) {
    addToDeck(P1, kInvalidId);
    auto top = addToDeck(P1, kInvalidId);
    EnginePathAgent agent(EnginePathAgent::first());
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) { exec.predict(P1, 1); }, &agent);
    EXPECT_EQ(agent.predict_kept_by_default, 0);
    EXPECT_EQ(state.player(P1).main_deck.front(), top) << "on the bottom";
}

// 9.S2: accepting() accepts every "may", and Predict's recycle is one.
TEST_F(FixtureContractTest, AcceptingRecyclesAtAPredictPrompt) {
    addToDeck(P1, kInvalidId);
    auto top = addToDeck(P1, kInvalidId);
    AcceptingEnginePathAgent agent;
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) { exec.predict(P1, 1); }, &agent);
    EXPECT_EQ(agent.prompts.size(), 1u);
    EXPECT_EQ(agent.predict_kept_by_default, 0);
    EXPECT_EQ(state.player(P1).main_deck.front(), top) << "recycled: on the bottom";
}

// ── 9.H5: choosing(id) and a prompt the test did not announce ───────────────

namespace {
std::vector<Intent> mayPrompt() {
    Intent yes, no;
    yes.type = no.type = IntentType::MakeChoice;
    yes.chosen_value = 1;
    yes.choice_label = "Yes — some other card's \"you may\"";
    no.chosen_value = 0;
    no.choice_label = "No — decline";
    return {no, yes};
}
std::vector<Intent> objectPrompt(std::initializer_list<GameObjectId> ids) {
    std::vector<Intent> out;
    for (auto id : ids) {
        Intent i;
        i.type = IntentType::MakeChoice;
        i.chosen_objects = {id};
        out.push_back(i);
    }
    return out;
}
} // namespace

// script(): an entry answers its own prompt or the test fails (9.AA4).
TEST(FixtureContractPickers, ScriptAnswersByValueThenByObject) {
    auto pick = EnginePathAgent::script({1, 9});
    EXPECT_EQ(pick(mayPrompt()).chosen_value.value_or(-1), 1);
    EXPECT_EQ(pick(objectPrompt({GameObjectId::fromRaw(3), GameObjectId::fromRaw(7), GameObjectId::fromRaw(9)})).chosen_objects.front(), GameObjectId::fromRaw(9));
}

TEST(FixtureContractPickers, ScriptFailsTheTestWhenAnEntryMatchesNothing) {
    auto pick = EnginePathAgent::script({8});
    Intent answer;
    EXPECT_NONFATAL_FAILURE(answer = pick(objectPrompt({GameObjectId::fromRaw(3), GameObjectId::fromRaw(7), GameObjectId::fromRaw(9)})), "matches no option");
    EXPECT_EQ(answer.chosen_objects.front(), GameObjectId::fromRaw(3));
}

TEST(FixtureContractPickers, ScriptFailsTheTestAtAPromptAfterItRanOut) {
    auto pick = EnginePathAgent::script({7});
    EXPECT_EQ(pick(objectPrompt({GameObjectId::fromRaw(3), GameObjectId::fromRaw(7), GameObjectId::fromRaw(9)})).chosen_objects.front(), GameObjectId::fromRaw(7));
    EXPECT_NONFATAL_FAILURE(pick(mayPrompt()), "after the script ran out");
}

TEST(FixtureContractPickers, ScriptTakesTheFirstOptionOnlyWhenToldTo) {
    auto pick = EnginePathAgent::script({EnginePathAgent::kFirst, 7, EnginePathAgent::kRest});
    EXPECT_EQ(pick(objectPrompt({GameObjectId::fromRaw(3), GameObjectId::fromRaw(7), GameObjectId::fromRaw(9)})).chosen_objects.front(), GameObjectId::fromRaw(3)) << "kFirst";
    EXPECT_EQ(pick(objectPrompt({GameObjectId::fromRaw(3), GameObjectId::fromRaw(7), GameObjectId::fromRaw(9)})).chosen_objects.front(), GameObjectId::fromRaw(7));
    EXPECT_EQ(pick(objectPrompt({GameObjectId::fromRaw(5), GameObjectId::fromRaw(6)})).chosen_objects.front(), GameObjectId::fromRaw(5)) << "kRest";
    EXPECT_EQ(pick(mayPrompt()).chosen_value.value_or(-1), 0) << "kRest goes on";
}

TEST(FixtureContractPickers, ChoosingPicksTheObjectWhenOffered) {
    auto pick = EnginePathAgent::choosing(GameObjectId::fromRaw(7));
    EXPECT_EQ(pick(objectPrompt({GameObjectId::fromRaw(3), GameObjectId::fromRaw(7), GameObjectId::fromRaw(9)})).chosen_objects.front(), GameObjectId::fromRaw(7));
}

TEST(FixtureContractPickers, ChoosingFailsTheTestAtAnUnannouncedMay) {
    auto pick = EnginePathAgent::choosing(GameObjectId::fromRaw(7));
    Intent answer;
    EXPECT_NONFATAL_FAILURE(answer = pick(mayPrompt()), "did not announce");
    EXPECT_EQ(answer.chosen_value.value_or(-1), 0) << "and it does not take the yes branch";
}

TEST(FixtureContractPickers, ChoosingAnswersAnAnnouncedMay) {
    EXPECT_EQ(EnginePathAgent::choosing(GameObjectId::fromRaw(7), EnginePathAgent::May::Yes)(mayPrompt())
                  .chosen_value.value_or(-1), 1);
    EXPECT_EQ(EnginePathAgent::choosing(GameObjectId::fromRaw(7), EnginePathAgent::May::No)(mayPrompt())
                  .chosen_value.value_or(-1), 0);
}

TEST(FixtureContractPickers, ChoosingFailsTheTestAtAModeOrUnlabelledYesNo) {
    auto pick = EnginePathAgent::choosing(GameObjectId::fromRaw(7), EnginePathAgent::May::Yes);
    Intent zero, one;
    zero.type = one.type = IntentType::MakeChoice;
    zero.chosen_value = 0;
    one.chosen_value = 1;
    EXPECT_NONFATAL_FAILURE(pick({zero, one}), "use script()");
}

namespace {
std::vector<Intent> noneOr(std::initializer_list<GameObjectId> ids) {
    Intent none;
    none.type = IntentType::MakeChoice;
    none.chosen_value = 0;
    none.choice_label = "No more targets";
    std::vector<Intent> out{none};
    for (const auto& i : objectPrompt(ids)) out.push_back(i);
    return out;
}
} // namespace

// "Choose a target ... choose another or stop": once the object has been
// chosen, the picker stops.
TEST(FixtureContractPickers, ChoosingStopsAtANoneOptionAfterItsObjectWasChosen) {
    auto pick = EnginePathAgent::choosing(GameObjectId::fromRaw(7));
    EXPECT_EQ(pick(noneOr({GameObjectId::fromRaw(3), GameObjectId::fromRaw(7)})).chosen_objects.front(), GameObjectId::fromRaw(7)) << "offered: chosen";
    EXPECT_TRUE(pick(noneOr({GameObjectId::fromRaw(3), GameObjectId::fromRaw(9)})).chosen_objects.empty()) << "7 was chosen: no more";
}

// The negative case: the object was never offered and "none" is available.
// Choosing nothing would let a test pass whose target is wrongly not offered,
// so this fails the test.
TEST(FixtureContractPickers, ChoosingFailsAtANoneOptionWhenItsObjectWasNeverOffered) {
    auto pick = EnginePathAgent::choosing(GameObjectId::fromRaw(7));
    EXPECT_NONFATAL_FAILURE(pick(noneOr({GameObjectId::fromRaw(3), GameObjectId::fromRaw(9)})), "did not announce");
}

TEST(FixtureContractPickers, ChoosingTakesTheOnlyOptionAndTheFirstOfOtherObjects) {
    auto pick = EnginePathAgent::choosing(GameObjectId::fromRaw(7));
    EXPECT_EQ(pick(objectPrompt({GameObjectId::fromRaw(3)})).chosen_objects.front(), GameObjectId::fromRaw(3));
    EXPECT_EQ(pick(objectPrompt({GameObjectId::fromRaw(3), GameObjectId::fromRaw(9)})).chosen_objects.front(), GameObjectId::fromRaw(3))
        << "7 is not offered: a test of that asserts on wasOffered";
}

} // namespace riftbound::test::fixture_contract
