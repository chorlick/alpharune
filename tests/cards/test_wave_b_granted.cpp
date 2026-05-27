/// @file test_wave_b_granted.cpp
/// Wave B aura-granted activated abilities:
///   Gardens of Becoming (769) — units here have "[E]: Gain 1 XP."
///   Forge of the Fluft (522)  — friendly legends have "[E]: Attach Equipment."
/// (Heimerdinger 111 deferred — needs registry access inside applyPassiveAura.)

#include "tests/cards/card_test_fixture.h"
#include <gtest/gtest.h>

namespace riftbound::test {
namespace {

using GrantedTest = CardTestFixture;

constexpr CardDefId kGardens = 769;
constexpr CardDefId kForge = 522;

TEST_F(GrantedTest, Gardens_GrantsAbilityToUnitsHere) {
    auto gardens = addUnit(P1, kGardens, 0, /*at_bf=*/-1);
    state.getObject(gardens).card_type = CardType::Battlefield;
    state.battlefields[0].card_object_id = gardens;
    auto unit = addUnit(P1, 1, 3, /*at_bf=*/0);
    auto elsewhere = addUnit(P1, 1, 3, /*at_bf=*/1);  // not at Gardens

    card_registry.get(kGardens)->applyPassiveAura(state, P1);
    ASSERT_EQ(state.getObject(unit).granted_abilities.size(), 1u);
    EXPECT_EQ(state.getObject(unit).granted_abilities[0].source_def_id, kGardens);
    EXPECT_TRUE(state.getObject(elsewhere).granted_abilities.empty());
}

TEST_F(GrantedTest, Gardens_OnActivateGainsXP) {
    auto unit = addUnit(P1, 1, 3, /*at_bf=*/0);
    int before = state.player(P1).xp;
    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, unit};
    card_registry.get(kGardens)->onActivate(ctx, 0, {});
    EXPECT_EQ(state.player(P1).xp, before + 1);
}

TEST_F(GrantedTest, Gardens_AbilityDescriptorIsExhaustOnly) {
    auto abs = card_registry.get(kGardens)->activatedAbilities();
    ASSERT_EQ(abs.size(), 1u);
    EXPECT_TRUE(abs[0].cost.exhaust);
    EXPECT_EQ(abs[0].cost.energy, 0);
}

TEST_F(GrantedTest, Forge_GrantsAbilityToControllersLegend) {
    auto forge = addUnit(P1, kForge, 0, /*at_bf=*/-1);
    state.getObject(forge).card_type = CardType::Battlefield;
    state.battlefields[0].card_object_id = forge;
    state.battlefields[0].controller = P1;
    // A legend in P1's legend zone.
    auto legend = addUnit(P1, 1, 0, /*at_bf=*/-1);
    state.getObject(legend).card_type = CardType::Legend;
    state.getObject(legend).zone = ZoneType::LegendZone;
    state.player(P1).legend_zone = legend;

    card_registry.get(kForge)->applyPassiveAura(state, P1);
    ASSERT_EQ(state.getObject(legend).granted_abilities.size(), 1u);
    EXPECT_EQ(state.getObject(legend).granted_abilities[0].source_def_id, kForge);
}

TEST_F(GrantedTest, Forge_NoGrantWhenUncontrolled) {
    auto legend = addUnit(P1, 1, 0, /*at_bf=*/-1);
    state.getObject(legend).zone = ZoneType::LegendZone;
    state.player(P1).legend_zone = legend;
    card_registry.get(kForge)->applyPassiveAura(state, PlayerId::None);
    EXPECT_TRUE(state.getObject(legend).granted_abilities.empty());
}

}  // namespace
}  // namespace riftbound::test
