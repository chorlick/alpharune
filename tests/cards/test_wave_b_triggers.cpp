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

}  // namespace
}  // namespace riftbound::test
