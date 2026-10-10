/// @file test_wave_b_altar.cpp
/// Altar of Blood (762): "If a unit here would die during combat, its controller
/// may pay [A][A][A] to heal it, exhaust it, and recall it instead."
/// The replacement is the battlefield card's own Card::applyReplacement, asked
/// by GameEngine::killUnit; the engine-path tests are AltarOfBloodTest in
/// tests/test_review_fix_auras_costs.cpp. Here: the card declares it.

#include "tests/cards/card_test_fixture.h"
#include <gtest/gtest.h>

namespace riftbound::test {
namespace {

using AltarTest = CardTestFixture;
constexpr CardDefId kAltar = 762;

TEST_F(AltarTest, DeclaresADeathReplacement) {
    EXPECT_TRUE(card_registry.get(kAltar)->hasReplacementEffect());
}

}  // namespace
}  // namespace riftbound::test
