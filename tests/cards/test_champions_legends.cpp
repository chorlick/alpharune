/// @file test_champions_legends.cpp
/// Per-card unit tests for the manual champion + legend implementations
/// in src/cards/manual/deck_cards.cpp.
///
/// Champions:
///   734  LeBlanc, Fragmented   (WhenIDie -> draw 1, or 2 in BeginningStep)
///   712  Vex, Apathetic        (Deflect-only, no overrides)
///   682  Rengar, Trophy Hunter (Ambush-only, no overrides)
///   676  Nidalee, Cat Form     (Ambush + future trigger; currently no overrides)
///    66  Ahri, Alluring        (WhenIHold -> +1 score)
///
/// Legends:
///   785  Gloomist               (WhenIHold -> exhaust + draw 1)
///   756  Deceiver               (WhenIConquerOrHold -> resumable discard + Reflection token)
///   506  Fire Below the Mountain (Activated [E] -> +1 universal_power)
///   262  Bounty Hunter          (Activated [E] -> give Ganking to a unit)
///
/// Champion units:
///   543  Sett, Brawler          (WhenIConquerOrHold -> buff; Activated -> spend buff for +4M)
///   552  Glorious Executioner   (WhenIWinCombat -> draw 1)
///   787  Voidreaver             (WhenIWinCombat -> +1 XP; Activated [E] + 1 XP -> buff)

#include "tests/cards/card_test_fixture.h"

#include "engine/game_engine.h"

using namespace riftbound;
using namespace riftbound::test;

namespace {

constexpr CardDefId kLeBlanc            = 734;
constexpr CardDefId kVexApathetic       = 712;
constexpr CardDefId kRengarTrophy       = 682;
constexpr CardDefId kNidaleeCat         = 676;
constexpr CardDefId kAhriAlluring       = 66;

constexpr CardDefId kGloomist           = 785;
constexpr CardDefId kDeceiver           = 756;
constexpr CardDefId kFireBelowMtn       = 506;
constexpr CardDefId kBountyHunter       = 262;

constexpr CardDefId kSettBrawler        = 543;
constexpr CardDefId kGloriousExecutioner = 552;
constexpr CardDefId kVoidreaver         = 787;

// Arbitrary deck-filler card-def. Any registered id works; we just need
// the addToDeck() helper to copy a name onto the GameObject.
constexpr CardDefId kFiller             = 687;

// ═══════════════════════════════════════════════════════════════════════════
// [734] LeBlanc, Fragmented — WhenIDie -> draw 1 (or 2 in BeginningStep)
// ═══════════════════════════════════════════════════════════════════════════

class LeBlancTest : public CardTestFixture {};

TEST_F(LeBlancTest, TriggerTypeIsWhenIDie) {
    Card* c = card_registry.get(kLeBlanc);
    EXPECT_EQ(c->triggerType(), TriggerType::WhenIDie);
}

TEST_F(LeBlancTest, DrawsOneOutsideBeginningPhase) {
    state.turn.phase = TurnPhase::MainPhase;
    auto src = addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/0);
    addToDeck(P1, kFiller);
    addToDeck(P1, kFiller);

    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, src};
    int initial_hand = handSize(P1);
    int initial_deck = deckSize(P1);

    card_registry.get(kLeBlanc)->onTrigger(ctx, {});

    EXPECT_EQ(handSize(P1), initial_hand + 1)
        << "outside BeginningStep, LeBlanc Deathknell draws 1";
    EXPECT_EQ(deckSize(P1), initial_deck - 1);
}

TEST_F(LeBlancTest, DrawsTwoDuringBeginningStep) {
    state.turn.phase = TurnPhase::BeginningStep;
    auto src = addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/0);
    addToDeck(P1, kFiller);
    addToDeck(P1, kFiller);
    addToDeck(P1, kFiller);

    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, src};
    int initial_hand = handSize(P1);

    card_registry.get(kLeBlanc)->onTrigger(ctx, {});

    EXPECT_EQ(handSize(P1), initial_hand + 2)
        << "during BeginningStep, LeBlanc Deathknell draws 2";
}

// ═══════════════════════════════════════════════════════════════════════════
// [712] Vex, Apathetic — keyword-only stub (Deflect handled engine-side)
// ═══════════════════════════════════════════════════════════════════════════

class VexApatheticTest : public CardTestFixture {};

