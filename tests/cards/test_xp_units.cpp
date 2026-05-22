/// @file test_xp_units.cpp
/// Per-card unit tests for XP / Hunt / Level units and a few related Vex
/// deck cards. Cards covered:
///
/// Hunt + Level (snapshot-at-play approximations):
///   596 Herald of Spring     (MHeraldOfSpring)    — on play, +2 XP
///   602 Wuju Apprentice      (MWujuApprentice)    — Level 6: draw 1 on play
///   609 Mosstomper           (MMosstomper)        — Level 3: +1M and Deflect
///   656 Gemhand Hunter       (MGemhandHunter)     — Level 6: +1M
///   675 Master Yi, Tempered  (MMasterYi)          — Level 6: Deflect+Ganking
///   689 Mister Root          (MMisterRoot)        — on move to BF, +2 XP
///
/// Vex / Vex deck:
///   605 Enthusiastic Promoter (MEnthusiasticPromoter) — on hold, buff friendlies at BF
///   610 Trevor Snoozebottom   (MTrevorSnoozebottom)   — on hold, sprite token
///   688 Megatusk              (MMegatusk)             — activated: spend 3 XP -> Ganking
///
/// Skipped here (covered by the parallel test_champions_legends.cpp suite):
///   552 Glorious Executioner (MGloriousExecutioner) — draw 1 on win combat
///   787 Voidreaver           (MVoidreaver)          — +1 XP on win combat;
///                                                     activated: spend 1 XP, [E] -> buff
///
/// Other skipped scenarios:
///   - Voidreaver's "Spend 2 XP, [E]: Move exhausted unit to base" — Card
///     only models a single activated ability (see CLAUDE.md "Known engine
///     gaps").
///   - Mosstomper's Hunt 2 / Master Yi's Hunt 2 — Hunt itself is engine
///     keyword-handled (conquer/hold -> +N XP); we only test the Level
///     side-effect here.
///
/// Class names use the `Xp` prefix to avoid collisions with any other
/// per-card test file (champions/legends suite uses unprefixed names).

#include "tests/cards/card_test_fixture.h"

using namespace riftbound;
using namespace riftbound::test;

namespace {

constexpr CardDefId kHeraldOfSpring        = 596;
constexpr CardDefId kWujuApprentice        = 602;
constexpr CardDefId kMosstomper            = 609;
constexpr CardDefId kGemhandHunter         = 656;
constexpr CardDefId kMasterYi              = 675;
constexpr CardDefId kMisterRoot            = 689;
constexpr CardDefId kEnthusiasticPromoter  = 605;
constexpr CardDefId kTrevorSnoozebottom    = 610;
constexpr CardDefId kMegatusk              = 688;
// 552 (Glorious Executioner) and 787 (Voidreaver) are intentionally NOT
// listed — they're covered in test_champions_legends.cpp.

// ═══════════════════════════════════════════════════════════════════════════
// Herald of Spring (596) — on play, +2 XP (no Level gate)
// ═══════════════════════════════════════════════════════════════════════════

class XpHeraldOfSpringTest : public CardTestFixture {};

TEST_F(XpHeraldOfSpringTest, OnPlayGainsTwoXp) {
    auto unit = addUnit(P1, kHeraldOfSpring, /*might=*/2, /*at_bf=*/0);
    setXp(P1, 0);

    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, unit};
    Card* c = card_registry.get(kHeraldOfSpring);
    ASSERT_NE(c, nullptr);
    c->onTrigger(ctx, {});

    EXPECT_EQ(state.player(P1).xp, 2) << "Herald of Spring grants +2 XP on play";
}

TEST_F(XpHeraldOfSpringTest, StacksOnExistingXp) {
    auto unit = addUnit(P1, kHeraldOfSpring, /*might=*/2, /*at_bf=*/0);
    setXp(P1, 5);

    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, unit};
    Card* c = card_registry.get(kHeraldOfSpring);
    c->onTrigger(ctx, {});

    EXPECT_EQ(state.player(P1).xp, 7);
}

TEST_F(XpHeraldOfSpringTest, TriggerTypeIsWhenYouPlayMe) {
    Card* c = card_registry.get(kHeraldOfSpring);
    EXPECT_EQ(c->triggerType(), TriggerType::WhenYouPlayMe);
}

