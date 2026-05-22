/// @file test_targeting_and_combat_invariants.cpp
/// Spot-check tests for tricky CR areas that are easy to misread without
/// a regression test:
///   - "Targeting" (CR 463 family) — null-target fizzle when the target
///     no longer exists at resolve time. Riftbound doesn't use the word
///     "target" but the semantics still apply (CR 463.2, "if a card is
///     no longer a legal choice for the effect, the effect does nothing
///     to it"). Hidden Blade [213] is the canonical test card because
///     its rider ("its controller draws 2") depends on the target.
///   - "Damage counts up, doesn't reduce Might" (CR 422) — damage_marked
///     accumulates; base_might / current_might are unchanged by damage.
///     Riftbound has no "Willpower" stat; this is a check against the
///     common MTG-brain assumption that damage subtracts from stats.
///   - "Tank takes lethal first" (CR 460.2.c) — the sortByTankBackline
///     reorder in combatDamageStep is the engine's enforcement; verify
///     the keyword detection that drives it.

#include "card_test_fixture.h"
#include "engine/game_engine.h"

#include <gtest/gtest.h>

namespace riftbound::test {
namespace {

class TargetingAndCombatTest : public CardTestFixture {};

// ─── Targeting fizzle — Hidden Blade [213] ──────────────────────────────────

TEST_F(TargetingAndCombatTest, HiddenBlade_TargetAlreadyDead_NoDrawNoCrash) {
    // CR 463 (no-target language): if the chosen unit no longer exists at
    // resolve time, the effect does nothing — including the "its
    // controller draws 2" rider.
    BattlefieldId bf = 0;
    auto victim = addUnit(P2, /*def_id=*/kInvalidId, /*might=*/3, /*at_bf=*/bf);
    auto src = addToHand(P1, 213);  // Hidden Blade

    // Seed P2 deck so we can detect any stray draw (Hidden Blade rider
    // says "ITS controller draws 2" → victim's controller is P2).
    addToDeck(P2, kInvalidId);
    addToDeck(P2, kInvalidId);
    int p2_hand_before = handSize(P2);
    int p1_hand_before = handSize(P1);

    // Simulate the "moved away in response / killed earlier on chain"
    // case: destroy the target object before Hidden Blade resolves.
    state.objects.erase(victim);

    EffectExecutor exec(state, events, card_db);
    invokeOnResolve(src, 213, P1, {victim}, exec);

    // No draw for either player; no crash.
    EXPECT_EQ(handSize(P2), p2_hand_before);
    EXPECT_EQ(handSize(P1), p1_hand_before);
}

TEST_F(TargetingAndCombatTest, HiddenBlade_LivingTarget_KillsAndDrawsForOpponent) {
    // Sanity baseline: the rider DOES fire when the target is still
    // valid at resolve time. Without this, the previous test would
    // pass trivially (no draw = "feature working").
    BattlefieldId bf = 0;
    auto victim = addUnit(P2, /*def_id=*/kInvalidId, /*might=*/3, /*at_bf=*/bf);
    auto src = addToHand(P1, 213);
    auto deck_card_1 = addToDeck(P2, kInvalidId);
    auto deck_card_2 = addToDeck(P2, kInvalidId);
    int p2_hand_before = handSize(P2);

    EffectExecutor exec(state, events, card_db);
    invokeOnResolve(src, 213, P1, {victim}, exec);

    // P2 (the target's controller) drew 2.
    EXPECT_EQ(handSize(P2), p2_hand_before + 2);
    EXPECT_TRUE(inHand(P2, deck_card_1));
    EXPECT_TRUE(inHand(P2, deck_card_2));
    // Victim is dead / no longer on a battlefield.
    if (state.objectExists(victim)) {
        EXPECT_NE(state.getObject(victim).zone, ZoneType::BattlefieldZone);
    }
}

// ─── Damage doesn't reduce Might (CR 422) ───────────────────────────────────

TEST_F(TargetingAndCombatTest, DealDamage_DoesNotReduceBaseMightOrCurrentMight) {
    // Common MTG-brain bug: assume damage = stat reduction. In Riftbound
    // damage accumulates in damage_marked; base_might / current_might
    // are stat values that stay unchanged.
    BattlefieldId bf = 0;
    auto unit = addUnit(P1, /*def_id=*/kInvalidId, /*might=*/5, /*at_bf=*/bf);
    ASSERT_EQ(state.getObject(unit).base_might, 5);
    ASSERT_EQ(state.getObject(unit).current_might, 5);
    ASSERT_EQ(state.getObject(unit).damage_marked, 0);

    EffectExecutor exec(state, events, card_db);
    exec.dealDamage(unit, 3, kInvalidId);

    // Damage counted up; might unchanged.
    EXPECT_EQ(state.getObject(unit).damage_marked, 3);
    EXPECT_EQ(state.getObject(unit).base_might, 5);
    EXPECT_EQ(state.getObject(unit).current_might, 5);

    // Stacking damage continues to accumulate; still no stat reduction.
    exec.dealDamage(unit, 1, kInvalidId);
    EXPECT_EQ(state.getObject(unit).damage_marked, 4);
    EXPECT_EQ(state.getObject(unit).base_might, 5);
    EXPECT_EQ(state.getObject(unit).current_might, 5);
}

// ─── Tank keyword detection (the driver of CR 460.2.c ordering) ─────────────

TEST_F(TargetingAndCombatTest, MutatedMouser_HasTankKeyword) {
    // Mutated Mouser [398] is the lone in-deck Tank source across our
    // test corpus. The engine's combatDamageStep sortByTankBackline
    // lambda reads `hasKeyword(Keyword::Tank)` — verify the registry
    // load wired the keyword through so that comparator sees it.
    constexpr CardDefId kMutatedMouser = 598;
    BattlefieldId bf = 0;
    auto unit = addUnit(P1, kMutatedMouser, /*might=*/0, /*at_bf=*/bf);
    EXPECT_TRUE(state.getObject(unit).hasKeyword(Keyword::Tank))
        << "Mutated Mouser must report Tank; combat ordering depends on it";
}

TEST_F(TargetingAndCombatTest, TankAndBacklineOrdering_PartitionedAsExpected) {
    // Direct check on the partition the engine uses: when iterating a
    // unit list in tank-first / backline-last order, Tank precedes
    // non-Tank, and non-Backline precedes Backline. We build the same
    // sort the engine does to verify the contract is testable without
    // running a full combat step.
    BattlefieldId bf = 0;
    auto normal = addUnit(P1, kInvalidId, 3, bf);
    auto tank = addUnit(P1, /*def_id=*/598, /*might=*/0, /*at_bf=*/bf);  // Mouser
    auto backline = addUnit(P1, kInvalidId, 4, bf);
    state.getObject(backline).keywords.set(Keyword::Backline);

    std::vector<GameObjectId> units = {normal, backline, tank};
    std::stable_sort(units.begin(), units.end(),
        [&](GameObjectId a, GameObjectId b) {
            auto& oa = state.getObject(a);
            auto& ob = state.getObject(b);
            bool a_tank = oa.hasKeyword(Keyword::Tank);
            bool b_tank = ob.hasKeyword(Keyword::Tank);
            bool a_back = oa.hasKeyword(Keyword::Backline);
            bool b_back = ob.hasKeyword(Keyword::Backline);
            if (a_tank != b_tank) return a_tank > b_tank;
            if (a_back != b_back) return a_back < b_back;
            return false;
        });

    // Tank first, normal middle, Backline last.
    EXPECT_EQ(units[0], tank);
    EXPECT_EQ(units[1], normal);
    EXPECT_EQ(units[2], backline);
}

// ─── Hidden Blade — target moved off battlefield mid-chain (CR 463) ─────────

TEST_F(TargetingAndCombatTest, HiddenBlade_TargetMovedToBaseBeforeResolve_FizzlesNoKillNoDraw) {
    // The user-described fizzle: opponent moves the targeted unit away
    // from the battlefield (e.g. to base, via a reaction). The target
    // no longer satisfies "a unit at a battlefield" → fizzle, no kill,
    // no rider draw.
    BattlefieldId bf = 0;
    auto victim = addUnit(P2, /*def_id=*/kInvalidId, /*might=*/3, /*at_bf=*/bf);
    auto src = addToHand(P1, 213);
    addToDeck(P2, kInvalidId);
    addToDeck(P2, kInvalidId);
    int p2_hand_before = handSize(P2);

    // Simulate move-to-base reaction: victim still exists, but no longer
    // at a battlefield.
    state.getObject(victim).zone = ZoneType::Base;
    state.getObject(victim).location = BaseLocation{P2};

    EffectExecutor exec(state, events, card_db);
    invokeOnResolve(src, 213, P1, {victim}, exec);

    // Fizzle: victim still alive at base, no draw.
    EXPECT_TRUE(state.objectExists(victim));
    EXPECT_EQ(state.getObject(victim).zone, ZoneType::Base);
    EXPECT_EQ(state.getObject(victim).damage_marked, 0);
    EXPECT_EQ(handSize(P2), p2_hand_before);
}

// ─── Elder Dragon + Mutated Mouser (Shield only applies during defense) ─────
//
// User's hypothesis: "Mouser's Shield would absorb Elder Dragon's 1 dmg
// on-play, so Dragon's aura wouldn't kill it." This is a great test
// because the rules text is subtle:
//
//   Mutated Mouser: "[Shield 2] (+2 [M] while I'm a defender.)"
//
// Shield is NOT damage absorption — it's "+might while designated as
// a defender". Elder Dragon's on-play AoE happens outside any combat
// showdown, so Mouser is not a defender at that moment → Shield value
// doesn't enter current_might → Mouser takes 1 damage at base might 1
// → already lethal even before Elder Dragon's aura kicks in.

class ElderDragonShieldTest : public CardTestFixture {
protected:
    std::unique_ptr<GameEngine> engine_;
    GameState* eng_state() { return &engine_->mutableState(); }

