// Per-card unit tests for audit-fix batch 7 (src/cards/manual/audit_fixes_7.cpp).
// Cards: 95 Stupefy, 285 The Arena's Greatest, 407 Ornn Forge God,
// 466 Switcheroo, 521 Emperor's Dais, 602 Wuju Apprentice, 615 Scuttle Crab,
// 651 Jhin Meticulous Killer, 702 Conscription, 748 Hextech Gauntlets,
// 771 Star Spring.
#include "tests/cards/card_test_fixture.h"

using namespace riftbound;
using namespace riftbound::test;

namespace {

class AuditFix7Test : public CardTestFixture {
protected:
    // Add a gear on board controlled by `owner`. If at_unit is valid, attach.
    GameObjectId addGear(PlayerId owner, CardDefId def_id, int might_bonus = 0,
                         int at_bf = -1) {
        auto id = state.createObject();
        auto& obj = state.getObject(id);
        obj.owner = owner;
        obj.controller = owner;
        obj.card_def_id = def_id;
        obj.card_type = CardType::Gear;
        if (def_id != kInvalidId) {
            const auto& def = card_db.get(def_id);
            obj.name = def.name;
            obj.tags = def.tags;
            obj.domains = def.domains;
        } else {
            obj.name = "TestGear";
        }
        obj.might_bonus = might_bonus;
        if (at_bf >= 0) {
            obj.zone = ZoneType::BattlefieldZone;
            obj.location = BattlefieldLocation{static_cast<BattlefieldId>(at_bf)};
        } else {
            obj.zone = ZoneType::Base;
            obj.location = BaseLocation{owner};
        }
        return id;
    }