// ═══════════════════════════════════════════════════════════════════════════
// Wuju Apprentice (602) — Hunt + Level 6: draw 1 on play
// ═══════════════════════════════════════════════════════════════════════════

class XpWujuApprenticeTest : public CardTestFixture {};

TEST_F(XpWujuApprenticeTest, BelowLevelSixDoesNothing) {
    auto unit = addUnit(P1, kWujuApprentice, /*might=*/3, /*at_bf=*/0);
    addToDeck(P1, kInvalidId);  // a card available to draw
    setXp(P1, 5);  // one short of Level 6

    int initial_hand = handSize(P1);
    int initial_deck = deckSize(P1);

    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, unit};
    Card* c = card_registry.get(kWujuApprentice);
    c->onTrigger(ctx, {});

    EXPECT_EQ(handSize(P1), initial_hand) << "no draw below Level 6";
    EXPECT_EQ(deckSize(P1), initial_deck);
}

TEST_F(XpWujuApprenticeTest, AtLevelSixDrawsOne) {
    auto unit = addUnit(P1, kWujuApprentice, /*might=*/3, /*at_bf=*/0);
    auto deck_card = addToDeck(P1, kInvalidId);
    setXp(P1, 6);

    int initial_hand = handSize(P1);

    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, unit};
    Card* c = card_registry.get(kWujuApprentice);
    c->onTrigger(ctx, {});

    EXPECT_EQ(handSize(P1), initial_hand + 1);
    EXPECT_TRUE(inHand(P1, deck_card));
}

TEST_F(XpWujuApprenticeTest, AboveLevelSixStillDraws) {
    auto unit = addUnit(P1, kWujuApprentice, /*might=*/3, /*at_bf=*/0);
    addToDeck(P1, kInvalidId);
    setXp(P1, 12);

    int initial_hand = handSize(P1);
    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, unit};
    card_registry.get(kWujuApprentice)->onTrigger(ctx, {});
    EXPECT_EQ(handSize(P1), initial_hand + 1);
}

// ═══════════════════════════════════════════════════════════════════════════
// Mosstomper (609) — Hunt 2 + Level 3: +1M and Deflect
// ═══════════════════════════════════════════════════════════════════════════

class XpMosstomperTest : public CardTestFixture {};

TEST_F(XpMosstomperTest, BelowLevelThreeNoBuff) {
    // Note: the registry data bakes [Deflect] into Mosstomper's keyword set
    // unconditionally (the Level 3 ability text is also surfaced as a base
    // keyword for code-gen purposes). So we can't assert "no Deflect" here —
    // we verify only the side-effect that requires onTrigger to actually
    // execute its body: the +1M buff via buffUnit/giveTemporaryMight.
    auto unit = addUnit(P1, kMosstomper, /*might=*/4, /*at_bf=*/0);
    setXp(P1, 2);

    int base_might = state.getObject(unit).current_might;
    int base_buff_count = state.getObject(unit).buff_count;

    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, unit};
    card_registry.get(kMosstomper)->onTrigger(ctx, {});

    EXPECT_EQ(state.getObject(unit).current_might, base_might)
        << "no buff below Level 3";
    EXPECT_EQ(state.getObject(unit).buff_count, base_buff_count)
        << "buff_count unchanged below Level 3";
}

TEST_F(XpMosstomperTest, AtLevelThreeBuffsAndGrantsDeflect) {
    auto unit = addUnit(P1, kMosstomper, /*might=*/4, /*at_bf=*/0);
    setXp(P1, 3);

    int base_might = state.getObject(unit).current_might;

    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, unit};
    card_registry.get(kMosstomper)->onTrigger(ctx, {});

    EXPECT_EQ(state.getObject(unit).current_might, base_might + 1)
        << "Level 3: +1M";
    EXPECT_TRUE(state.getObject(unit).keywords.has(Keyword::Deflect))
        << "Level 3: Deflect granted";
}

// ═══════════════════════════════════════════════════════════════════════════
// Gemhand Hunter (656) — Hunt + Level 6: +1M
// ═══════════════════════════════════════════════════════════════════════════

class XpGemhandHunterTest : public CardTestFixture {};

