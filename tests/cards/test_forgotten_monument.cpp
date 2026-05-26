/// @file test_forgotten_monument.cpp
/// Tests for [523] Forgotten Monument + the turn-gated scoring machinery.
///
/// "Players can't score here until their third turn."
///   - The card exposes minTurnToScore()==3.
///   - GameEngine::isScoreGatedByTurn blocks a player from scoring the BF until
///     their PlayerState::turns_taken reaches that threshold (their 3rd turn),
///     and applies to both players.

#include "tests/cards/card_test_fixture.h"

#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/events.h"
#include "core/game_state.h"
#include "engine/game_engine.h"

#include <gtest/gtest.h>

namespace riftbound::test {
namespace {

constexpr CardDefId kForgottenMonument = 523;

TEST_F(CardTestFixture, ForgottenMonument_CardReportsMinTurnThree) {
    Card* c = card_registry.get(kForgottenMonument);
    ASSERT_NE(c, nullptr);
    auto* bfc = dynamic_cast<BattlefieldCard*>(c);
    ASSERT_NE(bfc, nullptr) << "Forgotten Monument is a battlefield card";
    EXPECT_EQ(bfc->minTurnToScore(), 3)
        << "'can't score here until their third turn' → 3";
}

TEST_F(CardTestFixture, ForgottenMonument_GateBlocksUntilThirdTurn) {
    GameEngine engine(card_db, events, card_registry);
    auto& s = engine.mutableState();
    s.players[0].id = P1;
    s.players[1].id = P2;
    s.battlefields.push_back(BattlefieldState{});
    s.battlefields.back().id = 0;
    s.battlefields.back().min_turn_to_score = 3;   // as Forgotten Monument sets

    // A second, unrestricted battlefield as a control.
    s.battlefields.push_back(BattlefieldState{});
    s.battlefields.back().id = 1;

    // Turns 1 and 2: gated on the Monument; never gated on the normal BF.
    for (int t = 1; t <= 2; ++t) {
        s.players[0].turns_taken = t;
        EXPECT_TRUE(engine.isScoreGatedByTurn(P1, 0))
            << "P1 cannot score the Monument on their turn " << t;
        EXPECT_FALSE(engine.isScoreGatedByTurn(P1, 1))
            << "the unrestricted BF is never gated";
    }

    // Their third turn (and beyond): no longer gated.
    s.players[0].turns_taken = 3;
    EXPECT_FALSE(engine.isScoreGatedByTurn(P1, 0))
        << "P1 may score the Monument from their third turn on";
    s.players[0].turns_taken = 5;
    EXPECT_FALSE(engine.isScoreGatedByTurn(P1, 0));
}

TEST_F(CardTestFixture, ForgottenMonument_GateAppliesToBothPlayers) {
    GameEngine engine(card_db, events, card_registry);
    auto& s = engine.mutableState();
    s.players[0].id = P1;
    s.players[1].id = P2;
    s.battlefields.push_back(BattlefieldState{});
    s.battlefields.back().id = 0;
    s.battlefields.back().min_turn_to_score = 3;

    s.players[0].turns_taken = 2;
    s.players[1].turns_taken = 2;
    EXPECT_TRUE(engine.isScoreGatedByTurn(P1, 0));
    EXPECT_TRUE(engine.isScoreGatedByTurn(P2, 0)) << "rule binds both players";

    // The counter is per-player: P2 reaching turn 3 frees only P2.
    s.players[1].turns_taken = 3;
    EXPECT_TRUE(engine.isScoreGatedByTurn(P1, 0));
    EXPECT_FALSE(engine.isScoreGatedByTurn(P2, 0));
}

}  // namespace
}  // namespace riftbound::test
