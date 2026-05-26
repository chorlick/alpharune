/// @file test_wave_b_buff_trigger.cpp
/// Wave B primitive: TriggerType::WhenIAmBuffed, dispatched by
/// TriggerManager::onObjectStateChanged on the "buffed" ObjectStateChangedEvent.
/// Card under test: Simian Ancestor (370) — "When you buff me, ready me."

#include "tests/cards/card_test_fixture.h"
#include "engine/trigger_manager.h"
#include "engine/chain_manager.h"
#include <gtest/gtest.h>

namespace riftbound::test {
namespace {

using BuffTriggerTest = CardTestFixture;
constexpr CardDefId kSimianAncestor = 370;

// Engine dispatch: buffing Simian Ancestor enqueues its WhenIAmBuffed ability.
TEST_F(BuffTriggerTest, BuffSimian_EnqueuesTrigger) {
    auto simian = addUnit(P1, kSimianAncestor, /*might=*/5, /*at_bf=*/0);
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    EffectExecutor exec(state, events, card_db);
    size_t before = state.chain.items.size();
    exec.buffUnit(simian);                       // emits ObjectStateChangedEvent{"buffed"}
    EXPECT_GT(state.chain.items.size(), before)
        << "Simian Ancestor's WhenIAmBuffed ability should enqueue on buff";
}

// Control: a vanilla unit does NOT enqueue anything when buffed.
TEST_F(BuffTriggerTest, BuffVanilla_NoTrigger) {
    auto other = addUnit(P1, 1, /*might=*/5, /*at_bf=*/0);  // Blazing Scorcher (vanilla)
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    EffectExecutor exec(state, events, card_db);
    size_t before = state.chain.items.size();
    exec.buffUnit(other);
    EXPECT_EQ(state.chain.items.size(), before)
        << "a card without WhenIAmBuffed must not enqueue on buff";
}

// Card effect: when the trigger fires, Simian Ancestor readies itself.
TEST_F(BuffTriggerTest, SimianTrigger_ReadiesSelf) {
    auto simian = addUnit(P1, kSimianAncestor, 5, 0);
    state.getObject(simian).is_exhausted = true;
    EffectExecutor exec(state, events, card_db);
    fireTriggerAs(kSimianAncestor, P1, simian, TriggerType::WhenIAmBuffed, exec);
    EXPECT_FALSE(state.getObject(simian).is_exhausted)
        << "Simian Ancestor should ready itself when its buff trigger resolves";
}

}  // namespace
}  // namespace riftbound::test
