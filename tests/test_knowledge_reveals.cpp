/// What the knowledge tracker learns when a player is shown a hidden card, and
/// when a card both players could see goes back to a hidden zone (plans/09
/// item 9.SS1). Every case runs through the engine — a card is played, a
/// trigger resolves, an executor move is made — and asks both observers.
///
/// Before: a reveal reached the tracker only where the card file also called
/// knowledge::seen / seenInDeck itself (hand reveals did not, nor did Blind
/// Fury's reveal or Fate Weaver's look), and a card entering a hand from the
/// board, a trash, banishment or the chain, or going facedown from a hand, was
/// not reported at all. Now a reveal is recorded by the handler of
/// CardRevealedEvent for the players the event names (knowledge::recordReveal
/// -> knowledge::shown), and a card enters a hand or a facedown pile only
/// through deck::putInHand / drawTop / putFacedown, which tell the tracker.

#include "cards/card_test_fixture.h"

#include "engine/deck_ops.h"
#include "engine/knowledge_tracker.h"
#include "ml/feature_extractor.h"

#include <filesystem>
#include <fstream>
#include <regex>

namespace riftbound::test::knowledge_reveals {

constexpr CardDefId kSabotage = 156;         // "They reveal their hand. Choose a non-unit card from it, and recycle that card."
constexpr CardDefId kMindsplitter = 192;     // "They reveal their hand. Choose a card from it, and they discard that card."
constexpr CardDefId kScuttleCrab = 615;      // Deathknell: they reveal their hand; look at their facedown cards this turn
constexpr CardDefId kPakaa = 820;            // "When I move, reveal the top card of your Main Deck. If it's a unit, draw it..."
constexpr CardDefId kCalledShot = 443;       // look at the top 2, draw one, recycle the other
constexpr CardDefId kFateWeaver = 626;       // look at the top 4; may reveal a spell [4]+ and draw it
constexpr CardDefId kVoidHatchling = 341;    // "If you would reveal cards from a deck, look at the top card first."
constexpr CardDefId kMorbidReturn = 170;     // return a unit from your trash to your hand
constexpr CardDefId kChallenge = 128;         // a spell (non-unit) to hold in a hand
constexpr CardDefId kRepulse = 668;
constexpr CardDefId kBlindFury = 25;         // "Each opponent reveals the top card of their Main Deck..."

class KnowledgeReveals : public CardTestFixture {
protected:
    CardKnowledge knows(PlayerId observer, GameObjectId card) const {
        return knowledge::GameKnowledgeView(state, observer).about(card);
    }
    void expectKnownInHand(PlayerId observer, GameObjectId card, const char* why) {
        const auto k = knows(observer, card);
        EXPECT_TRUE(k.identity_known) << toString(observer) << ": " << why;
        EXPECT_EQ(k.last_known_zone, ZoneType::Hand) << why;
        EXPECT_TRUE(k.zone_confirmed) << why;
        EXPECT_EQ(k.card_def_id, state.getObject(card).card_def_id);
    }
    void expectUnknown(PlayerId observer, GameObjectId card, const char* why) {
        EXPECT_FALSE(knows(observer, card).identity_known) << toString(observer) << ": " << why;
    }
    void expectConsistent() {
        const auto problems = knowledge::check(state);
        EXPECT_TRUE(problems.empty()) << problems.front();
    }
    GameObjectId addToTrash(PlayerId owner, CardDefId def) {
        const GameObjectId id = addToHand(owner, def);
        auto& ps = state.player(owner);
        ps.hand.erase(std::remove(ps.hand.begin(), ps.hand.end(), id), ps.hand.end());
        state.getObject(id).zone = ZoneType::Trash;
        ps.trash.push_back(id);
        return id;
    }

