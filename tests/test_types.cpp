#include <gtest/gtest.h>
#include "core/types.h"

using namespace riftbound;

TEST(TypesTest, PlayerIdOpponent) {
    EXPECT_EQ(opponent(PlayerId::Player1), PlayerId::Player2);
    EXPECT_EQ(opponent(PlayerId::Player2), PlayerId::Player1);
}

TEST(TypesTest, PlayerIndex) {
    EXPECT_EQ(playerIndex(PlayerId::Player1), 0);
    EXPECT_EQ(playerIndex(PlayerId::Player2), 1);
}

TEST(TypesTest, KeywordSetBasic) {
    KeywordSet kw;
    EXPECT_FALSE(kw.has(Keyword::Accelerate));
    EXPECT_FALSE(kw.has(Keyword::Ganking));

    kw.set(Keyword::Accelerate);
    EXPECT_TRUE(kw.has(Keyword::Accelerate));
    EXPECT_FALSE(kw.has(Keyword::Ganking));

    kw.set(Keyword::Ganking);
    EXPECT_TRUE(kw.has(Keyword::Accelerate));
    EXPECT_TRUE(kw.has(Keyword::Ganking));

    kw.clear(Keyword::Accelerate);
    EXPECT_FALSE(kw.has(Keyword::Accelerate));
    EXPECT_TRUE(kw.has(Keyword::Ganking));
}

TEST(TypesTest, KeywordSetBitOps) {
    KeywordSet a, b;
    a.set(Keyword::Assault);
    a.set(Keyword::Shield);
    b.set(Keyword::Shield);
    b.set(Keyword::Tank);

    auto combined = a | b;
    EXPECT_TRUE(combined.has(Keyword::Assault));
    EXPECT_TRUE(combined.has(Keyword::Shield));
    EXPECT_TRUE(combined.has(Keyword::Tank));

    auto intersect = a & b;
    EXPECT_FALSE(intersect.has(Keyword::Assault));
    EXPECT_TRUE(intersect.has(Keyword::Shield));
    EXPECT_FALSE(intersect.has(Keyword::Tank));
}

TEST(TypesTest, KeywordSetReset) {
    KeywordSet kw;
    kw.set(Keyword::Accelerate);
    kw.set(Keyword::Ganking);
    kw.set(Keyword::Shield);
    kw.reset();
    EXPECT_EQ(kw.bits, 0u);
}

TEST(TypesTest, LocationIdVariant) {
    LocationId base = BaseLocation{PlayerId::Player1};
    LocationId bf = BattlefieldLocation{42};

    EXPECT_TRUE(std::holds_alternative<BaseLocation>(base));
    EXPECT_TRUE(std::holds_alternative<BattlefieldLocation>(bf));

    EXPECT_EQ(std::get<BaseLocation>(base).player, PlayerId::Player1);
    EXPECT_EQ(std::get<BattlefieldLocation>(bf).id, 42u);
}

TEST(TypesTest, LocationIdEquality) {
    LocationId a = BaseLocation{PlayerId::Player1};
    LocationId b = BaseLocation{PlayerId::Player1};
    LocationId c = BaseLocation{PlayerId::Player2};
    LocationId d = BattlefieldLocation{1};

    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
    EXPECT_NE(a, d);
}

TEST(TypesTest, ToStringCoverage) {
    // Ensure all toString functions return non-null
    EXPECT_STREQ(toString(CardType::Unit), "Unit");
    EXPECT_STREQ(toString(CardType::Legend), "Legend");
    EXPECT_STREQ(toString(SuperType::Champion), "Champion");
    EXPECT_STREQ(toString(Domain::Fury), "Fury");
    EXPECT_STREQ(toString(Domain::Order), "Order");
    EXPECT_STREQ(toString(TurnPhase::MainPhase), "MainPhase");
    EXPECT_STREQ(toString(PlayerId::Player1), "P1");
    EXPECT_STREQ(toString(IntentType::StandardMove), "StandardMove");
    EXPECT_STREQ(toString(CombatDesignation::Attacker), "Attacker");
    EXPECT_STREQ(toString(ScoreMethod::Hold), "Hold");
}

TEST(TypesTest, ModeOfPlayDefaults) {
    ModeOfPlay mode;
    EXPECT_EQ(mode.num_players, 2);
    EXPECT_EQ(mode.victory_score, 8);
    EXPECT_EQ(mode.battlefield_count, 2);
    EXPECT_TRUE(mode.second_player_extra_channel);
}