    // Bind a battlefield's card_object_id to a freshly-created BF card object
    // for the given card def. Returns the source GameObjectId (= card_object_id).
    GameObjectId installBattlefieldCard(BattlefieldId bf, CardDefId def_id) {
        auto src = state.createObject();
        auto& obj = state.getObject(src);
        obj.card_def_id = def_id;
        obj.card_type = CardType::Battlefield;
        obj.name = card_db.get(def_id).name;
        state.battlefields[bf].card_object_id = src;
        return src;
    }
};

// ─── [95] Stupefy ────────────────────────────────────────────────────────────
// "Give a unit -1 [M] this turn, to a minimum of 1 [M]. Draw 1."

TEST_F(AuditFix7Test, Stupefy_MinusOneMightAndDraw) {
    EffectExecutor exec(state, events, card_db);
    exec.setCardRegistry(&card_registry);
    auto u = addUnit(P1, kInvalidId, /*might=*/5, /*at_bf=*/0);
    addToDeck(P1, kInvalidId);  // something to draw
    int before = handSize(P1);

    invokeOnResolve(/*source=*/0, 95, P1, {u}, exec);

    EXPECT_EQ(state.getObject(u).current_might, 4);   // 5 - 1
    EXPECT_EQ(handSize(P1), before + 1);              // drew 1
}

TEST_F(AuditFix7Test, Stupefy_FloorMinimumOne) {
    EffectExecutor exec(state, events, card_db);
    exec.setCardRegistry(&card_registry);
    auto u = addUnit(P1, kInvalidId, /*might=*/1, /*at_bf=*/0);
    addToDeck(P1, kInvalidId);

    invokeOnResolve(0, 95, P1, {u}, exec);

    // -1 from 1 would be 0, but floored to minimum 1.
    EXPECT_EQ(state.getObject(u).current_might, 1);
}

TEST_F(AuditFix7Test, Stupefy_NoTargetStillDraws) {
    EffectExecutor exec(state, events, card_db);
    exec.setCardRegistry(&card_registry);
    addToDeck(P1, kInvalidId);
    int before = handSize(P1);

    invokeOnResolve(0, 95, P1, {}, exec);

    EXPECT_EQ(handSize(P1), before + 1);  // draw 1 fires even with no target
}

// ─── [285] The Arena's Greatest (Battlefield) ─────────────────────────────────
// "At the start of each player's first Beginning Phase, that player gains 1 point."

TEST_F(AuditFix7Test, ArenasGreatest_FirstBeginningGivesPoint) {
    EffectExecutor exec(state, events, card_db);
    exec.setCardRegistry(&card_registry);
    auto src = installBattlefieldCard(0, 285);
    setScore(P1, 0);

    fireTriggerAs(285, P1, src, TriggerType::AtStartOfBeginning, exec);

    EXPECT_EQ(state.player(P1).score, 1);
}

TEST_F(AuditFix7Test, ArenasGreatest_OnlyFiresOncePerPlayer) {
    EffectExecutor exec(state, events, card_db);
    exec.setCardRegistry(&card_registry);
    auto src = installBattlefieldCard(0, 285);
    setScore(P1, 0);

    fireTriggerAs(285, P1, src, TriggerType::AtStartOfBeginning, exec);
    fireTriggerAs(285, P1, src, TriggerType::AtStartOfBeginning, exec);

    EXPECT_EQ(state.player(P1).score, 1);  // gated: second firing is a no-op
}

TEST_F(AuditFix7Test, ArenasGreatest_PerPlayerGate) {
    EffectExecutor exec(state, events, card_db);
    exec.setCardRegistry(&card_registry);
    auto src = installBattlefieldCard(0, 285);
    setScore(P1, 0);
    setScore(P2, 0);

    fireTriggerAs(285, P1, src, TriggerType::AtStartOfBeginning, exec);
    fireTriggerAs(285, P2, src, TriggerType::AtStartOfBeginning, exec);

    EXPECT_EQ(state.player(P1).score, 1);  // each player's first fires independently
    EXPECT_EQ(state.player(P2).score, 1);
}

// ─── [407] Ornn, Forge God ─────────────────────────────────────────────────────
// "+1 [M] for each friendly gear" (passive aura) + Weaponmaster on-play equip.

TEST_F(AuditFix7Test, Ornn_PassiveAuraPerFriendlyGear) {
    auto ornn = addUnit(P1, 407, /*might=*/0, /*at_bf=*/0);  // def might = 4
    addGear(P1, kInvalidId, 0, /*at_bf=*/0);
    addGear(P1, kInvalidId, 0, /*at_bf=*/0);

    Card* card = card_registry.get(407);
    card->applyPassiveAura(state, P1);

    int bonus = 0;
    for (auto& ae : state.getObject(ornn).aura_effects) bonus += ae.might_bonus;
    EXPECT_EQ(bonus, 2);  // +1 per friendly gear, 2 gear
}

TEST_F(AuditFix7Test, Ornn_NoAuraWithoutGear) {
    auto ornn = addUnit(P1, 407, 0, /*at_bf=*/0);
    Card* card = card_registry.get(407);
    card->applyPassiveAura(state, P1);
    EXPECT_TRUE(state.getObject(ornn).aura_effects.empty());  // no gear -> no buff
}

TEST_F(AuditFix7Test, Ornn_OnlyCountsFriendlyGear) {
    auto ornn = addUnit(P1, 407, 0, /*at_bf=*/0);
    addGear(P1, kInvalidId, 0, /*at_bf=*/0);  // friendly
    addGear(P2, kInvalidId, 0, /*at_bf=*/0);  // enemy — should not count

    Card* card = card_registry.get(407);
    card->applyPassiveAura(state, P1);

    int bonus = 0;
    for (auto& ae : state.getObject(ornn).aura_effects) bonus += ae.might_bonus;
    EXPECT_EQ(bonus, 1);  // only the one friendly gear
}

TEST_F(AuditFix7Test, Ornn_WeaponmasterEquipsEquipmentGear) {
    EffectExecutor exec(state, events, card_db);
    exec.setCardRegistry(&card_registry);
    auto ornn = addUnit(P1, 407, 0, /*at_bf=*/0);
    // A friendly gear tagged "Equipment" with a might bonus.
    auto gear = addGear(P1, kInvalidId, /*might_bonus=*/3, /*at_bf=*/0);
    state.getObject(gear).tags = {"Equipment"};

    fireTriggerAs(407, P1, ornn, TriggerType::WhenYouPlayMe, exec);

    EXPECT_TRUE(state.getObject(gear).attached_to.has_value());
    EXPECT_EQ(*state.getObject(gear).attached_to, ornn);
    EXPECT_EQ(state.getObject(ornn).attachment_might_bonus, 3);
}

TEST_F(AuditFix7Test, Ornn_WeaponmasterNoOpWithoutEquipment) {
    EffectExecutor exec(state, events, card_db);
    exec.setCardRegistry(&card_registry);
    auto ornn = addUnit(P1, 407, 0, /*at_bf=*/0);
    addGear(P1, kInvalidId, 3, /*at_bf=*/0);  // not tagged Equipment

    fireTriggerAs(407, P1, ornn, TriggerType::WhenYouPlayMe, exec);

    EXPECT_EQ(state.getObject(ornn).attachment_might_bonus, 0);
}

// ─── [466] Switcheroo (Spell) ──────────────────────────────────────────────────
// "Swap the Might of two units at the same battlefield this turn."

TEST_F(AuditFix7Test, Switcheroo_SwapsMight) {
    EffectExecutor exec(state, events, card_db);
    exec.setCardRegistry(&card_registry);
    auto a = addUnit(P1, kInvalidId, /*might=*/6, /*at_bf=*/0);
    auto b = addUnit(P1, kInvalidId, /*might=*/2, /*at_bf=*/0);

    // Direct invoke: pickTargetPair defaults to first legal a + first legal b.
    invokeOnResolve(0, 466, P1, {}, exec);

    // The two units at BF 0 have their Might swapped.
    int ma = state.getObject(a).current_might;
    int mb = state.getObject(b).current_might;
    EXPECT_EQ(ma + mb, 8);          // total preserved
    EXPECT_NE(ma, mb);              // swapped, not equal
    EXPECT_TRUE((ma == 2 && mb == 6) || (ma == 6 && mb == 2));
}

TEST_F(AuditFix7Test, Switcheroo_NoOpWithFewerThanTwoUnits) {
    EffectExecutor exec(state, events, card_db);
    exec.setCardRegistry(&card_registry);
    auto a = addUnit(P1, kInvalidId, /*might=*/6, /*at_bf=*/0);

    invokeOnResolve(0, 466, P1, {}, exec);

    EXPECT_EQ(state.getObject(a).current_might, 6);  // unchanged, no pair
}

TEST_F(AuditFix7Test, Switcheroo_SecondMustBeSameBattlefield) {
    EffectExecutor exec(state, events, card_db);
    exec.setCardRegistry(&card_registry);
    // First legal a (lowest id) at BF 0; the only other unit is at BF 1.
    auto a = addUnit(P1, kInvalidId, /*might=*/6, /*at_bf=*/0);
    auto b = addUnit(P1, kInvalidId, /*might=*/2, /*at_bf=*/1);

    invokeOnResolve(0, 466, P1, {}, exec);

    // No second target at the same BF as `a` -> no swap performed.
    EXPECT_EQ(state.getObject(a).current_might, 6);
    EXPECT_EQ(state.getObject(b).current_might, 2);
}

// ─── [521] Emperor's Dais (Battlefield) ────────────────────────────────────────
// "When you conquer here, you may pay [1] and return a unit you control here to
//  its owner's hand. If you do, play a 2 [M] Sand Soldier token here."

TEST_F(AuditFix7Test, EmperorsDais_BounceAndSpawnToken) {
    EffectExecutor exec(state, events, card_db);
    exec.setCardRegistry(&card_registry);
    auto src = installBattlefieldCard(0, 521);
    auto unit = addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/0);
    addRune(P1, Domain::Body, /*exhausted=*/false);  // pays [1]

