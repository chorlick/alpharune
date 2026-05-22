/// @file test_tokens_cease_to_exist.cpp
/// Regression tests for CR 183.1 — "If a token is put into any Non-Board
/// Zone besides the chain, it ceases to exist immediately after moving
/// to its new zone."
///
/// Surfaced 2026-05-16 by an Ivern vs Miss Fortune replay where a Bird
/// token (super_type=Token) spawned by Frisky Hunter was bounced to
/// hand instead of being removed. Sabotage then "revealed" the
/// non-existent card from P1's hand to P2, contaminating the
/// observation memory bank.
///
/// Tests cover the two on-board exits that previously violated CR 183.1:
///   - bounceToHand on a token → token goes to Banishment, NOT hand
///   - killObject on a token → token goes to Banishment, NOT trash
/// Non-token units are unchanged (still go to hand / trash as before).

#include "tests/cards/card_test_fixture.h"

#include "core/events.h"
#include "core/game_state.h"
#include "engine/effect_executor.h"

#include <algorithm>

using namespace riftbound;
using namespace riftbound::test;

namespace {

// Helper: build a token unit on a battlefield. Returns the GameObjectId.
GameObjectId addTokenUnit(GameState& state, PlayerId owner,
                           const std::string& name, int might, int at_bf) {
    auto id = state.createObject();
    auto& obj = state.getObject(id);
    obj.owner = owner;
    obj.controller = owner;
    obj.name = name;
    obj.card_type = CardType::Unit;
    obj.super_type = SuperType::Token;
    obj.card_def_id = kInvalidId;
    obj.base_might = might;
    obj.current_might = might;
    obj.zone = ZoneType::BattlefieldZone;
    obj.location = BattlefieldLocation{static_cast<BattlefieldId>(at_bf)};
    return id;
}

}  // namespace

// ─── bounceToHand on a token ──────────────────────────────────────────────

TEST_F(CardTestFixture, BounceToHand_TokenCeasesToExist) {
    auto bird = addTokenUnit(state, P1, "Bird", /*might=*/1, /*at_bf=*/0);
    int hand_before = handSize(P1);
    int ban_before = state.player(P1).banishment.size();

    EffectExecutor exec(state, events, card_db);
    exec.bounceToHand(bird);

    // The token must NOT appear in the player's hand vector.
    EXPECT_FALSE(inHand(P1, bird))
        << "CR 183.1: a token bounced from board must cease to exist, "
           "not land in hand.";

    // It must NOT appear in the player's banishment vector either —
    // banishment is a public, queryable zone. "Ceases to exist" means
    // gone entirely. The object stays in state.objects for ID safety
    // (in-flight references like the LeftBoardEvent payload) but it's
    // not in any zone vector.
    auto& ban = state.player(P1).banishment;
    EXPECT_TRUE(std::find(ban.begin(), ban.end(), bird) == ban.end());
    EXPECT_EQ((int)state.player(P1).banishment.size(), ban_before);
    EXPECT_EQ(handSize(P1), hand_before);

    // The GameObject itself moves to the Banishment zone (semantically
    // "no board, no hand, no trash"). The cease-to-exist trace fires.
    ASSERT_TRUE(state.objectExists(bird));
    EXPECT_EQ(state.getObject(bird).zone, ZoneType::Banishment);
    EXPECT_FALSE(state.getObject(bird).location.has_value());
}

TEST_F(CardTestFixture, BounceToHand_NonTokenUnitStillGoesToHand) {
    // Regression-guard: the CR 183.1 fix must NOT affect real cards.
    // A non-token unit (super_type != Token) still bounces to hand.
    auto real_unit = addUnit(P1, /*def_id=*/kInvalidId, /*might=*/3,
                              /*at_bf=*/0);
    state.getObject(real_unit).super_type = SuperType::None;
    int hand_before = handSize(P1);

    EffectExecutor exec(state, events, card_db);
    exec.bounceToHand(real_unit);

    EXPECT_TRUE(inHand(P1, real_unit))
        << "Non-token units must still bounce to hand (CR 144 — bounce "
           "is a board-to-hand move for ordinary cards).";
    EXPECT_EQ(handSize(P1), hand_before + 1);
    EXPECT_EQ(state.getObject(real_unit).zone, ZoneType::Hand);
}

// ─── killObject on a token ────────────────────────────────────────────────

TEST_F(CardTestFixture, KillObject_TokenCeasesToExist_NotInTrash) {
    auto bird = addTokenUnit(state, P1, "Bird", /*might=*/1, /*at_bf=*/0);
    int trash_before = trashSize(P1);

    EffectExecutor exec(state, events, card_db);
    exec.killObject(bird);

    EXPECT_FALSE(inTrash(P1, bird))
        << "CR 183.1: a dying token must cease to exist, not land in "
           "trash. Putting it in trash would let Sabotage/Stacked Deck/"
           "recycle effects rebuild it later — that's wrong.";
    EXPECT_EQ(trashSize(P1), trash_before);
    EXPECT_EQ(state.getObject(bird).zone, ZoneType::Banishment);
}

// Note: GameEngine::killUnit (the combat lethal-damage kill path) has
// the same CR 183.1 banish-token logic as EffectExecutor::killObject,
// but it's a private method, so we can't unit-test it directly here.
// Coverage relies on:
//   - the identical-shape fix being applied in both code paths
//     (src/engine/game_engine.cpp killUnit + src/engine/effect_executor.cpp
//      killObject), AND
//   - the EffectExecutor::killObject tests below covering the shared
//     correctness contract.
// The combat path will be exercised end-to-end by MCTS replays — the
// trace line "TOKEN CEASES TO EXIST (CR 183.1)" should appear whenever
// a token dies in combat.

