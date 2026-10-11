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

TEST(FixtureContractPickers, ChoosingPicksTheObjectWhenOffered) {
    auto pick = EnginePathAgent::choosing(7);
    EXPECT_EQ(pick(objectPrompt({3, 7, 9})).chosen_objects.front(), 7u);
}

TEST(FixtureContractPickers, ChoosingFailsTheTestAtAnUnannouncedMay) {
    auto pick = EnginePathAgent::choosing(7);
    Intent answer;
    EXPECT_NONFATAL_FAILURE(answer = pick(mayPrompt()), "did not announce");
    EXPECT_EQ(answer.chosen_value.value_or(-1), 0) << "and it does not take the yes branch";
}

TEST(FixtureContractPickers, ChoosingAnswersAnAnnouncedMay) {
    EXPECT_EQ(EnginePathAgent::choosing(7, EnginePathAgent::May::Yes)(mayPrompt())
                  .chosen_value.value_or(-1), 1);
    EXPECT_EQ(EnginePathAgent::choosing(7, EnginePathAgent::May::No)(mayPrompt())
                  .chosen_value.value_or(-1), 0);
}

TEST(FixtureContractPickers, ChoosingFailsTheTestAtAModeOrUnlabelledYesNo) {
    auto pick = EnginePathAgent::choosing(7, EnginePathAgent::May::Yes);
    Intent zero, one;
    zero.type = one.type = IntentType::MakeChoice;
    zero.chosen_value = 0;
    one.chosen_value = 1;
    EXPECT_NONFATAL_FAILURE(pick({zero, one}), "use script()");
}

TEST(FixtureContractPickers, ChoosingTakesTheOnlyOptionAndTheFirstOfOtherObjects) {
    auto pick = EnginePathAgent::choosing(7);
    EXPECT_EQ(pick(objectPrompt({3})).chosen_objects.front(), 3u);
    EXPECT_EQ(pick(objectPrompt({3, 9})).chosen_objects.front(), 3u)
        << "7 is not offered: a test of that asserts on wasOffered";
}

} // namespace riftbound::test::fixture_contract
