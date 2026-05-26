/// @file test_wave_b_triggers.cpp
/// Wave B declared-trigger cluster.
///   Prize of Progress (398) — WhenYouActivateAGearAbility (gear's resolved
///   activated ability emits "gear_ability_used").

#include "tests/cards/card_test_fixture.h"
#include "engine/trigger_manager.h"
#include "engine/chain_manager.h"
#include <gtest/gtest.h>

namespace riftbound::test {
namespace {

using TriggerClusterTest = CardTestFixture;
constexpr CardDefId kPrizeOfProgress = 398;

TEST_F(TriggerClusterTest, PrizeOfProgress_EnqueuesOnGearAbility) {
    addUnit(P1, kPrizeOfProgress, 3, 0);
    auto gear = addUnit(P1, 1, 0, 0);  // a P1-controlled object (the "gear")
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    size_t before = state.chain.items.size();
    events.emit(ObjectStateChangedEvent{gear, "gear_ability_used"});
    EXPECT_GT(state.chain.items.size(), before)
        << "a gear ability use should enqueue Prize of Progress";
}

TEST_F(TriggerClusterTest, PrizeOfProgress_SelfBuffsOnTrigger) {
    auto prize = addUnit(P1, kPrizeOfProgress, 3, 0);
    int before = state.getObject(prize).current_might;
    EffectExecutor exec(state, events, card_db);
    fireTriggerAs(kPrizeOfProgress, P1, prize,
                  TriggerType::WhenYouActivateAGearAbility, exec);
    EXPECT_EQ(state.getObject(prize).current_might, before + 1);
}

// ── Stealthy Pursuer (177): WhenAFriendlyUnitMovesFromMyLocation ──
constexpr CardDefId kStealthyPursuer = 177;

TEST_F(TriggerClusterTest, StealthyPursuer_EnqueuesWithMoverSubject) {
    auto stealthy = addUnit(P1, kStealthyPursuer, 3, /*at_bf=*/0);
    auto mover = addUnit(P1, 1, 4, /*at_bf=*/0);   // shares Stealthy's location
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    size_t before = state.chain.items.size();
    events.emit(UnitMovedEvent{mover, P1, BattlefieldLocation{0},
                               BattlefieldLocation{1}, true});
    ASSERT_GT(state.chain.items.size(), before)
        << "a friendly unit leaving Stealthy's location should enqueue it";
    EXPECT_EQ(state.chain.items.back().triggering_subject, mover);
    EXPECT_EQ(state.chain.items.back().source, stealthy);
}

TEST_F(TriggerClusterTest, StealthyPursuer_FollowsTheMover) {
    auto stealthy = addUnit(P1, kStealthyPursuer, 3, /*at_bf=*/0);
    auto mover = addUnit(P1, 1, 4, /*at_bf=*/1);    // already moved to bf 1
    EffectExecutor exec(state, events, card_db);
    // Drive the resumable confirmOptional with a "yes", subject = mover.
    ChainItem ri; ri.id = state.chain.allocateId(); ri.controller = P1;
    ri.is_ability = true; ri.source = stealthy; ri.card_def_id = kStealthyPursuer;
    ri.fired_trigger = TriggerType::WhenAFriendlyUnitMovesFromMyLocation;
    ri.triggering_subject = mover;
    state.chain.resuming = ri;
    Card* card = card_registry.get(kStealthyPursuer);
    CardContext ctx{state, events, exec, P1, stealthy};
    ctx.firing_trigger = TriggerType::WhenAFriendlyUnitMovesFromMyLocation;
    card->onTrigger(ctx, {});
    while (exec.hasPendingChoice()) {
        auto p = exec.consumePendingChoice();
        exec.recordChoice(p.legal.back());   // "yes" (confirmOptional publishes [no, yes])
        card->onTrigger(ctx, {});
    }
    state.chain.resuming.reset();
    auto bf = state.getObject(stealthy).battlefieldId();
    ASSERT_TRUE(bf.has_value());
    EXPECT_EQ(*bf, 1u) << "Stealthy Pursuer follows the moved unit to bf 1";
}

}  // namespace
}  // namespace riftbound::test