TEST_F(CardTestFixture, KillObject_NonTokenUnitStillGoesToTrash) {
    // Regression-guard: ordinary units still die to trash.
    auto u = addUnit(P1, /*def_id=*/kInvalidId, /*might=*/3, /*at_bf=*/0);
    state.getObject(u).super_type = SuperType::None;
    int trash_before = trashSize(P1);

    EffectExecutor exec(state, events, card_db);
    exec.killObject(u);

    EXPECT_TRUE(inTrash(P1, u));
    EXPECT_EQ(trashSize(P1), trash_before + 1);
    EXPECT_EQ(state.getObject(u).zone, ZoneType::Trash);
}

// ─── Anti-regression for the original bug surface ────────────────────────
//
// The bug was that a Bird token bounced to hand survived as a "hand
// card" and Sabotage could subsequently reveal it. After the fix,
// Sabotage's enumeration of P1's hand should NOT see the bounced
// token at all (because it's not in the hand vector).

TEST_F(CardTestFixture, BouncedTokenDoesNotAppearInOpponentHandEnumeration) {
    auto bird = addTokenUnit(state, P1, "Bird", /*might=*/1, /*at_bf=*/0);

    EffectExecutor exec(state, events, card_db);
    exec.bounceToHand(bird);

    // Mirror what Sabotage's case-0 does: iterate the opponent's hand.
    const auto& opp_hand = state.player(P1).hand;
    EXPECT_TRUE(std::find(opp_hand.begin(), opp_hand.end(), bird) == opp_hand.end())
        << "Sabotage's hand-reveal loop must not see the bounced token. "
           "Otherwise the caster's observation bank gets polluted with a "
           "card that doesn't actually exist.";
}

// ─── Deathknell on a Reflection-copy still fires ─────────────────────────
//
// LeBlanc deck mechanic: Mirror Image plays a Reflection token, then
// copyUnit makes it a copy of a target unit (e.g. LeBlanc, Fragmented).
// The copy gains the source's Deathknell. When the copy dies, CR 183.1
// says it ceases to exist — but the Deathknell MUST still trigger,
// because the trigger fires off `UnitDiedEvent` (queued before the
// cease-to-exist transition), and the chain item carries the token's
// card_def_id (which copyUnit set to the source's).
//
// These tests validate the interaction so a future refactor doesn't
// regress LeBlanc / Karthus / Ruined Rex token-copy plays.

TEST_F(CardTestFixture, CopyUnit_PreservesCardDefIdOnToken) {
    // Reflection token → copy of LeBlanc, Fragmented (734).
    auto refl = addTokenUnit(state, P1, "Reflection", /*might=*/0, /*at_bf=*/0);
    auto src  = addUnit(P1, /*def_id=*/734, /*might=*/2, /*at_bf=*/0);

    EffectExecutor exec(state, events, card_db);
    exec.copyUnit(refl, src);

    EXPECT_EQ(state.getObject(refl).card_def_id, 734)
        << "copyUnit must propagate card_def_id so TriggerManager can find "
           "the source's Deathknell via card_registry lookup.";
    EXPECT_EQ(state.getObject(refl).super_type, SuperType::Token)
        << "Token's super_type stays Token after copy — it's still a "
           "token, just one whose printed traits match the source.";
}

TEST_F(CardTestFixture, ReflectionCopyEmitsUnitDiedEventWithCardDefId) {
    // Create the token, copy LeBlanc Fragmented onto it, then kill it.
    // The UnitDiedEvent must fire with card_def_id=734 (the source) AND
    // owner/controller=P1, AND the token must be banished (not trashed).
    auto refl = addTokenUnit(state, P1, "Reflection", /*might=*/0, /*at_bf=*/0);
    auto src  = addUnit(P1, /*def_id=*/734, /*might=*/2, /*at_bf=*/0);

    EffectExecutor exec(state, events, card_db);
    exec.copyUnit(refl, src);

    // Subscribe to UnitDiedEvent so we capture what TriggerManager would
    // see when it dispatches Deathknell.
    bool died_fired = false;
    GameObjectId died_id = kInvalidId;
    PlayerId     died_controller = PlayerId::None;
    auto conn = events.on_unit_died.connect(
        [&](const UnitDiedEvent& e) {
            died_fired = true;
            died_id = e.object;
            died_controller = e.controller;
        });

    exec.killObject(refl);
    conn.disconnect();

    EXPECT_TRUE(died_fired)
        << "Tokens must still emit UnitDiedEvent — that's what fires "
           "Deathknell via TriggerManager.";
    EXPECT_EQ(died_id, refl);
    EXPECT_EQ(died_controller, P1);

    // The token itself ceases to exist (CR 183.1) — Banishment, no
    // trash entry. The Deathknell trigger queued from UnitDiedEvent
    // resolves later via the chain; it reads the chain item's stored
    // card_def_id (734) rather than the now-banished GameObject's
    // location, so the cease-to-exist transition is transparent to
    // the trigger.
    EXPECT_EQ(state.getObject(refl).zone, ZoneType::Banishment);
    EXPECT_FALSE(inTrash(P1, refl));
    EXPECT_EQ(state.getObject(refl).card_def_id, 734)
        << "card_def_id stays 734 even after the token ceases to exist "
           "— this is what lets a queued Deathknell still resolve.";
}
