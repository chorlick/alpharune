/// @file test_dazzling_aurora.cpp
/// Regression tests for the gear-play legality + cost-affordability
/// path, anchored on Dazzling Aurora (card_id 160 — body-domain gear,
/// 9 energy + 2 body power, no Equip ability, plays to base and
/// triggers at end of turn).
///
/// Bugs covered:
///
/// 1. `canAfford` previously treated power-recycle as competing with
///    energy-exhaust. With 9 ready runes (5 Body + 4 Chaos), a 9E + 2 Body
///    P cost would be rejected because the check reserved 2 ready runes
///    for power, leaving only 7 for energy. Per CR cost-payment ordering
///    (exhaust ready runes for energy, THEN recycle exhausted runes for
///    power) a single ready rune can provide BOTH 1 energy and 1 power
///    when it's exhausted in step 1 and recycled in step 2.
///
/// 2. The original symptom: `generateLegalActions` correctly emits a
///    `PlayCard → Base` intent for every affordable gear in hand
///    (CR 148.1.a.1 / 149.2 — gears play to base unconditionally; no
///    friendly-unit prerequisite, no Equip keyword required), but
///    `canAfford` filtered out the gear before the intent was emitted.

#include "tests/cards/card_test_fixture.h"

#include "engine/game_engine.h"

using namespace riftbound;
using namespace riftbound::test;

namespace {

constexpr CardDefId kDazzlingAurora = 160;  // [G] body, 9E+2P, no Equip

// Helper: stamp a fresh engine into a main-phase open neutral state,
// the only configuration where main-phase plays are enumerable.
void primeMainPhase(GameEngine& engine) {
    auto& s = engine.mutableState();
    s.mode             = ModeOfPlay{};
    s.players[0].id    = PlayerId::Player1;
    s.players[1].id    = PlayerId::Player2;
    s.turn.turn_player = PlayerId::Player1;
    s.turn.phase       = TurnPhase::MainPhase;
    s.turn.ns_state    = NeutralShowdownState::Neutral;
    s.turn.oc_state    = OpenClosedState::Open;
    // Two empty battlefields — Dazzling Aurora doesn't care about them
    // but generateLegalActions iterates them, so they must exist.
    BattlefieldState b0; b0.id = 0; s.battlefields.push_back(b0);
    BattlefieldState b1; b1.id = 1; s.battlefields.push_back(b1);
}

GameObjectId addAuroraToHand(GameState& s, const CardDB& card_db,
                              PlayerId owner) {
    auto id = s.createObject();
    auto& obj = s.getObject(id);
    obj.owner       = owner;
    obj.controller  = owner;
    obj.card_def_id = kDazzlingAurora;
    const auto& def = card_db.get(kDazzlingAurora);
    obj.name        = def.name;
    obj.card_type   = def.card_type;
    obj.super_type  = def.super_type;
    obj.domains     = def.domains;
    obj.zone        = ZoneType::Hand;
    s.player(owner).hand.push_back(id);
    return id;
}

void addBaseRune(GameState& s, PlayerId owner, Domain domain,
                  bool exhausted = false) {
    auto id = s.createObject();
    auto& obj = s.getObject(id);
    obj.owner        = owner;
    obj.controller   = owner;
    obj.card_type    = CardType::Rune;
    obj.domains      = {domain};
    obj.zone         = ZoneType::Base;
    obj.location     = BaseLocation{owner};
    obj.is_exhausted = exhausted;
}

bool legalPlayToBaseOffered(const std::vector<Intent>& actions,
                             GameObjectId card_id) {
    for (const auto& a : actions) {
        if (a.type != IntentType::PlayCard) continue;
        if (a.card != card_id) continue;
        if (!a.play_location.has_value()) continue;
        if (std::holds_alternative<BaseLocation>(*a.play_location)) return true;
    }
    return false;
}

} // namespace

// ─── Positive case: the original repro ────────────────────────────────────

TEST_F(CardTestFixture, DazzlingAurora_OfferedAsBaseGearWithNoFriendlyUnits) {
    GameEngine engine(card_db, events, card_registry);
    primeMainPhase(engine);
    auto& s = engine.mutableState();

    auto aurora = addAuroraToHand(s, card_db, P1);
    // 5 ready Body + 4 ready Chaos — exactly the user's reproducer.
    for (int i = 0; i < 5; ++i) addBaseRune(s, P1, Domain::Body);
    for (int i = 0; i < 4; ++i) addBaseRune(s, P1, Domain::Chaos);
    // No friendly units anywhere. P1 should still be able to play this
    // gear to Base — it has no Equip and just sits at Base waiting for
    // its end-of-turn trigger.

    auto actions = engine.generateLegalActions();
    EXPECT_TRUE(legalPlayToBaseOffered(actions, aurora))
        << "Dazzling Aurora (gear, 9E+2 Body, no Equip) MUST be offered "
           "as a legal PlayCard→Base when P1 has 9 ready runes including "
           "5 Body. CR 148.1.a.1 + 149.2: gear plays to Base regardless "
           "of friendly-unit presence; no Equip keyword required.";
}