TEST_F(VexApatheticTest, RegisteredAndHasNoTrigger) {
    Card* c = card_registry.get(kVexApathetic);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->triggerType(), TriggerType::None)
        << "Vex Apathetic is intentionally keyword-only — no trigger override";
    EXPECT_FALSE(c->hasActivatedAbility());
}

// ═══════════════════════════════════════════════════════════════════════════
// [682] Rengar, Trophy Hunter — Ambush-only stub
// ═══════════════════════════════════════════════════════════════════════════

class RengarTrophyTest : public CardTestFixture {};

TEST_F(RengarTrophyTest, RegisteredAndHasNoTrigger) {
    Card* c = card_registry.get(kRengarTrophy);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->triggerType(), TriggerType::None)
        << "Rengar Trophy is keyword-only (Ambush handled by engine)";
    EXPECT_FALSE(c->hasActivatedAbility());
}

// ═══════════════════════════════════════════════════════════════════════════
// [676] Nidalee, Cat Form — Ambush keyword + WhenIWinCombat -> draw 1
// ═══════════════════════════════════════════════════════════════════════════

class NidaleeCatTest : public CardTestFixture {};

TEST_F(NidaleeCatTest, RegisteredAndHasWinCombatTrigger) {
    Card* c = card_registry.get(kNidaleeCat);
    ASSERT_NE(c, nullptr);
    EXPECT_TRUE(c->firesOn(TriggerType::WhenIWinCombat))
        << "Nidalee Cat now draws 1 when she wins combat";
    EXPECT_FALSE(c->hasActivatedAbility());
}

// ═══════════════════════════════════════════════════════════════════════════
// [66] Ahri, Alluring — WhenIHold -> +1 score
// ═══════════════════════════════════════════════════════════════════════════

class AhriAlluringTest : public CardTestFixture {};

TEST_F(AhriAlluringTest, TriggerTypeIsWhenIHold) {
    Card* c = card_registry.get(kAhriAlluring);
    EXPECT_EQ(c->triggerType(), TriggerType::WhenIHold);
}

TEST_F(AhriAlluringTest, ScoresOnePointWhenHolding) {
    auto src = addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/0);
    setScore(P1, 2);

    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, src};

    card_registry.get(kAhriAlluring)->onTrigger(ctx, {});

    EXPECT_EQ(state.player(P1).score, 3) << "Ahri scores +1 on hold trigger";
    EXPECT_EQ(state.player(P2).score, 0) << "opponent score untouched";
}

// ═══════════════════════════════════════════════════════════════════════════
// [162] Miss Fortune, Captain — "The first time I move each turn, you may
// ready something else that's exhausted." Pre-fix the trigger type was
// WhenAFriendlyUnitMovesToFB, which the engine skips for the moving unit
// itself (trigger_manager.cpp:524), so moving Captain never fired it.
// ═══════════════════════════════════════════════════════════════════════════

constexpr CardDefId kMissFortuneCaptain = 162;

class MissFortuneCaptainTest : public CardTestFixture {};

TEST_F(MissFortuneCaptainTest, TriggerTypeIsWhenIMove) {
    Card* c = card_registry.get(kMissFortuneCaptain);
    EXPECT_EQ(c->triggerType(), TriggerType::WhenIMove)
        << "Captain's text says 'When I move' — must fire on HER move";
}

TEST_F(MissFortuneCaptainTest, ReadiesExhaustedFriendlyOnYes) {
    auto captain = addUnit(P1, kMissFortuneCaptain, /*might=*/5);
    auto ally    = addUnit(P1, kInvalidId, /*might=*/7);
    state.getObject(ally).is_exhausted = true;

    EffectExecutor exec(state, events, card_db);
    // Picker: confirmOptional publishes [no, yes] (index 0/1). Pick yes.
    // pickTarget then publishes one option (the only legal target).
    int call = 0;
    driveResumableTrigger(kMissFortuneCaptain, P1, captain,
        [&](const std::vector<Intent>& legal) {
            return legal[call++ == 0 ? 1 : 0];  // yes, then the only target
        },
        exec);

    EXPECT_FALSE(state.getObject(ally).is_exhausted)
        << "the exhausted ally should be readied";
}

TEST_F(MissFortuneCaptainTest, DeclineDoesNothing) {
    auto captain = addUnit(P1, kMissFortuneCaptain, /*might=*/5);
    auto ally    = addUnit(P1, kInvalidId, /*might=*/7);
    state.getObject(ally).is_exhausted = true;

    EffectExecutor exec(state, events, card_db);
    driveResumableTrigger(kMissFortuneCaptain, P1, captain,
        [](const std::vector<Intent>& legal) { return legal[0]; }, // no
        exec);

    EXPECT_TRUE(state.getObject(ally).is_exhausted)
        << "declining must leave the ally exhausted";
}

