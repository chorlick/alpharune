#include <gtest/gtest.h>
#include "core/card_db.h"
#include "rules/deck_validator.h"

#include <filesystem>

using namespace riftbound;

namespace {

std::string registryPath() {
    for (auto& p : {"../cards/registry.json", "../../cards/registry.json",
                     "cards/registry.json"}) {
        if (std::filesystem::exists(p)) return p;
    }
    const char* root = std::getenv("RIFTBOUND_ROOT");
    if (root) {
        auto path = std::filesystem::path(root) / "cards" / "registry.json";
        if (std::filesystem::exists(path)) return path.string();
    }
    return "cards/registry.json";
}

std::string deckPath(const char* name) {
    for (auto& prefix : {"../decks/", "../../decks/", "decks/"}) {
        auto p = std::string(prefix) + name;
        if (std::filesystem::exists(p)) return p;
    }
    const char* root = std::getenv("RIFTBOUND_ROOT");
    if (root) {
        auto path = std::filesystem::path(root) / "decks" / name;
        if (std::filesystem::exists(path)) return path.string();
    }
    return std::string("decks/") + name;
}

} // namespace

class DeckValidatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto path = registryPath();
        if (!std::filesystem::exists(path)) {
            GTEST_SKIP() << "registry.json not found";
        }
        db.loadFromRegistry(path);
    }

    /// Build a valid LeBlanc deck for testing modifications against.
    DeckSubmission makeValidDeck() {
        auto* legend = db.findByName("Deceiver");
        auto* champion = db.findByName("LeBlanc, Fragmented");
        EXPECT_NE(legend, nullptr);
        EXPECT_NE(champion, nullptr);
        if (!legend || !champion) return {};

        DeckSubmission deck;
        deck.legend = legend->id;
        deck.chosen_champion = champion->id;

        // 39 main deck cards (Mind/Order domain identity)
        auto addCards = [&](const char* name, int count) {
            auto* card = db.findByName(name);
            if (card) {
                for (int i = 0; i < count; ++i)
                    deck.main_deck.push_back(card->id);
            }
        };

        addCards("Watchful Sentry", 3);
        addCards("Cull the Weak", 3);
        addCards("Soaring Scout", 3);
        addCards("Honest Broker", 3);
        addCards("Deathgrip", 3);
        addCards("Glasc Mixologist", 3);
        addCards("Ruined Rex", 3);
        addCards("Sacrifice", 3);
        addCards("Mirror Image", 3);
        addCards("Hidden Blade", 2);
        addCards("Shadow's Call", 2);
        addCards("Thousand-Tailed Watcher", 1);
        addCards("Facebreaker", 1);
        addCards("Black Rose Dignitary", 1);
        addCards("Tactical Retreat", 1);
        addCards("LeBlanc, Fragmented", 1);
        addCards("Karthus, Eternal", 3);

        // 12 runes
        auto* order_rune = db.findByName("Order Rune");
        auto* mind_rune = db.findByName("Mind Rune");
        if (order_rune && mind_rune) {
            for (int i = 0; i < 8; ++i) deck.rune_deck.push_back(order_rune->id);
            for (int i = 0; i < 4; ++i) deck.rune_deck.push_back(mind_rune->id);
        }

        // 3 battlefields
        auto addBF = [&](const char* name) {
            auto* bf = db.findByName(name);
            if (bf) deck.battlefields.push_back(bf->id);
        };
        addBF("Windswept Hillock");
        addBF("Dusk Rose Lab");
        addBF("Forbidding Waste");

        // Sideboard
        auto addSB = [&](const char* name, int count) {
            auto* card = db.findByName(name);
            if (card) {
                for (int i = 0; i < count; ++i)
                    deck.sideboard.push_back(card->id);
            }
        };
        addSB("Unchecked Power", 2);
        addSB("Salvage", 2);
        addSB("Facebreaker", 1);
        addSB("LeBlanc, Everywhere at Once", 1);
        addSB("LeBlanc, Fragmented", 1);
        addSB("Tactical Retreat", 1);

        return deck;
    }

    CardDB db;
};

TEST_F(DeckValidatorTest, ValidDeckPasses) {
    auto deck = makeValidDeck();
    DeckValidator validator(db);
    auto result = validator.validate(deck);
    EXPECT_TRUE(result.is_legal) << "Errors:";
    for (auto& e : result.errors) {
        ADD_FAILURE() << "  " << e;
    }
}

TEST_F(DeckValidatorTest, LoadFromJsonAndValidate) {
    auto path = deckPath("leblanc_test.json");
    if (!std::filesystem::exists(path)) {
        GTEST_SKIP() << "leblanc_test.json not found";
    }

    auto deck = DeckValidator::loadFromJson(path, db);
    DeckValidator validator(db);
    auto result = validator.validate(deck);
    EXPECT_TRUE(result.is_legal);
    for (auto& e : result.errors) {
        ADD_FAILURE() << "  " << e;
    }
}

