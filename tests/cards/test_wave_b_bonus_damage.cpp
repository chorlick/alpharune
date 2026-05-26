/// @file test_wave_b_bonus_damage.cpp
/// Wave B: generic Bonus-Damage + "can't gain points" continuous effects.
///   Annie, Fiery (controller bonus_damage_dealt), Void Gate (per-location
///   aura_bonus_damage_taken), Tianna Crownguard (opponent cannot_gain_points).
///   dealDamage adds the bonus to spell/ability damage.

#include "tests/cards/card_test_fixture.h"
#include <gtest/gtest.h>

namespace riftbound::test {
namespace {

using BonusDamageTest = CardTestFixture;
constexpr CardDefId kAnnieFiery = 299;
constexpr CardDefId kVoidGate = 291;
constexpr CardDefId kTianna = 383;

TEST_F(BonusDamageTest, AnnieRaisesControllerBonusDamage) {
    addUnit(P1, kAnnieFiery, 4, 0);
    Card* c = card_registry.get(kAnnieFiery);
    ASSERT_NE(c, nullptr);
    state.player(P1).bonus_damage_dealt = 0;
    c->applyPassiveAura(state, P1);
    EXPECT_EQ(state.player(P1).bonus_damage_dealt, 1);
}

TEST_F(BonusDamageTest, VoidGateAddsBonusToUnitsHere) {
    // Battlefield 0 backed by a Void Gate card object; a unit sits there.
    auto gate_obj = addUnit(P1, kVoidGate, 0, 0);     // stand-in object for the BF card
    state.battlefields[0].card_object_id = gate_obj;
    auto unit = addUnit(P1, 1, 6, /*at_bf=*/0);
    auto elsewhere = addUnit(P1, 1, 6, /*at_bf=*/1);   // not at the Void Gate
    Card* c = card_registry.get(kVoidGate);
    ASSERT_NE(c, nullptr);
    c->applyPassiveAura(state, PlayerId::None);
    int here = 0, there = 0;
    for (auto& ae : state.getObject(unit).aura_effects) here += ae.bonus_damage_taken;
    for (auto& ae : state.getObject(elsewhere).aura_effects) there += ae.bonus_damage_taken;
    EXPECT_EQ(here, 1) << "units at the Void Gate take +1 bonus damage";
    EXPECT_EQ(there, 0) << "units elsewhere are unaffected";
}

TEST_F(BonusDamageTest, TiannaLocksOpponentScoringWhenAtBattlefield) {
    Card* c = card_registry.get(kTianna);
    ASSERT_NE(c, nullptr);
    // At a battlefield -> opponent can't gain points.
    auto t = addUnit(P1, kTianna, 5, /*at_bf=*/0);
    state.player(P2).cannot_gain_points = false;
    c->applyPassiveAura(state, P1);
    EXPECT_TRUE(state.player(P2).cannot_gain_points);
    // At base (not a battlefield) -> no lock.
    state.getObject(t).location = BaseLocation{P1};
    state.getObject(t).zone = ZoneType::Base;
    state.player(P2).cannot_gain_points = false;
    c->applyPassiveAura(state, P1);
    EXPECT_FALSE(state.player(P2).cannot_gain_points);
}

TEST_F(BonusDamageTest, DealDamageAppliesBonusToSpellAbilityDamage) {
    // A spell-source dealing damage to a unit with +1 bonus_damage_taken.
    auto target = addUnit(P1, 1, /*might=*/10, 0);
    state.getObject(target).aura_bonus_damage_taken = 1;
    // Source object on the Chain → counts as spell/ability damage.
    auto src = state.createObject();
    state.getObject(src).zone = ZoneType::Chain;
    state.getObject(src).controller = P2;
    EffectExecutor exec(state, events, card_db);
    exec.dealDamage(target, 2, src);
    EXPECT_EQ(state.getObject(target).damage_marked, 3)
        << "2 base + 1 bonus damage";
}

}  // namespace
}  // namespace riftbound::test
