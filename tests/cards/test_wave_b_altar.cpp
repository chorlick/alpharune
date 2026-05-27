/// @file test_wave_b_altar.cpp
/// Altar of Blood (762): "If a unit here would die during combat, its controller
/// may pay [A][A][A] to heal it, exhaust it, and recall it instead."
/// (The kill-path recall is engine-internal in GameEngine::killUnit; here we
/// verify the card sets the BattlefieldState flag that killUnit reads.)

#include "tests/cards/card_test_fixture.h"
#include <gtest/gtest.h>

namespace riftbound::test {
namespace {

using AltarTest = CardTestFixture;
constexpr CardDefId kAltar = 762;

TEST_F(AltarTest, SetsDeathRecallFlagOnItsBattlefield) {
    auto altar = addUnit(P1, kAltar, 0, /*at_bf=*/-1);
    state.getObject(altar).card_type = CardType::Battlefield;
    state.battlefields[0].card_object_id = altar;
    EXPECT_FALSE(state.battlefields[0].death_recall_for_pay);
    card_registry.get(kAltar)->applyPassiveAura(state, PlayerId::None);
    EXPECT_TRUE(state.battlefields[0].death_recall_for_pay);
    EXPECT_FALSE(state.battlefields[1].death_recall_for_pay);
}

}  // namespace
}  // namespace riftbound::test
