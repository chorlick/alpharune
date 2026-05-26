/// @file test_wave_b_play_locations.cpp
/// Wave B play-location / aura-flag cluster (Group B):
///   Perched Grimwyrm (338)   — restrictsPlayLocations + conquered-this-turn BFs
///   Miss Fortune, Buccaneer (193) — grant_friendly_units_open_bf
///   Mageseeker Warden (70)   — units_play_base_only on the opponent
///   Rengar, Trophy Hunter (682)   — ambushToEnemyBattlefields
///   Renata Glasc, Industrialist (492) — tokens_enter_ready replacement

#include "tests/cards/card_test_fixture.h"
#include <gtest/gtest.h>

namespace riftbound::test {
namespace {

using PlayLocTest = CardTestFixture;

constexpr CardDefId kPerchedGrimwyrm = 338;
constexpr CardDefId kMissFortuneBucc = 193;
constexpr CardDefId kMageseekerWarden = 70;
constexpr CardDefId kRengarTrophy = 682;
constexpr CardDefId kRenataGlasc = 492;

// ── Perched Grimwyrm: narrow to battlefields conquered this turn ──
TEST_F(PlayLocTest, PerchedGrimwyrm_RestrictsAndUsesConqueredBFs) {
    Card* c = card_registry.get(kPerchedGrimwyrm);
    ASSERT_NE(c, nullptr);
    EXPECT_TRUE(c->restrictsPlayLocations());
    // No conquered BF this turn → no legal play locations.
    EXPECT_TRUE(c->getPlayLocations(state, P1).empty());
    // Mark bf 1 conquered by P1 this turn.
    state.battlefields[1].conquered_by_player = P1;
    state.battlefields[1].conquered_on_turn = state.turn.turn_number;
    auto locs = c->getPlayLocations(state, P1);
    ASSERT_EQ(locs.size(), 1u);
    ASSERT_TRUE(std::holds_alternative<BattlefieldLocation>(locs[0]));
    EXPECT_EQ(std::get<BattlefieldLocation>(locs[0]).id, 1u);
    // A stale conquer (previous turn) does not count.
    state.battlefields[1].conquered_on_turn = state.turn.turn_number - 1;
    EXPECT_TRUE(c->getPlayLocations(state, P1).empty());
}

// ── Miss Fortune: grants open-BF plays to the controller's units ──
TEST_F(PlayLocTest, MissFortune_GrantsOpenBattlefieldFlag) {
    auto mf = addUnit(P1, kMissFortuneBucc, 4, /*at_bf=*/0);
    EXPECT_FALSE(state.player(P1).grant_friendly_units_open_bf);
    Card* c = card_registry.get(kMissFortuneBucc);
    c->applyPassiveAura(state, P1);
    EXPECT_TRUE(state.player(P1).grant_friendly_units_open_bf);
    (void)mf;
}

// ── Mageseeker Warden: restricts the OPPONENT to base unit-plays when at a BF ──
TEST_F(PlayLocTest, MageseekerWarden_RestrictsOpponentWhenAtBattlefield) {
    Card* c = card_registry.get(kMageseekerWarden);
    ASSERT_NE(c, nullptr);
    // At base → no restriction.
    addUnit(P1, kMageseekerWarden, 5, /*at_bf=*/-1);
    c->applyPassiveAura(state, P1);
    EXPECT_FALSE(state.player(P2).units_play_base_only);
    // At a battlefield → opponent restricted.
    addUnit(P1, kMageseekerWarden, 5, /*at_bf=*/0);
    c->applyPassiveAura(state, P1);
    EXPECT_TRUE(state.player(P2).units_play_base_only);
    EXPECT_FALSE(state.player(P1).units_play_base_only);
}

// ── Rengar: opts into extended Ambush placement ──
TEST_F(PlayLocTest, RengarTrophy_AmbushToEnemyBattlefields) {
    Card* c = card_registry.get(kRengarTrophy);
    ASSERT_NE(c, nullptr);
    EXPECT_TRUE(c->ambushToEnemyBattlefields());
}

// ── Renata: tokens enter ready while she's on board ──
TEST_F(PlayLocTest, Renata_TokensEnterReady) {
    addUnit(P1, kRenataGlasc, 4, /*at_bf=*/0);
    Card* c = card_registry.get(kRenataGlasc);
    c->applyPassiveAura(state, P1);
    ASSERT_TRUE(state.player(P1).tokens_enter_ready);
    EffectExecutor exec(state, events, card_db);
    auto tok = exec.createToken(P1, CardType::Unit, "Soldier", 1, {}, {},
                                BaseLocation{P1}, /*enter_ready=*/false);
    EXPECT_FALSE(state.getObject(tok).is_exhausted)
        << "Renata forces friendly tokens to enter ready";
    // Opponent's tokens are unaffected.
    auto tok2 = exec.createToken(P2, CardType::Unit, "Soldier", 1, {}, {},
                                 BaseLocation{P2}, /*enter_ready=*/false);
    EXPECT_TRUE(state.getObject(tok2).is_exhausted);
}

}  // namespace
}  // namespace riftbound::test