TEST_F(MissFortuneCaptainTest, NoOpWhenNothingToReady) {
    auto captain = addUnit(P1, kMissFortuneCaptain, /*might=*/5);
    auto ally    = addUnit(P1, kInvalidId, /*might=*/7);
    // ally is already ready — nothing exhausted to target.

    EffectExecutor exec(state, events, card_db);
    driveResumableTrigger(kMissFortuneCaptain, P1, captain,
        [](const std::vector<Intent>& legal) {
            return legal.empty() ? Intent{} : legal[0];
        },
        exec);

    // confirmOptional's still_legal() returns false → MAY_NOT_OFFERED,
    // no prompt published, no state change.
    EXPECT_FALSE(state.getObject(ally).is_exhausted)
        << "ally stays ready (no change)";
}

TEST_F(MissFortuneCaptainTest, OnlyFiresOncePerTurn) {
    auto captain = addUnit(P1, kMissFortuneCaptain, /*might=*/5);
    auto ally1   = addUnit(P1, kInvalidId, /*might=*/3);
    auto ally2   = addUnit(P1, kInvalidId, /*might=*/4);
    state.getObject(ally1).is_exhausted = true;
    state.getObject(ally2).is_exhausted = true;

    EffectExecutor exec(state, events, card_db);
    // First move: accept and ready the first target offered.
    int call1 = 0;
    driveResumableTrigger(kMissFortuneCaptain, P1, captain,
        [&](const std::vector<Intent>& legal) {
            return legal[call1++ == 0 ? 1 : 0];  // yes, then first target
        },
        exec);

    // Exactly one of the two should now be ready.
    bool a1_ready = !state.getObject(ally1).is_exhausted;
    bool a2_ready = !state.getObject(ally2).is_exhausted;
    EXPECT_NE(a1_ready, a2_ready) << "exactly one ally readied on first move";

    // Re-fire the trigger this same turn: per-turn gate blocks it, no prompt
    // is published, no further ready.
    driveResumableTrigger(kMissFortuneCaptain, P1, captain,
        [](const std::vector<Intent>& legal) {
            return legal.empty() ? Intent{} : legal[0];
        },
        exec);

    EXPECT_EQ(!state.getObject(ally1).is_exhausted, a1_ready)
        << "no further ready: ally1 state unchanged on second move";
    EXPECT_EQ(!state.getObject(ally2).is_exhausted, a2_ready)
        << "no further ready: ally2 state unchanged on second move";
}

// ═══════════════════════════════════════════════════════════════════════════
// [785] Gloomist (legend) — WhenIHold -> exhaust self + draw 1
// ═══════════════════════════════════════════════════════════════════════════

class GloomistTest : public CardTestFixture {};

TEST_F(GloomistTest, TriggerTypeIsWhenIHold) {
    Card* c = card_registry.get(kGloomist);
    EXPECT_EQ(c->triggerType(), TriggerType::WhenIHold);
}

TEST_F(GloomistTest, ExhaustsAndDrawsWhenReady) {
    auto src = addUnit(P1, kInvalidId, /*might=*/0);
    state.getObject(src).is_exhausted = false;
    addToDeck(P1, kFiller);

    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, src};
    int initial_hand = handSize(P1);

    card_registry.get(kGloomist)->onTrigger(ctx, {});

    EXPECT_TRUE(state.getObject(src).is_exhausted)
        << "Gloomist must exhaust itself as the cost";
    EXPECT_EQ(handSize(P1), initial_hand + 1) << "Gloomist draws 1 on hold";
}

TEST_F(GloomistTest, NoOpWhenAlreadyExhausted) {
    auto src = addUnit(P1, kInvalidId, /*might=*/0);
    state.getObject(src).is_exhausted = true;
    addToDeck(P1, kFiller);

    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, src};
    int initial_hand = handSize(P1);

    card_registry.get(kGloomist)->onTrigger(ctx, {});

    EXPECT_EQ(handSize(P1), initial_hand)
        << "exhausted Gloomist can't pay the cost — no draw";
}

