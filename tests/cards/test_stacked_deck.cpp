/// @file test_stacked_deck.cpp
/// Reproduces the user's "Stacked Deck draws nothing" report under
/// controlled conditions. If these tests pass, the behavior is correct
/// in the engine and the user's earlier observation was likely from a
/// stale replay. If they fail, there's a real bug to fix.

#include "tests/cards/card_test_fixture.h"

using namespace riftbound;
using namespace riftbound::test;

namespace {

constexpr CardDefId kStackedDeck = 183;
constexpr CardDefId kArbitrary   = 687;  // any spell — used as deck filler

class StackedDeckTest : public CardTestFixture {};

// ─── Core: deck shrinks by 1 + chosen card lands in hand ──────────────────

TEST_F(StackedDeckTest, DrawsOneRecyclesTwoFromFiveCardDeck) {
    // 5 cards in deck — Stacked Deck peeks the top 3, picks 1, recycles 2.
    // Net change: deck -1, hand +1.
    auto a = addToDeck(P1, kArbitrary);
    auto b = addToDeck(P1, kArbitrary);
    auto c = addToDeck(P1, kArbitrary);
    auto d = addToDeck(P1, kArbitrary);  // top - 1 (peeked)
    auto e = addToDeck(P1, kArbitrary);  // top    (peeked)
    (void)a; (void)b; (void)c;

    int initial_deck = deckSize(P1);
    int initial_hand = handSize(P1);

    auto src = state.createObject();
    state.getObject(src).owner = P1;
    state.getObject(src).controller = P1;

    EffectExecutor exec(state, events, card_db);
    // Pick legal[0] — first peeked card becomes the keeper.
    driveResumable(kStackedDeck, P1, src,
        [](const std::vector<Intent>& legal) { return legal.front(); }, exec);

    EXPECT_EQ(deckSize(P1), initial_deck - 1)
        << "deck must shrink by 1 (3 popped, 2 recycled, 1 to hand)";
    EXPECT_EQ(handSize(P1), initial_hand + 1)
        << "hand must grow by 1 (chosen keeper)";
    (void)d; (void)e;
}

TEST_F(StackedDeckTest, ChosenCardEndsUpInHand) {
    // Stash specific ids so we can verify which card became the keeper.
    auto a = addToDeck(P1, kArbitrary);
    auto b = addToDeck(P1, kArbitrary);
    auto c = addToDeck(P1, kArbitrary);  // bottom of peeked window
    auto d = addToDeck(P1, kArbitrary);
    auto top = addToDeck(P1, kArbitrary);  // top of deck — first peek
    (void)a; (void)b;

    auto src = state.createObject();
    state.getObject(src).owner = P1;
    state.getObject(src).controller = P1;

    EffectExecutor exec(state, events, card_db);
    // Picker forces "keep `top`": find the Intent whose chosen_objects[0]
    // matches `top` and return it.
    driveResumable(kStackedDeck, P1, src,
        [top](const std::vector<Intent>& legal) {
            for (auto& i : legal) {
                if (!i.chosen_objects.empty() && i.chosen_objects[0] == top)
                    return i;
            }
            return legal.front();
        }, exec);

    EXPECT_TRUE(inHand(P1, top))
        << "the agent's chosen card must land in P1's hand";
    EXPECT_EQ(state.getObject(top).zone, ZoneType::Hand);

    // Other peeked cards should be back in the deck (recycled to bottom).
    EXPECT_TRUE(inDeck(P1, c) || inDeck(P1, d))
        << "non-keeper peeked cards must be recycled back to the deck";
    (void)c; (void)d;
}

TEST_F(StackedDeckTest, ShallowDeckUsesAvailableCards) {
    // Only 2 cards in deck — Stacked Deck should peek both, pick 1, recycle 1.
    addToDeck(P1, kArbitrary);
    addToDeck(P1, kArbitrary);

    auto src = state.createObject();
    state.getObject(src).owner = P1;
    state.getObject(src).controller = P1;

    EffectExecutor exec(state, events, card_db);
    driveResumable(kStackedDeck, P1, src,
        [](const std::vector<Intent>& legal) { return legal.front(); }, exec);

    EXPECT_EQ(deckSize(P1), 1) << "2 deck → 2 peeked → 1 to hand → 1 recycled";
    EXPECT_EQ(handSize(P1), 1);
}

TEST_F(StackedDeckTest, EmptyDeckIsNoOp) {
    auto src = state.createObject();
    state.getObject(src).owner = P1;
    state.getObject(src).controller = P1;

    EffectExecutor exec(state, events, card_db);
    EXPECT_NO_THROW(driveResumable(kStackedDeck, P1, src,
        [](const std::vector<Intent>& legal) {
            return legal.empty() ? Intent{} : legal.front();
        }, exec));
    EXPECT_EQ(deckSize(P1), 0);
    EXPECT_EQ(handSize(P1), 0);
}

// ─── Zone invariants — exactly the right counts ───────────────────────────

TEST_F(StackedDeckTest, RecyclesExactlyTwoOfTheThreePeeked) {
    // 5 in deck. Stacked Deck peeks 3, keeps 1, recycles exactly 2 back.
    addToDeck(P1, kArbitrary);
    addToDeck(P1, kArbitrary);
    addToDeck(P1, kArbitrary);
    auto peek1 = addToDeck(P1, kArbitrary);
    auto peek2 = addToDeck(P1, kArbitrary);
    auto peek3 = state.createObject();
    state.getObject(peek3).card_def_id = kArbitrary;
    state.getObject(peek3).owner = P1;
    state.getObject(peek3).controller = P1;
    state.getObject(peek3).zone = ZoneType::MainDeck;
    state.player(P1).main_deck.push_back(peek3);
    (void)peek1; (void)peek2;

    auto src = state.createObject();
    state.getObject(src).owner = P1;
    state.getObject(src).controller = P1;

    EffectExecutor exec(state, events, card_db);
    // Pick the LAST peeked card (deepest of the 3 peeked) — leaves the
    // other 2 to be recycled.
    driveResumable(kStackedDeck, P1, src,
        [](const std::vector<Intent>& legal) { return legal.back(); }, exec);

    // Hand grew by exactly 1.
    EXPECT_EQ(handSize(P1), 1);
    // Deck dropped from 6 to 5 (3 removed, 2 recycled, 1 to hand).
    EXPECT_EQ(deckSize(P1), 5);
    // Trash didn't grow (no Stacked-Deck-driven discards).
    EXPECT_EQ(trashSize(P1), 0);
}

TEST_F(StackedDeckTest, OpponentZonesUntouched) {
    // P1's Stacked Deck must not modify P2's hand/deck/trash.
    for (int i = 0; i < 5; ++i) addToDeck(P1, kArbitrary);
    for (int i = 0; i < 3; ++i) addToDeck(P2, kArbitrary);
    addToHand(P2, kArbitrary);

    int p2_deck = deckSize(P2);
    int p2_hand = handSize(P2);
    int p2_trash = trashSize(P2);

    auto src = state.createObject();
    state.getObject(src).owner = P1;
    state.getObject(src).controller = P1;

    EffectExecutor exec(state, events, card_db);
    driveResumable(kStackedDeck, P1, src,
        [](const std::vector<Intent>& legal) { return legal.front(); }, exec);

    EXPECT_EQ(deckSize(P2), p2_deck) << "P2 deck must be untouched";
    EXPECT_EQ(handSize(P2), p2_hand) << "P2 hand must be untouched";
    EXPECT_EQ(trashSize(P2), p2_trash) << "P2 trash must be untouched";
}

TEST_F(StackedDeckTest, ConservationOfCards) {
    // Total card count in P1's zones must be conserved. Stacked Deck
    // doesn't create or banish cards — it only moves them between zones.
    for (int i = 0; i < 10; ++i) addToDeck(P1, kArbitrary);
    int total_before = deckSize(P1) + handSize(P1) + trashSize(P1);

    auto src = state.createObject();
    state.getObject(src).owner = P1;
    state.getObject(src).controller = P1;

    EffectExecutor exec(state, events, card_db);
    driveResumable(kStackedDeck, P1, src,
        [](const std::vector<Intent>& legal) { return legal.front(); }, exec);

    int total_after = deckSize(P1) + handSize(P1) + trashSize(P1);
    EXPECT_EQ(total_after, total_before)
        << "no cards should appear or disappear — only zone changes";
}

// ─── Integration: drive Stacked Deck through the FULL chain ────────────────
//
// The earlier tests use driveResumable which BYPASSES ChainManager. This
// integration test drives Stacked Deck through processFEPR exactly as the
// real engine does — the spell enters the chain, gets resolved, and the
// keeper card lands in hand. Reproduces the user's V&V scenario:
// "Stacked Deck on chain → keeper goes to hand."

TEST_F(StackedDeckTest, IntegrationKeeperLandsInHandViaFullChain) {
    auto sdeck = addToHand(P1, kStackedDeck);
    // Mark the in-hand card as a Spell with the right def so the engine
    // recognizes it through the chain.
    state.getObject(sdeck).card_type = CardType::Spell;

    // Seed 3 specific cards on top of P1's deck so we can predict
    // exactly which ones get peeked.
    auto deeper = addToDeck(P1, kArbitrary);
    auto deepest = addToDeck(P1, kArbitrary);
    auto top1 = addToDeck(P1, kArbitrary);
    auto top2 = addToDeck(P1, kArbitrary);
    auto top3 = addToDeck(P1, kArbitrary);  // very top
    (void)deeper; (void)deepest;

    // Scripted picker: prefer the LAST option (= top1, the deepest of the
    // 3 peeked). Other 2 (top3, top2) get recycled.
    PickByPredicateAgent agent(
        [](int /*call*/, const std::vector<Intent>& legal) -> Intent {
            return legal.back();
        });

    EffectExecutor exec(state, events, card_db);
    driveThroughChain(sdeck, P1, /*targets=*/{}, agent, exec);

    // After the full FEPR loop:
    //  - Stacked Deck itself is in trash (resolved)
    //  - The chosen keeper (top1) is in P1's hand
    //  - The other 2 peeked cards are back in the deck
    EXPECT_EQ(state.getObject(sdeck).zone, ZoneType::Trash)
        << "Stacked Deck itself should resolve to trash";
    EXPECT_TRUE(inTrash(P1, sdeck));

    EXPECT_TRUE(inHand(P1, top1))
        << "the agent's chosen keeper must end up in hand";
    EXPECT_EQ(state.getObject(top1).zone, ZoneType::Hand);

    EXPECT_TRUE(inDeck(P1, top2));
    EXPECT_TRUE(inDeck(P1, top3));

    // Choice prompt label should include candidate names — surfaces the
    // peeked cards directly in the choice request rather than burying
    // them in the trace log.
    EXPECT_GE(agent.call_count, 1) << "agent must have been queried for choice";
}

}  // namespace