TEST_F(DeckValidatorTest, MissingLegend) {
    auto deck = makeValidDeck();
    deck.legend = 0;
    DeckValidator validator(db);
    auto result = validator.validate(deck);
    EXPECT_FALSE(result.is_legal);
    EXPECT_EQ(result.errors.size(), 1u);
}

TEST_F(DeckValidatorTest, MissingChampion) {
    auto deck = makeValidDeck();
    deck.chosen_champion = 0;
    DeckValidator validator(db);
    auto result = validator.validate(deck);
    EXPECT_FALSE(result.is_legal);
}

TEST_F(DeckValidatorTest, WrongDeckSize) {
    auto deck = makeValidDeck();
    deck.main_deck.pop_back(); // 38 + champion = 39, not 40
    DeckValidator validator(db);
    auto result = validator.validate(deck);
    EXPECT_FALSE(result.is_legal);
    bool found_size_error = false;
    for (auto& e : result.errors) {
        if (e.find("40 cards") != std::string::npos) found_size_error = true;
    }
    EXPECT_TRUE(found_size_error);
}

TEST_F(DeckValidatorTest, WrongRuneCount) {
    auto deck = makeValidDeck();
    deck.rune_deck.pop_back(); // 11 runes
    DeckValidator validator(db);
    auto result = validator.validate(deck);
    EXPECT_FALSE(result.is_legal);
}

TEST_F(DeckValidatorTest, TooManyBattlefields) {
    auto deck = makeValidDeck();
    deck.battlefields.push_back(deck.battlefields[0]); // 4 BFs
    DeckValidator validator(db);
    auto result = validator.validate(deck);
    EXPECT_FALSE(result.is_legal);
}

TEST_F(DeckValidatorTest, DuplicateBattlefieldNames) {
    auto deck = makeValidDeck();
    deck.battlefields[1] = deck.battlefields[0]; // same name twice
    DeckValidator validator(db);
    auto result = validator.validate(deck);
    EXPECT_FALSE(result.is_legal);
    bool found_dupe = false;
    for (auto& e : result.errors) {
        if (e.find("Duplicate") != std::string::npos) found_dupe = true;
    }
    EXPECT_TRUE(found_dupe);
}

TEST_F(DeckValidatorTest, SideboardTooLarge) {
    auto deck = makeValidDeck();
    // Add cards until sideboard > 8
    auto* card = db.findByName("Salvage");
    if (card) {
        while (deck.sideboard.size() < 10) {
            deck.sideboard.push_back(card->id);
        }
    }
    DeckValidator validator(db);
    auto result = validator.validate(deck);
    EXPECT_FALSE(result.is_legal);
}

TEST_F(DeckValidatorTest, DomainViolation) {
    auto deck = makeValidDeck();
    // Add a Fury card to a Mind/Order deck
    auto* fury_card = db.findByName("Blazing Scorcher");
    if (fury_card) {
        deck.main_deck.back() = fury_card->id; // replace last card
        DeckValidator validator(db);
        auto result = validator.validate(deck);
        EXPECT_FALSE(result.is_legal);
        bool found_domain = false;
        for (auto& e : result.errors) {
            if (e.find("domain") != std::string::npos) found_domain = true;
        }
        EXPECT_TRUE(found_domain);
    }
}

TEST_F(DeckValidatorTest, WrongRuneDomain) {
    auto deck = makeValidDeck();
    // Replace an Order Rune with a Fury Rune
    auto* fury_rune = db.findByName("Fury Rune");
    if (fury_rune) {
        deck.rune_deck[0] = fury_rune->id;
        DeckValidator validator(db);
        auto result = validator.validate(deck);
        EXPECT_FALSE(result.is_legal);
    }
}

TEST_F(DeckValidatorTest, ChampionTagMismatch) {
    auto deck = makeValidDeck();
    // Replace champion with one from a different legend
    auto* wrong_champ = db.findByName("Jinx, Demolitionist");
    if (wrong_champ) {
        deck.chosen_champion = wrong_champ->id;
        DeckValidator validator(db);
        auto result = validator.validate(deck);
        EXPECT_FALSE(result.is_legal);
        bool found_tag = false;
        for (auto& e : result.errors) {
            if (e.find("champion tag") != std::string::npos ||
                e.find("domain") != std::string::npos) {
                found_tag = true;
            }
        }
        EXPECT_TRUE(found_tag);
    }
}

TEST_F(DeckValidatorTest, CopyLimitExceeded) {
    auto deck = makeValidDeck();
    // Add a 4th copy of a card that already has 3 + 1 in sideboard
    auto* card = db.findByName("Facebreaker");
    if (card) {
        // Already have 1 main + 1 side = 2. Add 2 more to main.
        deck.main_deck.push_back(card->id);
        deck.main_deck.push_back(card->id);
        // Now need to remove 2 cards to keep deck at 40
        // (but we're testing copy limits, not size, so just check for that error)
        DeckValidator validator(db);
        auto result = validator.validate(deck);
        EXPECT_FALSE(result.is_legal);
        bool found_copies = false;
        for (auto& e : result.errors) {
            if (e.find("copies") != std::string::npos) found_copies = true;
        }
        EXPECT_TRUE(found_copies);
    }
}