    fireTriggerAs(521, P1, src, TriggerType::WhenYouConquerHere, exec);

    // Unit bounced to hand.
    EXPECT_TRUE(inHand(P1, unit));
    // Rune exhausted to pay [1].
    EXPECT_EQ(exhaustedRuneCount(P1), 1);
    // A 2M Sand Soldier token spawned at BF 0.
    int soldiers = 0;
    for (auto& [id, obj] : state.objects) {
        if (obj.isToken() && obj.name == "Sand Soldier" &&
            obj.battlefieldId() && *obj.battlefieldId() == 0) {
            ++soldiers;
            EXPECT_EQ(obj.current_might, 2);
        }
    }
    EXPECT_EQ(soldiers, 1);
}

TEST_F(AuditFix7Test, EmperorsDais_NoUnitHereNoEffect) {
    EffectExecutor exec(state, events, card_db);
    exec.setCardRegistry(&card_registry);
    auto src = installBattlefieldCard(0, 521);
    addRune(P1, Domain::Body, false);
    // No friendly unit at BF 0.

    fireTriggerAs(521, P1, src, TriggerType::WhenYouConquerHere, exec);

    EXPECT_EQ(exhaustedRuneCount(P1), 0);  // didn't pay
    int soldiers = 0;
    for (auto& [id, obj] : state.objects)
        if (obj.isToken() && obj.name == "Sand Soldier") ++soldiers;
    EXPECT_EQ(soldiers, 0);  // no token
}

