/// @file test_burn_out_and_scoring.cpp
/// CR-level unit tests for Burn Out (CR 431) and Winning Point scoring
/// rules (CR 466.1.b). These exercise game-state boundaries that are
/// expensive to hit organically in MCTS replays.
///
/// CRs covered:
///   - 431.1.c: reveal beyond deck does NOT cause Burn Out
///   - 431.2.a-c: Burn Out sequence (recycle trash, opponent gains 1)
///   - 431.3:    burn-out point that puts opp at victory_score wins game
///   - 466.1.a.1: non-Conquer/non-Hold score sources skip WP restrictions
///   - 466.1.b.1: Hold at penultimate always grants Winning Point
///   - 466.1.b.2: Conquer at penultimate requires all BFs scored this turn,
///                else draws a card instead

#include "card_test_fixture.h"
#include "engine/game_engine.h"

#include <gtest/gtest.h>

namespace riftbound::test {
namespace {

class BurnOutScoringTest : public CardTestFixture {
protected:
    // GameEngine owns its own GameState — the fixture's `state` member is
    // separate. For winning-point tests we drive the engine's internal
    // state directly via mutableState(), bypassing the fixture's `state`.
    std::unique_ptr<GameEngine> engine_;
    GameState* eng_state() { return &engine_->mutableState(); }

    // Phase-step hooks sweep both legend zones, so engine-driven tests need a
    // legend object per player (as the action-generation tests do).
    void seedLegends() {
        auto& s = *eng_state();
        for (int p = 0; p < 2; ++p) {
            auto leg = s.createObject();
            auto& lo = s.getObject(leg);
            lo.owner = lo.controller = s.players[p].id;
            lo.card_type = CardType::Legend;
            lo.zone = ZoneType::LegendZone;
            s.players[p].legend_zone = leg;
        }
    }

