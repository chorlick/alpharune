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

}  // namespace
}  // namespace riftbound::test
