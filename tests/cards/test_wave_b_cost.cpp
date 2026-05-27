/// @file test_wave_b_cost.cpp
/// Wave B cost-modifier cluster (card-side contributions; the engine subtraction
/// is wired symmetrically in GameEngine::canAfford + beginCostPayment).
///   Irelia, Graceful (462) — "your spells that choose me cost [1]/[A] less"

#include "tests/cards/card_test_fixture.h"
#include <gtest/gtest.h>

namespace riftbound::test {
namespace {

using CostTest = CardTestFixture;
constexpr CardDefId kIrelia = 462;
constexpr CardDefId kJhin = 651;

TEST_F(CostTest, Irelia_MarksHerselfAsCostReducerOnBoard) {
    auto irelia = addUnit(P1, kIrelia, 4, /*at_bf=*/0);
    EXPECT_EQ(state.getObject(irelia).spells_targeting_me_cost_reduction, 0);
    card_registry.get(kIrelia)->applyPassiveAura(state, P1);
    EXPECT_EQ(state.getObject(irelia).spells_targeting_me_cost_reduction, 1)
        << "spells that choose Irelia cost 1 less (staged into transient_play_discount "
           "by executePlaySpell, subtracted in canAfford/beginCostPayment)";
}

TEST_F(CostTest, Irelia_NoMarkFromHandOrBase) {
    // Off-board Irelia grants nothing.
    auto irelia = addUnit(P1, kIrelia, 4, /*at_bf=*/-1);
    state.getObject(irelia).location = std::nullopt;
    state.getObject(irelia).zone = ZoneType::Hand;
    card_registry.get(kIrelia)->applyPassiveAura(state, P1);
    EXPECT_EQ(state.getObject(irelia).spells_targeting_me_cost_reduction, 0);
}

// ── Jhin, Meticulous Killer: alt play cost [B] when spent 4+ on a spell ──
TEST_F(CostTest, Jhin_AltCostGatedOnSpellSpend) {
    Card* c = card_registry.get(kJhin);
    ASSERT_NE(c, nullptr);
    // Not enough spent -> no alternative cost.
    state.player(P1).max_spell_spent_this_turn = 3;
    EXPECT_FALSE(c->alternativePlayCost(state, P1).valid);
    // Spent 4+ -> may play for [B] (1 Mind power).
    state.player(P1).max_spell_spent_this_turn = 4;
    auto alt = c->alternativePlayCost(state, P1);
    ASSERT_TRUE(alt.valid);
    EXPECT_EQ(alt.energy, 0);
    EXPECT_EQ(alt.power, 1);
    EXPECT_EQ(alt.power_domain, Domain::Mind);
}

}  // namespace
}  // namespace riftbound::test