// ═══════════════════════════════════════════════════════════════════════════
// [756] Deceiver (legend) — WhenIConquerOrHold, resumable: discard 1 +
// exhaust to play a Reflection token.
// ═══════════════════════════════════════════════════════════════════════════

class DeceiverTest : public CardTestFixture {};

TEST_F(DeceiverTest, TriggerTypeIsWhenIConquerOrHold) {
    Card* c = card_registry.get(kDeceiver);
    EXPECT_EQ(c->triggerType(), TriggerType::WhenIConquerOrHold);
}

TEST_F(DeceiverTest, NoOpWhenHandEmpty) {
    auto src = addUnit(P1, kInvalidId, /*might=*/0);
    state.getObject(src).is_exhausted = false;

    EffectExecutor exec(state, events, card_db);

    // Drive the resumable. Hand is empty, so case 0 returns early — no
    // pending choice, no token. Mirror the resume-loop shape from
    // CardTestFixture::driveResumable but for a trigger (LegendCard's
    // onTrigger, not onResolve).
    ChainItem ri;
    ri.id = state.chain.allocateId();
    ri.controller = P1;
    ri.is_spell = false;
    ri.resume_point = 0;
    ri.source = src;
    ri.card_def_id = kDeceiver;
    state.chain.resuming = ri;

    Card* card = card_registry.get(kDeceiver);
    CardContext ctx{state, events, exec, P1, src};
    card->onTrigger(ctx, {});

    EXPECT_FALSE(exec.hasPendingChoice())
        << "empty hand -> case 0 returns early, no choice published";
    EXPECT_FALSE(state.getObject(src).is_exhausted)
        << "no exhaust if we couldn't pay the discard";

    // No token created (only Deceiver itself + 0 base bf objects).
    int unit_token_count = 0;
    for (auto& [id, obj] : state.objects) {
        if (obj.super_type == SuperType::Token) ++unit_token_count;
    }
    EXPECT_EQ(unit_token_count, 0);
    state.chain.resuming.reset();
}

TEST_F(DeceiverTest, DiscardsAndCreatesReflectionToken) {
    auto src = addUnit(P1, kInvalidId, /*might=*/0);
    state.getObject(src).is_exhausted = false;
    auto h1 = addToHand(P1, kFiller);

    EffectExecutor exec(state, events, card_db);

    // Resume loop for a trigger.
    ChainItem ri;
    ri.id = state.chain.allocateId();
    ri.controller = P1;
    ri.is_spell = false;
    ri.resume_point = 0;
    ri.source = src;
    ri.card_def_id = kDeceiver;
    state.chain.resuming = ri;

    Card* card = card_registry.get(kDeceiver);
    CardContext ctx{state, events, exec, P1, src};

    // case 0 — publishes discard choice
    card->onTrigger(ctx, {});
    ASSERT_TRUE(exec.hasPendingChoice());
    auto pending = exec.consumePendingChoice();
    ASSERT_FALSE(pending.legal.empty());
    // Pick the choice that discards h1.
    Intent picked;
    for (auto& i : pending.legal) {
        if (!i.chosen_objects.empty() && i.chosen_objects[0] == h1) {
            picked = i;
            break;
        }
    }
    ASSERT_FALSE(picked.chosen_objects.empty()) << "expected an Intent for h1";
    exec.recordChoice(picked);
    // case 1 — commits discard + creates the token
    card->onTrigger(ctx, {});

    EXPECT_TRUE(state.getObject(src).is_exhausted) << "Deceiver exhausts itself";
    EXPECT_TRUE(inTrash(P1, h1))
        << "the chosen hand card lands in the trash";

    int reflection_count = 0;
    for (auto& [id, obj] : state.objects) {
        if (obj.super_type == SuperType::Token && obj.name == "Reflection")
            ++reflection_count;
    }
    EXPECT_EQ(reflection_count, 1) << "Deceiver creates exactly 1 Reflection token";
    state.chain.resuming.reset();
}

// ═══════════════════════════════════════════════════════════════════════════
// [506] Fire Below the Mountain (legend) — Activated [E] -> +1 universal_power
// ═══════════════════════════════════════════════════════════════════════════

class FireBelowMtnTest : public CardTestFixture {};

TEST_F(FireBelowMtnTest, HasActivatedAbilityWithExhaustCost) {
    Card* c = card_registry.get(kFireBelowMtn);
    EXPECT_TRUE(c->hasActivatedAbility());
    auto cost = c->getActivationCost();
    EXPECT_TRUE(cost.exhaust);
    EXPECT_EQ(cost.xp_cost, 0);
}