    GameObjectId deckCard(PlayerId p, CardDefId def) { return addToDeck(p, def); }
};

// ── A revealed hand ─────────────────────────────────────────────────────────

TEST_F(KnowledgeReveals, ARevealedHandIsKnownWhileItIsChosenFromAndAfter) {
    auto sabotage = addToHand(P1, kSabotage);
    auto a = addToHand(P2, kChallenge);
    auto b = addToHand(P2, kRepulse);
    auto unit = addToHand(P2, kPakaa);
    for (int i = 0; i < 3; ++i) deckCard(P2, kChallenge);
    expectUnknown(P1, a, "before the reveal");

    // At the prompt the caster already knows every card it is choosing from.
    int options_known = 0, options = 0;
    struct AtPrompt : AgentInterface {
        std::function<void(const GameState&, const std::vector<Intent>&)> see;
        Intent selectAction(const GameState& s, const std::vector<Intent>& legal) override {
            const bool prompt = legal.size() > 1 && std::all_of(legal.begin(), legal.end(),
                [](const Intent& i) { return i.type == IntentType::MakeChoice && !i.chosen_objects.empty(); });
            if (prompt) see(s, legal);
            for (const auto& i : legal) if (i.type == IntentType::PassPriority) return i;
            return legal.front();
        }
    } p1;
    p1.see = [&](const GameState& s, const std::vector<Intent>& legal) {
        const knowledge::GameKnowledgeView view(s, P1);
        for (const auto& i : legal) {
            ++options;
            if (view.about(i.chosen_objects.front()).identity_known) ++options_known;
        }
    };
    playThroughEngine(sabotage, &p1);
    EXPECT_EQ(options, 2) << "the two non-unit cards";
    EXPECT_EQ(options_known, 2) << "the caster is choosing between cards it has been shown";

    // One of {a, b} was recycled; the rest of the hand stays known to P1.
    const GameObjectId recycled = inHand(P2, a) ? b : a;
    const GameObjectId kept = recycled == a ? b : a;
    ASSERT_TRUE(inDeck(P2, recycled));
    expectKnownInHand(P1, kept, "revealed and still in the hand");
    expectKnownInHand(P1, unit, "revealed and still in the hand");
    // The recycled card was chosen in the open: both players know it is the
    // bottom card of that deck.
    for (PlayerId who : {P1, P2}) {
        const auto k = knows(who, recycled);
        EXPECT_TRUE(k.identity_known) << toString(who);
        EXPECT_EQ(k.deck_position, DeckPosition::Bottom) << toString(who);
        EXPECT_EQ(k.deck_rank, 0);
        EXPECT_EQ(k.deck_rank_span, 1);
    }
    // The count of reveals is kept as well (it is not the tracker).
    EXPECT_EQ(state.player(P1).observed_cards.size(), 3u);
    expectConsistent();

    // A hand card the other player cannot follow leaves: what P1 knew of the
    // hand is no longer certain (the tracker's rule for an unseen move).
    withEngine([&](GameEngine& e) {
        Intent hide;
        hide.type = IntentType::HideCard;
        hide.player = P2;
        hide.card = kept;
        hide.chosen_battlefield = BattlefieldId::fromRaw(0);
        e.mutableState().battlefields[0].controller = P2;
        e.mutableState().getObject(kept).keywords.set(Keyword::Hidden);
        e.mutableState().player(P2).hide_ignoring_costs_this_turn = true;
        e.testHook_executeIntent(hide);
    }, nullptr, nullptr, P2);
    ASSERT_EQ(state.getObject(kept).zone, ZoneType::FacedownZone);
    EXPECT_FALSE(knows(P1, unit).zone_confirmed)
        << "a card left the hand unseen: it may have been this one";
    EXPECT_TRUE(knows(P1, unit).identity_known);
    EXPECT_TRUE(knows(P2, kept).identity_known) << "its owner knows its own facedown card";
    expectConsistent();
}

TEST_F(KnowledgeReveals, ADiscardChosenFromARevealedHandLeavesTheRestKnown) {
    auto mind = addToHand(P1, kMindsplitter);
    auto a = addToHand(P2, kChallenge);
    auto b = addToHand(P2, kRepulse);
    playThroughEngine(mind);
    const GameObjectId kept = inHand(P2, a) ? a : b;
    ASSERT_TRUE(inHand(P2, kept));
    ASSERT_EQ(state.player(P2).hand.size(), 1u);
    expectKnownInHand(P1, kept, "the discard was public, so what stays is still known");
    expectConsistent();
}

// ── Reveals and looks at a deck ─────────────────────────────────────────────

TEST_F(KnowledgeReveals, ATopCardRevealedToAllIsKnownToBothWhereItGoes) {
    auto pakaa = addUnit(P1, kPakaa, 3);
    deckCard(P1, kChallenge);
    auto unit_on_top = deckCard(P1, kScuttleCrab);       // a unit: "draw it"
    fireTriggerThroughEngine(pakaa, TriggerType::WhenIMove);
    ASSERT_TRUE(inHand(P1, unit_on_top));
    expectKnownInHand(P2, unit_on_top, "revealed from the top, then drawn in the open");
    EXPECT_TRUE(knows(P1, unit_on_top).identity_known);
    expectConsistent();
}

TEST_F(KnowledgeReveals, AnOpponentsRevealedTopCardIsKnownToBoth) {
    // Blind Fury emitted the reveal and told the tracker nothing.
    auto fury = addToHand(P1, kBlindFury);
    deckCard(P2, kRepulse);
    auto top = deckCard(P2, kChallenge);   // a spell: revealed, not played
    playThroughEngine(fury);
    if (inDeck(P2, top)) {
        for (PlayerId who : {P1, P2}) {
            const auto k = knows(who, top);
            EXPECT_TRUE(k.identity_known) << toString(who);
            EXPECT_EQ(k.deck_position, DeckPosition::Top) << toString(who);
            EXPECT_EQ(k.deck_rank, 0);
        }
    } else {
        // Wherever the card's text sent it, it is somewhere public or known.
        EXPECT_TRUE(knows(P1, top).identity_known);
    }
    expectConsistent();
}

TEST_F(KnowledgeReveals, APrivateLookTellsTheLookerOnlyAndAShuffleForgetsThePlace) {
    auto shot = addToHand(P1, kCalledShot);
    auto third = deckCard(P1, kRepulse);
    auto second = deckCard(P1, kChallenge);
    auto top = deckCard(P1, kScuttleCrab);
    // Draw the top one; the other goes to the bottom.
    EnginePathAgent pick(EnginePathAgent::choosing(top));
    playThroughEngine(shot, &pick);
    ASSERT_TRUE(inHand(P1, top));
    ASSERT_TRUE(inDeck(P1, second));
    expectUnknown(P2, top, "a private look and a private draw");
    expectUnknown(P2, second, "recycled face down after a private look");
    {
        const auto k = knows(P1, second);
        EXPECT_TRUE(k.identity_known) << "the looker saw it go to the bottom";
        EXPECT_EQ(k.deck_position, DeckPosition::Bottom);
        EXPECT_EQ(k.deck_rank, 0);
    }
    EXPECT_FALSE(knows(P1, third).identity_known && knows(P1, third).deck_position != DeckPosition::Unknown)
        << "the third card was never looked at";
    expectConsistent();

    // A shuffle: nobody knows a place in that deck any more.
    std::mt19937_64 rng(5);
    deck::shuffle(state, P1, rng);
    EXPECT_EQ(knows(P1, second).deck_position, DeckPosition::Unknown);
    expectConsistent();
}

TEST_F(KnowledgeReveals, ALookedAtCardLeftOnTopStaysKnownToTheLooker) {
    // Void Hatchling: look at the top card before a reveal and leave it there.
    addUnit(P1, kVoidHatchling, 2);
    auto pakaa = addUnit(P1, kPakaa, 3);
    deckCard(P1, kRepulse);
    auto top = deckCard(P1, kChallenge);   // not a unit: Pakaa puts it in the trash
    int looks = 0;
    bool looker_knew = false, other_knew = true;
    struct Keep : AgentInterface {
        std::function<void(const GameState&)> at_look;
        Intent selectAction(const GameState& s, const std::vector<Intent>& legal) override {
            if (legal.size() == 2 && legal[0].choice_label.find("Void Hatchling") != std::string::npos) {
                at_look(s);
                return legal[0];   // leave it
            }
            for (const auto& i : legal) if (i.type == IntentType::PassPriority) return i;
            return legal.front();
        }
    } p1;
    p1.at_look = [&](const GameState& s) {
        ++looks;
        const auto mine = knowledge::GameKnowledgeView(s, P1).about(top);
        looker_knew = mine.identity_known && mine.deck_position == DeckPosition::Top &&
                      mine.deck_rank == 0;
        other_knew = knowledge::GameKnowledgeView(s, P2).about(top).identity_known;
    };
    withEngine([](GameEngine& e) { e.testHook_cleanup(); });   // the Hatchling's aura
    fireTriggerThroughEngine(pakaa, TriggerType::WhenIMove, &p1);
    EXPECT_EQ(looks, 1);
    EXPECT_TRUE(looker_knew) << "the looker knows the top card it looked at";
    EXPECT_FALSE(other_knew) << "a look is private";
    expectConsistent();
}

TEST_F(KnowledgeReveals, ALookAtTheTopCardsIsKnownBeforeTheOfferMadeFromThem) {
    // Fate Weaver looked at the top four without telling the tracker.
    auto weaver = addToHand(P1, kFateWeaver);
    std::vector<GameObjectId> top;
    for (int i = 0; i < 5; ++i) top.push_back(deckCard(P1, kChallenge));
    playThroughEngine(weaver);
    int known = 0;
    for (auto id : top) {
        if (!inDeck(P1, id)) continue;
        const auto k = knows(P1, id);
        if (k.identity_known && k.deck_position != DeckPosition::Unknown) ++known;
        expectUnknown(P2, id, "a private look");
    }
    EXPECT_EQ(known, 4) << "the four looked at, wherever the look left them";
    expectConsistent();
}

// ── Hidden cards looked at ──────────────────────────────────────────────────

TEST_F(KnowledgeReveals, AHandAndFacedownCardsShownToOnePlayer) {
    auto crab = addUnit(P1, kScuttleCrab, 1);
    auto in_hand = addToHand(P2, kChallenge);
    auto facedown = addToHand(P2, kRepulse);
    state.battlefields[1].controller = P2;
    addUnit(P2, kInvalidId, 2, 1);
    withEngine([&](GameEngine& e) {
        Intent hide;
        hide.type = IntentType::HideCard;
        hide.player = P2;
        hide.card = facedown;
        hide.chosen_battlefield = BattlefieldId::fromRaw(1);
        e.mutableState().getObject(facedown).keywords.set(Keyword::Hidden);
        e.mutableState().player(P2).hide_ignoring_costs_this_turn = true;
        e.testHook_executeIntent(hide);
    }, nullptr, nullptr, P2);
    ASSERT_EQ(state.getObject(facedown).zone, ZoneType::FacedownZone);
    expectUnknown(P1, facedown, "hidden from an unknown hand");
    EXPECT_TRUE(knows(P2, facedown).identity_known);

    resolveThroughEngine([&](EffectExecutor& ex, GameState&) { ex.killObject(crab); });
    expectKnownInHand(P1, in_hand, "they revealed their hand");
    {
        const auto k = knows(P1, facedown);
        EXPECT_TRUE(k.identity_known) << "P1 may look at their facedown cards";
        EXPECT_EQ(k.last_known_zone, ZoneType::FacedownZone);
    }
    expectConsistent();
}

// ── A card both players could see goes to a hidden zone ─────────────────────

TEST_F(KnowledgeReveals, AUnitReturnedToHandIsKnownToBeThere) {
    auto unit = addUnit(P2, kPakaa, 3, 0);
    auto other = addToHand(P2, kChallenge);
    resolveThroughEngine([&](EffectExecutor& ex, GameState&) { ex.bounceToHand(unit); });
    ASSERT_TRUE(inHand(P2, unit));
    expectKnownInHand(P1, unit, "it was on the board");
    expectUnknown(P1, other, "the rest of the hand is as hidden as before");
    expectConsistent();
}

TEST_F(KnowledgeReveals, ACardReturnedFromTheTrashIsKnownToBeInTheHand) {
    auto spell = addToHand(P1, kMorbidReturn);
    auto dead = addToTrash(P1, kPakaa);
    playThroughEngine(spell);
    ASSERT_TRUE(inHand(P1, dead));
    expectKnownInHand(P2, dead, "it was in the trash, face up");
    expectConsistent();
}

// ── The flat features do not read the tracker ───────────────────────────────

TEST_F(KnowledgeReveals, TheFlatFeaturesDoNotChangeWithWhatTheTrackerKnows) {
    auto unit = addUnit(P2, kPakaa, 3, 0);
    addToHand(P2, kChallenge);
    resolveThroughEngine([&](EffectExecutor& ex, GameState&) { ex.bounceToHand(unit); });
    ASSERT_FALSE(state.knowledge.facts[0].empty());
    GameState blank = state;
    blank.knowledge = KnowledgeState{};
    for (PlayerId p : {P1, P2})
        EXPECT_EQ(ml::extractStateFeatures(state, p, card_db),
                  ml::extractStateFeatures(blank, p, card_db));
}

// ── One place ───────────────────────────────────────────────────────────────

/// Source lines of `dir` (recursively, .cpp and .h) that match `pattern`,
/// as "file:line".
std::vector<std::string> sourceLines(const std::string& dir, const std::regex& pattern) {
    const char* root = std::getenv("RIFTBOUND_ROOT");
    std::vector<std::string> hits;
    const std::filesystem::path base = std::filesystem::path(root ? root : "..") / dir;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(base)) {
        const auto ext = entry.path().extension();
        if (ext != ".cpp" && ext != ".h") continue;
        std::ifstream in(entry.path());
        std::string line;
        for (int n = 1; std::getline(in, line); ++n) {
            const auto comment = line.find("//");
            if (std::regex_search(line.substr(0, comment), pattern))
                hits.push_back(entry.path().filename().string() + ":" + std::to_string(n));
        }
    }
    std::sort(hits.begin(), hits.end());
    return hits;
}

