/// @file test_wave_b_repeat.cpp
/// Wave B [Repeat]-grant cluster (CR 820):
///   Syndra, Transcendent (708) — spells have [Repeat] [2][P] while in a showdown
///   The Academy (772)          — hold here grants next spell [Repeat] = base cost
///   Marai Spire (525)          — friendly [Repeat] costs cost [1] less

#include "tests/cards/card_test_fixture.h"
#include <gtest/gtest.h>

namespace riftbound::test {
namespace {

using RepeatTest = CardTestFixture;

constexpr CardDefId kSyndra = 708;
constexpr CardDefId kAcademy = 772;
constexpr CardDefId kMaraiSpire = 525;

TEST_F(RepeatTest, Syndra_GrantsRepeatOnlyDuringShowdown) {
    auto syndra = addUnit(P1, kSyndra, 6, /*at_bf=*/0);
    Card* c = card_registry.get(kSyndra);
    // No showdown -> no grant.
    c->applyPassiveAura(state, P1);
    EXPECT_EQ(state.player(P1).spells_have_repeat_energy, 0);
    // Showdown at her BF -> grant [2][P].
    state.battlefields[0].showdown_in_progress = true;
    c->applyPassiveAura(state, P1);
    EXPECT_EQ(state.player(P1).spells_have_repeat_energy, 2);
    EXPECT_EQ(state.player(P1).spells_have_repeat_power, 1);
    EXPECT_EQ(state.player(P1).spells_have_repeat_domain, Domain::Chaos);
    (void)syndra;
}

TEST_F(RepeatTest, Academy_GrantsRepeatBaseToNextSpell) {
    Card* c = card_registry.get(kAcademy);
    ASSERT_NE(c, nullptr);
    EXPECT_FALSE(state.player(P1).grant_repeat_base_to_next_spell);
    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, kInvalidId};
    ctx.firing_trigger = TriggerType::WhenYouHoldHere;
    c->onTrigger(ctx, {});
    EXPECT_TRUE(state.player(P1).grant_repeat_base_to_next_spell);
}

TEST_F(RepeatTest, MaraiSpire_ReducesRepeatCostForController) {
    Card* c = card_registry.get(kMaraiSpire);
    ASSERT_NE(c, nullptr);
    c->applyPassiveAura(state, P1);
    EXPECT_EQ(state.player(P1).repeat_cost_reduction, 1);
    // Uncontrolled battlefield (controller None) -> early return, no change.
    state.player(P1).repeat_cost_reduction = 0;
    c->applyPassiveAura(state, PlayerId::None);
    EXPECT_EQ(state.player(P1).repeat_cost_reduction, 0);
}

}  // namespace
}  // namespace riftbound::test