TEST_F(FireBelowMtnTest, ActivateAddsUniversalPower) {
    auto src = addUnit(P1, kInvalidId, /*might=*/0);  // legend stand-in object
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 0);

    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, src};
    card_registry.get(kFireBelowMtn)->onActivate(ctx, {});

    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 1)
        << "Fire Below the Mountain adds 1 universal power on activate";
    EXPECT_EQ(state.player(P2).rune_pool.universal_power, 0)
        << "opponent's pool is untouched";
}

// ═══════════════════════════════════════════════════════════════════════════
// [262] Bounty Hunter (legend) — Activated [E] -> give Ganking to a unit
// ═══════════════════════════════════════════════════════════════════════════

class BountyHunterTest : public CardTestFixture {};

TEST_F(BountyHunterTest, HasActivatedAbilityWithExhaustCost) {
    Card* c = card_registry.get(kBountyHunter);
    // Phase 6r — multi-ability API
    auto abilities = c->activatedAbilities();
    ASSERT_EQ(abilities.size(), 1u);
    EXPECT_TRUE(abilities[0].cost.exhaust);
    EXPECT_EQ(abilities[0].targets.count, 1);
    EXPECT_TRUE(abilities[0].targets.must_be_unit);
    EXPECT_TRUE(abilities[0].needs_activation_time_target)
        << "Phase 6r/6q: target deferred to activation-time pickTarget";
}

TEST_F(BountyHunterTest, GivesGankingToTarget) {
    auto src = addUnit(P1, kInvalidId, /*might=*/0);
    auto target = addUnit(P1, kInvalidId, /*might=*/2, /*at_bf=*/0);
    EXPECT_FALSE(state.getObject(target).keywords.has(Keyword::Ganking));

    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, src};
    // Pass targets directly via the legacy single-target path (test-only
    // back-compat bypass of pickTarget). Production goes through pickTarget.
    card_registry.get(kBountyHunter)->onActivate(ctx, 0, {target});

    EXPECT_TRUE(state.getObject(target).keywords.has(Keyword::Ganking))
        << "Bounty Hunter grants Ganking on activate";
}

TEST_F(BountyHunterTest, NoOpWithEmptyTargets) {
    auto src = addUnit(P1, kInvalidId, /*might=*/0);

    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, src};
    // Empty targets + no chain.resuming → pickTarget returns kInvalidId
    // (no legal targets). onActivate exits cleanly.
    EXPECT_NO_THROW(
        card_registry.get(kBountyHunter)->onActivate(ctx, 0, {})
    );
}

// Regression: action-gen MUST offer Bounty Hunter's [E] ability when the
// legend is ready, a friendly unit is on the board, and the cost is free.
// Previously MBountyHunter declared targets only via activatedAbilities()
// (the multi-ability descriptor) but didn't override enumerateLegalTargets
// or getTargetRequirements — so the engine's legality gate
// (`if (legal.empty()) continue;` in generateActivateAbilityActions) saw
// count=0 from the legacy default and silently dropped the ability.
TEST_F(BountyHunterTest, ActivatedAbilityOfferedInLegalActions) {
    GameEngine engine(card_db, events, card_registry);
    auto& s = engine.mutableState();
    s.mode             = ModeOfPlay{};
    s.players[0].id    = P1;
    s.players[1].id    = P2;
    s.turn.turn_player = P1;
    s.turn.phase       = TurnPhase::MainPhase;
    s.turn.ns_state    = NeutralShowdownState::Neutral;
    s.turn.oc_state    = OpenClosedState::Open;
    BattlefieldState b0; b0.id = 0; s.battlefields.push_back(b0);
    BattlefieldState b1; b1.id = 1; s.battlefields.push_back(b1);

    // Bounty Hunter ready in the legend zone.
    auto legend = s.createObject();
    auto& leg = s.getObject(legend);
    leg.owner       = P1;
    leg.controller  = P1;
    leg.card_def_id = kBountyHunter;
    const auto& def = card_db.get(kBountyHunter);
    leg.name        = def.name;
    leg.card_type   = def.card_type;  // Legend
    leg.zone        = ZoneType::LegendZone;
    leg.is_exhausted = false;
    s.player(P1).legend_zone = legend;

    // A friendly unit at base — the only legal target right now.
    auto unit = s.createObject();
    auto& u = s.getObject(unit);
    u.owner = P1; u.controller = P1;
    u.card_type = CardType::Unit;
    u.base_might = 3; u.current_might = 3;
    u.zone = ZoneType::Base;
    u.location = BaseLocation{P1};
    u.is_exhausted = false;

    auto actions = engine.generateLegalActions();
    bool offered = false;
    for (const auto& a : actions) {
        if (a.type != IntentType::ActivateAbility) continue;
        if (a.ability_source != legend) continue;
        offered = true;
        break;
    }
    EXPECT_TRUE(offered)
        << "Bounty Hunter [E]: 'Give a unit [Ganking] this turn' MUST be "
           "offered in generateLegalActions when the legend is ready and "
           "at least one friendly unit is on the board.";
}

