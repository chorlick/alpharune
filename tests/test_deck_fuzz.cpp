/// The random deck generator (rules/random_deck.h) and the fuzz game loop
/// (tools/fuzz/fuzz_game.h), in a size that runs in seconds. The large run is
/// the `deck_fuzz` tool (tools/deck_fuzz.cpp; CLAUDE.md, Testing): it shares
/// both with this file, so neither can rot uncompiled.

#include <gtest/gtest.h>

#include "cards/card_registry.h"
#include "core/card_db.h"
#include "rules/deck_validator.h"
#include "rules/random_deck.h"
#include "tools/fuzz/fuzz_game.h"

#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace riftbound::test::deck_fuzz {

class DeckFuzz : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        registry_ = new CardRegistry();
        registry_->loadAll();
        db_ = new CardDB();
        db_->buildFromClasses(*registry_);
        builder_ = new RandomDeckBuilder(*db_);
    }
    static void TearDownTestSuite() {
        delete builder_;
        delete db_;
        delete registry_;
    }
    static CardRegistry* registry_;
    static CardDB* db_;
    static RandomDeckBuilder* builder_;
};
CardRegistry* DeckFuzz::registry_ = nullptr;
CardDB* DeckFuzz::db_ = nullptr;
RandomDeckBuilder* DeckFuzz::builder_ = nullptr;

bool sameDeck(const DeckSubmission& a, const DeckSubmission& b) {
    return a.legend == b.legend && a.chosen_champion == b.chosen_champion &&
           a.main_deck == b.main_deck && a.rune_deck == b.rune_deck &&
           a.battlefields == b.battlefields && a.sideboard == b.sideboard;
}

// DeckValidator is the judge of what a legal deck is.
TEST_F(DeckFuzz, EverySeedGivesADeckTheValidatorAccepts) {
    const DeckValidator validator(*db_);
    for (uint64_t seed = 0; seed < 3000; ++seed) {
        const DeckSubmission deck = builder_->build(seed);
        const ValidationResult v = validator.validate(deck);
        ASSERT_TRUE(v.is_legal) << "seed " << seed << ": "
                                << (v.errors.empty() ? "" : v.errors.front());
        ASSERT_EQ(deck.main_deck.size(), 39u);
        ASSERT_TRUE(deck.sideboard.empty());
    }
}

// [Unique] (CR 825.3.a): one card of that name in a deck. The validator does
// not check it; the builder keeps to it.
TEST_F(DeckFuzz, AUniqueCardIsInADeckOnce) {
    int decks_with_one = 0;
    for (uint64_t seed = 0; seed < 3000; ++seed) {
        const DeckSubmission deck = builder_->build(seed);
        std::map<std::string, int> unique_copies;
        for (CardDefId id : deck.main_deck) {
            const CardDef& def = db_->get(id);
            if (def.keywords.has(Keyword::Unique)) ++unique_copies[def.name];
        }
        for (const auto& [name, n] : unique_copies) ASSERT_EQ(n, 1) << name << ", seed " << seed;
        decks_with_one += !unique_copies.empty();
    }
    EXPECT_GT(decks_with_one, 0);
}

TEST_F(DeckFuzz, ASeedIsOneDeck) {
    for (uint64_t seed : {uint64_t{0}, uint64_t{1}, uint64_t{20261011}, ~uint64_t{0}})
        EXPECT_TRUE(sameDeck(builder_->build(seed), builder_->build(seed))) << seed;
    EXPECT_FALSE(sameDeck(builder_->build(1), builder_->build(2)));
    // A second builder on the same card table gives the same decks: nothing
    // depends on the order of a hash map.
    const RandomDeckBuilder again(*db_);
    for (uint64_t seed = 0; seed < 50; ++seed)
        EXPECT_TRUE(sameDeck(builder_->build(seed), again.build(seed))) << seed;
}

constexpr uint64_t kDeckOfSeedOne = 13376009122867512520ull;

// Pinned: a deck seed in a findings document or a result file means this deck.
// It changes when the card pool does (a new set, a ban); say so where the
// seeds are quoted.
TEST_F(DeckFuzz, DeckOfSeedOneIsPinned) {
    const DeckSubmission deck = builder_->build(1);
    uint64_t h = 1469598103934665603ull;
    auto mix = [&h](CardDefId id) { h = (h ^ id.raw()) * 1099511628211ull; };
    mix(deck.legend);
    mix(deck.chosen_champion);
    for (CardDefId id : deck.main_deck) mix(id);
    for (CardDefId id : deck.rune_deck) mix(id);
    for (CardDefId id : deck.battlefields) mix(id);
    EXPECT_EQ(h, kDeckOfSeedOne) << "the deck of seed 1 changed: h = " << h;
}

