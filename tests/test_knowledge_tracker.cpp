/// Knowledge tracker (v6 tasks P1.2 and P1.4): one test per kind of move, on a
/// hand-built state. Each test makes the physical move and calls the hook the
/// way the engine helpers will, then asks the view what each player knows.
///
/// Decks are stored top = back.

#include "engine/knowledge_tracker.h"

#include <gtest/gtest.h>

#include <algorithm>

namespace riftbound {
namespace {

using knowledge::DeckSlot;
using knowledge::GameKnowledgeView;
using knowledge::Place;
using knowledge::everyone;
using knowledge::nobody;
using knowledge::only;

constexpr PlayerId P1 = PlayerId::Player1;
constexpr PlayerId P2 = PlayerId::Player2;

struct Table {
    GameState s;

    Table() {
        s.battlefields.emplace_back();
        s.battlefields.back().id = BattlefieldId::fromRaw(0);
    }

    GameObjectId make(PlayerId owner, CardDefId card, ZoneType zone) {
        const GameObjectId id = s.createObject();
        auto& o = s.getObject(id);
        o.owner = owner;
        o.controller = owner;
        o.card_def_id = card;
        o.zone = zone;
        auto& p = s.player(owner);
        if (zone == ZoneType::Hand) p.hand.push_back(id);
        if (zone == ZoneType::MainDeck) p.main_deck.push_back(id);
        if (zone == ZoneType::Trash) p.trash.push_back(id);
        return id;
    }

    /// A deck of `n` cards with ids `first_card`, `first_card + 1`, ...; the
    /// last one made is the top card. Returns them top first.
    std::vector<GameObjectId> deck(PlayerId owner, int n, CardDefId first_card = 100) {
        std::vector<GameObjectId> ids;
        for (int i = 0; i < n; ++i) {
            ids.push_back(make(owner, CardDefId::fromRaw(first_card.raw() + i), ZoneType::MainDeck));
        }
        std::reverse(ids.begin(), ids.end());
        return ids;
    }

    void take(std::vector<GameObjectId>& from, GameObjectId id) {
        from.erase(std::remove(from.begin(), from.end(), id), from.end());
    }

    // Physical moves, each followed by its hook.

    GameObjectId draw(PlayerId p) {
        auto& ps = s.player(p);
        const GameObjectId id = ps.main_deck.back();
        ps.main_deck.pop_back();
        ps.hand.push_back(id);
        s.getObject(id).zone = ZoneType::Hand;
        knowledge::moved(s, id, Place::Deck, Place::Hand, nobody(),
                         DeckSlot{DeckEnd::Top, 0});
        return id;
    }

    void handToBottom(GameObjectId id, knowledge::Seers seers = nobody()) {
        auto& ps = s.player(s.getObject(id).owner);
        take(ps.hand, id);
        ps.main_deck.insert(ps.main_deck.begin(), id);
        s.getObject(id).zone = ZoneType::MainDeck;
        knowledge::moved(s, id, Place::Hand, Place::Deck, seers, {}, DeckEnd::Bottom);
    }

    void trashToBottom(GameObjectId id) {
        auto& ps = s.player(s.getObject(id).owner);
        take(ps.trash, id);
        ps.main_deck.insert(ps.main_deck.begin(), id);
        s.getObject(id).zone = ZoneType::MainDeck;
        knowledge::moved(s, id, Place::Public, Place::Deck, nobody(), {}, DeckEnd::Bottom);
    }