// Regression: Bounty Hunter's [E] grants Ganking "this turn" — the
// keyword MUST be removed at end of turn unless the unit already has
// it from another source (printed on CardDef or granted by an active
// aura). Card text: "[E]: Give a unit [Ganking] this turn."
TEST_F(BountyHunterTest, GankingExpiresAtEndOfTurn) {
    auto src = addUnit(P1, kInvalidId, /*might=*/3);
    auto target = addUnit(P1, kInvalidId, /*might=*/2);
    // Sanity: the target does NOT print Ganking on its (invalid) CardDef.
    EXPECT_FALSE(state.getObject(target).keywords.has(Keyword::Ganking));

    // Grant Ganking via Bounty Hunter's [E].
    EffectExecutor exec(state, events, card_db, &card_registry);
    CardContext ctx{state, events, exec, P1, src};
    card_registry.get(kBountyHunter)->onActivate(ctx, 0, {target});
    EXPECT_TRUE(state.getObject(target).keywords.has(Keyword::Ganking))
        << "post-grant: target should have Ganking";
    EXPECT_TRUE(state.getObject(target).temp_keywords.has(Keyword::Ganking))
        << "post-grant: target should be tagged as having Ganking 'this turn' "
           "so the expiration step knows to revoke it";

    // Direct invocation of the expiration helper — exercises the same
    // code path the engine's full ExpirationStep walks for every object,
    // without needing to spin up runChain / chain_manager_ etc.
    GameEngine::expireTemporaryKeywords(state.getObject(target), card_db);

    EXPECT_FALSE(state.getObject(target).keywords.has(Keyword::Ganking))
        << "post-expiration: Ganking granted 'this turn' should be revoked "
           "(card text: '... [Ganking] this turn'). If this fires, the temp "
           "tracking didn't survive the expiration pass.";
    EXPECT_FALSE(state.getObject(target).temp_keywords.has(Keyword::Ganking))
        << "post-expiration: temp_keywords should be cleared too.";
}

// Counter-regression: a unit that BASE-PRINTS Ganking keeps it across
// the expiration step even if it was also temporarily granted (e.g.
// Bounty Hunter targeted it redundantly).
TEST_F(BountyHunterTest, GankingPersistsOnExpirationIfPrintedOnCard) {
    // Find any card_def_id that prints Ganking on its base text.
    CardDefId printed_ganking_def = kInvalidId;
    for (const auto& [id, def] : card_db.all()) {
        if (def.keywords.has(Keyword::Ganking)) {
            printed_ganking_def = id;
            break;
        }
    }
    ASSERT_NE(printed_ganking_def, kInvalidId)
        << "Test scaffold: no card in registry prints Ganking — adjust the "
           "fixture to use a known-printed-Ganking card_def_id directly.";

    auto src = addUnit(P1, kInvalidId, /*might=*/3);
    auto target = addUnit(P1, printed_ganking_def, /*might=*/2);
    ASSERT_TRUE(state.getObject(target).keywords.has(Keyword::Ganking))
        << "addUnit should copy printed keywords from CardDef.";

    EffectExecutor exec(state, events, card_db, &card_registry);
    CardContext ctx{state, events, exec, P1, src};
    card_registry.get(kBountyHunter)->onActivate(ctx, 0, {target});

    GameEngine::expireTemporaryKeywords(state.getObject(target), card_db);

    EXPECT_TRUE(state.getObject(target).keywords.has(Keyword::Ganking))
        << "Printed-Ganking units keep Ganking after expiration — only "
           "the temp grant is revoked, not the base printing.";
}