TEST(KnowledgeOnePlace, ACardEntersAHandOrAFacedownPileOnlyThroughDeckOps) {
    // A card put into a hand or a facedown pile by any other code would move
    // without the tracker hearing of it. Engine and card code call
    // deck::putInHand / drawTop / putFacedown.
    const std::regex into(R"((hand|facedown)\s*\.\s*(push_back|insert|emplace|emplace_back)\s*\()");
    std::vector<std::string> found;
    for (const char* dir : {"src/cards", "src/engine", "src/core", "src/agents"})
        for (auto& hit : sourceLines(dir, into)) found.push_back(hit);
    std::vector<std::string> other;
    int in_deck_ops = 0, self_reinsert = 0;
    for (const auto& hit : found) {
        if (hit.rfind("deck_ops.cpp:", 0) == 0) ++in_deck_ops;
        // Card::payOptionalAdditionalCost takes the card being played out of
        // its own hand while a discard is chosen and puts it back: no move.
        else if (hit.rfind("card.cpp:", 0) == 0) ++self_reinsert;
        else other.push_back(hit);
    }
    EXPECT_EQ(in_deck_ops, 3);
    EXPECT_EQ(self_reinsert, 1);
    std::string list;
    for (const auto& o : other) list += " " + o;
    EXPECT_TRUE(other.empty()) << "cards enter a hand or a facedown pile outside deck_ops:" << list;
}

TEST(KnowledgeOnePlace, CardsAnnounceARevealAndDoNotTellTheTrackerThemselves) {
    // knowledge::seen / seenInDeck are for the tracker's own entry points
    // (knowledge::shown, called by the CardRevealedEvent handler and by a
    // look that announces no event). A card file that calls them is telling
    // the tracker card by card again.
    const std::regex direct(R"(knowledge::(seen|seenInDeck)\s*\()");
    std::string list;
    for (const char* dir : {"src/cards", "src/engine"})
        for (const auto& hit : sourceLines(dir, direct))
            if (hit.rfind("knowledge_tracker.", 0) != 0) list += " " + hit;
    EXPECT_TRUE(list.empty()) << "direct calls:" << list;
}

} // namespace riftbound::test::knowledge_reveals
