/// @file test_sabotage.cpp
/// Per-card tests for Sabotage [156].
///
/// Card text:
///   Choose an opponent. They reveal their hand. Choose a non-unit card
///   from it, and recycle that card.
///
/// Mechanics covered here:
///   1. Opponent's full hand is revealed to the caster (CardRevealedEvent
///      fires with revealed_to = caster for every hand card).
///   2. The reveal lands in the caster's `observed_cards` memory bank
///      (PlayerState::observed_cards), indexed by card_def_id.
///   3. The CASTER picks the recycle target (not the opponent).
///   4. Only NON-UNIT cards are valid choices. Units in hand are filtered
///      out of the choice set.
///   5. The chosen card is RECYCLED (bottom of opponent's main deck),
///      NOT discarded (no trash, no has_discarded_this_turn flag).
///   6. If the opponent has no non-unit cards in hand, the reveal still
///      happens but the recycle does not (no legal choice).

#include "tests/cards/card_test_fixture.h"

#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/events.h"
#include "core/game_state.h"
#include "engine/effect_executor.h"

#include <algorithm>

using namespace riftbound;
using namespace riftbound::test;

namespace {

constexpr CardDefId kSabotage     = 156;
constexpr CardDefId kFlurry       = 153;  // a non-unit spell (Flurry of Blades)
constexpr CardDefId kLunarBoon    = 687;  // a non-unit reaction spell
constexpr CardDefId kBaronNashor  = 709;  // a unit (Baron Nashor)
constexpr CardDefId kNoxusHopeful = 12;   // a unit (Noxus Hopeful)

// Helper: subscribe the test's EventBus to GameState::recordReveal so
// that CardRevealedEvent updates the observed_cards bank exactly as it
// would in a live game (where GameEngine wires the same hook). Returns
// the connection so the test can disconnect if needed; in practice we
// just let it tear down with the fixture.
inline boost::signals2::connection wireObservationBank(EventBus& events,
                                                        GameState& state) {
    return events.on_card_revealed.connect(
        [&state](const CardRevealedEvent& e) { state.recordReveal(e); });
}

}  // namespace

// ─── 1. Reveal: every card in opponent's hand fires CardRevealedEvent ─────

TEST_F(CardTestFixture, Sabotage_RevealsEveryCardInOpponentsHand) {
    // P1 casts Sabotage; P2 has 3 cards in hand. We expect 3 reveal events
    // fired, all addressed (revealed_to) to P1.
    addToHand(P2, kFlurry);
    addToHand(P2, kLunarBoon);
    addToHand(P2, kBaronNashor);

    int reveal_count = 0;
    int revealed_to_caster_count = 0;
    auto conn = events.on_card_revealed.connect(
        [&](const CardRevealedEvent& e) {
            ++reveal_count;
            if (e.revealed_to == P1 && !e.revealed_to_all) ++revealed_to_caster_count;
        });

    auto src = state.createObject();
    state.getObject(src).owner = P1;
    state.getObject(src).controller = P1;
    state.getObject(src).card_def_id = kSabotage;
    state.getObject(src).card_type = CardType::Spell;
    EffectExecutor exec(state, events, card_db);
    driveResumable(kSabotage, P1, src,
                    [](const std::vector<Intent>& legal) {
                        return legal.empty() ? Intent{} : legal.front();
                    },
                    exec);
    conn.disconnect();

    EXPECT_EQ(reveal_count, 3);
    EXPECT_EQ(revealed_to_caster_count, 3)
        << "All reveals must be private to the caster (revealed_to = P1, "
           "revealed_to_all = false). The opponent does not learn anything "
           "new about their own hand.";
}

// ─── 2. Reveals land in caster's observed_cards memory bank ────────────────

TEST_F(CardTestFixture, Sabotage_RevealedCardsLandInCasterMemoryBank) {
    // P1 casts Sabotage; P2 reveals 3 distinct cards. P1's observed_cards
    // bank should contain +1 for each card_def_id; P2's bank stays empty
    // (P2 doesn't observe their own hand via this reveal).
    addToHand(P2, kFlurry);
    addToHand(P2, kLunarBoon);
    addToHand(P2, kBaronNashor);

    auto conn = wireObservationBank(events, state);

    auto src = state.createObject();
    state.getObject(src).owner = P1;
    state.getObject(src).controller = P1;
    state.getObject(src).card_def_id = kSabotage;
    state.getObject(src).card_type = CardType::Spell;
    EffectExecutor exec(state, events, card_db);
    driveResumable(kSabotage, P1, src,
                    [](const std::vector<Intent>& legal) {
                        return legal.empty() ? Intent{} : legal.front();
                    },
                    exec);
    conn.disconnect();

    const auto& p1_bank = state.player(P1).observed_cards;
    EXPECT_EQ(p1_bank.size(), 3u);
    EXPECT_EQ(p1_bank.at(kFlurry),      1);
    EXPECT_EQ(p1_bank.at(kLunarBoon),   1);
    EXPECT_EQ(p1_bank.at(kBaronNashor), 1);

    const auto& p2_bank = state.player(P2).observed_cards;
    EXPECT_TRUE(p2_bank.empty())
        << "P2 must not observe their own hand cards via Sabotage. The "
           "reveal is private to the caster, P1.";
}

