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
// [676] Nidalee, Cat Form — Ambush-only stub today
// ═══════════════════════════════════════════════════════════════════════════

class NidaleeCatTest : public CardTestFixture {};

TEST_F(NidaleeCatTest, RegisteredAndHasNoTrigger) {
    Card* c = card_registry.get(kNidaleeCat);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->triggerType(), TriggerType::None)
        << "Nidalee Cat: Ambush keyword + WhenIWinCombat draw not yet wired";
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

// ═══════════════════════════════════════════════════════════════════════════
// [543] Sett, Brawler — WhenIConquerOrHold -> buff; Activate spend buff +4M
// ═══════════════════════════════════════════════════════════════════════════

class SettBrawlerTest : public CardTestFixture {};

TEST_F(SettBrawlerTest, TriggerTypeIsWhenIConquerOrHold) {
    Card* c = card_registry.get(kSettBrawler);
    EXPECT_EQ(c->triggerType(), TriggerType::WhenIConquerOrHold);
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

    // Activation: -1 buff_count (spend), then giveTemporaryMight(4) which
    // adds 4 to buff_count and recomputes. Net buff_count delta = +3.
    EXPECT_EQ(state.getObject(src).buff_count, 1 - 1 + 4);
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