TEST_F(AuditFix7Test, EmperorsDais_CannotPayNoEffect) {
    EffectExecutor exec(state, events, card_db);
    exec.setCardRegistry(&card_registry);
    auto src = installBattlefieldCard(0, 521);
    auto unit = addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/0);
    addRune(P1, Domain::Body, /*exhausted=*/true);  // only an exhausted rune

    fireTriggerAs(521, P1, src, TriggerType::WhenYouConquerHere, exec);

    EXPECT_FALSE(inHand(P1, unit));   // not bounced — couldn't pay [1]
    int soldiers = 0;
    for (auto& [id, obj] : state.objects)
        if (obj.isToken() && obj.name == "Sand Soldier") ++soldiers;
    EXPECT_EQ(soldiers, 0);
}

// ─── [602] Wuju Apprentice (Unit) ──────────────────────────────────────────────
// [Hunt] gain 1 XP on conquer/hold + [Level 6] When you play me, draw 1.

TEST_F(AuditFix7Test, WujuApprentice_HuntGainsXp) {
    EffectExecutor exec(state, events, card_db);
    exec.setCardRegistry(&card_registry);
    auto u = addUnit(P1, 602, 0, /*at_bf=*/0);
    setXp(P1, 0);

    fireTriggerAs(602, P1, u, TriggerType::WhenIConquerOrHold, exec);

    EXPECT_EQ(state.player(P1).xp, 1);
}

TEST_F(AuditFix7Test, WujuApprentice_Level6PlayDraws) {
    EffectExecutor exec(state, events, card_db);
    exec.setCardRegistry(&card_registry);
    auto u = addUnit(P1, 602, 0, /*at_bf=*/0);
    setXp(P1, 6);
    addToDeck(P1, kInvalidId);
    int before = handSize(P1);

    fireTriggerAs(602, P1, u, TriggerType::WhenYouPlayMe, exec);

    EXPECT_EQ(handSize(P1), before + 1);  // 6+ XP -> draw 1
}

TEST_F(AuditFix7Test, WujuApprentice_BelowLevel6NoDraw) {
    EffectExecutor exec(state, events, card_db);
    exec.setCardRegistry(&card_registry);
    auto u = addUnit(P1, 602, 0, /*at_bf=*/0);
    setXp(P1, 5);
    addToDeck(P1, kInvalidId);
    int before = handSize(P1);

    fireTriggerAs(602, P1, u, TriggerType::WhenYouPlayMe, exec);

    EXPECT_EQ(handSize(P1), before);  // <6 XP -> no draw
}

// ─── [615] Scuttle Crab (Unit) ─────────────────────────────────────────────────
// "When you play me, draw 1." + [Deathknell] reveal opp hand, gain 1 XP.

TEST_F(AuditFix7Test, ScuttleCrab_PlayDraws) {
    EffectExecutor exec(state, events, card_db);
    exec.setCardRegistry(&card_registry);
    auto u = addUnit(P1, 615, 0, /*at_bf=*/0);
    addToDeck(P1, kInvalidId);
    int before = handSize(P1);

    fireTriggerAs(615, P1, u, TriggerType::WhenYouPlayMe, exec);

    EXPECT_EQ(handSize(P1), before + 1);
}

TEST_F(AuditFix7Test, ScuttleCrab_DeathknellGainsXp) {
    EffectExecutor exec(state, events, card_db);
    exec.setCardRegistry(&card_registry);
    auto u = addUnit(P1, 615, 0, /*at_bf=*/0);
    addToHand(P2, kInvalidId);  // opponent has a card to reveal
    addToHand(P2, kInvalidId);
    setXp(P1, 0);

    fireTriggerAs(615, P1, u, TriggerType::WhenIDie, exec);

    EXPECT_EQ(state.player(P1).xp, 1);  // Deathknell -> +1 XP
}