    void makeEngineAndSeedBattlefields(int n_bfs = 2) {
        engine_ = std::make_unique<GameEngine>(card_db, events, card_registry);
        auto& s = *eng_state();
        s.mode = ModeOfPlay{};
        s.players[0].id = P1;
        s.players[1].id = P2;
        for (int i = 0; i < n_bfs; ++i) {
            BattlefieldState bf;
            bf.id = BattlefieldId::fromRaw(s.battlefields.size());
            s.battlefields.push_back(bf);
        }
    }
};

// ─── CR 431.2 — Burn Out: opponent gains 1, not self -1 ─────────────────────

TEST_F(BurnOutScoringTest, BurnOut_OpponentGainsPoint_SelfScoreUnchanged) {
    // P1 deck empty, trash has 3 cards; P1 must draw 1.
    auto t1 = addToDeck(P1, 1);
    auto t2 = addToDeck(P1, 1);
    auto t3 = addToDeck(P1, 1);
    state.player(P1).main_deck.clear();
    for (auto id : {t1, t2, t3}) {
        state.getObject(id).zone = ZoneType::Trash;
        state.player(P1).trash.push_back(id);
    }
    state.player(P1).score = 3;
    state.player(P2).score = 2;

    EffectExecutor exec(state, events, card_db);
    exec.drawCards(P1, 1);

    // Self score unchanged.
    EXPECT_EQ(state.player(P1).score, 3);
    // Opponent gains 1.
    EXPECT_EQ(state.player(P2).score, 3);
    // Trash recycled, then drew 1 from the shuffled deck.
    EXPECT_TRUE(state.player(P1).trash.empty());
    EXPECT_EQ(state.player(P1).hand.size(), 1u);
    EXPECT_EQ(state.player(P1).main_deck.size(), 2u);
    EXPECT_TRUE(state.player(P1).burned_out);
}

TEST_F(BurnOutScoringTest, BurnOut_AtScoreZero_OpponentStillGainsPoint) {
    // CR 431.2.c has no floor: opp +1 even when burning-out player is at 0.
    auto t1 = addToDeck(P1, 1);
    state.player(P1).main_deck.clear();
    state.getObject(t1).zone = ZoneType::Trash;
    state.player(P1).trash.push_back(t1);
    state.player(P1).score = 0;
    state.player(P2).score = 0;

    EffectExecutor exec(state, events, card_db);
    exec.drawCards(P1, 1);

    EXPECT_EQ(state.player(P1).score, 0);
    EXPECT_EQ(state.player(P2).score, 1);
}

TEST_F(BurnOutScoringTest, BurnOut_BringsOpponentToVictoryScore_OpponentWinsImmediately) {
    // CR 431.3: burn-out point that puts opp at >= victory_score with more
    // points than us wins the game immediately, no cleanup wait.
    auto t1 = addToDeck(P1, 1);
    state.player(P1).main_deck.clear();
    state.getObject(t1).zone = ZoneType::Trash;
    state.player(P1).trash.push_back(t1);
    state.player(P1).score = 3;
    state.player(P2).score = state.mode.victory_score - 1;  // one short

    EffectExecutor exec(state, events, card_db);
    exec.drawCards(P1, 1);

    EXPECT_TRUE(state.game_over);
    EXPECT_EQ(state.winner, P2);
    EXPECT_EQ(state.player(P2).score, state.mode.victory_score);
}

TEST_F(BurnOutScoringTest, BurnOut_DeckAndTrashEmpty_BurnsOutUntilOpponentWins) {
    // CR 432.3 / 432.3.a: with an empty trash the deck stays empty, so trying
    // to finish the draw burns out again — a point to the opponent each time —
    // until the opponent passes the Victory Score and wins (432.3.c.1:
    // immediately, without waiting for a cleanup).
    state.player(P1).score = 4;
    state.player(P2).score = 2;
    // Both deck + trash already empty.

    EffectExecutor exec(state, events, card_db);
    exec.drawCards(P1, 1);

    EXPECT_EQ(state.player(P1).score, 4);
    EXPECT_EQ(state.player(P2).score, state.mode.victory_score);
    EXPECT_TRUE(state.game_over);
    EXPECT_EQ(state.winner, P2);
    EXPECT_TRUE(state.player(P1).hand.empty());
}

TEST_F(BurnOutScoringTest, BurnOut_EmptyDeckAndTrash_NeedsMorePointsThanOpponent) {
    // CR 432.3.c: the burn-out point wins only with MORE points than every
    // opponent, so it keeps going past the Victory Score when behind.
    state.player(P1).score = state.mode.victory_score + 1;   // (already ahead; contrived)
    state.player(P2).score = 0;

    EffectExecutor exec(state, events, card_db);
    exec.drawCards(P1, 1);

    EXPECT_TRUE(state.game_over);
    EXPECT_EQ(state.winner, P2);
    EXPECT_EQ(state.player(P2).score, state.mode.victory_score + 2);
}

TEST_F(BurnOutScoringTest, Burn_EmptyDeckAndTrash_AlsoBurnsOutToALoss) {
    // CR 441.4 + 432.1.b: Burn past an empty deck burns out the same way.
    state.player(P1).score = 0;
    state.player(P2).score = 0;

    EffectExecutor exec(state, events, card_db);
    auto burned = exec.burn(P1, 2);

    EXPECT_TRUE(burned.empty());
    EXPECT_TRUE(state.game_over);
    EXPECT_EQ(state.winner, P2);
}

// ─── CR 319 — a Cleanup runs before the Scoring Step ───────────────────────

TEST_F(BurnOutScoringTest, Hold_NotScoredWhenLastUnitDiedInBeginningStep) {
    // P1 controls BF 0 with a single [Temporary] unit. It is killed at the
    // start of P1's Beginning Phase "before scoring", so P1 no longer has a
    // unit there and must not score the Hold.
    makeEngineAndSeedBattlefields(2);
    FirstChoiceAgent a1, a2;
    engine_->testHook_setAgents(&a1, &a2);
    seedLegends();
    auto& s = *eng_state();
    s.turn.turn_player = P1;
    auto unit = s.createObject();
    {
        auto& u = s.getObject(unit);
        u.owner = u.controller = P1;
        u.card_type = CardType::Unit;
        u.name = "TemporaryHolder";
        u.base_might = u.current_might = 2;
        u.keywords.set(Keyword::Temporary);
        u.zone = ZoneType::BattlefieldZone;
        u.location = BattlefieldLocation{0};
    }
    s.battlefields[0].controller = P1;
    s.player(P1).score = 0;

    engine_->testHook_beginningStep();
    engine_->testHook_scoringStep();

    EXPECT_EQ(s.player(P1).score, 0) << "no unit left at the battlefield -> no Hold";
    EXPECT_FALSE(s.battlefields[0].controller.has_value() &&
                 *s.battlefields[0].controller == P1);
}

TEST_F(BurnOutScoringTest, Hold_ScoredWhenUnitSurvivesBeginningStep) {
    makeEngineAndSeedBattlefields(2);
    FirstChoiceAgent a1, a2;
    engine_->testHook_setAgents(&a1, &a2);
    seedLegends();
    auto& s = *eng_state();
    s.turn.turn_player = P1;
    auto unit = s.createObject();
    {
        auto& u = s.getObject(unit);
        u.owner = u.controller = P1;
        u.card_type = CardType::Unit;
        u.name = "Holder";
        u.base_might = u.current_might = 2;
        u.zone = ZoneType::BattlefieldZone;
        u.location = BattlefieldLocation{0};
    }
    s.battlefields[0].controller = P1;
    s.player(P1).score = 0;

    engine_->testHook_beginningStep();
    engine_->testHook_scoringStep();

    EXPECT_EQ(s.player(P1).score, 1);
}

// ─── CR 431.1.c — Reveal beyond deck does NOT cause Burn Out ────────────────

TEST_F(BurnOutScoringTest, RevealUntil_EmptyDeck_DoesNotBurnOut) {
    // CR 431.1.c.1: reveal-beyond-deck ignores subsequent instructions and
    // does NOT cause a Burn Out, even if those instructions would move
    // cards between zones.
    state.player(P1).score = 3;
    state.player(P2).score = 2;
    // P1 has a non-empty trash so a Burn Out, if it fired, would be
    // visible (opp would gain a point).
    auto t = addToDeck(P1, 1);
    state.player(P1).main_deck.clear();
    state.getObject(t).zone = ZoneType::Trash;
    state.player(P1).trash.push_back(t);

    EffectExecutor exec(state, events, card_db);
    auto [unit_id, rest] = exec.revealUntil(P1, CardType::Unit);

    EXPECT_EQ(unit_id, kInvalidId);
    EXPECT_TRUE(rest.empty());
    // Critically: no burn out, no opp point.
    EXPECT_EQ(state.player(P1).score, 3);
    EXPECT_EQ(state.player(P2).score, 2);
    EXPECT_FALSE(state.player(P1).burned_out);
    // Deck + trash unchanged (no recycle).
    EXPECT_TRUE(state.player(P1).main_deck.empty());
    EXPECT_EQ(state.player(P1).trash.size(), 1u);
}

TEST_F(BurnOutScoringTest, RevealUntil_AllNonUnits_NoMatch_NoBurnOut) {
    // Deck has only non-units → revealUntil walks the whole deck,
    // returns kInvalidId + all-revealed. No burn out either way.
    state.player(P1).score = 0;
    state.player(P2).score = 0;
    // 3 gear cards (CardDefId 698 = Scryer's Bloom — a gear).
    addToDeck(P1, 698);
    addToDeck(P1, 698);
    addToDeck(P1, 698);

    EffectExecutor exec(state, events, card_db);
    auto [unit_id, rest] = exec.revealUntil(P1, CardType::Unit);

    EXPECT_EQ(unit_id, kInvalidId);
    EXPECT_EQ(rest.size(), 3u);
    EXPECT_EQ(state.player(P2).score, 0);  // no burn out
}

// ─── Dazzling Aurora-on-empty-deck (composes 431.1.c with revealUntil) ──────

TEST_F(BurnOutScoringTest, DazzlingAurora_EmptyDeck_SilentNoOp) {
    // CR 431.1.c.1: Aurora reveals until a unit is found. Empty deck
    // means zero reveals — the banish + play-ignoring-cost subsequent
    // instructions are ignored, and crucially no burn out is triggered
    // (Aurora doesn't draw, it reveals).
    state.player(P1).score = 5;
    state.player(P2).score = 4;
    // Trash + hand non-empty so we'd notice a stray side-effect.
    auto t = addToDeck(P1, 1);
    state.player(P1).main_deck.clear();
    state.getObject(t).zone = ZoneType::Trash;
    state.player(P1).trash.push_back(t);

    auto src = addToHand(P1, 160);  // Dazzling Aurora
    EffectExecutor exec(state, events, card_db);
    invokeOnResolve(src, 160, P1, {}, exec);

    EXPECT_EQ(state.player(P1).score, 5);
    EXPECT_EQ(state.player(P2).score, 4);    // no burn-out point
    EXPECT_FALSE(state.player(P1).burned_out);
    EXPECT_EQ(state.player(P1).main_deck.size(), 0u);
    EXPECT_EQ(state.player(P1).trash.size(), 1u);  // untouched
}

// ─── CR 466.1.b.1 — Hold at penultimate always grants Winning Point ─────────

TEST_F(BurnOutScoringTest, ScoreHold_AtPenultimate_AlwaysGrantsWinningPoint) {
    makeEngineAndSeedBattlefields();
    auto& s = *eng_state();
    s.player(P1).score = s.mode.victory_score - 1;
    BattlefieldId bf0 = s.battlefields[0].id;

    engine_->testHook_scoreHold(P1, bf0);

    EXPECT_EQ(s.player(P1).score, s.mode.victory_score);
    EXPECT_TRUE(s.game_over);
    EXPECT_EQ(s.winner, P1);
}

// ─── CR 466.1.b.2 — Conquer at penultimate path A: all BFs scored → WP ──────

TEST_F(BurnOutScoringTest, ScoreConquer_AtPenultimate_AllBFsScored_GrantsWinningPoint) {
    makeEngineAndSeedBattlefields();
    auto& s = *eng_state();
    s.player(P1).score = s.mode.victory_score - 1;
    BattlefieldId bf0 = s.battlefields[0].id;
    BattlefieldId bf1 = s.battlefields[1].id;
    s.player(P1).battlefields_scored_this_turn.insert(bf1);

    engine_->testHook_scoreConquer(P1, bf0);

    EXPECT_EQ(s.player(P1).score, s.mode.victory_score);
    EXPECT_TRUE(s.game_over);
    EXPECT_EQ(s.winner, P1);
}

// ─── CR 466.1.b.2 — Conquer at penultimate path B: not all BFs → draw 1 ─────

TEST_F(BurnOutScoringTest,
       ScoreConquer_AtPenultimate_NotAllBFsScored_DrawsCardInsteadOfWinningPoint) {
    makeEngineAndSeedBattlefields();
    auto& s = *eng_state();
    s.player(P1).score = s.mode.victory_score - 1;
    BattlefieldId bf0 = s.battlefields[0].id;
    // P1 has only scored BF0 this turn (the one we're about to conquer);
    // BF1 has never been scored — so winning point via conquer is blocked.
    // Seed a card on the engine's deck so we can observe the draw.
    auto top = s.createObject();
    s.getObject(top).owner = P1;
    s.getObject(top).controller = P1;
    s.getObject(top).zone = ZoneType::MainDeck;
    s.player(P1).main_deck.push_back(top);

    int hand_before = s.player(P1).hand.size();
    engine_->testHook_scoreConquer(P1, bf0);

    EXPECT_EQ(s.player(P1).score, s.mode.victory_score - 1);
    EXPECT_FALSE(s.game_over);
    EXPECT_EQ(s.player(P1).hand.size(), hand_before + 1);
    EXPECT_NE(std::find(s.player(P1).hand.begin(), s.player(P1).hand.end(), top),
              s.player(P1).hand.end());
}

TEST_F(BurnOutScoringTest,
       ScoreConquer_AtPenultimate_NotAllBFsScored_DrawWithEmptyDeck_TriggersBurnOut) {
    makeEngineAndSeedBattlefields();
    auto& s = *eng_state();
    s.player(P1).score = s.mode.victory_score - 1;
    s.player(P2).score = 2;
    BattlefieldId bf0 = s.battlefields[0].id;
    // P1 deck empty, trash has 1 card → forced draw triggers burn out.
    auto t = s.createObject();
    s.getObject(t).owner = P1;
    s.getObject(t).controller = P1;
    s.getObject(t).zone = ZoneType::Trash;
    s.player(P1).trash.push_back(t);

    engine_->testHook_scoreConquer(P1, bf0);

    EXPECT_EQ(s.player(P1).score, s.mode.victory_score - 1);
    EXPECT_EQ(s.player(P2).score, 3);
    EXPECT_TRUE(s.player(P1).burned_out);
    EXPECT_FALSE(s.game_over);
}

// ─── CR 466.1.b — score below penultimate: no winning-point gate at all ─────

TEST_F(BurnOutScoringTest, ScoreConquer_BelowPenultimate_AlwaysScoresRegardless) {
    makeEngineAndSeedBattlefields();
    auto& s = *eng_state();
    s.player(P1).score = 3;
    BattlefieldId bf0 = s.battlefields[0].id;

    engine_->testHook_scoreConquer(P1, bf0);

    EXPECT_EQ(s.player(P1).score, 4);
    EXPECT_FALSE(s.game_over);
}

// ─── CR 465 — Same BF can't be scored twice in one turn ─────────────────────

TEST_F(BurnOutScoringTest, ScoreConquer_SameBFTwice_SecondCallIsNoOp) {
    makeEngineAndSeedBattlefields();
    auto& s = *eng_state();
    s.player(P1).score = 2;
    BattlefieldId bf0 = s.battlefields[0].id;

    engine_->testHook_scoreConquer(P1, bf0);
    EXPECT_EQ(s.player(P1).score, 3);
    engine_->testHook_scoreConquer(P1, bf0);
    EXPECT_EQ(s.player(P1).score, 3);
}

// ─── Regression: stuck contested BF (2026-05-17 scoring 0-0 bug) ────────────
//
// Reproduces the bug that produced 0-0 draws in MCTS:sims=20 jhin vs
// miss_fortune replays. Sequence:
//   1. Player A moves a unit to an empty BF → is_contested=true,
//      showdown_staged=true.
//   2. Showdown runs. Mid-showdown a reaction bounces/kills A's unit so
//      BOTH sides end at 0 units → sole_player=None → control NOT
//      established. is_contested stays true.
//   3. Subsequent turn: Player B moves a unit to that BF. Old code:
//      `if (!bf.is_contested)` short-circuits → NO new staging → BF
//      stuck forever → no scoring ever happens.
//
// Fix: re-evaluate staging on every arrival at a contested BF whose
// staging slots are all free. This test asserts the fix by setting up
// the "stuck" state directly and verifying a fresh move re-stages.

TEST_F(BurnOutScoringTest, ContestedBF_StuckAfter0vs0Showdown_NextMoveReStages) {
    makeEngineAndSeedBattlefields();
    auto& s = *eng_state();

    // Manually put BF#0 into the post-stuck state.
    auto& bf = s.battlefields[0];
    bf.is_contested = true;
    bf.contested_by = P1;  // some player contested it earlier
    bf.showdown_staged = false;
    bf.combat_staged = false;
    bf.showdown_in_progress = false;
    bf.combat_in_progress = false;
    bf.controller.reset();  // no one controls — the stuck case

    // P2 moves a unit to BF#0 (P1 has no units there → showdown should
    // be the staging result, not combat).
    auto u = s.createObject();
    auto& uo = s.getObject(u);
    uo.card_type = CardType::Unit;
    uo.owner = P2; uo.controller = P2;
    uo.zone = ZoneType::Base;
    uo.location = BaseLocation{P2};
    uo.base_might = 3; uo.current_might = 3;
    uo.name = "TestUnit";

    Intent move;
    move.type = IntentType::StandardMove;
    move.player = P2;
    move.units_to_move = {u};
    move.move_destination = BattlefieldLocation{bf.id};
    engine_->testHook_executeStandardMove(move);

    EXPECT_TRUE(bf.showdown_staged)
        << "Move to a stuck-contested empty BF must re-stage the showdown";
    EXPECT_FALSE(bf.combat_staged)
        << "Opponent has no units at BF — should be non-combat showdown";
    EXPECT_TRUE(bf.is_contested) << "Still contested";
}

TEST_F(BurnOutScoringTest, ContestedBF_StuckAfter0vs0Showdown_MoveByOpponentStagesCombat) {
    makeEngineAndSeedBattlefields();
    auto& s = *eng_state();

    auto& bf = s.battlefields[0];
    bf.is_contested = true;
    bf.contested_by = P1;
    bf.controller.reset();

    // P1 already has a unit at BF#0 (left over from previous turn).
    auto u1 = s.createObject();
    auto& u1o = s.getObject(u1);
    u1o.card_type = CardType::Unit;
    u1o.owner = P1; u1o.controller = P1;
    u1o.zone = ZoneType::BattlefieldZone;
    u1o.location = BattlefieldLocation{bf.id};
    u1o.base_might = 3; u1o.current_might = 3;
    u1o.name = "P1 Unit";

    // P2 moves their unit in — now both players have units → combat.
    auto u2 = s.createObject();
    auto& u2o = s.getObject(u2);
    u2o.card_type = CardType::Unit;
    u2o.owner = P2; u2o.controller = P2;
    u2o.zone = ZoneType::Base;
    u2o.location = BaseLocation{P2};
    u2o.base_might = 3; u2o.current_might = 3;
    u2o.name = "P2 Unit";

    Intent move;
    move.type = IntentType::StandardMove;
    move.player = P2;
    move.units_to_move = {u2};
    move.move_destination = BattlefieldLocation{bf.id};
    engine_->testHook_executeStandardMove(move);

    EXPECT_TRUE(bf.combat_staged)
        << "Move into a stuck-contested BF that has enemy units must "
           "stage COMBAT, not just showdown";
    EXPECT_FALSE(bf.showdown_staged);
}

// ─── CR 487.7 / 488.7 — First Turn Process of a 1v1 game: both players draw ──

// Helper: push a fresh deck card directly into the engine's GameState
// (fixture `state` is a separate object — addToDeck() doesn't touch the
// engine's state).
namespace {
GameObjectId seedDeckCard(GameState& s, PlayerId owner) {
    auto id = s.createObject();
    auto& obj = s.getObject(id);
    obj.owner = owner;
    obj.controller = owner;
    obj.card_def_id = 1;
    obj.zone = ZoneType::MainDeck;
    s.player(owner).main_deck.push_back(id);
    return id;
}
}  // namespace

TEST_F(BurnOutScoringTest, FirstPlayerFirstTurn_DrawsInOneVersusOne) {
    // CR 487.7 / 488.7 (1v1 Duel / Match), in full: "The player going second
    // channels an extra Rune from their Rune Deck during their first Channel
    // Phase of the game." The player going first draws as usual (CR
    // 315.4.b). "The player going first does not draw a card during their
    // first Draw Phase" is the text of 489.7 / 490.7 / 491.7 (multiplayer).
    // (Testing phase G1. Old expectation: no draw, citing "CR 482.7" — the
    // multiplayer rule under its old number, applied to 1v1.)
    makeEngineAndSeedBattlefields();
    auto& s = *eng_state();
    s.turn.turn_player = P1;
    s.turn.starting_player = P1;
    s.turn.turn_number = 0;
    s.player(P1).is_first_turn = true;
    s.player(P2).is_first_turn = true;

    seedDeckCard(s, P1);
    seedDeckCard(s, P1);
    int hand_before = s.player(P1).hand.size();
    int deck_before = s.player(P1).main_deck.size();

    engine_->testHook_drawPhase();

    EXPECT_EQ(s.player(P1).hand.size(), static_cast<size_t>(hand_before + 1))
        << "P1 (going first) draws on their first Draw Phase in 1v1";
    EXPECT_EQ(s.player(P1).main_deck.size(), static_cast<size_t>(deck_before - 1));
}

TEST_F(BurnOutScoringTest, SecondPlayerFirstTurn_DrawPhaseDrawsNormally) {
    // P2's first turn (P1 went first) — P2 draws as normal.
    makeEngineAndSeedBattlefields();
    auto& s = *eng_state();
    s.turn.turn_player = P2;
    s.turn.starting_player = P1;
    s.turn.turn_number = 1;
    s.player(P1).is_first_turn = false;  // P1 already had its turn 0
    s.player(P2).is_first_turn = true;

    seedDeckCard(s, P2);
    seedDeckCard(s, P2);

    engine_->testHook_drawPhase();

    EXPECT_EQ(s.player(P2).hand.size(), 1u)
        << "P2 (going second) must draw on their first Draw Phase";
    EXPECT_EQ(s.player(P2).main_deck.size(), 1u);
}

TEST_F(BurnOutScoringTest, FirstPlayerSecondTurn_DrawPhaseDrawsNormally) {
    // P1's second turn — skip flag no longer applies.
    makeEngineAndSeedBattlefields();
    auto& s = *eng_state();
    s.turn.turn_player = P1;
    s.turn.starting_player = P1;
    s.turn.turn_number = 2;
    s.player(P1).is_first_turn = false;  // already had first turn
    s.player(P2).is_first_turn = false;

    seedDeckCard(s, P1);

    engine_->testHook_drawPhase();

    EXPECT_EQ(s.player(P1).hand.size(), 1u)
        << "P1's second turn — normal draw resumes";
    EXPECT_EQ(s.player(P1).main_deck.size(), 0u);
}

}  // namespace
}  // namespace riftbound::test
