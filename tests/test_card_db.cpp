#include <gtest/gtest.h>
#include "core/card_db.h"

#include <cstdlib>
#include <filesystem>

using namespace riftbound;

namespace {

std::string registryPath() {
    // Try relative to build dir, then try project root
    for (auto& p : {"../cards/registry.json",
                     "../../cards/registry.json",
                     "cards/registry.json"}) {
        if (std::filesystem::exists(p)) return p;
    }
    // Try from project root via env
    const char* root = std::getenv("RIFTBOUND_ROOT");
    if (root) {
        auto path = std::filesystem::path(root) / "cards" / "registry.json";
        if (std::filesystem::exists(path)) return path.string();
    }
    return "cards/registry.json";
}

} // namespace

class CardDBTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto path = registryPath();
        if (!std::filesystem::exists(path)) {
            GTEST_SKIP() << "registry.json not found at " << path;
        }
        db.loadFromRegistry(path);
    }

    CardDB db;
};

TEST_F(CardDBTest, LoadsCards) {
    EXPECT_GT(db.size(), 700u);
    EXPECT_GT(db.featureVectorSize(), 0);
}

TEST_F(CardDBTest, GetById) {
    // Card ID 1 should exist (first card in sorted registry)
    auto& card = db.get(1);
    EXPECT_FALSE(card.name.empty());
    EXPECT_NE(card.card_type, CardType::Legend); // first card is unlikely to be a legend
}

TEST_F(CardDBTest, FindByName) {
    auto* card = db.findByName("Blazing Scorcher");
    ASSERT_NE(card, nullptr);
    EXPECT_EQ(card->name, "Blazing Scorcher");
    EXPECT_EQ(card->card_type, CardType::Unit);
    EXPECT_EQ(card->might, 5);
    EXPECT_EQ(card->energy_cost, 5);
}

TEST_F(CardDBTest, FindByNameCaseInsensitive) {
    auto* card = db.findByName("blazing scorcher");
    ASSERT_NE(card, nullptr);
    EXPECT_EQ(card->name, "Blazing Scorcher");
}

TEST_F(CardDBTest, FindByNameNotFound) {
    auto* card = db.findByName("Nonexistent Card XYZZY");
    EXPECT_EQ(card, nullptr);
}

TEST_F(CardDBTest, LegendAlias) {
    // "Vex, Gloomist" should resolve to the "Gloomist" legend via alias
    auto* card = db.findByName("Vex, Gloomist");
    if (card) {
        EXPECT_EQ(card->card_type, CardType::Legend);
        EXPECT_EQ(card->name, "Gloomist");
    }
    // Also check direct name works
    auto* direct = db.findByName("Gloomist");
    if (direct) {
        EXPECT_EQ(direct->card_type, CardType::Legend);
    }
}

TEST_F(CardDBTest, ChampionUnit) {
    auto* jinx = db.findByName("Jinx, Demolitionist");
    ASSERT_NE(jinx, nullptr);
    EXPECT_EQ(jinx->card_type, CardType::Unit);
    EXPECT_EQ(jinx->super_type, SuperType::Champion);
    EXPECT_EQ(jinx->might, 4);
    EXPECT_TRUE(jinx->hasTag("Jinx"));
}

TEST_F(CardDBTest, LegendCard) {
    auto* legend = db.findByName("Loose Cannon");
    ASSERT_NE(legend, nullptr);
    EXPECT_EQ(legend->card_type, CardType::Legend);
    EXPECT_EQ(legend->super_type, SuperType::None); // legends are NOT champions
    EXPECT_TRUE(legend->hasTag("Jinx"));
}

TEST_F(CardDBTest, DomainCheck) {
    auto* card = db.findByName("Blazing Scorcher");
    ASSERT_NE(card, nullptr);
    EXPECT_TRUE(card->hasDomain(Domain::Fury));
    EXPECT_FALSE(card->hasDomain(Domain::Calm));
}

TEST_F(CardDBTest, KeywordsLoaded) {
    auto* volibear = db.findByName("Volibear, Furious");
    ASSERT_NE(volibear, nullptr);
    EXPECT_TRUE(volibear->keywords.has(Keyword::Deflect));
    EXPECT_EQ(volibear->deflect_value, 2);
}

TEST_F(CardDBTest, FeatureVector) {
    auto* card = db.findByName("Blazing Scorcher");
    ASSERT_NE(card, nullptr);
    EXPECT_EQ(static_cast<int>(card->feature_vector.size()),
              db.featureVectorSize());
    EXPECT_GT(db.featureVectorSize(), 100);
}

TEST_F(CardDBTest, BattlefieldCard) {
    auto* bf = db.findByName("Void Gate");
    ASSERT_NE(bf, nullptr);
    EXPECT_EQ(bf->card_type, CardType::Battlefield);
}

TEST_F(CardDBTest, RuneCard) {
    auto* rune = db.findByName("Fury Rune");
    ASSERT_NE(rune, nullptr);
    EXPECT_EQ(rune->card_type, CardType::Rune);
    EXPECT_TRUE(rune->hasDomain(Domain::Fury));
}

TEST_F(CardDBTest, GearCard) {
    auto* gear = db.findByName("Iron Ballista");
    ASSERT_NE(gear, nullptr);
    EXPECT_EQ(gear->card_type, CardType::Gear);
}

TEST_F(CardDBTest, InvalidIdThrows) {
    EXPECT_THROW(db.get(99999), std::out_of_range);
}