// Every card that may be in a constructed deck is in some seed's deck; what
// is not reachable is banned or a token's reference card, nothing else.
TEST_F(DeckFuzz, EveryLegalCardIsInSomeSeedsDeck) {
    std::set<CardDefId> seen;
    for (uint64_t seed = 0; seed < 4000; ++seed) {
        const DeckSubmission deck = builder_->build(seed);
        seen.insert(deck.legend);
        seen.insert(deck.chosen_champion);
        seen.insert(deck.main_deck.begin(), deck.main_deck.end());
        seen.insert(deck.rune_deck.begin(), deck.rune_deck.end());
        seen.insert(deck.battlefields.begin(), deck.battlefields.end());
    }
    for (CardDefId id : builder_->reachable())
        EXPECT_TRUE(seen.count(id)) << db_->get(id).name << " (" << id.raw()
                                    << ") is in none of 4000 decks";
    EXPECT_EQ(seen.size(), builder_->reachable().size());

    EXPECT_EQ(builder_->reachable().size() + builder_->unreachable().size(), db_->size());
    for (const auto& [id, why] : builder_->unreachable()) {
        const CardDef& def = db_->get(id);
        EXPECT_TRUE(def.banned || def.isTokenCard())
            << def.name << " (" << id.raw() << ") is legal and in no deck: " << why;
        EXPECT_FALSE(seen.count(id)) << def.name;
    }
}

// The deck list a deck prints is read back as a deck of the same names.
TEST_F(DeckFuzz, ADeckListNamesTheDecksCards) {
    const DeckSubmission deck = builder_->build(7);
    const std::string text = builder_->toDeckList(deck);
    auto count = [&](const std::string& name) {
        int n = 0;
        for (CardDefId id : deck.main_deck) n += db_->get(id).name == name;
        return n;
    };
    const std::string first = db_->get(deck.main_deck.front()).name;
    EXPECT_NE(text.find(std::to_string(count(first)) + " " + first + "\n"), std::string::npos);
    EXPECT_NE(text.find("Legend:\n1 " + db_->get(deck.legend).name), std::string::npos);
}

// The small default of the fuzz: a few dozen games between random decks.
// Each ends with a winner and a reason, plays again as the same game, and the
// coverage listener sees cards played and abilities resolve.
TEST_F(DeckFuzz, ASmallRunPlaysEveryGameToItsEnd) {
    constexpr int kDecks = 12, kGamesPerPair = 3;
    std::vector<DeckSubmission> decks;
    for (int i = 0; i < kDecks; ++i) decks.push_back(builder_->build(9000 + i));

    fuzz::CardCoverage coverage;
    int games = 0;
    for (int i = 0; i < kDecks; ++i) {
        for (int g = 0; g < kGamesPerPair; ++g) {
            const DeckSubmission& a = decks[i];
            const DeckSubmission& b = decks[(i + 1) % kDecks];
            const uint64_t seed = 500 + 10 * i + g;
            fuzz::GameOptions opt;
            opt.coverage = &coverage;
            const fuzz::GameOutcome r = fuzz::playGame(*db_, *registry_, a, b, seed, opt);
            ++games;
            const std::string where = "decks " + std::to_string(9000 + i) + " and " +
                                      std::to_string(9000 + (i + 1) % kDecks) + ", game seed " +
                                      std::to_string(seed);
            EXPECT_FALSE(r.failed) << where << ": " << r.termination << " " << r.exception;
            EXPECT_NE(r.winner, PlayerId::None) << where;
            EXPECT_FALSE(r.termination.empty()) << where;
            EXPECT_GT(r.decisions, 0) << where;

            if (g == 0) {   // one game per pair: the same seeds are the same game
                const fuzz::GameOutcome again = fuzz::playGame(*db_, *registry_, a, b, seed);
                EXPECT_EQ(again.decisions, r.decisions) << where;
                EXPECT_EQ(again.turns, r.turns) << where;
                EXPECT_EQ(again.winner, r.winner) << where;
                EXPECT_EQ(again.termination, r.termination) << where;
            }
        }
    }
    EXPECT_EQ(coverage.games_counted, static_cast<uint64_t>(games));

    auto cardsWith = [&](int kind) {
        int n = 0;
        for (uint32_t g : coverage.games[kind]) n += g > 0;
        return n;
    };
    // 12 decks hold some 300 different cards; most are played in 36 games.
    EXPECT_GT(cardsWith(fuzz::kInDeck), 200);
    EXPECT_GT(cardsWith(fuzz::kInPlay), 150);
    EXPECT_GT(cardsWith(fuzz::kTriggered), 30);
    EXPECT_GT(cardsWith(fuzz::kActivated), 5);
    EXPECT_GT(cardsWith(fuzz::kResolved), 60);
    // Every legend of the run was in play, in every game it was in a deck of.
    for (const auto& d : decks) {
        const std::size_t i = d.legend.raw();
        EXPECT_EQ(coverage.games[fuzz::kInPlay][i], coverage.games[fuzz::kInDeck][i])
            << db_->get(d.legend).name;
    }
    // Nothing is counted for a card that was in no deck and is not a token.
    for (std::size_t i = 1; i < fuzz::kCardSlots; ++i) {
        if (coverage.games[fuzz::kInDeck][i] > 0) continue;
        const CardDefId id = CardDefId::fromRaw(static_cast<uint32_t>(i));
        const bool counted = coverage.games[fuzz::kInPlay][i] > 0 ||
                             coverage.games[fuzz::kTriggered][i] > 0 ||
                             coverage.games[fuzz::kActivated][i] > 0 ||
                             coverage.games[fuzz::kResolved][i] > 0;
        if (counted) {
            EXPECT_TRUE(db_->get(id).isTokenCard()) << db_->get(id).name;
        }
    }
}

