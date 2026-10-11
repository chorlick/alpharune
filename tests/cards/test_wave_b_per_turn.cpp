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

// Through the engine: real draws, the trigger condition is checked as the
// cards are drawn, and the controller chooses the friendly unit (card text:
// "give a friendly unit +2 [M]").
namespace frigid_jewel_tests {
GameObjectId addJewel(GameState& state, PlayerId owner) {
    auto id = state.createObject();
    auto& g = state.getObject(id);
    g.owner = g.controller = owner;
    g.card_type = CardType::Gear;
    g.card_def_id = 636;
    g.name = "Frigid Jewel";
    g.zone = ZoneType::Base;
    g.location = BaseLocation{owner};
    return id;
}
}  // namespace frigid_jewel_tests

TEST_F(PerTurnTest, FrigidJewel_BuffsAFriendlyUnitOnSecondDraw) {
    frigid_jewel_tests::addJewel(state, P1);
    auto other = addUnit(P1, 1, 4, 0);
    auto unit = addUnit(P1, 1, 4, 0);
    for (int i = 0; i < 4; ++i) addToDeck(P1, kInvalidId);
    state.player(P1).draws_this_turn = 1;
    const int before = state.getObject(unit).current_might;

    EnginePathAgent agent(EnginePathAgent::choosing(unit));
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) { exec.drawCards(P1, 1); },
                         &agent);
    EXPECT_TRUE(agent.wasOffered(other)) << "the controller chooses which friendly unit";
    EXPECT_EQ(state.getObject(unit).current_might, before + 2)
        << "2nd draw -> the chosen friendly unit +2 [M]";
    EXPECT_EQ(state.getObject(other).current_might, before);

    // Once per turn: a 3rd draw does not trigger it again.
    EnginePathAgent again(EnginePathAgent::choosing(unit));
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) { exec.drawCards(P1, 1); },
                         &again);
    EXPECT_TRUE(again.prompts.empty());
    EXPECT_EQ(state.getObject(unit).current_might, before + 2)
        << "Frigid Jewel only fires on the 2nd draw each turn";
}

TEST_F(PerTurnTest, FrigidJewel_NoBuffBeforeSecondDraw) {
    frigid_jewel_tests::addJewel(state, P1);
    auto unit = addUnit(P1, 1, 4, 0);
    for (int i = 0; i < 2; ++i) addToDeck(P1, kInvalidId);
    state.player(P1).draws_this_turn = 0;   // this is the first draw
    const int before = state.getObject(unit).current_might;
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) { exec.drawCards(P1, 1); });
    EXPECT_EQ(state.getObject(unit).current_might, before);
}

TEST_F(PerTurnTest, Sivir_GainsMightAndGankingWhenPowerSpent) {
    auto sivir = addUnit(P1, 464, 5, 0);
    Card* c = card_registry.get(464);
    ASSERT_NE(c, nullptr);

    // < 2 power spent -> no aura.
    state.player(P1).power_spent_this_turn = 1;
    state.getObject(sivir).aura_effects.clear();
    c->applyPassiveAura(state, P1);
    EXPECT_TRUE(state.getObject(sivir).aura_effects.empty());

    // >= 2 power spent -> +2 [M] and [Ganking].
    state.player(P1).power_spent_this_turn = 2;
    state.getObject(sivir).aura_effects.clear();
    c->applyPassiveAura(state, P1);
    int mb = 0; bool gank = false;
    for (auto& ae : state.getObject(sivir).aura_effects) {
        mb += ae.might_bonus;
        if (ae.keyword == Keyword::Ganking) gank = true;
    }
    EXPECT_EQ(mb, 2);
    EXPECT_TRUE(gank);
}

}  // namespace
}  // namespace riftbound::test