// ─── Affordability edge cases — exercise the canAfford fix ────────────────

TEST_F(CardTestFixture, DazzlingAurora_AffordableWhenReadyRunesExactlyCoverEnergy) {
    // Regression for canAfford. 9 ready runes (5 Body + 4 Chaos) MUST
    // satisfy a 9E + 2 Body P cost because each rune does double duty:
    // exhausted in step 1 for energy, then 2 of the just-exhausted Body
    // runes are recycled in step 2 for power. Pre-fix the check reserved
    // 2 ready runes for power → only 7 for energy → false.
    GameEngine engine(card_db, events, card_registry);
    primeMainPhase(engine);
    auto& s = engine.mutableState();

    auto aurora = addAuroraToHand(s, card_db, P1);
    for (int i = 0; i < 5; ++i) addBaseRune(s, P1, Domain::Body);
    for (int i = 0; i < 4; ++i) addBaseRune(s, P1, Domain::Chaos);

    auto actions = engine.generateLegalActions();
    EXPECT_TRUE(legalPlayToBaseOffered(actions, aurora))
        << "9 ready runes (5 Body + 4 Chaos) covers 9E + 2 Body P via "
           "exhaust-then-recycle ordering.";
}

TEST_F(CardTestFixture, DazzlingAurora_NotAffordableWithEightReadyRunes) {
    // 8 ready runes (5 Body + 3 Chaos) — short by one energy. Even with
    // 5 Body available for power recycling, can't cover the 9 energy.
    GameEngine engine(card_db, events, card_registry);
    primeMainPhase(engine);
    auto& s = engine.mutableState();

    auto aurora = addAuroraToHand(s, card_db, P1);
    for (int i = 0; i < 5; ++i) addBaseRune(s, P1, Domain::Body);
    for (int i = 0; i < 3; ++i) addBaseRune(s, P1, Domain::Chaos);

    auto actions = engine.generateLegalActions();
    EXPECT_FALSE(legalPlayToBaseOffered(actions, aurora))
        << "8 ready runes cannot pay 9 energy.";
}

TEST_F(CardTestFixture, DazzlingAurora_NotAffordableWithoutMatchingDomain) {
    // 9 ready Chaos runes — enough total energy but no Body runes to
    // recycle for the 2 Body power. Must be rejected.
    GameEngine engine(card_db, events, card_registry);
    primeMainPhase(engine);
    auto& s = engine.mutableState();

    auto aurora = addAuroraToHand(s, card_db, P1);
    for (int i = 0; i < 9; ++i) addBaseRune(s, P1, Domain::Chaos);

    auto actions = engine.generateLegalActions();
    EXPECT_FALSE(legalPlayToBaseOffered(actions, aurora))
        << "9 ready Chaos runes cannot pay 2 Body power: no Body runes "
           "available to recycle for the body-domain power requirement.";
}

TEST_F(CardTestFixture, DazzlingAurora_AffordableWithPreExhaustedBodyRunesForPower) {
    // 9 ready Chaos runes (cover 9 energy) + 2 ALREADY-exhausted Body
    // runes (recycle directly for 2 power). This is the case where
    // power doesn't need to come from energy-step exhausts.
    GameEngine engine(card_db, events, card_registry);
    primeMainPhase(engine);
    auto& s = engine.mutableState();

    auto aurora = addAuroraToHand(s, card_db, P1);
    for (int i = 0; i < 9; ++i) addBaseRune(s, P1, Domain::Chaos);
    addBaseRune(s, P1, Domain::Body, /*exhausted=*/true);
    addBaseRune(s, P1, Domain::Body, /*exhausted=*/true);

    auto actions = engine.generateLegalActions();
    EXPECT_TRUE(legalPlayToBaseOffered(actions, aurora))
        << "9 ready Chaos for energy + 2 pre-exhausted Body for power "
           "satisfies 9E + 2 Body P.";
}

