/// @file test_wave_a_batch2.cpp
/// Per-card unit tests for Wave A batch 2 cards. Each TEST_F drives the card's
/// implemented ability through the shared CardTestFixture and asserts the
/// resulting state change. Cards whose printed ability could not be wired
/// (missing engine primitive) are escalated in the implementation files and
/// have no behavioral test here.

#include "tests/cards/card_test_fixture.h"

namespace riftbound::test {

namespace {
constexpr CardDefId kCithriaOfCloudfield = 139;
constexpr CardDefId kTaricProtector      = 74;
constexpr CardDefId kRagingSoul          = 19;
constexpr CardDefId kRecruitTheVanguard  = 313;
constexpr CardDefId kGold564             = 564;
constexpr CardDefId kRhasaTheSunderer    = 195;
constexpr CardDefId kEagerDrakehound     = 329;
constexpr CardDefId kRuinRunner          = 427;
constexpr CardDefId kUndyingLegion       = 587;
}  // namespace

// ═══════════════════════════════════════════════════════════════════════════
// Cithria of Cloudfield (139) — "When you play another unit, buff me."
// ═══════════════════════════════════════════════════════════════════════════

class CithriaTest : public CardTestFixture {};

TEST_F(CithriaTest, BuffsSelfWhenUnbuffed) {
    auto cithria = addUnit(P1, kCithriaOfCloudfield, /*might=*/1, /*at_bf=*/0);
    EXPECT_EQ(state.getObject(cithria).buff_count, 0);

    EffectExecutor exec(state, events, card_db, &card_registry);
    CardContext ctx{state, events, exec, P1, cithria};
    card_registry.get(kCithriaOfCloudfield)->onTrigger(ctx, {});

    EXPECT_EQ(state.getObject(cithria).buff_count, 1) << "first unit played -> +1 buff";
    EXPECT_EQ(state.getObject(cithria).current_might, 2);
}

TEST_F(CithriaTest, NoSecondBuffWhenAlreadyBuffed) {
    auto cithria = addUnit(P1, kCithriaOfCloudfield, /*might=*/1, /*at_bf=*/0);
    state.getObject(cithria).buff_count = 1;
    state.getObject(cithria).recomputeMight();

    EffectExecutor exec(state, events, card_db, &card_registry);
    CardContext ctx{state, events, exec, P1, cithria};
    card_registry.get(kCithriaOfCloudfield)->onTrigger(ctx, {});

    EXPECT_EQ(state.getObject(cithria).buff_count, 1)
        << "'If I don't have a buff' — already buffed, no additional buff";
}

// ═══════════════════════════════════════════════════════════════════════════
// Taric, Protector (74) — "Other friendly units here have [Shield]."
// ═══════════════════════════════════════════════════════════════════════════

class TaricTest : public CardTestFixture {};

TEST_F(TaricTest, GrantsShieldToOtherFriendlyUnitsHere) {
    auto taric = addUnit(P1, kTaricProtector, /*might=*/4, /*at_bf=*/0);
    auto ally  = addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/0);   // same BF

    card_registry.get(kTaricProtector)->applyPassiveAura(state, P1);

    bool ally_has_shield = false;
    for (const auto& ae : state.getObject(ally).aura_effects)
        if (ae.keyword == Keyword::Shield) ally_has_shield = true;
    EXPECT_TRUE(ally_has_shield) << "ally at same BF gains Shield aura";

    // "Other" — Taric does not grant Shield to itself via this aura.
    bool taric_self_shield = false;
    for (const auto& ae : state.getObject(taric).aura_effects)
        if (ae.keyword == Keyword::Shield) taric_self_shield = true;
    EXPECT_FALSE(taric_self_shield) << "aura is 'other' units only";
}