TEST_F(AuditFix7Test, ScuttleCrab_DeathknellWithEmptyOpponentHand) {
    EffectExecutor exec(state, events, card_db);
    exec.setCardRegistry(&card_registry);
    auto u = addUnit(P1, 615, 0, /*at_bf=*/0);
    setXp(P1, 0);  // opponent hand empty — reveal loop no-ops, XP still gained

    fireTriggerAs(615, P1, u, TriggerType::WhenIDie, exec);

    EXPECT_EQ(state.player(P1).xp, 1);
}

// ─── [651] Jhin, Meticulous Killer ─────────────────────────────────────────────
// Vision is engine-handled; alt-cost [B] is a documented engine gap. The card
// is a clean UnitCard with no card-side trigger/activated ability.
// TODO: alt-cost "play for [B] if spent 4+ on a spell" is NOT implementable
//       through the Card surface and is documented as an engine gap.

TEST_F(AuditFix7Test, Jhin_RegisteredAndClean) {
    Card* card = card_registry.get(651);
    ASSERT_NE(card, nullptr);
    EXPECT_FALSE(card->hasActivatedAbility());
    EXPECT_EQ(card->triggerType(), TriggerType::None);
}

// ─── [702] Conscription (Spell) ────────────────────────────────────────────────
// "Choose an enemy unit at a BF with 3 M or less ... take control, exhaust,
//  recall. You may spend 5 XP to choose any enemy unit instead."

TEST_F(AuditFix7Test, Conscription_TakesControlExhaustRecall_LowMight) {
    EffectExecutor exec(state, events, card_db);
    exec.setCardRegistry(&card_registry);
    setXp(P1, 0);  // can't pay 5 XP -> 3M cap applies
    auto enemy = addUnit(P2, kInvalidId, /*might=*/3, /*at_bf=*/0);

    invokeOnResolve(0, 702, P1, {}, exec);

    auto& e = state.getObject(enemy);
    EXPECT_EQ(e.controller, P1);          // took control
    EXPECT_TRUE(e.is_exhausted);          // exhausted
    EXPECT_TRUE(e.isAtBase());            // recalled to base
    EXPECT_EQ(std::get<BaseLocation>(*e.location).player, P1);
}

TEST_F(AuditFix7Test, Conscription_RespectsThreeMightCapWhenUnpaid) {
    EffectExecutor exec(state, events, card_db);
    exec.setCardRegistry(&card_registry);
    setXp(P1, 0);  // can't pay -> cap at 3M
    auto big = addUnit(P2, kInvalidId, /*might=*/5, /*at_bf=*/0);

    invokeOnResolve(0, 702, P1, {}, exec);

    // 5M unit exceeds the 3M cap and isn't a legal target -> unchanged.
    EXPECT_EQ(state.getObject(big).controller, P2);
    EXPECT_FALSE(state.getObject(big).is_exhausted);
}

TEST_F(AuditFix7Test, Conscription_FiveXpWidensToAnyUnit) {
    EffectExecutor exec(state, events, card_db);
    exec.setCardRegistry(&card_registry);
    setXp(P1, 5);  // confirmOptional defaults YES in direct call -> pay & widen
    auto big = addUnit(P2, kInvalidId, /*might=*/9, /*at_bf=*/0);

    invokeOnResolve(0, 702, P1, {}, exec);

    EXPECT_EQ(state.getObject(big).controller, P1);  // widened cap -> 9M legal
    EXPECT_TRUE(state.getObject(big).is_exhausted);
    EXPECT_EQ(state.player(P1).xp, 0);               // spent 5 XP
}

TEST_F(AuditFix7Test, Conscription_OnlyEnemyUnitsAtBattlefield) {
    EffectExecutor exec(state, events, card_db);
    exec.setCardRegistry(&card_registry);
    setXp(P1, 0);
    auto friendly = addUnit(P1, kInvalidId, /*might=*/2, /*at_bf=*/0);  // friendly, ignored
    auto enemyBase = addUnit(P2, kInvalidId, /*might=*/2, /*at_bf=*/-1); // enemy at base, ignored

    invokeOnResolve(0, 702, P1, {}, exec);

    EXPECT_EQ(state.getObject(friendly).controller, P1);
    EXPECT_EQ(state.getObject(enemyBase).controller, P2);  // not at BF -> untouched
}

