/// @file test_finish_batch2.cpp
/// Per-card unit tests for the "finish batch 2" card set.
///
/// Most cards in this batch hit documented engine gaps (see the per-card
/// .cpp ESCALATE comments) and have no card-layer behavior to test. The one
/// implementable clause is Needlessly Large Yordle's ENERGY cost reduction,
/// which is exercised here.

#include "tests/cards/card_test_fixture.h"

#include "cards/card.h"
#include "core/game_state.h"

#include <gtest/gtest.h>

namespace riftbound::test {

// ─── [378] Needlessly Large Yordle ──────────────────────────────────────────
// "I cost [2][G] less for each point you scored from holding this turn."
// Only the ENERGY half (2 per hold point) is implemented via selfCostReduction;
// the [G] (Order power) half is an escalated engine gap. selfCostReduction
// returns the raw reduction (the engine clamps the net cost to >= 0).

constexpr CardDefId kNeedlesslyLargeYordle = 378;

TEST_F(CardTestFixture, NeedlesslyLargeYordle_NoReductionWithoutHoldPoints) {
    Card* c = card_registry.get(kNeedlesslyLargeYordle);
    ASSERT_NE(c, nullptr);
    state.player(P1).hold_points_this_turn = 0;
    EXPECT_EQ(c->selfCostReduction(state, P1), 0)
        << "no hold points scored -> no discount";
}

TEST_F(CardTestFixture, NeedlesslyLargeYordle_EnergyReductionScalesPerHoldPoint) {
    Card* c = card_registry.get(kNeedlesslyLargeYordle);
    ASSERT_NE(c, nullptr);
    // printed energy cost = 10; discount is [2] energy per hold point.
    state.player(P1).hold_points_this_turn = 1;
    EXPECT_EQ(c->selfCostReduction(state, P1), 2)  // 10 - 2 = 8
        << "1 hold point -> 2 energy less";
    state.player(P1).hold_points_this_turn = 3;
    EXPECT_EQ(c->selfCostReduction(state, P1), 6)  // 10 - 6 = 4
        << "3 hold points -> 6 energy less";
}

TEST_F(CardTestFixture, NeedlesslyLargeYordle_DiscountIsPerControllerHoldCount) {
    Card* c = card_registry.get(kNeedlesslyLargeYordle);
    ASSERT_NE(c, nullptr);
    // The discount reads the queried player's counter, not the opponent's.
    state.player(P1).hold_points_this_turn = 2;
    state.player(P2).hold_points_this_turn = 5;
    EXPECT_EQ(c->selfCostReduction(state, P1), 4)
        << "P1 discount derives from P1's hold points";
    EXPECT_EQ(c->selfCostReduction(state, P2), 10)
        << "P2 discount derives from P2's hold points";
}

}  // namespace riftbound::test
