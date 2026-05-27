/// @file test_wave_b_combat.cpp
/// Wave B combat-resolution cluster:
///   Symbol of the Solari (227) — attacker-tie recalls ALL units
///     (combatResolutionStep is private; we verify the card sets the
///      PlayerState flag the engine's tie branch reads.)

#include "tests/cards/card_test_fixture.h"
#include <gtest/gtest.h>

namespace riftbound::test {
namespace {

using CombatTest = CardTestFixture;

constexpr CardDefId kSymbolSolari = 227;

TEST_F(CombatTest, SymbolOfTheSolari_SetsRecallAllFlag) {
    auto sym = addUnit(P1, kSymbolSolari, 0, /*at_bf=*/-1);
    state.getObject(sym).card_type = CardType::Gear;
    EXPECT_FALSE(state.player(P1).recall_all_on_attacker_tie);
    card_registry.get(kSymbolSolari)->applyPassiveAura(state, P1);
    EXPECT_TRUE(state.player(P1).recall_all_on_attacker_tie)
        << "Symbol of the Solari arms attacker-tie recall-all for its controller";
}

}  // namespace
}  // namespace riftbound::test