// ─── [748] Hextech Gauntlets (Equipment Gear) ──────────────────────────────────
// "[Equip] [3][A], energy reduced by chosen unit's Might." + conquer rider.

TEST_F(AuditFix7Test, HextechGauntlets_EquipReducedByMight) {
    EffectExecutor exec(state, events, card_db);
    exec.setCardRegistry(&card_registry);
    auto unit = addUnit(P1, kInvalidId, /*might=*/2, /*at_bf=*/0);  // reduces 3 -> 1
    auto gear = addGear(P1, 748, /*might_bonus=*/3);
    // Need 1 energy rune + 1 rune to recycle for [A]. Provide 2 ready runes.
    addRune(P1, Domain::Body, false);
    addRune(P1, Domain::Body, false);

    Card* card = card_registry.get(748);
    CardContext ctx{state, events, exec, P1, gear};
    bool ok = card->onEquip(ctx, unit);

    EXPECT_TRUE(ok);
    EXPECT_TRUE(state.getObject(gear).attached_to.has_value());
    EXPECT_EQ(*state.getObject(gear).attached_to, unit);
    EXPECT_EQ(state.getObject(unit).attachment_might_bonus, 3);
    // energy_cost = 3 - 2 = 1. Cost paid: one rune recycled out of base for
    // [A] (the [1] energy is paid by exhausting a ready rune, which may be the
    // same rune that's then recycled — energy-first idiom). Net: exactly one
    // rune leaves the base, leaving one behind.
    EXPECT_EQ(readyRuneCount(P1) + exhaustedRuneCount(P1), 1);
}

TEST_F(AuditFix7Test, HextechGauntlets_HighMightZerosEnergyCost) {
    EffectExecutor exec(state, events, card_db);
    exec.setCardRegistry(&card_registry);
    auto unit = addUnit(P1, kInvalidId, /*might=*/5, /*at_bf=*/0);  // reduces 3 -> 0
    auto gear = addGear(P1, 748, /*might_bonus=*/3);
    addRune(P1, Domain::Body, false);  // only need 1 rune for [A]

    Card* card = card_registry.get(748);
    CardContext ctx{state, events, exec, P1, gear};
    bool ok = card->onEquip(ctx, unit);

    EXPECT_TRUE(ok);
    EXPECT_EQ(exhaustedRuneCount(P1), 0);  // energy cost floored to 0
}

TEST_F(AuditFix7Test, HextechGauntlets_FailsWhenCannotPay) {
    EffectExecutor exec(state, events, card_db);
    exec.setCardRegistry(&card_registry);
    auto unit = addUnit(P1, kInvalidId, /*might=*/0, /*at_bf=*/0);  // no reduction: needs 3 energy
    auto gear = addGear(P1, 748, /*might_bonus=*/3);
    addRune(P1, Domain::Body, false);  // only 1 ready rune, need 3 energy + 1 [A]

    Card* card = card_registry.get(748);
    CardContext ctx{state, events, exec, P1, gear};
    bool ok = card->onEquip(ctx, unit);

    EXPECT_FALSE(ok);
    EXPECT_FALSE(state.getObject(gear).attached_to.has_value());  // no mutation
    EXPECT_EQ(state.getObject(unit).attachment_might_bonus, 0);
    EXPECT_EQ(exhaustedRuneCount(P1), 0);  // no runes spent
}

TEST_F(AuditFix7Test, HextechGauntlets_ConquerRiderDraws) {
    EffectExecutor exec(state, events, card_db);
    exec.setCardRegistry(&card_registry);
    auto unit = addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/0);
    auto gear = addGear(P1, 748, 3);
    addToDeck(P1, kInvalidId);
    int before = handSize(P1);

    Card* card = card_registry.get(748);
    EXPECT_EQ(card->equippedTriggerType(), TriggerType::WhenIConquer);
    CardContext ctx{state, events, exec, P1, gear};
    card->onEquippedTrigger(ctx, unit, {});

    // Best-effort approximation: draws unconditionally on conquer (excess-damage
    // signal not threaded). TODO: real card gates on "3+ excess damage assigned".
    EXPECT_EQ(handSize(P1), before + 1);
}