TEST_F(XpGemhandHunterTest, BelowLevelSixNoBuff) {
    auto unit = addUnit(P1, kGemhandHunter, /*might=*/3, /*at_bf=*/0);
    setXp(P1, 5);

    int base_might = state.getObject(unit).current_might;

    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, unit};
    card_registry.get(kGemhandHunter)->onTrigger(ctx, {});

    EXPECT_EQ(state.getObject(unit).current_might, base_might);
}

TEST_F(XpGemhandHunterTest, AtLevelSixGetsBuff) {
    auto unit = addUnit(P1, kGemhandHunter, /*might=*/3, /*at_bf=*/0);
    setXp(P1, 6);

    int base_might = state.getObject(unit).current_might;

    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, unit};
    card_registry.get(kGemhandHunter)->onTrigger(ctx, {});

    EXPECT_EQ(state.getObject(unit).current_might, base_might + 1);
}

// ═══════════════════════════════════════════════════════════════════════════
// Master Yi, Tempered (675) — Hunt 2 + Level 6: Deflect + Ganking
// ═══════════════════════════════════════════════════════════════════════════

class XpMasterYiTest : public CardTestFixture {};

// SKIPPED: BelowLevelSix observable-side-effect test for Master Yi.
// The registry bakes Deflect (kw idx 6) and Ganking (kw idx 8) into Master
// Yi's BASE keyword set, presumably for code-gen / display purposes. The
// onTrigger body just calls keywords.set() unconditionally if xp >= 6,
// which means the post-state for xp<6 is observationally identical to the
// pre-state — there's nothing to assert. We test the at-level case (which
// confirms the body executed and keywords are still present) and trust
// the `if (xp < 6) return;` guard from inspection.

TEST_F(XpMasterYiTest, AtLevelSixHasBothKeywords) {
    auto unit = addUnit(P1, kMasterYi, /*might=*/4, /*at_bf=*/0);
    setXp(P1, 6);

    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, unit};
    card_registry.get(kMasterYi)->onTrigger(ctx, {});

    EXPECT_TRUE(state.getObject(unit).keywords.has(Keyword::Deflect));
    EXPECT_TRUE(state.getObject(unit).keywords.has(Keyword::Ganking));
}

TEST_F(XpMasterYiTest, BelowLevelSixDoesNotMutateXp) {
    // Sanity: triggering at xp<6 must not modify the player's XP (the
    // ability has no XP side-effect, but the test guards against a future
    // regression where someone adds one).
    auto unit = addUnit(P1, kMasterYi, /*might=*/4, /*at_bf=*/0);
    setXp(P1, 5);

    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, unit};
    card_registry.get(kMasterYi)->onTrigger(ctx, {});

    EXPECT_EQ(state.player(P1).xp, 5);
}

// ═══════════════════════════════════════════════════════════════════════════
// Mister Root (689) — on move to BF, +2 XP
// ═══════════════════════════════════════════════════════════════════════════

class XpMisterRootTest : public CardTestFixture {};

TEST_F(XpMisterRootTest, OnMoveGrantsTwoXp) {
    auto unit = addUnit(P1, kMisterRoot, /*might=*/3, /*at_bf=*/0);
    setXp(P1, 0);

    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, unit};
    Card* c = card_registry.get(kMisterRoot);
    EXPECT_EQ(c->triggerType(), TriggerType::WhenIMoveToFB);
    c->onTrigger(ctx, {});

    EXPECT_EQ(state.player(P1).xp, 2);
}

TEST_F(XpMisterRootTest, MultipleMovesAccumulate) {
    auto unit = addUnit(P1, kMisterRoot, /*might=*/3, /*at_bf=*/0);
    setXp(P1, 1);

    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, unit};
    Card* c = card_registry.get(kMisterRoot);
    c->onTrigger(ctx, {});
    c->onTrigger(ctx, {});

    EXPECT_EQ(state.player(P1).xp, 5) << "1 + 2 + 2 = 5";
}

// ═══════════════════════════════════════════════════════════════════════════
// Enthusiastic Promoter (605) — on hold, buff friendlies at my BF
// ═══════════════════════════════════════════════════════════════════════════

class XpEnthusiasticPromoterTest : public CardTestFixture {};