TEST_F(TaricTest, NoShieldForUnitsElsewhereOrEnemies) {
    addUnit(P1, kTaricProtector, /*might=*/4, /*at_bf=*/0);
    auto far_ally = addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/1);   // different BF
    auto enemy    = addUnit(P2, kInvalidId, /*might=*/3, /*at_bf=*/0);   // same BF, enemy

    card_registry.get(kTaricProtector)->applyPassiveAura(state, P1);

    EXPECT_TRUE(state.getObject(far_ally).aura_effects.empty())
        << "unit at a different battlefield gets no Shield";
    EXPECT_TRUE(state.getObject(enemy).aura_effects.empty())
        << "enemy unit gets no Shield";
}

// ═══════════════════════════════════════════════════════════════════════════
// Raging Soul (19) — "If you've discarded a card this turn, I have
//                     [Assault] and [Ganking]."
// ═══════════════════════════════════════════════════════════════════════════

class RagingSoulTest : public CardTestFixture {};

TEST_F(RagingSoulTest, NoKeywordsWithoutDiscard) {
    auto soul = addUnit(P1, kRagingSoul, /*might=*/4, /*at_bf=*/0);
    state.player(P1).has_discarded_this_turn = false;

    card_registry.get(kRagingSoul)->applyPassiveAura(state, P1);

    EXPECT_TRUE(state.getObject(soul).aura_effects.empty())
        << "no discard this turn -> no Assault/Ganking grant";
    // Base def must NOT print the keywords unconditionally.
    const auto& def = card_db.get(kRagingSoul);
    EXPECT_FALSE(def.keywords.has(Keyword::Assault))
        << "Assault must be conditional, not a base keyword";
    EXPECT_FALSE(def.keywords.has(Keyword::Ganking))
        << "Ganking must be conditional, not a base keyword";
}

TEST_F(RagingSoulTest, GrantsKeywordsAfterDiscard) {
    auto soul = addUnit(P1, kRagingSoul, /*might=*/4, /*at_bf=*/0);
    state.player(P1).has_discarded_this_turn = true;

    card_registry.get(kRagingSoul)->applyPassiveAura(state, P1);

    bool has_assault = false, has_ganking = false;
    for (const auto& ae : state.getObject(soul).aura_effects) {
        if (ae.keyword == Keyword::Assault) has_assault = true;
        if (ae.keyword == Keyword::Ganking) has_ganking = true;
    }
    EXPECT_TRUE(has_assault) << "discarded this turn -> Assault granted";
    EXPECT_TRUE(has_ganking) << "discarded this turn -> Ganking granted";
}

TEST_F(RagingSoulTest, AssaultAddsMightWhileAttacking) {
    auto soul = addUnit(P1, kRagingSoul, /*might=*/4, /*at_bf=*/0);
    state.player(P1).has_discarded_this_turn = true;
    state.getObject(soul).combat_designation = CombatDesignation::Attacker;

    card_registry.get(kRagingSoul)->applyPassiveAura(state, P1);

    int aura_might = 0;
    for (const auto& ae : state.getObject(soul).aura_effects) aura_might += ae.might_bonus;
    EXPECT_EQ(aura_might, 1) << "Assault confers +1 [M] while attacking";
}

// ═══════════════════════════════════════════════════════════════════════════
// Recruit the Vanguard (313) — "Play four 1 [M] Recruit unit tokens."
// ═══════════════════════════════════════════════════════════════════════════

class RecruitVanguardTest : public CardTestFixture {};

TEST_F(RecruitVanguardTest, CreatesFourRecruitTokens) {
    auto src = state.createObject();
    state.getObject(src).controller = P1;

    EffectExecutor exec(state, events, card_db, &card_registry);
    invokeOnResolve(src, kRecruitTheVanguard, P1, {}, exec);

    int recruits = 0;
    for (auto& [id, obj] : state.objects) {
        if (obj.isUnit() && obj.controller == P1 && obj.name == "Recruit") {
            ++recruits;
            EXPECT_EQ(obj.current_might, 1) << "Recruit token is 1 [M]";
            EXPECT_TRUE(obj.isAtBase()) << "free token play defaults to base";
        }
    }
    EXPECT_EQ(recruits, 4) << "exactly four Recruit tokens created";
}

