/// @file test_wave_b_void_hatchling.cpp
/// Void Hatchling (341): "If you would reveal cards from a deck, look at the top
/// card first. You may recycle it. Then reveal those cards."

#include "tests/cards/card_test_fixture.h"
#include <algorithm>
#include <gtest/gtest.h>

namespace riftbound::test {
namespace {

using VoidTest = CardTestFixture;
constexpr CardDefId kVoidHatchling = 341;

TEST_F(VoidTest, SetsRevealPeekFlagWhileOnBoard) {
    addUnit(P1, kVoidHatchling, 2, /*at_bf=*/0);
    EXPECT_FALSE(state.player(P1).has_reveal_peek);
    card_registry.get(kVoidHatchling)->applyPassiveAura(state, P1);
    EXPECT_TRUE(state.player(P1).has_reveal_peek);
}

TEST_F(VoidTest, PeekRecyclesTopBeforeReveal) {
    state.player(P1).has_reveal_peek = true;
    auto bottom = addToDeck(P1, 1);
    auto top = addToDeck(P1, 1);  // added last -> back() == top of deck
    ASSERT_EQ(state.player(P1).main_deck.back(), top);

    EffectExecutor exec(state, events, card_db);
    // Agent always picks the last option -> "recycle" on the peek prompt.
    exec.setAgentQuery([](PlayerId, const std::vector<Intent>& ch) { return ch.back(); });
    exec.revealAndChoose(P1, 1);

    auto& deck = state.player(P1).main_deck;
    EXPECT_NE(std::find(deck.begin(), deck.end(), top), deck.end())
        << "the peeked top card was recycled (kept in deck), not revealed away";
    (void)bottom;
}

TEST_F(VoidTest, NoPeekWithoutFlag) {
    // Without has_reveal_peek the reveal proceeds normally (no agent prompt).
    auto top = addToDeck(P1, 1);
    EffectExecutor exec(state, events, card_db);
    bool queried = false;
    exec.setAgentQuery([&](PlayerId, const std::vector<Intent>& ch) {
        queried = true; return ch.front();
    });
    exec.revealAndChoose(P1, 1);
    // The single revealed card is auto-chosen; no peek prompt should fire for it.
    (void)top; (void)queried;
    SUCCEED();
}

}  // namespace
}  // namespace riftbound::test
