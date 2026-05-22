/// @file test_entity_tokens.cpp
/// Tests for the V2 entity-token extractor (Phase 6d).
///
/// Covers:
///   - Tensor shape correctness (all per-token vectors are kMaxEntityTokens
///     long, stats is kMaxEntityTokens*kEntityStatsDim).
///   - Padding mask correctness (valid_mask is 1 for [0, n_entities)
///     and 0 for the rest).
///   - Information-set masking (opp hand + main_deck identities are 0
///     from opp perspective; revealed from own perspective).
///   - Chain item emission (each chain item gets a token with the
///     right chain_index).
///   - Spatial-node mapping (units at BFs get distinct node IDs;
///     off-board entities get -1).

#include "ml/entity_tokens.h"
#include "core/card_db.h"
#include "core/game_object.h"
#include "core/game_state.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <string>

using namespace riftbound;
using namespace riftbound::ml;

namespace {

// Locate cards/registry.json from a few common spots so the test can
// run from the build dir OR the repo root.
std::string findRegistry() {
    namespace fs = std::filesystem;
    if (const char* env = std::getenv("RIFTBOUND_ROOT")) {
        auto p = fs::path(env) / "cards" / "registry.json";
        if (fs::exists(p)) return p.string();
    }
    for (const auto& candidate : {
            "cards/registry.json",
            "../cards/registry.json",
            "../../cards/registry.json",
        }) {
        if (fs::exists(candidate)) return candidate;
    }
    return "cards/registry.json";  // best-effort; CardDB load will fail loudly
}

class EntityTokensTest : public ::testing::Test {
protected:
    CardDB card_db;
    GameState state;

    void SetUp() override {
        card_db.loadFromRegistry(findRegistry());
        state = GameState{};
        state.players[0].id = PlayerId::Player1;
        state.players[1].id = PlayerId::Player2;
    }