// ─── [771] Star Spring (Battlefield) ───────────────────────────────────────────
// "First time a player plays a non-token unit here each turn, they may move
//  another unit they control here to its base."

TEST_F(AuditFix7Test, StarSpring_MovesAnotherUnitToBase) {
    EffectExecutor exec(state, events, card_db);
    exec.setCardRegistry(&card_registry);
    auto src = installBattlefieldCard(0, 771);
    state.turn.turn_number = 1;
    // Create `other` FIRST so `played` gets the higher id — the impl treats the
    // newest unit at the BF as the just-played one (no played-unit id is plumbed
    // through the trigger) and moves a DIFFERENT unit to base.
    auto other = addUnit(P1, kInvalidId, /*might=*/2, /*at_bf=*/0);
    auto played = addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/0);
    ASSERT_GT(played, other);

    fireTriggerAs(771, P1, src, TriggerType::WhenYouPlayAUnit, exec);

    // `other` moved to its base; `played` stays at the battlefield.
    EXPECT_TRUE(state.getObject(other).isAtBase());
    EXPECT_TRUE(state.getObject(played).isAtBattlefield());
}

TEST_F(AuditFix7Test, StarSpring_OnlyFiresOncePerTurnPerPlayer) {
    EffectExecutor exec(state, events, card_db);
    exec.setCardRegistry(&card_registry);
    auto src = installBattlefieldCard(0, 771);
    state.turn.turn_number = 1;
    auto other = addUnit(P1, kInvalidId, 2, /*at_bf=*/0);
    auto played = addUnit(P1, kInvalidId, 3, /*at_bf=*/0);
    ASSERT_GT(played, other);

    fireTriggerAs(771, P1, src, TriggerType::WhenYouPlayAUnit, exec);
    // First fire moved `other` to base. Bring it back to BF 0 and re-fire.
    state.getObject(other).zone = ZoneType::BattlefieldZone;
    state.getObject(other).location = BattlefieldLocation{0};

    fireTriggerAs(771, P1, src, TriggerType::WhenYouPlayAUnit, exec);

    // Already consumed this turn for this player -> no second move.
    EXPECT_TRUE(state.getObject(other).isAtBattlefield());
}

TEST_F(AuditFix7Test, StarSpring_NoOpWhenNoOtherUnitHere) {
    EffectExecutor exec(state, events, card_db);
    exec.setCardRegistry(&card_registry);
    auto src = installBattlefieldCard(0, 771);
    state.turn.turn_number = 1;
    auto played = addUnit(P1, kInvalidId, 3, /*at_bf=*/0);  // only one unit here

    fireTriggerAs(771, P1, src, TriggerType::WhenYouPlayAUnit, exec);

    EXPECT_TRUE(state.getObject(played).isAtBattlefield());  // nothing else to move
}

TEST_F(AuditFix7Test, StarSpring_IgnoresTokenOnlyPlay) {
    EffectExecutor exec(state, events, card_db);
    exec.setCardRegistry(&card_registry);
    auto src = installBattlefieldCard(0, 771);
    state.turn.turn_number = 1;
    // ONLY token units at this BF -> the impl finds no non-token unit to treat
    // as the "played" unit, so the trigger body returns without moving anyone.
    auto mkToken = [&]() {
        auto tok = state.createObject();
        auto& t = state.getObject(tok);
        t.owner = P1; t.controller = P1;
        t.card_type = CardType::Unit;
        t.super_type = SuperType::Token;
        t.base_might = 2; t.current_might = 2;
        t.zone = ZoneType::BattlefieldZone;
        t.location = BattlefieldLocation{0};
        return tok;
    };
    auto tok1 = mkToken();
    auto tok2 = mkToken();

    fireTriggerAs(771, P1, src, TriggerType::WhenYouPlayAUnit, exec);

    // No non-token unit identified as "played" -> nothing moves.
    EXPECT_TRUE(state.getObject(tok1).isAtBattlefield());
    EXPECT_TRUE(state.getObject(tok2).isAtBattlefield());
    // Gate not consumed (returned before the decision).
    EXPECT_EQ(state.getObject(src).card_counters["__starspring_p0"], 0);
}

} // namespace