// A game the engine cannot finish is an outcome, not an exception: here the
// decision cap, which is how a game that never ends is recorded.
// B1 (batch 2): a Cleanup that follows a quiet one with nothing announced is
// skipped in an optimised build; the checked build runs it and throws if it
// would have changed anything. This game (mel vs fiora, the batch-2 checked
// pairing run) reached the Expiration Step's second pass, whose body ended a
// "this turn" -5 [M] without an announcement: Rengar became Mighty again and
// Grand Duelist triggered in the Cleanup that would have been skipped. The
// body now forgets the skip (GameEngine::doExpirationBody). On the checked
// build this game failed before the fix; on any build it ends normally.
TEST_F(DeckFuzz, B1_TheCleanupAfterTheExpirationBodyIsNeverSkipped) {
    const char* root = std::getenv("RIFTBOUND_ROOT");
    const std::string dir = std::string(root ? root : ".") + "/decks/";
    const DeckSubmission mel = DeckValidator::loadFromDeckList(dir + "mel.txt", *db_);
    const DeckSubmission fiora = DeckValidator::loadFromDeckList(dir + "fiora.txt", *db_);
    const fuzz::GameOutcome r = fuzz::playGame(*db_, *registry_, mel, fiora, 173237097);
    EXPECT_FALSE(r.failed) << r.termination << " " << r.exception;
    EXPECT_NE(r.winner, PlayerId::None);
}

// Found by batch 1b's checked fuzz (random-player:pass=0, seed 20262103):
// Experimental Hexplate gives its bearer the Mech tag in the aura pass and
// Rumble, Scrapper gives Mechs +1 [M] in the same pass; with Rumble's hook
// first, the tag arrived after Rumble had looked. The Cleanup's change check
// did not see a tag change, so the Cleanup ended as "nothing changed" and the
// next one — which gives the +1 [M] — would have been skipped (batch 2's B1
// shortcut). The check now sees an object's tags. On the checked build this
// game threw "a Cleanup that would not have been run ... changed the state".
TEST_F(DeckFuzz, B1_ATagGivenInTheAuraPassIsAChangeTheCleanupSees) {
    fuzz::GameOptions opt;
    opt.agents[0] = "random-player:pass=0";
    opt.agents[1] = "random-player:pass=0";
    const fuzz::GameOutcome r = fuzz::playGame(
        *db_, *registry_, builder_->build(12991493837931873759ULL),
        builder_->build(13821284144776085220ULL), 1546771062, opt);
    EXPECT_FALSE(r.failed) << r.termination << " " << r.exception;
    EXPECT_NE(r.winner, PlayerId::None);
}

TEST_F(DeckFuzz, AGameThatIsStoppedIsRecordedNotThrown) {
    fuzz::GameOptions opt;
    opt.max_decisions = 10;
    const fuzz::GameOutcome r =
        fuzz::playGame(*db_, *registry_, builder_->build(1), builder_->build(2), 77, opt);
    EXPECT_TRUE(r.failed);
    EXPECT_EQ(r.termination, "decision_cap");
    EXPECT_EQ(r.winner, PlayerId::None);
    EXPECT_GE(r.decisions, 10);
}

// An exception thrown inside the engine's game (here by the deck it is given:
// a legend that is no card) is the game's outcome, with its text.
TEST_F(DeckFuzz, AnEngineExceptionIsRecordedWithItsText) {
    DeckSubmission broken = builder_->build(1);
    broken.legend = CardDefId::fromRaw(2047);   // spare range: no such card
    fuzz::GameOutcome r;
    ASSERT_NO_THROW(r = fuzz::playGame(*db_, *registry_, broken, builder_->build(2), 77));
    EXPECT_TRUE(r.failed);
    EXPECT_EQ(r.termination, "engine_error");
    EXPECT_FALSE(r.exception.empty());
}

// The trace of one game: the last lines before it ends.
TEST_F(DeckFuzz, TheLogTailIsTheLastLinesOfTheGame) {
    fuzz::GameOptions opt;
    opt.log_tail = 30;
    const fuzz::GameOutcome r =
        fuzz::playGame(*db_, *registry_, builder_->build(1), builder_->build(2), 77, opt);
    EXPECT_EQ(r.log.size(), 30u);
    // The log does not change the game.
    const fuzz::GameOutcome plain =
        fuzz::playGame(*db_, *registry_, builder_->build(1), builder_->build(2), 77);
    EXPECT_EQ(plain.decisions, r.decisions);
    EXPECT_EQ(plain.winner, r.winner);
    EXPECT_TRUE(plain.log.empty());
}

} // namespace riftbound::test::deck_fuzz