// ─── 3. Caster (not opponent) is the chooser ───────────────────────────────

TEST_F(CardTestFixture, Sabotage_CasterIsTheChooser) {
    // P1 casts Sabotage. The MakeChoice intents published must list P1 as
    // the choosing player (i.e. the spell controller, not P2 who owns the
    // hand). This is the difference between "they pick what to discard"
    // (the prior approximation) and "you pick what to recycle" (correct).
    addToHand(P2, kFlurry);
    addToHand(P2, kLunarBoon);

    EffectExecutor exec(state, events, card_db);
    auto src = state.createObject();
    state.getObject(src).owner = P1;
    state.getObject(src).controller = P1;
    state.getObject(src).card_def_id = kSabotage;
    state.getObject(src).card_type = CardType::Spell;

    // We can't call driveResumable because we need to inspect the pending
    // request before resolving. Replicate the case-0 entry path inline.
    ChainItem ri;
    ri.id = state.chain.allocateId();
    ri.controller = P1;
    ri.is_spell = true;
    ri.resume_point = 0;
    ri.source = src;
    ri.card_def_id = kSabotage;
    state.chain.resuming = ri;

    Card* card = card_registry.get(kSabotage);
    ASSERT_NE(card, nullptr);
    CardContext ctx{state, events, exec, P1, src};
    card->onResolve(ctx, /*targets=*/{});

    ASSERT_TRUE(exec.hasPendingChoice());
    auto pending = exec.consumePendingChoice();
    EXPECT_EQ(pending.player, P1)
        << "Sabotage published its choice as if P2 were choosing. The "
           "caster (P1) must be the chooser per the card text 'Choose a "
           "non-unit card from it'.";

    state.chain.resuming.reset();
}

// ─── 4. Non-unit filter — units are excluded from the choice set ───────────

TEST_F(CardTestFixture, Sabotage_FiltersOutUnitsFromChoiceSet) {
    // P2's hand: 2 units (Baron Nashor, Noxus Hopeful) + 2 non-units
    // (Flurry, Lunar Boon). Sabotage's choice set must have exactly 2
    // entries, one for each non-unit, and neither unit may appear.
    auto baron    = addToHand(P2, kBaronNashor);
    auto noxus    = addToHand(P2, kNoxusHopeful);
    auto flurry   = addToHand(P2, kFlurry);
    auto boon     = addToHand(P2, kLunarBoon);

    EffectExecutor exec(state, events, card_db);
    auto src = state.createObject();
    state.getObject(src).owner = P1;
    state.getObject(src).controller = P1;
    state.getObject(src).card_def_id = kSabotage;
    state.getObject(src).card_type = CardType::Spell;

    ChainItem ri;
    ri.id = state.chain.allocateId();
    ri.controller = P1;
    ri.is_spell = true;
    ri.resume_point = 0;
    ri.source = src;
    ri.card_def_id = kSabotage;
    state.chain.resuming = ri;

    Card* card = card_registry.get(kSabotage);
    ASSERT_NE(card, nullptr);
    CardContext ctx{state, events, exec, P1, src};
    card->onResolve(ctx, /*targets=*/{});

    ASSERT_TRUE(exec.hasPendingChoice());
    auto pending = exec.consumePendingChoice();

    EXPECT_EQ(pending.legal.size(), 2u)
        << "Expected 2 choices (the two non-units). Got " << pending.legal.size()
        << ". Units must be filtered out.";

    // Collect the chosen GameObjectIds across the legal set.
    std::vector<GameObjectId> picks;
    for (const auto& i : pending.legal) {
        ASSERT_FALSE(i.chosen_objects.empty());
        picks.push_back(i.chosen_objects[0]);
    }
    auto contains = [&](GameObjectId id) {
        return std::find(picks.begin(), picks.end(), id) != picks.end();
    };
    EXPECT_TRUE(contains(flurry));
    EXPECT_TRUE(contains(boon));
    EXPECT_FALSE(contains(baron))
        << "Baron Nashor is a unit — Sabotage must not offer it as a "
           "recycle target.";
    EXPECT_FALSE(contains(noxus))
        << "Noxus Hopeful is a unit — Sabotage must not offer it as a "
           "recycle target.";

    state.chain.resuming.reset();
}