// Counter-test: no friendly unit on the board → ability NOT offered.
TEST_F(BountyHunterTest, ActivatedAbilityNotOfferedWhenNoUnitsExist) {
    GameEngine engine(card_db, events, card_registry);
    auto& s = engine.mutableState();
    s.mode             = ModeOfPlay{};
    s.players[0].id    = P1;
    s.players[1].id    = P2;
    s.turn.turn_player = P1;
    s.turn.phase       = TurnPhase::MainPhase;
    s.turn.ns_state    = NeutralShowdownState::Neutral;
    s.turn.oc_state    = OpenClosedState::Open;
    BattlefieldState b0; b0.id = 0; s.battlefields.push_back(b0);
    BattlefieldState b1; b1.id = 1; s.battlefields.push_back(b1);

    auto legend = s.createObject();
    auto& leg = s.getObject(legend);
    leg.owner       = P1;
    leg.controller  = P1;
    leg.card_def_id = kBountyHunter;
    const auto& def = card_db.get(kBountyHunter);
    leg.name        = def.name;
    leg.card_type   = def.card_type;
    leg.zone        = ZoneType::LegendZone;
    leg.is_exhausted = false;
    s.player(P1).legend_zone = legend;
    // No units in play.

    auto actions = engine.generateLegalActions();
    bool offered = false;
    for (const auto& a : actions) {
        if (a.type == IntentType::ActivateAbility && a.ability_source == legend) {
            offered = true; break;
        }
    }
    EXPECT_FALSE(offered)
        << "Bounty Hunter's [E] requires a unit target — no units → no offer.";
}

// ═══════════════════════════════════════════════════════════════════════════
// [543] Sett, Brawler — WhenYouPlayMe + WhenIConquer -> buff; Activate spend buff +4M
// ═══════════════════════════════════════════════════════════════════════════

class SettBrawlerTest : public CardTestFixture {};

TEST_F(SettBrawlerTest, FiresOnPlayAndConquer) {
    // Sett is now a multi-trigger card (buffs on play AND on conquer, NOT on
    // hold). triggerType() returns None; query firesOn for each trigger.
    Card* c = card_registry.get(kSettBrawler);
    EXPECT_EQ(c->triggerType(), TriggerType::None);
    EXPECT_TRUE(c->firesOn(TriggerType::WhenYouPlayMe));
    EXPECT_TRUE(c->firesOn(TriggerType::WhenIConquer));
    EXPECT_FALSE(c->firesOn(TriggerType::WhenIConquerOrHold))
        << "Sett no longer over-fires on hold";
    EXPECT_TRUE(c->hasActivatedAbility());
}

TEST_F(SettBrawlerTest, TriggerBuffsSelf) {
    auto src = addUnit(P1, kInvalidId, /*might=*/2, /*at_bf=*/0);
    int base = state.getObject(src).current_might;

    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, src};
    card_registry.get(kSettBrawler)->onTrigger(ctx, {});

    EXPECT_EQ(state.getObject(src).buff_count, 1)
        << "Sett gains a buff counter on conquer/hold";
    EXPECT_EQ(state.getObject(src).current_might, base + 1)
        << "buff translates to +1M";
}

TEST_F(SettBrawlerTest, ActivateSpendsBuffForBigMight) {
    auto src = addUnit(P1, kInvalidId, /*might=*/2, /*at_bf=*/0);
    state.getObject(src).buff_count = 1;
    state.getObject(src).recomputeMight();
    int with_buff = state.getObject(src).current_might;  // base + 1

    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, src};
    card_registry.get(kSettBrawler)->onActivate(ctx, {});

    // Activation: spend the 1 permanent buff (buff_count 1->0, -1M) then
    // +4 temp might. buff_count is now properly consumed (temp might no longer
    // pollutes it), so the buff is truly spent and the ability can't re-fire.
    EXPECT_EQ(state.getObject(src).buff_count, 0) << "the buff is spent";
    EXPECT_EQ(state.getObject(src).current_might, with_buff + 3)
        << "spend a buff (-1M) then +4M = net +3M over the previously buffed state";
}

TEST_F(SettBrawlerTest, ActivateNoOpWithoutBuff) {
    auto src = addUnit(P1, kInvalidId, /*might=*/2, /*at_bf=*/0);
    int base = state.getObject(src).current_might;
    EXPECT_EQ(state.getObject(src).buff_count, 0);

    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, src};
    card_registry.get(kSettBrawler)->onActivate(ctx, {});

    EXPECT_EQ(state.getObject(src).buff_count, 0)
        << "no buff to spend -> activate is a no-op";
    EXPECT_EQ(state.getObject(src).current_might, base);
}

