/// Information-set ObservationTensor masking tests.
///
/// Phase B requirement: ObservationTensor(player) must mask hidden bits
/// — opponent hand identities, opponent main_deck ordering, facedown
/// card contents. These tests construct a GameState, take P1's
/// observation, mutate P2's hidden zones, and assert P1's observation
/// is byte-identical (i.e., the mutation is genuinely invisible).
///
/// The complement is also verified: mutating P2's hidden state DOES
/// produce a different observation from P2's own perspective. This is
/// the "real masking, not constant-zero" guardrail — without it, an
/// implementation that returned a zero tensor would also pass the
/// identical-bytes check.

#include <gtest/gtest.h>

#include "core/card_db.h"
#include "core/game_state.h"
#include "ml/feature_extractor.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

using namespace riftbound;

namespace {

std::string registryPath() {
    for (auto& p : {"../cards/registry.json",
                     "../../cards/registry.json",
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

/// Build a state with two players, each holding cards in hand and main_deck.
/// Cards get distinct CardDefIds so the multi-hot zone encoding produces
/// non-trivial output.
struct ObsState {
    GameState state;
    CardDB card_db;

    ObsState() {
        auto path = registryPath();
        card_db.loadFromRegistry(path);
        state.player(PlayerId::Player1).id = PlayerId::Player1;
        state.player(PlayerId::Player2).id = PlayerId::Player2;
        // Add two BFs so battlefield features aren't all zero.
        BattlefieldState bf1; bf1.id = 0; state.battlefields.push_back(bf1);
        BattlefieldState bf2; bf2.id = 1; state.battlefields.push_back(bf2);
    }

    GameObjectId addToHand(PlayerId player, CardDefId def_id) {
        return addInZone(player, def_id, ZoneType::Hand,
                          state.player(player).hand);
    }

    GameObjectId addToDeck(PlayerId player, CardDefId def_id) {
        return addInZone(player, def_id, ZoneType::MainDeck,
                          state.player(player).main_deck);
    }

private:
    GameObjectId addInZone(PlayerId player, CardDefId def_id, ZoneType zone,
                            std::vector<GameObjectId>& zone_vec) {
        GameObjectId id = next_id_++;
        state.objects[id] = GameObject{};
        auto& o = state.objects[id];
        o.id = id;
        o.card_def_id = def_id;
        o.owner = player;
        o.controller = player;
        o.zone = zone;
        zone_vec.push_back(id);
        return id;
    }

    GameObjectId next_id_ = 1;
};

} // namespace

class ObservationTensorMasking : public ::testing::Test {
protected:
    void SetUp() override {
        if (!std::filesystem::exists(registryPath())) {
            GTEST_SKIP() << "registry.json not found at " << registryPath();
        }
    }
};

// P1 observation is invariant under permutation of P2's main_deck.
TEST_F(ObservationTensorMasking, P2DeckReorderInvisibleToP1) {
    ObsState s;
    // P1's deck (visible to P1)
    s.addToDeck(PlayerId::Player1, 10);
    s.addToDeck(PlayerId::Player1, 11);
    // P2's deck (hidden to P1 — ordering must not leak)
    s.addToDeck(PlayerId::Player2, 200);
    s.addToDeck(PlayerId::Player2, 201);
    s.addToDeck(PlayerId::Player2, 202);
    // P1 hand and P2 hand
    s.addToHand(PlayerId::Player1, 50);
    s.addToHand(PlayerId::Player2, 250);

    auto before = ml::extractStateFeatures(s.state, PlayerId::Player1, s.card_db);

    // Mutate P2's deck order — should NOT change P1's observation.
    auto& p2_deck = s.state.player(PlayerId::Player2).main_deck;
    std::reverse(p2_deck.begin(), p2_deck.end());

    auto after = ml::extractStateFeatures(s.state, PlayerId::Player1, s.card_db);

    EXPECT_EQ(before, after)
        << "P1's observation changed when P2's hidden deck order was permuted";
}

// P1 observation is invariant under change of P2's hand identities, as long
// as the count is the same.
TEST_F(ObservationTensorMasking, P2HandIdentityInvisibleToP1) {
    ObsState s;
    s.addToHand(PlayerId::Player1, 10);
    s.addToHand(PlayerId::Player2, 200);

    auto before = ml::extractStateFeatures(s.state, PlayerId::Player1, s.card_db);

    // Swap the identity of P2's hand card. P1 must not see this.
    s.state.getObject(s.state.player(PlayerId::Player2).hand[0]).card_def_id = 300;

    auto after = ml::extractStateFeatures(s.state, PlayerId::Player1, s.card_db);

    EXPECT_EQ(before, after)
        << "P1's observation changed when P2's hidden hand identity was mutated";
}

// Complement: the same mutation IS visible from P2's perspective. Without
// this check, a tensor of all zeros would also be "invariant" — confirming
// masking is real, not vacuous.
TEST_F(ObservationTensorMasking, P2DeckReorderVisibleToP2) {
    ObsState s;
    s.addToDeck(PlayerId::Player2, 200);
    s.addToDeck(PlayerId::Player2, 201);
    s.addToDeck(PlayerId::Player2, 202);

    auto before = ml::extractStateFeatures(s.state, PlayerId::Player2, s.card_db);

    // Re-identify the cards in place (changes the multi-hot count vector
    // because the zone holds different def_ids now).
    s.state.getObject(s.state.player(PlayerId::Player2).main_deck[0]).card_def_id = 700;
    s.state.getObject(s.state.player(PlayerId::Player2).main_deck[1]).card_def_id = 701;

    auto after = ml::extractStateFeatures(s.state, PlayerId::Player2, s.card_db);

    EXPECT_NE(before, after)
        << "P2's own observation didn't change after mutating its own deck — "
           "masking may be over-broad (zeroing out self-visible info)";
}

// Complement: P2's hand identity change is visible to P2 itself.
TEST_F(ObservationTensorMasking, P2HandIdentityVisibleToP2) {
    ObsState s;
    s.addToHand(PlayerId::Player2, 200);

    auto before = ml::extractStateFeatures(s.state, PlayerId::Player2, s.card_db);

    s.state.getObject(s.state.player(PlayerId::Player2).hand[0]).card_def_id = 300;

    auto after = ml::extractStateFeatures(s.state, PlayerId::Player2, s.card_db);

    EXPECT_NE(before, after)
        << "P2's own observation didn't change after mutating its own hand";
}