// ═══════════════════════════════════════════════════════════════════════════
// Gold (564) — "[Reaction] [>] Kill this, [E]: [Add] [A]."
// ═══════════════════════════════════════════════════════════════════════════

class Gold564Test : public CardTestFixture {};

TEST_F(Gold564Test, KillSelfAddsOneUniversalPower) {
    // Place a Gold gear token on the board.
    auto gold = state.createObject();
    {
        auto& g = state.getObject(gold);
        g.owner = P1; g.controller = P1;
        g.card_def_id = kGold564;
        g.card_type = CardType::Gear;
        g.super_type = SuperType::Token;
        g.zone = ZoneType::Base;
        g.location = BaseLocation{P1};
    }
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 0);

    EffectExecutor exec(state, events, card_db, &card_registry);
    CardContext ctx{state, events, exec, P1, gold};
    card_registry.get(kGold564)->onActivate(ctx, {});

    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 1)
        << "[Add] [A] floats 1 universal power";
    // Token ceased to exist (CR 183.1) — no longer on board.
    EXPECT_FALSE(state.objectExists(gold) &&
                 state.getObject(gold).location.has_value())
        << "Gold token killed itself off the board";
}

TEST_F(Gold564Test, AbilityIsReactionTiming) {
    Card* c = card_registry.get(kGold564);
    EXPECT_TRUE(c->hasActivatedAbility());
    EXPECT_TRUE(c->isReactionAbility()) << "Gold's ability is [Reaction]";
    EXPECT_TRUE(c->getActivationCost().exhaust) << "[E] cost present";
}

// ═══════════════════════════════════════════════════════════════════════════
// Rhasa the Sunderer (195) — "I cost [1] less for each card in your trash."
// ═══════════════════════════════════════════════════════════════════════════

class RhasaTest : public CardTestFixture {};

TEST_F(RhasaTest, CostReductionScalesWithTrashSize) {
    Card* c = card_registry.get(kRhasaTheSunderer);
    EXPECT_EQ(c->selfCostReduction(state, P1), 0) << "empty trash -> no reduction";

    // Put 3 cards into P1's trash.
    for (int i = 0; i < 3; ++i) {
        auto id = state.createObject();
        state.getObject(id).owner = P1;
        state.getObject(id).zone = ZoneType::Trash;
        state.player(P1).trash.push_back(id);
    }
    EXPECT_EQ(c->selfCostReduction(state, P1), 3)
        << "[1] less per card in trash";
}

// ═══════════════════════════════════════════════════════════════════════════
// Eager Drakehound (329) — "I enter ready."
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(CardTestFixture, EagerDrakehoundEntersReady) {
    EXPECT_TRUE(card_registry.get(kEagerDrakehound)->entersReadyOnPlay())
        << "Eager Drakehound enters ready";
}

// ═══════════════════════════════════════════════════════════════════════════
// Ruin Runner (427) — "I can't be chosen by enemy spells and abilities."
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(CardTestFixture, RuinRunnerCannotBeChosenByEnemy) {
    EXPECT_FALSE(card_registry.get(kRuinRunner)->canBeChosenByEnemy())
        << "Ruin Runner can't be chosen by enemy spells/abilities";
}

// ═══════════════════════════════════════════════════════════════════════════
// Undying Legion (587) — "[Legion] [>] You may play me from your trash ..."
// Only the Legion gate is implementable from the card layer; the
// play-from-trash alternative source is escalated.
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(CardTestFixture, UndyingLegionRequiresLegion) {
    EXPECT_TRUE(card_registry.get(kUndyingLegion)->requiresLegion())
        << "Undying Legion's effect is Legion-gated";
}

}  // namespace riftbound::test