// ═══════════════════════════════════════════════════════════════════════════
// [552] Glorious Executioner (legend) — WhenIWinCombat -> draw 1
// ═══════════════════════════════════════════════════════════════════════════

class GloriousExecutionerTest : public CardTestFixture {};

TEST_F(GloriousExecutionerTest, TriggerTypeIsWhenIWinCombat) {
    Card* c = card_registry.get(kGloriousExecutioner);
    EXPECT_EQ(c->triggerType(), TriggerType::WhenIWinCombat);
}

TEST_F(GloriousExecutionerTest, DrawsOneOnWinCombat) {
    auto src = addUnit(P1, kInvalidId, /*might=*/4, /*at_bf=*/0);
    auto deck_card = addToDeck(P1, kFiller);

    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, src};
    int initial_hand = handSize(P1);

    card_registry.get(kGloriousExecutioner)->onTrigger(ctx, {});

    EXPECT_EQ(handSize(P1), initial_hand + 1)
        << "Glorious Executioner draws 1 on win combat";
    EXPECT_TRUE(inHand(P1, deck_card));
}

// ═══════════════════════════════════════════════════════════════════════════
// [787] Voidreaver (legend) — WhenIWinCombat -> +1 XP; Activate spend 1 XP +
// exhaust to buff a unit.
// ═══════════════════════════════════════════════════════════════════════════

class VoidreaverTest : public CardTestFixture {};

TEST_F(VoidreaverTest, TriggerTypeIsWhenIWinCombat) {
    Card* c = card_registry.get(kVoidreaver);
    EXPECT_EQ(c->triggerType(), TriggerType::WhenIWinCombat);
}

TEST_F(VoidreaverTest, HasActivatedAbilityWithExhaustAndOneXp) {
    Card* c = card_registry.get(kVoidreaver);
    // Phase 6r — Voidreaver now exposes BOTH activated abilities.
    auto abilities = c->activatedAbilities();
    ASSERT_EQ(abilities.size(), 2u)
        << "Voidreaver has two activated abilities (Spend 1 XP buff; "
           "Spend 2 XP recall)";
    EXPECT_TRUE(abilities[0].cost.exhaust);
    EXPECT_EQ(abilities[0].cost.xp_cost, 1);
    EXPECT_EQ(abilities[0].targets.count, 1);
    EXPECT_TRUE(abilities[0].targets.must_be_unit);
    EXPECT_TRUE(abilities[0].needs_activation_time_target);
    // Ability 1 — Spend 2 XP, [E]: recall (move to base) exhausted friendly.
    EXPECT_TRUE(abilities[1].cost.exhaust);
    EXPECT_EQ(abilities[1].cost.xp_cost, 2);
    EXPECT_TRUE(abilities[1].targets.must_be_friendly);
    EXPECT_TRUE(abilities[1].targets.must_be_at_battlefield);
}

TEST_F(VoidreaverTest, TriggerGrantsOneXp) {
    auto src = addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/0);
    setXp(P1, 2);

    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, src};
    card_registry.get(kVoidreaver)->onTrigger(ctx, {});

    EXPECT_EQ(state.player(P1).xp, 3) << "+1 XP on win combat";
}

TEST_F(VoidreaverTest, ActivateBuffsTargetUnit) {
    // Note: the engine deducts the XP cost during the ActivateAbility intent
    // execution path (PlayerState::xp -= cost.xp_cost); the Card's onActivate
    // itself only runs the effect. So we set XP, then assert the effect.
    auto src = addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/0);
    auto target = addUnit(P1, kInvalidId, /*might=*/2, /*at_bf=*/0);
    setXp(P1, 5);
    int base_might = state.getObject(target).current_might;

    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, src};
    card_registry.get(kVoidreaver)->onActivate(ctx, 0, {target});

    EXPECT_EQ(state.getObject(target).buff_count, 1)
        << "Voidreaver activate buffs the target";
    EXPECT_EQ(state.getObject(target).current_might, base_might + 1);

    // Manually deduct the XP the engine would have charged. Assert the
    // bookkeeping a real activate() flow leaves on PlayerState.
    // Phase 6r — read from the new multi-ability API.
    auto abilities = card_registry.get(kVoidreaver)->activatedAbilities();
    state.player(P1).xp -= abilities[0].cost.xp_cost;
    EXPECT_EQ(state.player(P1).xp, 4) << "1 XP spent on activation";
}

}  // namespace