    void makeEngine(int n_bfs = 2) {
        engine_ = std::make_unique<GameEngine>(card_db, events, card_registry);
        auto& s = *eng_state();
        s.mode = ModeOfPlay{};
        s.players[0].id = P1;
        s.players[1].id = P2;
        for (int i = 0; i < n_bfs; ++i) {
            BattlefieldState bf;
            bf.id = static_cast<int>(s.battlefields.size());
            s.battlefields.push_back(bf);
        }
    }

    // Place a Mutated Mouser at the given BF for the given player. Reads
    // the registry def so Tank + Shield 2 keywords / values are wired.
    GameObjectId placeMouser(PlayerId owner, BattlefieldId at_bf) {
        constexpr CardDefId kMouser = 598;
        auto& s = *eng_state();
        auto id = s.createObject();
        auto& obj = s.getObject(id);
        const auto& def = card_db.get(kMouser);
        obj.card_def_id = kMouser;
        obj.owner = owner;
        obj.controller = owner;
        obj.card_type = CardType::Unit;
        obj.name = def.name;
        obj.super_type = def.super_type;
        obj.keywords = def.keywords;
        obj.domains = def.domains;
        obj.base_might = def.might;
        obj.current_might = def.might;
        obj.shield_value = def.shield_value;
        obj.zone = ZoneType::BattlefieldZone;
        obj.location = BattlefieldLocation{at_bf};
        return id;
    }
};

TEST_F(ElderDragonShieldTest, MouserNotDefender_ShieldDoesNotApplyToCurrentMight) {
    // Sanity check the precondition: a Mutated Mouser sitting on a BF
    // with no combat designation has current_might == base_might (1),
    // ignoring the Shield 2 bonus. Establishes that Shield is gated on
    // combat_designation == Defender.
    makeEngine();
    auto& s = *eng_state();
    BattlefieldId bf0 = s.battlefields[0].id;
    auto mouser = placeMouser(P2, bf0);
    s.getObject(mouser).recomputeMight();

    EXPECT_EQ(s.getObject(mouser).base_might, 1);
    EXPECT_EQ(s.getObject(mouser).shield_value, 2);
    EXPECT_EQ(s.getObject(mouser).current_might, 1)
        << "Shield must not contribute to current_might outside combat";
    EXPECT_EQ(s.getObject(mouser).combat_designation, CombatDesignation::None);
}

TEST_F(ElderDragonShieldTest, MouserAsDefender_ShieldContributesToCurrentMight) {
    // Contrast: designate Mouser as Defender (the only state in which
    // Shield contributes), recompute, current_might jumps to base + 2.
    makeEngine();
    auto& s = *eng_state();
    BattlefieldId bf0 = s.battlefields[0].id;
    auto mouser = placeMouser(P2, bf0);
    s.getObject(mouser).combat_designation = CombatDesignation::Defender;
    s.getObject(mouser).recomputeMight();
    EXPECT_EQ(s.getObject(mouser).current_might, 1 + 2);
}

TEST_F(ElderDragonShieldTest,
       ElderDragonOnPlayAoE_MouserDiesFromNaturalLethal_ShieldGivesNoProtection) {
    // The core question: Elder Dragon's on-play AoE deals 1 damage to
    // one enemy at each battlefield. Mouser at base might 1, not a
    // defender, takes 1 damage. damage_marked >= current_might → killed
    // by natural lethal in the trigger itself. Shield doesn't engage.
    makeEngine();
    auto& s = *eng_state();
    BattlefieldId bf0 = s.battlefields[0].id;
    auto mouser = placeMouser(P2, bf0);

    // Place Elder Dragon on P1's board and emit its on-play trigger.
    // We don't go through the full "play card" path (cost / chain /
    // resolve) — we invoke the trigger directly via card_registry,
    // which is the same dispatch GameEngine uses internally.
    constexpr CardDefId kElderDragon = 680;
    auto dragon = s.createObject();
    auto& dobj = s.getObject(dragon);
    const auto& def = card_db.get(kElderDragon);
    dobj.card_def_id = kElderDragon;
    dobj.owner = P1;
    dobj.controller = P1;
    dobj.card_type = CardType::Unit;
    dobj.name = def.name;
    dobj.base_might = def.might;
    dobj.current_might = def.might;
    dobj.zone = ZoneType::Base;
    dobj.location = BaseLocation{P1};

    EffectExecutor exec(s, events, card_db);
    Card* card = card_registry.get(kElderDragon);
    ASSERT_NE(card, nullptr);
    CardContext ctx{s, events, exec, P1, dragon};
    card->onTrigger(ctx, {});

    // Mouser is dead (in trash) — natural lethal at 1/1.
    EXPECT_FALSE(s.objectExists(mouser) &&
                 s.getObject(mouser).zone == ZoneType::BattlefieldZone)
        << "Mouser at base might 1 takes 1 dmg → natural lethal → killed";
}

TEST_F(ElderDragonShieldTest,
       BuffedMouser_OnPlayAoE_SurvivesTriggerButDiesInCleanupViaAura) {
    // Sharper test of the aura: give Mouser +1 buff so base might is 1
    // but current_might becomes 2. Now 1 damage is NOT natural lethal.
    // The trigger leaves Mouser at damage_marked=1. processLethalDamage
    // (the engine's per-state cleanup pass) then sees Elder Dragon
    // controlled by P1 and damage_marked>0 on a P2 unit → applies the
    // "any damage kills" rule → Mouser dies.
    makeEngine();
    auto& s = *eng_state();
    BattlefieldId bf0 = s.battlefields[0].id;
    auto mouser = placeMouser(P2, bf0);
    s.getObject(mouser).buff_count = 1;
    s.getObject(mouser).recomputeMight();
    ASSERT_EQ(s.getObject(mouser).current_might, 2);

    constexpr CardDefId kElderDragon = 680;
    auto dragon = s.createObject();
    auto& dobj = s.getObject(dragon);
    const auto& def = card_db.get(kElderDragon);
    dobj.card_def_id = kElderDragon;
    dobj.owner = P1;
    dobj.controller = P1;
    dobj.card_type = CardType::Unit;
    dobj.name = def.name;
    dobj.base_might = def.might;
    dobj.current_might = def.might;
    dobj.zone = ZoneType::Base;
    dobj.location = BaseLocation{P1};

    EffectExecutor exec(s, events, card_db);
    Card* card = card_registry.get(kElderDragon);
    CardContext ctx{s, events, exec, P1, dragon};
    card->onTrigger(ctx, {});

    // After trigger, Mouser is still alive on the BF (not natural lethal):
    ASSERT_TRUE(s.objectExists(mouser));
    EXPECT_EQ(s.getObject(mouser).zone, ZoneType::BattlefieldZone);
    EXPECT_EQ(s.getObject(mouser).damage_marked, 1);

    // Cleanup pass picks up Elder Dragon's aura rule.
    engine_->testHook_processLethalDamage();

    // Mouser dies via Elder Dragon's "any damage kills" — Shield still
    // doesn't matter (Mouser wasn't a Defender at any point).
    EXPECT_FALSE(s.objectExists(mouser) &&
                 s.getObject(mouser).zone == ZoneType::BattlefieldZone);
}

}  // namespace
}  // namespace riftbound::test