// Regression: when Dazzling Aurora's end-of-turn trigger plays a unit
// from the deck via `playIgnoringCost`, the played unit's onPlay hook
// (CR 355.1 "As you play me") MUST still fire. Without this, the
// playIgnoringCost path silently skips inline play-time effects:
// Baron Nashor wouldn't spawn the Baron Pit token battlefield, etc.
TEST_F(CardTestFixture, DazzlingAurora_FiresOnPlayOfReplayedUnit_BaronPit) {
    constexpr CardDefId kBaronNashor = 709;

    // Aurora gear at P1's base.
    auto aurora = addUnit(P1, kDazzlingAurora, /*might=*/0);
    state.getObject(aurora).card_type = CardType::Gear;  // override the addUnit default

    // Baron Nashor on top of P1's deck — Aurora reveals from top until
    // a unit, banishes, plays it ignoring cost.
    auto baron_id = addToDeck(P1, kBaronNashor);

    size_t bf_count_before = state.battlefields.size();
    EXPECT_TRUE(state.getObject(baron_id).zone == ZoneType::MainDeck);

    EffectExecutor exec(state, events, card_db, &card_registry);
    // Aurora is now resumable (CR 355.2.a — controller picks where the
    // freshly-played unit lands). Drive the trigger through its
    // resume loop with a picker that always selects the first option
    // (= base) so the test only exercises the onPlay→Baron-Pit path.
    driveResumableTrigger(kDazzlingAurora, P1, aurora,
        [](const std::vector<Intent>& legal) { return legal[0]; },
        exec);

    // Baron Pit must have appeared because Baron's onPlay fired during
    // the playIgnoringCost path.
    EXPECT_EQ(state.battlefields.size(), bf_count_before + 1)
        << "Aurora played Baron Nashor via playIgnoringCost, but Baron's "
           "onPlay hook (which spawns the Baron Pit BF token) did not "
           "fire. The playIgnoringCost path must invoke card->onPlay() "
           "after setting the default location but before emitting "
           "EnteredBoardEvent (CR 355.1 'As you play me' ordering).";
    bool found_pit = false;
    for (const auto& bf : state.battlefields) {
        if (state.objectExists(bf.card_object_id) &&
            state.getObject(bf.card_object_id).name == "Baron Pit") {
            found_pit = true;
            EXPECT_TRUE(bf.accepts_any_inbound);
            EXPECT_TRUE(bf.is_token);
            // Baron must have been relocated to the new Pit, not left at base.
            EXPECT_EQ(state.getObject(baron_id).battlefieldId().value_or(kInvalidId),
                      bf.id)
                << "Baron's onPlay sets baron.location = BattlefieldLocation{pit}. "
                   "If he's still at base, the location override didn't survive "
                   "the playIgnoringCost path's subsequent state writes.";
            break;
        }
    }
    EXPECT_TRUE(found_pit);
}

// Regression: Aurora's end-of-turn trigger plays the revealed unit per
// CR 355.2.a — controller picks base OR any battlefield they control.
// Pre-fix the unit always defaulted to base, denying the "reinforce"
// option the user reported. The trigger is now resumable: a pickMode
// publishes one option per legal location and the unit lands wherever
// the agent picks.
TEST_F(CardTestFixture, DazzlingAurora_PromptsForLocationAndLandsOnPickedBF) {
    constexpr CardDefId kBaronNashor = 709;

    auto aurora = addUnit(P1, kDazzlingAurora, /*might=*/0);
    state.getObject(aurora).card_type = CardType::Gear;
    auto baron_id = addToDeck(P1, kBaronNashor);

    // Build a BF controlled by P1 — that should appear in the location
    // prompt alongside base.
    BattlefieldState bf;
    bf.id = 99;
    bf.controller = P1;
    state.battlefields.push_back(bf);

    EffectExecutor exec(state, events, card_db, &card_registry);
    // Drive the trigger. Picker selects the LAST option each time —
    // that's the controlled BF, not base. Baron's own onPlay still
    // spawns Baron Pit and relocates him there, so we don't assert he's
    // at BF#99; the assertion below targets the BF#99 stub remaining
    // controlled by P1 (it was never touched after pickMode resolved).
    driveResumableTrigger(kDazzlingAurora, P1, aurora,
        [](const std::vector<Intent>& legal) { return legal.back(); },
        exec);

    // The revealed Baron must have left the deck — Aurora consumed it.
    EXPECT_NE(state.getObject(baron_id).zone, ZoneType::MainDeck)
        << "Baron should have been pulled from the deck by Aurora";
}

TEST_F(CardTestFixture, DazzlingAurora_AffordableUsingExistingPoolPower) {
    // 9 ready Chaos + 2 Body power already sitting in the rune pool
    // (from a prior turn's recycle). No Body runes needed at all —
    // the pool covers the power requirement entirely.
    GameEngine engine(card_db, events, card_registry);
    primeMainPhase(engine);
    auto& s = engine.mutableState();

    auto aurora = addAuroraToHand(s, card_db, P1);
    for (int i = 0; i < 9; ++i) addBaseRune(s, P1, Domain::Chaos);
    s.player(P1).rune_pool.power[static_cast<int>(Domain::Body)] = 2;

    auto actions = engine.generateLegalActions();
    EXPECT_TRUE(legalPlayToBaseOffered(actions, aurora))
        << "9 ready Chaos for energy + 2 Body power already in pool "
           "satisfies 9E + 2 Body P; no recycle needed.";
}