// ─── 5. Recycle (not discard) — chosen card goes to bottom of opp deck ─────

TEST_F(CardTestFixture, Sabotage_ChosenCardIsRecycledNotDiscarded) {
    // P2's hand has one non-unit (Flurry). Sabotage resolves with P1
    // picking Flurry. Expected post-state:
    //   • Flurry leaves P2's hand
    //   • Flurry is at the BOTTOM of P2's main deck (position 0; top = back)
    //   • Flurry is NOT in P2's trash
    //   • P2's has_discarded_this_turn flag is NOT set (recycle ≠ discard)
    auto flurry = addToHand(P2, kFlurry);
    // Seed a non-empty deck so we can assert "bottom = position 0".
    auto deck_filler = addToDeck(P2, kFlurry);  // any id at position 0
    // Move filler to TOP so deck = [filler @ bottom]. addToDeck pushes to
    // back, and back = top, so re-arrange:
    state.player(P2).main_deck = {deck_filler};
    ASSERT_EQ(state.player(P2).main_deck.size(), 1u);

    EffectExecutor exec(state, events, card_db);
    auto src = state.createObject();
    state.getObject(src).owner = P1;
    state.getObject(src).controller = P1;
    state.getObject(src).card_def_id = kSabotage;
    state.getObject(src).card_type = CardType::Spell;
    driveResumable(kSabotage, P1, src,
                    [&](const std::vector<Intent>& legal) {
                        // The only legal recycle target should be Flurry.
                        EXPECT_EQ(legal.size(), 1u);
                        return legal.front();
                    },
                    exec);

    // Flurry is gone from P2's hand.
    EXPECT_FALSE(inHand(P2, flurry));
    // Flurry is NOT in P2's trash (this would be "discard", not "recycle").
    EXPECT_FALSE(inTrash(P2, flurry));
    // P2's has_discarded_this_turn is NOT set.
    EXPECT_FALSE(state.player(P2).has_discarded_this_turn);
    // Flurry sits at the BOTTOM of P2's deck (position 0).
    ASSERT_EQ(state.player(P2).main_deck.size(), 2u);
    EXPECT_EQ(state.player(P2).main_deck.front(), flurry)
        << "Recycle must place the chosen card at the bottom of the "
           "opponent's main deck (position 0; the deck-front is bottom).";
    // Filler still sits on top.
    EXPECT_EQ(state.player(P2).main_deck.back(), deck_filler);
    // Object's zone is updated.
    EXPECT_EQ(state.getObject(flurry).zone, ZoneType::MainDeck);
}

// ─── 6. No non-unit in hand — reveal happens, recycle does not ─────────────

TEST_F(CardTestFixture, Sabotage_NoNonUnitInHand_SkipsRecycle) {
    // P2's hand is entirely units. Sabotage should still REVEAL the hand
    // (CardRevealedEvent fires for each card, observed_cards updated),
    // but no choice is requested and no card is recycled.
    auto baron = addToHand(P2, kBaronNashor);
    auto noxus = addToHand(P2, kNoxusHopeful);

    int reveal_count = 0;
    auto conn1 = events.on_card_revealed.connect(
        [&](const CardRevealedEvent&) { ++reveal_count; });
    auto conn2 = wireObservationBank(events, state);

    EffectExecutor exec(state, events, card_db);
    auto src = state.createObject();
    state.getObject(src).owner = P1;
    state.getObject(src).controller = P1;
    state.getObject(src).card_def_id = kSabotage;
    state.getObject(src).card_type = CardType::Spell;
    driveResumable(kSabotage, P1, src,
                    [](const std::vector<Intent>& legal) {
                        return legal.empty() ? Intent{} : legal.front();
                    },
                    exec);
    conn1.disconnect();
    conn2.disconnect();

    EXPECT_EQ(reveal_count, 2)
        << "Reveal must still happen even when there's no legal recycle "
           "target.";
    EXPECT_EQ(state.player(P1).observed_cards.size(), 2u);
    // Nothing moved out of P2's hand.
    EXPECT_TRUE(inHand(P2, baron));
    EXPECT_TRUE(inHand(P2, noxus));
    EXPECT_FALSE(exec.hasPendingChoice())
        << "No legal choice means no choice should be pending after "
           "resolution.";
}
