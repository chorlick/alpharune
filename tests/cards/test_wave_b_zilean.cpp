/// @file test_wave_b_zilean.cpp
/// Zilean, Time Mage (648) — once/turn, a friendly token unit is created with an
/// additional copy while a Zilean is at a battlefield.

#include "tests/cards/card_test_fixture.h"
#include <gtest/gtest.h>

namespace riftbound::test {
namespace {

using ZileanTest = CardTestFixture;
constexpr CardDefId kZilean = 648;

static int countP1TokenUnits(GameState& state) {
    int n = 0;
    for (auto& [id, obj] : state.objects)
        if (obj.isUnit() && obj.isToken() && obj.controller == PlayerId::Player1) ++n;
    return n;
}

TEST_F(ZileanTest, DoublesTokenUnitOncePerTurn) {
    auto z = addUnit(P1, kZilean, 5, /*at_bf=*/0);
    card_registry.get(kZilean)->applyPassiveAura(state, P1);
    ASSERT_TRUE(state.player(P1).zilean_present);

    EffectExecutor exec(state, events, card_db);
    // First token unit -> original + 1 copy.
    exec.createToken(P1, CardType::Unit, "Soldier", 1, {}, {}, BaseLocation{P1}, false);
    EXPECT_EQ(countP1TokenUnits(state), 2) << "Zilean spawns an extra copy";

    // Second token unit this turn -> NOT doubled (once per turn).
    exec.createToken(P1, CardType::Unit, "Soldier", 1, {}, {}, BaseLocation{P1}, false);
    EXPECT_EQ(countP1TokenUnits(state), 3) << "only one doubling per turn";
    (void)z;
}

TEST_F(ZileanTest, NoDoublingWithoutZileanAtBattlefield) {
    // Zilean present at base (not a battlefield) -> no doubling.
    addUnit(P1, kZilean, 5, /*at_bf=*/-1);
    card_registry.get(kZilean)->applyPassiveAura(state, P1);
    EXPECT_FALSE(state.player(P1).zilean_present);
    EffectExecutor exec(state, events, card_db);
    exec.createToken(P1, CardType::Unit, "Soldier", 1, {}, {}, BaseLocation{P1}, false);
    EXPECT_EQ(countP1TokenUnits(state), 1);
}

}  // namespace
}  // namespace riftbound::test