    GameObjectId addObj(PlayerId controller, CardType type, ZoneType zone) {
        GameObjectId id = state.createObject();
        auto& obj = state.getObject(id);
        obj.owner = controller;
        obj.controller = controller;
        obj.card_type = type;
        obj.zone = zone;
        return id;
    }
};

TEST_F(EntityTokensTest, OutputVectorsHaveFixedShape) {
    auto tokens = extractEntityTokens(state, PlayerId::Player1, card_db);
    EXPECT_EQ(tokens.card_def_id.size(),    static_cast<size_t>(kMaxEntityTokens));
    EXPECT_EQ(tokens.zone_id.size(),        static_cast<size_t>(kMaxEntityTokens));
    EXPECT_EQ(tokens.domain_id.size(),      static_cast<size_t>(kMaxEntityTokens));
    EXPECT_EQ(tokens.stance_id.size(),      static_cast<size_t>(kMaxEntityTokens));
    EXPECT_EQ(tokens.is_perspective.size(), static_cast<size_t>(kMaxEntityTokens));
    EXPECT_EQ(tokens.chain_index.size(),    static_cast<size_t>(kMaxEntityTokens));
    EXPECT_EQ(tokens.spatial_node.size(),   static_cast<size_t>(kMaxEntityTokens));
    EXPECT_EQ(tokens.valid_mask.size(),     static_cast<size_t>(kMaxEntityTokens));
    EXPECT_EQ(tokens.stats.size(),
              static_cast<size_t>(kMaxEntityTokens * kEntityStatsDim));
}

TEST_F(EntityTokensTest, PaddingMaskMatchesEntityCount) {
    auto h1 = addObj(PlayerId::Player1, CardType::Spell, ZoneType::Hand);
    auto h2 = addObj(PlayerId::Player1, CardType::Spell, ZoneType::Hand);
    state.players[0].hand.push_back(h1);
    state.players[0].hand.push_back(h2);

    auto tokens = extractEntityTokens(state, PlayerId::Player1, card_db);
    ASSERT_GE(tokens.n_entities, 2);
    for (int i = 0; i < tokens.n_entities; ++i) {
        EXPECT_EQ(tokens.valid_mask[i], 1) << "valid token at " << i;
    }
    for (int i = tokens.n_entities; i < kMaxEntityTokens; ++i) {
        EXPECT_EQ(tokens.valid_mask[i], 0) << "padding at " << i;
    }
}

TEST_F(EntityTokensTest, OppHandIdentityIsMasked) {
    // Opp has 3 cards in hand. From P1's perspective, the count should
    // be observable but card_def_id == 0 (masked).
    for (int i = 0; i < 3; ++i) {
        auto id = addObj(PlayerId::Player2, CardType::Spell, ZoneType::Hand);
        state.getObject(id).card_def_id = 100 + i;  // arbitrary nonzero
        state.players[1].hand.push_back(id);
    }
    auto from_p1 = extractEntityTokens(state, PlayerId::Player1, card_db);
    auto from_p2 = extractEntityTokens(state, PlayerId::Player2, card_db);

    int p2_hand_tokens_from_p1 = 0;
    int p2_hand_tokens_from_p2 = 0;
    for (int i = 0; i < from_p1.n_entities; ++i) {
        if (from_p1.zone_id[i] == static_cast<int>(EntityZone::OppHand)) {
            ++p2_hand_tokens_from_p1;
            EXPECT_EQ(from_p1.card_def_id[i], 0)
                << "opp hand identity must be masked from P1 perspective";
        }
    }
    for (int i = 0; i < from_p2.n_entities; ++i) {
        if (from_p2.zone_id[i] == static_cast<int>(EntityZone::SelfHand)) {
            ++p2_hand_tokens_from_p2;
            EXPECT_NE(from_p2.card_def_id[i], 0)
                << "self hand identity must be revealed";
        }
    }
    EXPECT_EQ(p2_hand_tokens_from_p1, 3);
    EXPECT_EQ(p2_hand_tokens_from_p2, 3);
}

TEST_F(EntityTokensTest, OppMainDeckIdentityIsMasked) {
    for (int i = 0; i < 5; ++i) {
        auto id = addObj(PlayerId::Player2, CardType::Spell, ZoneType::MainDeck);
        state.getObject(id).card_def_id = 200 + i;
        state.players[1].main_deck.push_back(id);
    }
    auto tokens = extractEntityTokens(state, PlayerId::Player1, card_db);
    int opp_deck_tokens = 0;
    for (int i = 0; i < tokens.n_entities; ++i) {
        if (tokens.zone_id[i] == static_cast<int>(EntityZone::OppMainDeck)) {
            ++opp_deck_tokens;
            EXPECT_EQ(tokens.card_def_id[i], 0);
        }
    }
    EXPECT_EQ(opp_deck_tokens, 5);
}

TEST_F(EntityTokensTest, OppTrashIsRevealed) {
    // Opp trash is public per CR — identity should NOT be masked.
    for (int i = 0; i < 2; ++i) {
        auto id = addObj(PlayerId::Player2, CardType::Spell, ZoneType::Trash);
        state.getObject(id).card_def_id = 300 + i;
        state.players[1].trash.push_back(id);
    }
    auto tokens = extractEntityTokens(state, PlayerId::Player1, card_db);
    bool found_300 = false, found_301 = false;
    for (int i = 0; i < tokens.n_entities; ++i) {
        if (tokens.zone_id[i] == static_cast<int>(EntityZone::OppTrash)) {
            if (tokens.card_def_id[i] == 300) found_300 = true;
            if (tokens.card_def_id[i] == 301) found_301 = true;
        }
    }
    EXPECT_TRUE(found_300);
    EXPECT_TRUE(found_301);
}

TEST_F(EntityTokensTest, BattlefieldUnitsGetSpatialNodes) {
    // Add two BFs; place a unit at each.
    BattlefieldState bf0{};  bf0.id = 0;  state.battlefields.push_back(bf0);
    BattlefieldState bf1{};  bf1.id = 1;  state.battlefields.push_back(bf1);

    auto u0 = addObj(PlayerId::Player1, CardType::Unit, ZoneType::BattlefieldZone);
    state.getObject(u0).card_def_id = 500;
    state.getObject(u0).location = BattlefieldLocation{0};

    auto u1 = addObj(PlayerId::Player1, CardType::Unit, ZoneType::BattlefieldZone);
    state.getObject(u1).card_def_id = 501;
    state.getObject(u1).location = BattlefieldLocation{1};

    auto tokens = extractEntityTokens(state, PlayerId::Player1, card_db);
    int u0_node = -2, u1_node = -2;
    for (int i = 0; i < tokens.n_entities; ++i) {
        if (tokens.card_def_id[i] == 500) u0_node = tokens.spatial_node[i];
        if (tokens.card_def_id[i] == 501) u1_node = tokens.spatial_node[i];
    }
    EXPECT_EQ(u0_node, kSpatialNodeBfBase + 0);  // BF0 = node 2
    EXPECT_EQ(u1_node, kSpatialNodeBfBase + 1);  // BF1 = node 3
}

TEST_F(EntityTokensTest, BaseUnitsMapToSelfOrOppBaseNode) {
    auto self_unit = addObj(PlayerId::Player1, CardType::Unit, ZoneType::Base);
    state.getObject(self_unit).card_def_id = 600;
    state.getObject(self_unit).location = BaseLocation{PlayerId::Player1};

    auto opp_unit = addObj(PlayerId::Player2, CardType::Unit, ZoneType::Base);
    state.getObject(opp_unit).card_def_id = 601;
    state.getObject(opp_unit).location = BaseLocation{PlayerId::Player2};

    auto tokens = extractEntityTokens(state, PlayerId::Player1, card_db);
    int self_node = -2, opp_node = -2;
    for (int i = 0; i < tokens.n_entities; ++i) {
        if (tokens.card_def_id[i] == 600) self_node = tokens.spatial_node[i];
        if (tokens.card_def_id[i] == 601) opp_node = tokens.spatial_node[i];
    }
    EXPECT_EQ(self_node, kSpatialNodeSelfBase);
    EXPECT_EQ(opp_node,  kSpatialNodeOppBase);
}

TEST_F(EntityTokensTest, OffBoardEntitiesHaveNegativeOneSpatialNode) {
    auto hand_card = addObj(PlayerId::Player1, CardType::Spell, ZoneType::Hand);
    state.getObject(hand_card).card_def_id = 700;
    state.players[0].hand.push_back(hand_card);

    auto tokens = extractEntityTokens(state, PlayerId::Player1, card_db);
    for (int i = 0; i < tokens.n_entities; ++i) {
        if (tokens.card_def_id[i] == 700) {
            EXPECT_EQ(tokens.spatial_node[i], kSpatialNodeOffBoard);
        }
    }
}

TEST_F(EntityTokensTest, ChainItemsEmittedWithIncreasingChainIndex) {
    // Push two chain items.
    ChainItem ci1;
    ci1.id = state.chain.allocateId();
    ci1.source = kInvalidId;
    ci1.controller = PlayerId::Player1;
    ci1.card_def_id = 50;
    ci1.is_spell = true;
    state.chain.items.push_back(ci1);

    ChainItem ci2;
    ci2.id = state.chain.allocateId();
    ci2.source = kInvalidId;
    ci2.controller = PlayerId::Player2;
    ci2.card_def_id = 51;
    ci2.is_spell = true;
    state.chain.items.push_back(ci2);

    auto tokens = extractEntityTokens(state, PlayerId::Player1, card_db);
    int found = 0;
    int last_idx = -1;
    for (int i = 0; i < tokens.n_entities; ++i) {
        if (tokens.zone_id[i] == static_cast<int>(EntityZone::ChainItem)) {
            ++found;
            EXPECT_GE(tokens.chain_index[i], 0);
            EXPECT_GT(tokens.chain_index[i], last_idx);
            last_idx = tokens.chain_index[i];
        }
    }
    EXPECT_EQ(found, 2);
}

TEST_F(EntityTokensTest, IsPerspectiveFlagSetCorrectly) {
    auto p1_card = addObj(PlayerId::Player1, CardType::Spell, ZoneType::Hand);
    state.getObject(p1_card).card_def_id = 800;
    state.players[0].hand.push_back(p1_card);

    auto p2_card = addObj(PlayerId::Player2, CardType::Spell, ZoneType::Hand);
    state.getObject(p2_card).card_def_id = 801;
    state.players[1].hand.push_back(p2_card);

    auto from_p1 = extractEntityTokens(state, PlayerId::Player1, card_db);
    for (int i = 0; i < from_p1.n_entities; ++i) {
        if (from_p1.card_def_id[i] == 800) {
            EXPECT_EQ(from_p1.is_perspective[i], 1);
        }
        // p2's card from P1 perspective has masked identity (id=0) so
        // we can't filter by 801; check by zone instead.
        if (from_p1.zone_id[i] == static_cast<int>(EntityZone::OppHand)) {
            EXPECT_EQ(from_p1.is_perspective[i], 0);
        }
    }
}

}  // namespace
