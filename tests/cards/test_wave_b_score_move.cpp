/// @file test_wave_b_score_move.cpp
/// Wave B triggers keyed off existing events:
///   WhenOpponentScores (Sumpworks Map), WhenAUnitMovesFromHere (Back-Alley Bar),
///   WhenAnOpponentMovesToBattlefield (Volibear, Imposing).

#include "tests/cards/card_test_fixture.h"
#include "engine/trigger_manager.h"
#include "engine/chain_manager.h"
#include <gtest/gtest.h>

namespace riftbound::test {
namespace {

using ScoreMoveTest = CardTestFixture;
constexpr CardDefId kSumpworksMap = 647;
constexpr CardDefId kBackAlleyBar = 272;
constexpr CardDefId kVolibearImposing = 158;

// ── Sumpworks Map: "when an opponent scores, draw 1" ──
TEST_F(ScoreMoveTest, SumpworksMap_EnqueuesWhenOpponentScores) {
    addUnit(P2, kSumpworksMap, 0, 0);          // P2 owns the gear (on board)
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    size_t before = state.chain.items.size();
    events.emit(ScoreEvent{P1, /*bf=*/0, ScoreMethod::Conquer, /*new_score=*/1});
    EXPECT_GT(state.chain.items.size(), before)
        << "P1 scoring should enqueue P2's Sumpworks Map";
}

TEST_F(ScoreMoveTest, SumpworksMap_NoFireOnOwnScore) {
    addUnit(P1, kSumpworksMap, 0, 0);          // P1 owns it
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    size_t before = state.chain.items.size();
    events.emit(ScoreEvent{P1, 0, ScoreMethod::Conquer, 1});  // owner scores
    EXPECT_EQ(state.chain.items.size(), before)
        << "Sumpworks Map must not fire when its OWNER scores";
}

TEST_F(ScoreMoveTest, SumpworksMap_DrawsOne) {
    auto map = addUnit(P1, kSumpworksMap, 0, 0);
    addToDeck(P1, 1); addToDeck(P1, 1);        // cards to draw
    size_t hand_before = state.player(P1).hand.size();
    EffectExecutor exec(state, events, card_db);
    fireTriggerAs(kSumpworksMap, P1, map, TriggerType::WhenOpponentScores, exec);
    EXPECT_EQ(state.player(P1).hand.size(), hand_before + 1);
}

// ── Back-Alley Bar: "when a unit moves from here, give it +1 [M] this turn" ──
TEST_F(ScoreMoveTest, BackAlleyBar_GivesDepartedUnitTempMight) {
    auto bar = addUnit(P1, kBackAlleyBar, 0, 0);   // stand-in object for the bf card
    auto unit = addUnit(P1, 1, 4, 0);
    int before = state.getObject(unit).current_might;
    EffectExecutor exec(state, events, card_db);
    ChainItem ri; ri.source = bar; ri.controller = P1;
    ri.fired_trigger = TriggerType::WhenAUnitMovesFromHere;
    ri.triggering_subject = unit;
    state.chain.resuming = ri;
    fireTriggerAs(kBackAlleyBar, P1, bar, TriggerType::WhenAUnitMovesFromHere, exec);
    state.chain.resuming.reset();
    EXPECT_EQ(state.getObject(unit).current_might, before + 1);
}

// ── Volibear, Imposing: "when an opponent moves to a battlefield other than
//    mine, draw 1" ──
TEST_F(ScoreMoveTest, Volibear_EnqueuesOnEnemyMove) {
    addUnit(P1, kVolibearImposing, 6, /*at_bf=*/0);    // Volibear at bf 0
    auto enemy = addUnit(P2, 1, 3, /*at_bf=*/1);        // enemy at bf 1
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    size_t before = state.chain.items.size();
    events.emit(UnitMovedEvent{enemy, P2, BaseLocation{P2},
                               BattlefieldLocation{1}, true});
    EXPECT_GT(state.chain.items.size(), before)
        << "an enemy moving to a battlefield should enqueue Volibear";
}

TEST_F(ScoreMoveTest, Volibear_DrawsWhenEnemyMovesElsewhere) {
    auto voli = addUnit(P1, kVolibearImposing, 6, /*at_bf=*/0);
    auto enemy = addUnit(P2, 1, 3, /*at_bf=*/1);        // moved to bf 1 (not mine)
    addToDeck(P1, 1);
    size_t hand_before = state.player(P1).hand.size();
    EffectExecutor exec(state, events, card_db);
    ChainItem ri; ri.source = voli; ri.controller = P1;
    ri.fired_trigger = TriggerType::WhenAnOpponentMovesToBattlefield;
    ri.triggering_subject = enemy;
    state.chain.resuming = ri;
    fireTriggerAs(kVolibearImposing, P1, voli,
                  TriggerType::WhenAnOpponentMovesToBattlefield, exec);
    state.chain.resuming.reset();
    EXPECT_EQ(state.player(P1).hand.size(), hand_before + 1);
}

TEST_F(ScoreMoveTest, Volibear_NoDrawWhenEnemyMovesToMyBattlefield) {
    auto voli = addUnit(P1, kVolibearImposing, 6, /*at_bf=*/0);
    auto enemy = addUnit(P2, 1, 3, /*at_bf=*/0);        // SAME bf as Volibear
    addToDeck(P1, 1);
    size_t hand_before = state.player(P1).hand.size();
    EffectExecutor exec(state, events, card_db);
    ChainItem ri; ri.source = voli; ri.controller = P1;
    ri.fired_trigger = TriggerType::WhenAnOpponentMovesToBattlefield;
    ri.triggering_subject = enemy;
    state.chain.resuming = ri;
    fireTriggerAs(kVolibearImposing, P1, voli,
                  TriggerType::WhenAnOpponentMovesToBattlefield, exec);
    state.chain.resuming.reset();
    EXPECT_EQ(state.player(P1).hand.size(), hand_before)
        << "no draw when the enemy moves to Volibear's own battlefield";
}

}  // namespace
}  // namespace riftbound::test