    void expectConsistent() {
        const auto problems = knowledge::check(s);
        EXPECT_TRUE(problems.empty()) << problems.front();
    }
};

TEST(KnowledgeTracker, OwnHandAndPublicCardsAreSimplyKnown) {
    Table t;
    const auto mine = t.make(P1, 7, ZoneType::Hand);
    const auto theirs = t.make(P2, 8, ZoneType::Hand);
    const auto trashed = t.make(P2, 9, ZoneType::Trash);
    GameKnowledgeView v(t.s, P1);

    EXPECT_TRUE(v.about(mine).identity_known);
    EXPECT_EQ(v.about(mine).card_def_id, CardDefId{7});
    EXPECT_FALSE(v.about(theirs).identity_known);
    EXPECT_EQ(v.about(theirs).last_known_zone, ZoneType::Hand);
    EXPECT_TRUE(v.about(trashed).identity_known);
    EXPECT_EQ(v.unknownCount(P2, ZoneType::Hand), 1);
    EXPECT_EQ(v.unknownCount(P1, ZoneType::Hand), 0);
}

TEST(KnowledgeTracker, RevealedHandCardBecomesUncertainAfterAnUnseenMove) {
    Table t;
    t.deck(P2, 5);
    const auto shown = t.make(P2, 8, ZoneType::Hand);
    const auto other = t.make(P2, 9, ZoneType::Hand);
    knowledge::seen(t.s, shown, Place::Hand, only(P1));
    {
        GameKnowledgeView v(t.s, P1);
        EXPECT_TRUE(v.about(shown).identity_known);
        EXPECT_TRUE(v.about(shown).zone_confirmed);
        EXPECT_EQ(v.unknownCount(P2, ZoneType::Hand), 1);
    }

    // The opponent recycles a hand card face down. It was not the shown one,
    // but the observer cannot tell.
    t.handToBottom(other);
    GameKnowledgeView v(t.s, P1);
    EXPECT_TRUE(v.about(shown).identity_known);
    EXPECT_EQ(v.about(shown).last_known_zone, ZoneType::Hand);
    EXPECT_FALSE(v.about(shown).zone_confirmed);
    EXPECT_FALSE(v.about(other).identity_known);
    EXPECT_EQ(v.unknownCount(P2, ZoneType::Hand), 1);

    // The owner knows exactly where it went.
    GameKnowledgeView owner(t.s, P2);
    const auto k = owner.about(other);
    EXPECT_TRUE(k.identity_known);
    EXPECT_EQ(k.deck_position, DeckPosition::Bottom);
    EXPECT_EQ(k.deck_rank, 0);
    EXPECT_EQ(k.deck_rank_span, 1);
    t.expectConsistent();
}

TEST(KnowledgeTracker, KnownTopCardsMoveUpAsCardsAreDrawn) {
    Table t;
    const auto d = t.deck(P1, 6);
    for (int r = 0; r < 3; ++r) {
        knowledge::seenInDeck(t.s, d[r], only(P1), DeckEnd::Top, r);
    }
    t.draw(P1);

    GameKnowledgeView v(t.s, P1);
    const auto top = v.knownDeckTop(P1);
    ASSERT_EQ(top.size(), 2u);
    EXPECT_EQ(top[0].object, d[1]);
    EXPECT_EQ(top[0].deck_rank, 0);
    EXPECT_EQ(top[1].object, d[2]);
    EXPECT_EQ(top[1].deck_rank, 1);
    // The opponent learned nothing.
    GameKnowledgeView opp(t.s, P2);
    EXPECT_TRUE(opp.knownDeckTop(P1).empty());
    EXPECT_FALSE(opp.about(d[1]).identity_known);
    t.expectConsistent();
}

TEST(KnowledgeTracker, OpponentFollowsAKnownTopCardIntoTheHand) {
    Table t;
    const auto d = t.deck(P2, 4);
    knowledge::seenInDeck(t.s, d[0], everyone(), DeckEnd::Top, 0);
    const auto drawn = t.draw(P2);
    ASSERT_EQ(drawn, d[0]);

    GameKnowledgeView v(t.s, P1);
    const auto k = v.about(d[0]);
    EXPECT_TRUE(k.identity_known);
    EXPECT_EQ(k.last_known_zone, ZoneType::Hand);
    EXPECT_TRUE(k.zone_confirmed);
    EXPECT_EQ(v.unknownCount(P2, ZoneType::Hand), 0);
    t.expectConsistent();
}

TEST(KnowledgeTracker, AnUnseenDrawMakesAKnownDeckCardUncertain) {
    Table t;
    const auto d = t.deck(P2, 4);
    // Shown to the opponent, then shuffled away: in the deck, place unknown.
    knowledge::seen(t.s, d[2], Place::Deck, only(P1));
    {
        GameKnowledgeView v(t.s, P1);
        EXPECT_TRUE(v.about(d[2]).zone_confirmed);
        EXPECT_EQ(v.about(d[2]).deck_position, DeckPosition::Unknown);
        EXPECT_EQ(v.unknownCount(P2, ZoneType::MainDeck), 3);
    }
    t.draw(P2);
    GameKnowledgeView v(t.s, P1);
    EXPECT_TRUE(v.about(d[2]).identity_known);
    EXPECT_EQ(v.about(d[2]).last_known_zone, ZoneType::MainDeck);
    EXPECT_FALSE(v.about(d[2]).zone_confirmed);
    t.expectConsistent();
}

TEST(KnowledgeTracker, CardsRecycledTogetherAreOneUnorderedGroup) {
    for (bool swapped : {false, true}) {
        Table t;
        t.deck(P1, 5);
        const auto a = t.make(P1, 31, ZoneType::Hand);
        const auto b = t.make(P1, 30, ZoneType::Hand);

        // A mulligan: both set aside, placed on the bottom in a random order.
        auto& ps = t.s.player(P1);
        ps.hand.clear();
        ps.main_deck.insert(ps.main_deck.begin(), swapped ? a : b);
        ps.main_deck.insert(ps.main_deck.begin(), swapped ? b : a);
        knowledge::movedTogether(t.s,
            {{a, Place::Hand, nobody()}, {b, Place::Hand, nobody()}}, DeckEnd::Bottom);

        GameKnowledgeView v(t.s, P1);
        for (auto id : {a, b}) {
            const auto k = v.about(id);
            EXPECT_EQ(k.deck_position, DeckPosition::Bottom);
            EXPECT_EQ(k.deck_rank, 0);
            EXPECT_EQ(k.deck_rank_span, 2);
        }
        // Reported in card order, never in deck order.
        const auto bottom = v.knownDeckBottom(P1);
        ASSERT_EQ(bottom.size(), 2u);
        EXPECT_EQ(bottom[0].object, b);
        EXPECT_EQ(bottom[1].object, a);
        // The composition leaves out the cards whose place is known.
        EXPECT_EQ(v.ownDeckComposition().size(), 5u);

        GameKnowledgeView opp(t.s, P2);
        EXPECT_FALSE(opp.about(a).identity_known);
        EXPECT_EQ(opp.unknownCount(P1, ZoneType::MainDeck), 7);
        t.expectConsistent();

        // One more card goes under them: the group is now slots 1 and 2.
        const auto c = t.make(P1, 40, ZoneType::Hand);
        t.handToBottom(c);
        GameKnowledgeView later(t.s, P1);
        EXPECT_EQ(later.about(a).deck_rank, 1);
        EXPECT_EQ(later.about(a).deck_rank_span, 2);
        EXPECT_EQ(later.about(c).deck_rank, 0);
        EXPECT_EQ(later.knownDeckBottom(P1).size(), 3u);
        t.expectConsistent();
    }
}

TEST(KnowledgeTracker, APublicCardRecycledAloneIsKnownToBothPlayers) {
    Table t;
    t.deck(P2, 4);
    const auto card = t.make(P2, 55, ZoneType::Trash);
    t.trashToBottom(card);
    for (PlayerId p : {P1, P2}) {
        GameKnowledgeView v(t.s, p);
        const auto k = v.about(card);
        EXPECT_TRUE(k.identity_known);
        EXPECT_EQ(k.card_def_id, CardDefId{55});
        EXPECT_EQ(k.deck_position, DeckPosition::Bottom);
        EXPECT_EQ(k.deck_rank, 0);
    }
    t.expectConsistent();
}

TEST(KnowledgeTracker, AShuffleKeepsIdentitiesAndDropsPositions) {
    Table t;
    const auto d = t.deck(P2, 4);
    const auto card = t.make(P2, 55, ZoneType::Trash);
    t.trashToBottom(card);
    knowledge::seenInDeck(t.s, d[0], only(P2), DeckEnd::Top, 0);

    auto& deck = t.s.player(P2).main_deck;
    std::rotate(deck.begin(), deck.begin() + 2, deck.end());
    knowledge::shuffled(t.s, P2);

    GameKnowledgeView opp(t.s, P1);
    EXPECT_TRUE(opp.about(card).identity_known);
    EXPECT_TRUE(opp.about(card).zone_confirmed);
    EXPECT_EQ(opp.about(card).deck_position, DeckPosition::Unknown);
    EXPECT_TRUE(opp.knownDeckBottom(P2).empty());

    GameKnowledgeView owner(t.s, P2);
    EXPECT_TRUE(owner.knownDeckTop(P2).empty());
    EXPECT_TRUE(owner.knownDeckBottom(P2).empty());
    EXPECT_EQ(owner.ownDeckComposition().size(), 5u);
    t.expectConsistent();
}

TEST(KnowledgeTracker, BurnOutMakesTheWholeTrashKnownButUnordered) {
    Table t;
    std::vector<GameObjectId> trash;
    for (int i = 0; i < 3; ++i) trash.push_back(t.make(P2, CardDefId::fromRaw(60 + i), ZoneType::Trash));
    auto& ps = t.s.player(P2);
    for (auto id : trash) {
        ps.main_deck.push_back(id);
        knowledge::moved(t.s, id, Place::Public, Place::Deck, nobody());
    }
    ps.trash.clear();
    knowledge::shuffled(t.s, P2);

    GameKnowledgeView opp(t.s, P1);
    EXPECT_EQ(opp.knownHiddenCards(P2).size(), 3u);
    EXPECT_EQ(opp.unknownCount(P2, ZoneType::MainDeck), 0);
    for (auto id : trash) {
        EXPECT_EQ(opp.about(id).deck_position, DeckPosition::Unknown);
    }
    t.expectConsistent();
}

TEST(KnowledgeTracker, PredictTellsTheLookerAndNobodyElse) {
    Table t;
    const auto d = t.deck(P1, 6);
    // Both players knew the top card (say it was revealed and put back).
    knowledge::seenInDeck(t.s, d[0], everyone(), DeckEnd::Top, 0);

    // P1 looks at the top two and puts the second on the bottom.
    knowledge::seenInDeck(t.s, d[0], only(P1), DeckEnd::Top, 0);
    knowledge::seenInDeck(t.s, d[1], only(P1), DeckEnd::Top, 1);
    auto& deck = t.s.player(P1).main_deck;
    t.take(deck, d[1]);
    deck.insert(deck.begin(), d[1]);
    knowledge::moved(t.s, d[1], Place::Deck, Place::Deck, only(P1),
                     DeckSlot{DeckEnd::Top, 1}, DeckEnd::Bottom, /*slot_public=*/false);

    GameKnowledgeView me(t.s, P1);
    ASSERT_EQ(me.knownDeckTop(P1).size(), 1u);
    EXPECT_EQ(me.knownDeckTop(P1)[0].object, d[0]);
    ASSERT_EQ(me.knownDeckBottom(P1).size(), 1u);
    EXPECT_EQ(me.knownDeckBottom(P1)[0].object, d[1]);

    // The opponent saw two cards looked at and one go down, not which: the
    // card they knew is still in the deck, at a place they no longer know.
    GameKnowledgeView opp(t.s, P2);
    const auto k = opp.about(d[0]);
    EXPECT_TRUE(k.identity_known);
    EXPECT_TRUE(k.zone_confirmed);
    EXPECT_EQ(k.deck_position, DeckPosition::Unknown);
    EXPECT_FALSE(opp.about(d[1]).identity_known);
    t.expectConsistent();
}

TEST(KnowledgeTracker, CardsHeldDuringALookAreKnownToTheLooker) {
    Table t;
    const auto d = t.deck(P1, 5);
    auto& ps = t.s.player(P1);
    // Look at the top two, holding them outside the deck.
    for (int i = 0; i < 2; ++i) {
        ps.main_deck.pop_back();
        knowledge::moved(t.s, d[i], Place::Deck, Place::Held, only(P1),
                         DeckSlot{DeckEnd::Top, 0});
    }
    {
        GameKnowledgeView v(t.s, P1);
        EXPECT_EQ(v.knownHiddenCards(P1).size(), 2u);
        EXPECT_TRUE(v.about(d[0]).identity_known);
        GameKnowledgeView opp(t.s, P2);
        EXPECT_TRUE(opp.knownHiddenCards(P1).empty());
        EXPECT_FALSE(opp.about(d[0]).identity_known);
    }
    // One goes to hand, the other to the bottom.
    ps.hand.push_back(d[0]);
    knowledge::moved(t.s, d[0], Place::Held, Place::Hand, only(P1));
    ps.main_deck.insert(ps.main_deck.begin(), d[1]);
    knowledge::moved(t.s, d[1], Place::Held, Place::Deck, only(P1), {}, DeckEnd::Bottom);

    GameKnowledgeView v(t.s, P1);
    ASSERT_EQ(v.knownDeckBottom(P1).size(), 1u);
    EXPECT_EQ(v.knownDeckBottom(P1)[0].object, d[1]);
    EXPECT_EQ(v.ownDeckComposition().size(), 3u);
    t.expectConsistent();
}

TEST(KnowledgeTracker, AFacedownCardIsFollowedOnceSeen) {
    Table t;
    const auto shown = t.make(P2, 8, ZoneType::Hand);
    const auto hidden = t.make(P2, 9, ZoneType::Hand);
    knowledge::seen(t.s, shown, Place::Hand, only(P1));

    // The opponent hides a card. The observer cannot tell which.
    auto& ps = t.s.player(P2);
    t.take(ps.hand, hidden);
    t.s.battlefields[0].facedown.push_back(hidden);
    t.s.getObject(hidden).is_hidden = true;
    knowledge::moved(t.s, hidden, Place::Hand, Place::Facedown, nobody());
    {
        GameKnowledgeView v(t.s, P1);
        EXPECT_FALSE(v.about(hidden).identity_known);
        EXPECT_EQ(v.about(hidden).last_known_zone, ZoneType::FacedownZone);
        EXPECT_FALSE(v.about(shown).zone_confirmed);
        EXPECT_EQ(v.unknownCount(P2, ZoneType::FacedownZone), 1);
        GameKnowledgeView owner(t.s, P2);
        EXPECT_TRUE(owner.about(hidden).identity_known);
    }

    // An effect lets the observer look at it; later it returns to hand.
    knowledge::seen(t.s, hidden, Place::Facedown, only(P1));
    {
        GameKnowledgeView v(t.s, P1);
        EXPECT_TRUE(v.about(hidden).identity_known);
        EXPECT_EQ(v.unknownCount(P2, ZoneType::FacedownZone), 0);
    }
    t.s.battlefields[0].facedown.clear();
    ps.hand.push_back(hidden);
    knowledge::moved(t.s, hidden, Place::Facedown, Place::Hand, nobody());
    GameKnowledgeView v(t.s, P1);
    EXPECT_EQ(v.about(hidden).last_known_zone, ZoneType::Hand);
    EXPECT_TRUE(v.about(hidden).zone_confirmed);
    t.expectConsistent();
}

TEST(KnowledgeTracker, OwnDeckCompositionCountsCopiesAndHidesOrder) {
    Table t;
    for (CardDefId card : std::initializer_list<CardDefId>{5, 3, 5, 9, 3, 5}) t.make(P1, card, ZoneType::MainDeck);
    GameKnowledgeView v(t.s, P1);
    const auto comp = v.ownDeckComposition();
    const std::vector<std::pair<CardDefId, int>> want = {
        {CardDefId{3}, 2}, {CardDefId{5}, 3}, {CardDefId{9}, 1}};
    EXPECT_EQ(comp, want);
}

TEST(KnowledgeTracker, ACardThatBecomesPublicIsForgottenByPrune) {
    Table t;
    const auto shown = t.make(P2, 8, ZoneType::Hand);
    knowledge::seen(t.s, shown, Place::Hand, only(P1));
    auto& ps = t.s.player(P2);
    ps.hand.clear();
    ps.trash.push_back(shown);
    t.s.getObject(shown).zone = ZoneType::Trash;

    // No hook for hand -> trash: the view reads the trash from the state.
    GameKnowledgeView v(t.s, P1);
    EXPECT_TRUE(v.about(shown).identity_known);
    EXPECT_EQ(v.about(shown).last_known_zone, ZoneType::Trash);
    EXPECT_TRUE(v.knownHiddenCards(P2).empty());
    knowledge::prune(t.s);
    EXPECT_TRUE(t.s.knowledge.facts[0].empty());
}

TEST(KnowledgeTracker, ACopiedStateCarriesTheKnowledge) {
    Table t;
    const auto d = t.deck(P1, 4);
    knowledge::seenInDeck(t.s, d[0], only(P1), DeckEnd::Top, 0);
    const GameState clone = t.s;
    GameKnowledgeView v(clone, P1);
    ASSERT_EQ(v.knownDeckTop(P1).size(), 1u);
    EXPECT_EQ(v.knownDeckTop(P1)[0].object, d[0]);
}

TEST(KnowledgeTracker, CheckReportsADeckEditThatBypassedTheHooks) {
    Table t;
    const auto d = t.deck(P1, 4);
    knowledge::seenInDeck(t.s, d[1], only(P1), DeckEnd::Top, 1);
    EXPECT_TRUE(knowledge::check(t.s).empty());
    t.s.player(P1).main_deck.pop_back();   // a draw with no hook
    EXPECT_FALSE(knowledge::check(t.s).empty());
}

} // namespace
} // namespace riftbound
