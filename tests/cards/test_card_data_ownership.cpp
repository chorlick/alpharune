/// @file test_card_data_ownership.cpp
/// Card data ownership: each Card class implements the def() interface contract
/// — the engine REQUESTS the card's CardDef; the card answers. The runtime
/// CardDB is materialized from those def()s (CardDB::buildFromClasses).
/// registry.json + ban-list.csv are retired; the C++ sources are the only
/// source of truth. These tests prove:
///   - a concrete card answers def() and cardDefId() derives from it,
///   - EVERY registered card answers def() (classDefIds covers the whole DB),
///   - the class-owned def() round-trips into the CardDB,
///   - the baked image_url is populated.

#include "tests/cards/card_test_fixture.h"

#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/card_db.h"

#include <gtest/gtest.h>

namespace riftbound::test {
namespace {

// A minimal concrete card to exercise the def() contract in isolation.
class FakeUnit : public UnitCard {
public:
    const CardDef& def() const override {
        static const CardDef d = [] {
            CardDef d;
            d.id = 999001;
            d.name = "Fake Unit";
            d.card_type = CardType::Unit;
            d.energy_cost = 3;
            return d;
        }();
        return d;
    }
};

using DataOwnershipTest = CardTestFixture;

TEST_F(DataOwnershipTest, CardAnswersDefAndCardDefId) {
    CardRegistry reg;
    reg.registerCard(999001, std::make_unique<FakeUnit>());

    const Card* c = reg.get(999001);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->cardDefId(), 999001u);     // derived from def().id
    EXPECT_EQ(c->def().name, "Fake Unit");
    EXPECT_EQ(c->def().energy_cost, 3);

    const CardDef* cd = reg.classDef(999001);
    ASSERT_NE(cd, nullptr);
    EXPECT_EQ(cd, &c->def());               // classDef just forwards def()
}

TEST_F(DataOwnershipTest, EveryCardAnswersDef) {
    // classDefIds() must cover the whole DB, and every card's def() id matches.
    EXPECT_EQ(card_registry.classDefIds().size(), card_db.size());
    for (const auto& [id, def] : card_db.all()) {
        const Card* c = card_registry.get(id);
        ASSERT_NE(c, nullptr) << "card " << id << " (" << def.name
                              << ") has no registered class";
        EXPECT_EQ(c->cardDefId(), id) << def.name;
        EXPECT_EQ(c->def().id, id) << def.name;
    }
}

TEST_F(DataOwnershipTest, DefRoundTripsIntoCardDb) {
    // CardDB is built from def()s: every authored field must match what the
    // engine queries at runtime.
    for (CardDefId id : card_registry.classDefIds()) {
        const CardDef& c = card_registry.get(id)->def();
        const CardDef& d = card_db.get(id);
        EXPECT_EQ(c.name, d.name) << id;
        EXPECT_EQ(static_cast<int>(c.card_type), static_cast<int>(d.card_type)) << id;
        EXPECT_EQ(c.energy_cost, d.energy_cost) << id;
        EXPECT_EQ(c.power_cost, d.power_cost) << id;
        EXPECT_EQ(c.might, d.might) << id;
        EXPECT_EQ(c.domains, d.domains) << id;
        EXPECT_EQ(c.tags, d.tags) << id;
        EXPECT_EQ(c.keywords.bits, d.keywords.bits) << id;
        EXPECT_NE(card_db.findByName(d.name), nullptr) << d.name;
    }
}

TEST_F(DataOwnershipTest, BakedImageUrlIsPopulated) {
    const Card* c = card_registry.get(1);
    ASSERT_NE(c, nullptr);
    EXPECT_NE(c->def().image_url.find("cmsassets.rgpub.io"), std::string::npos)
        << "image_url should be baked into the card's def()";
    EXPECT_EQ(card_db.get(1).image_url, c->def().image_url);
}

}  // namespace
}  // namespace riftbound::test