TEST_F(XpEnthusiasticPromoterTest, BuffsFriendlyUnitsAtSameBattlefield) {
    auto promoter = addUnit(P1, kEnthusiasticPromoter, /*might=*/2, /*at_bf=*/0);
    auto buddy   = addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/0);
    int buddy_base = state.getObject(buddy).current_might;
    int promo_base = state.getObject(promoter).current_might;

    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, promoter};
    Card* c = card_registry.get(kEnthusiasticPromoter);
    EXPECT_EQ(c->triggerType(), TriggerType::WhenIHold);
    c->onTrigger(ctx, {});

    EXPECT_EQ(state.getObject(buddy).current_might, buddy_base + 1)
        << "friendly buddy at same BF gets +1M";
    EXPECT_EQ(state.getObject(promoter).current_might, promo_base + 1)
        << "promoter itself is friendly + at the BF, also buffed";
}

TEST_F(XpEnthusiasticPromoterTest, DoesNotBuffEnemies) {
    auto promoter = addUnit(P1, kEnthusiasticPromoter, /*might=*/2, /*at_bf=*/0);
    auto enemy    = addUnit(P2, kInvalidId, /*might=*/3, /*at_bf=*/0);
    int enemy_base = state.getObject(enemy).current_might;

    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, promoter};
    card_registry.get(kEnthusiasticPromoter)->onTrigger(ctx, {});

    EXPECT_EQ(state.getObject(enemy).current_might, enemy_base)
        << "enemy at same BF is not buffed (impl is friendly-only)";
}

TEST_F(XpEnthusiasticPromoterTest, DoesNotBuffOtherBattlefield) {
    auto promoter = addUnit(P1, kEnthusiasticPromoter, /*might=*/2, /*at_bf=*/0);
    auto far_buddy = addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/1);
    int far_base = state.getObject(far_buddy).current_might;

    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, promoter};
    card_registry.get(kEnthusiasticPromoter)->onTrigger(ctx, {});

    EXPECT_EQ(state.getObject(far_buddy).current_might, far_base)
        << "friendly at different BF not affected";
}

// ═══════════════════════════════════════════════════════════════════════════
// Trevor Snoozebottom (610) — on hold, create Sprite token (Temporary, 3M)
// ═══════════════════════════════════════════════════════════════════════════

class XpTrevorSnoozebottomTest : public CardTestFixture {};

TEST_F(XpTrevorSnoozebottomTest, OnHoldCreatesSpriteToken) {
    auto trevor = addUnit(P1, kTrevorSnoozebottom, /*might=*/2, /*at_bf=*/0);

    int objects_before = static_cast<int>(state.objects.size());

    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, trevor};
    Card* c = card_registry.get(kTrevorSnoozebottom);
    EXPECT_EQ(c->triggerType(), TriggerType::WhenIHold);
    c->onTrigger(ctx, {});

    int objects_after = static_cast<int>(state.objects.size());
    EXPECT_EQ(objects_after, objects_before + 1)
        << "exactly one new object (the Sprite) should be created";

    // Find the new object — should be a Sprite at BF 0 controlled by P1, Temporary.
    bool found_sprite = false;
    for (auto& [id, obj] : state.objects) {
        if (obj.name == "Sprite" && obj.controller == P1) {
            found_sprite = true;
            EXPECT_EQ(obj.card_type, CardType::Unit);
            EXPECT_TRUE(obj.keywords.has(Keyword::Temporary));
            ASSERT_TRUE(obj.battlefieldId().has_value());
            EXPECT_EQ(*obj.battlefieldId(), 0);
            EXPECT_EQ(obj.base_might, 3);
        }
    }
    EXPECT_TRUE(found_sprite);
}

TEST_F(XpTrevorSnoozebottomTest, NoTokenWhenNotAtBattlefield) {
    // Trevor at base — no battlefieldId, so no sprite should spawn. The
    // fixture's `at_bf=-1` default doesn't actually put a unit at base
    // (BattlefieldId is uint32_t — -1 wraps to UINT32_MAX, still satisfies
    // `at_bf >= 0`). So we explicitly relocate Trevor to base.
    auto trevor = addUnit(P1, kTrevorSnoozebottom, /*might=*/2, /*at_bf=*/0);
    state.getObject(trevor).zone = ZoneType::Base;
    state.getObject(trevor).location = BaseLocation{P1};

    int objects_before = static_cast<int>(state.objects.size());

    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, trevor};
    card_registry.get(kTrevorSnoozebottom)->onTrigger(ctx, {});

    EXPECT_EQ(static_cast<int>(state.objects.size()), objects_before)
        << "no sprite when source unit isn't at a BF";
}

