/// @file test_wave_b_suppression.cpp
/// Wave B suppression cluster:
///   Mageseeker Warden (70) clause 2 — effects can't ready enemy units/gear
///   LeBlanc, Everywhere at Once (652) — Temporary triggers at her BF suppressed

#include "tests/cards/card_test_fixture.h"
#include "engine/trigger_manager.h"
#include "engine/chain_manager.h"
#include <gtest/gtest.h>

namespace riftbound::test {
namespace {

using SuppressionTest = CardTestFixture;

constexpr CardDefId kMageseekerWarden = 70;
constexpr CardDefId kLeBlanc = 652;
constexpr CardDefId kPrizeOfProgress = 398;  // fires on "gear_ability_used"

// ── Mageseeker Warden clause 2: effects can't ready enemy units/gear ──
TEST_F(SuppressionTest, MageseekerWarden_SuppressesEffectReady) {
    // Warden (P1) at a battlefield -> P2's units can't be readied by effects.
    addUnit(P1, kMageseekerWarden, 5, /*at_bf=*/0);
    card_registry.get(kMageseekerWarden)->applyPassiveAura(state, P1);
    EXPECT_TRUE(state.player(P2).effects_cant_ready_my_units);

    auto enemy = addUnit(P2, 1, 3, /*at_bf=*/0);
    state.getObject(enemy).is_exhausted = true;
    EffectExecutor exec(state, events, card_db);
    exec.readyObject(enemy);
    EXPECT_TRUE(state.getObject(enemy).is_exhausted)
        << "enemy unit must stay exhausted (Warden cl2)";

    // A friendly (P1) unit is unaffected.
    auto friendly = addUnit(P1, 1, 3, /*at_bf=*/0);
    state.getObject(friendly).is_exhausted = true;
    exec.readyObject(friendly);
    EXPECT_FALSE(state.getObject(friendly).is_exhausted);
}

TEST_F(SuppressionTest, MageseekerWarden_NoSuppressFromBase) {
    addUnit(P1, kMageseekerWarden, 5, /*at_bf=*/-1);  // at base, not a BF
    card_registry.get(kMageseekerWarden)->applyPassiveAura(state, P1);
    EXPECT_FALSE(state.player(P2).effects_cant_ready_my_units);
}

// ── LeBlanc: a Temporary unit's trigger at her BF is suppressed ──
TEST_F(SuppressionTest, LeBlanc_SuppressesTemporaryTriggerHere) {
    EXPECT_TRUE(card_registry.get(kLeBlanc)->suppressesTemporaryTriggersHere());

    addUnit(P1, kLeBlanc, 4, /*at_bf=*/0);
    auto prize = addUnit(P1, kPrizeOfProgress, 3, /*at_bf=*/0);
    state.getObject(prize).keywords.set(Keyword::Temporary);  // make it Temporary
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    size_t before = state.chain.items.size();
    events.emit(ObjectStateChangedEvent{prize, "gear_ability_used"});
    EXPECT_EQ(state.chain.items.size(), before)
        << "Temporary unit's trigger is suppressed at LeBlanc's battlefield";
}

TEST_F(SuppressionTest, LeBlanc_DoesNotSuppressNonTemporaryOrElsewhere) {
    addUnit(P1, kLeBlanc, 4, /*at_bf=*/0);
    // Prize is NOT Temporary -> fires normally even at LeBlanc's BF.
    auto prize = addUnit(P1, kPrizeOfProgress, 3, /*at_bf=*/0);
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    size_t before = state.chain.items.size();
    events.emit(ObjectStateChangedEvent{prize, "gear_ability_used"});
    EXPECT_GT(state.chain.items.size(), before)
        << "non-Temporary trigger fires normally";
}

}  // namespace
}  // namespace riftbound::test
