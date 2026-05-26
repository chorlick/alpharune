/// @file test_wave_b_per_turn.cpp
/// Wave B per-turn tracking flags. Towering Pairofant (570):
/// "[Assault] If a unit died this turn, I enter ready." reads
/// TurnState::any_unit_died_this_turn.

#include "tests/cards/card_test_fixture.h"
#include <gtest/gtest.h>

namespace riftbound::test {
namespace {

using PerTurnTest = CardTestFixture;

TEST_F(PerTurnTest, ToweringPairofant_EntersReadyOnlyIfUnitDied) {
    Card* c = card_registry.get(570);
    ASSERT_NE(c, nullptr);

    state.turn.any_unit_died_this_turn = false;
    EXPECT_FALSE(c->entersReadyOnPlay(state, P1))
        << "no unit died -> Towering Pairofant enters exhausted";

    state.turn.any_unit_died_this_turn = true;
    EXPECT_TRUE(c->entersReadyOnPlay(state, P1))
        << "a unit died this turn -> Towering Pairofant enters ready";
}

TEST_F(PerTurnTest, ShadowWatcher_EntersReadyIfFriendlyDiedInBeginning) {
    Card* c = card_registry.get(599);
    ASSERT_NE(c, nullptr);

    state.player(P1).unit_died_in_beginning_this_turn = false;
    EXPECT_FALSE(c->entersReadyOnPlay(state, P1));

    state.player(P1).unit_died_in_beginning_this_turn = true;
    EXPECT_TRUE(c->entersReadyOnPlay(state, P1));

    // It's controller-scoped: an enemy's flag doesn't ready P1's Shadow Watcher.
    state.player(P1).unit_died_in_beginning_this_turn = false;
    state.player(P2).unit_died_in_beginning_this_turn = true;
    EXPECT_FALSE(c->entersReadyOnPlay(state, P1));
}

TEST_F(PerTurnTest, FrigidJewel_BuffsAFriendlyUnitOnSecondDraw) {
    auto jewel = addUnit(P1, 636, 0, 0);   // gear stand-in (skipped as target)
    auto unit = addUnit(P1, 1, 4, 0);
    state.player(P1).draws_this_turn = 2;
    int before = state.getObject(unit).current_might;
    EffectExecutor exec(state, events, card_db);
    fireTriggerAs(636, P1, jewel, TriggerType::WhenYouDrawACard, exec);
    EXPECT_EQ(state.getObject(unit).current_might, before + 2)
        << "2nd draw -> a friendly unit +2 [M]";
    // Once per turn: a 3rd draw fires the trigger again but does nothing more.
    fireTriggerAs(636, P1, jewel, TriggerType::WhenYouDrawACard, exec);
    EXPECT_EQ(state.getObject(unit).current_might, before + 2)
        << "Frigid Jewel only fires on the 2nd draw each turn";
}

TEST_F(PerTurnTest, FrigidJewel_NoBuffBeforeSecondDraw) {
    auto jewel = addUnit(P1, 636, 0, 0);
    auto unit = addUnit(P1, 1, 4, 0);
    state.player(P1).draws_this_turn = 1;   // only one draw so far
    int before = state.getObject(unit).current_might;
    EffectExecutor exec(state, events, card_db);
    fireTriggerAs(636, P1, jewel, TriggerType::WhenYouDrawACard, exec);
    EXPECT_EQ(state.getObject(unit).current_might, before);
}

}  // namespace
}  // namespace riftbound::test
