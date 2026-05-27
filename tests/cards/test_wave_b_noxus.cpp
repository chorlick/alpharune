/// @file test_wave_b_noxus.cpp
/// Noxus Saboteur (18): "Your opponents' [Hidden] cards can't be revealed here."
/// Faithful state representation (BattlefieldState::opp_hidden_unrevealable);
/// currently inert (no reveal-Hidden mechanic exists to consult it yet).

#include "tests/cards/card_test_fixture.h"
#include <gtest/gtest.h>

namespace riftbound::test {
namespace {

using NoxusTest = CardTestFixture;
constexpr CardDefId kNoxusSaboteur = 18;

TEST_F(NoxusTest, SetsUnrevealableFlagOnItsBattlefield) {
    addUnit(P1, kNoxusSaboteur, 3, /*at_bf=*/0);
    EXPECT_FALSE(state.battlefields[0].opp_hidden_unrevealable);
    card_registry.get(kNoxusSaboteur)->applyPassiveAura(state, P1);
    EXPECT_TRUE(state.battlefields[0].opp_hidden_unrevealable);
    EXPECT_FALSE(state.battlefields[1].opp_hidden_unrevealable);
}

TEST_F(NoxusTest, NoFlagFromBase) {
    addUnit(P1, kNoxusSaboteur, 3, /*at_bf=*/-1);  // at base
    card_registry.get(kNoxusSaboteur)->applyPassiveAura(state, P1);
    EXPECT_FALSE(state.battlefields[0].opp_hidden_unrevealable);
}

}  // namespace
}  // namespace riftbound::test