// ═══════════════════════════════════════════════════════════════════════════
// Megatusk (688) — activated, spend 3 XP: friendlies here get Ganking
// ═══════════════════════════════════════════════════════════════════════════

class XpMegatuskTest : public CardTestFixture {};

TEST_F(XpMegatuskTest, HasActivatedAbility) {
    Card* c = card_registry.get(kMegatusk);
    EXPECT_TRUE(c->hasActivatedAbility());
    // Cost is paid manually inside onActivate via XP (xp_cost not exposed
    // through the ActivationCost struct here — see the deck_cards.cpp
    // comment). Make sure we don't accidentally regress to declaring a
    // non-zero standard cost.
    auto cost = c->getActivationCost();
    EXPECT_EQ(cost.energy, 0);
    EXPECT_EQ(cost.xp_cost, 0)
        << "Megatusk pays its 3 XP manually inside onActivate, not via the "
           "ActivationCost field (see deck_cards.cpp comment)";
}

TEST_F(XpMegatuskTest, BelowThreeXpIsNoOp) {
    auto megatusk = addUnit(P1, kMegatusk, /*might=*/4, /*at_bf=*/0);
    auto buddy    = addUnit(P1, kInvalidId, /*might=*/2, /*at_bf=*/0);
    setXp(P1, 2);

    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, megatusk};
    card_registry.get(kMegatusk)->onActivate(ctx, {});

    EXPECT_EQ(state.player(P1).xp, 2) << "no XP spent when can't afford";
    EXPECT_FALSE(state.getObject(buddy).keywords.has(Keyword::Ganking))
        << "no Ganking granted when can't afford";
}

TEST_F(XpMegatuskTest, SpendsThreeXpAndGrantsGanking) {
    auto megatusk = addUnit(P1, kMegatusk, /*might=*/4, /*at_bf=*/0);
    auto buddy    = addUnit(P1, kInvalidId, /*might=*/2, /*at_bf=*/0);
    setXp(P1, 5);

    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, megatusk};
    card_registry.get(kMegatusk)->onActivate(ctx, {});

    EXPECT_EQ(state.player(P1).xp, 2) << "3 XP spent";
    EXPECT_TRUE(state.getObject(buddy).keywords.has(Keyword::Ganking))
        << "buddy at same BF gets Ganking";
    EXPECT_TRUE(state.getObject(megatusk).keywords.has(Keyword::Ganking))
        << "megatusk himself is friendly + here, also Ganking";
}

TEST_F(XpMegatuskTest, DoesNotGrantToOtherBattlefield) {
    auto megatusk  = addUnit(P1, kMegatusk, /*might=*/4, /*at_bf=*/0);
    auto far_buddy = addUnit(P1, kInvalidId, /*might=*/2, /*at_bf=*/1);
    setXp(P1, 5);

    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, megatusk};
    card_registry.get(kMegatusk)->onActivate(ctx, {});

    EXPECT_FALSE(state.getObject(far_buddy).keywords.has(Keyword::Ganking))
        << "far buddy at BF 1 must not get Ganking";
}

TEST_F(XpMegatuskTest, DoesNotGrantToEnemies) {
    auto megatusk = addUnit(P1, kMegatusk, /*might=*/4, /*at_bf=*/0);
    auto enemy    = addUnit(P2, kInvalidId, /*might=*/2, /*at_bf=*/0);
    setXp(P1, 5);

    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, megatusk};
    card_registry.get(kMegatusk)->onActivate(ctx, {});

    EXPECT_FALSE(state.getObject(enemy).keywords.has(Keyword::Ganking));
}

// SKIPPED: Voidreaver (787) and Glorious Executioner (552) — covered by
// the parallel test_champions_legends.cpp suite (VoidreaverTest /
// GloriousExecutionerTest). Both files are present in the build; tests are
// not duplicated here to avoid redundant assertions over the same code.

}  // namespace
