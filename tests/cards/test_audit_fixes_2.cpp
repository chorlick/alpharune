/// @file test_audit_fixes_2.cpp
/// Per-card unit tests for audit-fix batch 2 (src/cards/manual/audit_fixes_2.cpp).
///
/// Cards covered (one fixture, AuditFix2Test):
///   29  Falling Star        — two independent "Deal 3" targets
///   153 Overt Operation     — spend buffs to ready + mass buff
///   365 Brutalizer          — equip + "+2 [M] if attached this turn" aura
///   435 Lucian, Merciless   — Weaponmaster equip + first-conquer-ready
///   471 Last Rites          — equip cost + play-from-trash on conquer/hold
///   531 Seat of Power       — draw 1 per OTHER controlled battlefield
///   606 Flurry of Feathers  — modal: counter a spell OR spawn 4 Bird tokens
///   640 Sprite Fountain     — play effect + Deathknell repeat
///   657 Grim Resolve        — +3 [M] + win-combat XP rider (delayed ability)
///   720 Shepherd's Heirloom — gain 1 XP on play + [Equip] Spend 1 XP
///   764 Black Flame Altar   — Temporary units here gain [Shield]
///
/// The resumable helpers (confirmOptional / pickTarget / pickMode /
/// pickTargetPair) fall back to deterministic defaults when invoked OUTSIDE
/// a chain (no state.chain.resuming): confirmOptional -> YES (if legal),
/// pickTarget/pickTargetPair -> first legal, pickMode -> first legal mode.
/// We use the direct invocation helpers for the "yes / first" branch and
/// driveResumableTrigger / driveResumable for the explicit yes/no/mode picks.

#include "tests/cards/card_test_fixture.h"

using namespace riftbound;
using namespace riftbound::test;

namespace {

constexpr CardDefId kFallingStar      = 29;
constexpr CardDefId kOvertOperation   = 153;
constexpr CardDefId kBrutalizer       = 365;
constexpr CardDefId kLucianMerciless  = 435;
constexpr CardDefId kLastRites        = 471;
constexpr CardDefId kSeatOfPower      = 531;
constexpr CardDefId kFlurryOfFeathers = 606;
constexpr CardDefId kSpriteFountain   = 640;
constexpr CardDefId kGrimResolve      = 657;
constexpr CardDefId kShepherdsHeirloom = 720;
constexpr CardDefId kBlackFlameAltar  = 764;

class AuditFix2Test : public CardTestFixture {
protected:
    /// Build a gear object on board (optionally attached later).
    GameObjectId addGear(PlayerId owner, CardDefId def_id, int at_bf = -1) {
        auto id = state.createObject();
        auto& obj = state.getObject(id);
        obj.owner = owner;
        obj.controller = owner;
        obj.card_def_id = def_id;
        obj.card_type = CardType::Gear;
        if (def_id != kInvalidId) {
            const auto& def = card_db.get(def_id);
            obj.name = def.name;
            obj.super_type = def.super_type;
            obj.keywords = def.keywords;
            obj.domains = def.domains;
            obj.tags = def.tags;
            obj.might_bonus = def.might_bonus;
        }
        if (at_bf >= 0) {
            obj.zone = ZoneType::BattlefieldZone;
            obj.location = BattlefieldLocation{static_cast<BattlefieldId>(at_bf)};
        } else {
            obj.zone = ZoneType::Base;
            obj.location = BaseLocation{owner};
        }
        return id;
    }

    /// A trash card (so trash size / recycling can be tested).
    GameObjectId addToTrash(PlayerId owner, CardType type = CardType::Spell,
                            CardDefId def_id = kInvalidId) {
        auto id = state.createObject();
        auto& obj = state.getObject(id);
        obj.owner = owner;
        obj.controller = owner;
        obj.card_def_id = def_id;
        obj.card_type = type;
        obj.name = "TrashCard";
        obj.zone = ZoneType::Trash;
        state.player(owner).trash.push_back(id);
        return id;
    }

    /// Run a gear's onEquip against `unit`, return success.
    bool runEquip(GameObjectId gear, GameObjectId unit, EffectExecutor& exec) {
        Card* c = card_registry.get(state.getObject(gear).card_def_id);
        if (!c) return false;
        CardContext ctx{state, events, exec, state.getObject(gear).controller, gear};
        return c->onEquip(ctx, unit);
    }

    /// Picker that selects the intent with chosen_value == v (for confirm / mode).
    static std::function<Intent(const std::vector<Intent>&)> pickValue(int v) {
        return [v](const std::vector<Intent>& legal) -> Intent {
            for (auto& i : legal) {
                if (i.chosen_value.has_value() && *i.chosen_value == v) return i;
            }
            return legal.empty() ? Intent{} : legal.front();
        };
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// [29] Falling Star — "Deal 3 to a unit. Deal 3 to a unit."
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(AuditFix2Test, FallingStar_OptsIntoPlayTimeTargetPair) {
    Card* c = card_registry.get(kFallingStar);
    ASSERT_NE(c, nullptr);
    EXPECT_TRUE(c->needsPlayTimeTargetPair());
    EXPECT_EQ(c->getTargetRequirements().count, 2);
    EXPECT_TRUE(c->getTargetRequirements().must_be_unit);
}

TEST_F(AuditFix2Test, FallingStar_TwoDifferentUnitsEachTakeThree) {
    // Two distinct units; the pickTargetPair fallback picks first-legal for
    // both, BUT legal_b is recomputed and (since both clauses allow any unit)
    // ordering is by object id. We assert TOTAL damage is distributed: with a
    // chain-driven pick we'd choose two distinct units. Here we drive through
    // driveResumable so the picker chooses A then B.
    auto u1 = addUnit(P1, kInvalidId, /*might=*/10, /*at_bf=*/0);
    auto u2 = addUnit(P2, kInvalidId, /*might=*/10, /*at_bf=*/0);
    auto spell = pushSpellOnChain(P1, kFallingStar);

    EffectExecutor exec(state, events, card_db);
    // Picker: first prompt -> u1, second prompt -> u2.
    int prompt = 0;
    driveResumable(kFallingStar, P1, spell,
        [&](const std::vector<Intent>& legal) -> Intent {
            GameObjectId want = (prompt++ == 0) ? u1 : u2;
            for (auto& i : legal)
                if (!i.chosen_objects.empty() && i.chosen_objects.front() == want)
                    return i;
            return legal.empty() ? Intent{} : legal.front();
        }, exec);

    EXPECT_EQ(state.getObject(u1).damage_marked, 3);
    EXPECT_EQ(state.getObject(u2).damage_marked, 3);
}

TEST_F(AuditFix2Test, FallingStar_BothClausesCanHitSameUnitForSix) {
    // Only one unit on board — both clauses fall on it (3 + 3 = 6).
    auto u1 = addUnit(P1, kInvalidId, /*might=*/10, /*at_bf=*/0);
    auto spell = pushSpellOnChain(P1, kFallingStar);

    EffectExecutor exec(state, events, card_db);
    // Direct invocation: pickTargetPair fallback picks u1 then u1.
    invokeOnResolve(spell, kFallingStar, P1, {}, exec);

    EXPECT_EQ(state.getObject(u1).damage_marked, 6);
}

TEST_F(AuditFix2Test, FallingStar_SixDamageKillsLowMightUnit) {
    auto u1 = addUnit(P1, kInvalidId, /*might=*/4, /*at_bf=*/0);
    auto spell = pushSpellOnChain(P1, kFallingStar);

    EffectExecutor exec(state, events, card_db);
    invokeOnResolve(spell, kFallingStar, P1, {}, exec);
    // 3 (first) doesn't kill a 4-might unit; second 3 -> 6 >= 4 -> killed.
    EXPECT_EQ(state.getObject(u1).zone, ZoneType::Trash);
}

TEST_F(AuditFix2Test, FallingStar_NoUnitsIsNoOp) {
    auto spell = pushSpellOnChain(P1, kFallingStar);
    EffectExecutor exec(state, events, card_db);
    // No units exist; pickTargetPair fallback returns invalid -> hit() no-ops.
    invokeOnResolve(spell, kFallingStar, P1, {}, exec);
    SUCCEED();  // no crash, nothing to damage
}

// ═══════════════════════════════════════════════════════════════════════════
// [153] Overt Operation — spend buffs to ready, then buff all friendly units.
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(AuditFix2Test, OvertOperation_SpendBuffReadiesAndRebuffs) {
    // A friendly unit with one buff, exhausted. Yes-branch: spend the buff
    // (ready it, -1 buff), then buff all friendly units (+1).
    auto u = addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/0);
    auto& uo = state.getObject(u);
    uo.is_exhausted = true;
    uo.buff_count = 1;
    uo.temp_might_bonus = 1;
    uo.recomputeMight();

    EffectExecutor exec(state, events, card_db);
    auto spell = pushSpellOnChain(P1, kOvertOperation);
    // Direct invocation: confirmOptional fallback returns YES (precondition met).
    invokeOnResolve(spell, kOvertOperation, P1, {}, exec);

    EXPECT_FALSE(state.getObject(u).is_exhausted) << "should be readied";
    // -1 from spend then +1 from rebuff = net 1 buff.
    EXPECT_EQ(state.getObject(u).buff_count, 1);
}

TEST_F(AuditFix2Test, OvertOperation_DeclineStillBuffsAll) {
    // Drive via resumable trigger... actually onResolve uses confirmOptional;
    // drive via driveResumable picking the "no" (chosen_value == 0) option.
    auto u = addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/0);
    auto& uo = state.getObject(u);
    uo.is_exhausted = true;
    uo.buff_count = 1;
    uo.temp_might_bonus = 1;
    uo.recomputeMight();

    EffectExecutor exec(state, events, card_db);
    auto spell = pushSpellOnChain(P1, kOvertOperation);
    driveResumable(kOvertOperation, P1, spell, pickValue(0), exec);

    // Declined the spend -> still exhausted; but mass-buff still applies (+1).
    EXPECT_TRUE(state.getObject(u).is_exhausted);
    EXPECT_EQ(state.getObject(u).buff_count, 2) << "1 existing + 1 mass-buff";
}

TEST_F(AuditFix2Test, OvertOperation_NoBuffsStillMassBuffs) {
    // No friendly unit has a buff -> spend-to-ready step is skipped (precond
    // false), but every friendly unit still gets +1 [M].
    auto u1 = addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/0);
    auto u2 = addUnit(P1, kInvalidId, /*might=*/2, /*at_bf=*/0);

    EffectExecutor exec(state, events, card_db);
    auto spell = pushSpellOnChain(P1, kOvertOperation);
    invokeOnResolve(spell, kOvertOperation, P1, {}, exec);

    EXPECT_EQ(state.getObject(u1).buff_count, 1);
    EXPECT_EQ(state.getObject(u2).buff_count, 1);
}

TEST_F(AuditFix2Test, OvertOperation_DoesNotBuffEnemyUnits) {
    auto friendly = addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/0);
    auto enemy    = addUnit(P2, kInvalidId, /*might=*/3, /*at_bf=*/0);

    EffectExecutor exec(state, events, card_db);
    auto spell = pushSpellOnChain(P1, kOvertOperation);
    invokeOnResolve(spell, kOvertOperation, P1, {}, exec);

    EXPECT_EQ(state.getObject(friendly).buff_count, 1);
    EXPECT_EQ(state.getObject(enemy).buff_count, 0) << "enemy must not be buffed";
}

// ═══════════════════════════════════════════════════════════════════════════
// [365] Brutalizer — equip [G]; +2 [M] aura if attached THIS turn.
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(AuditFix2Test, Brutalizer_HasEquipAndDeclaresAbility) {
    Card* c = card_registry.get(kBrutalizer);
    ASSERT_NE(c, nullptr);
    EXPECT_TRUE(c->hasEquipAbility());
}

TEST_F(AuditFix2Test, Brutalizer_EquipStampsAttachTurnAndGrantsAura) {
    auto unit = addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/0);
    auto gear = addGear(P1, kBrutalizer, /*at_bf=*/0);
    addRune(P1, Domain::Calm);  // matching-domain power rune for recycle
    state.turn.turn_number = 5;

    EffectExecutor exec(state, events, card_db);
    EXPECT_TRUE(runEquip(gear, unit, exec));
    EXPECT_EQ(state.getObject(gear).attached_to, std::optional<GameObjectId>(unit));
    EXPECT_EQ(state.getObject(gear).card_counters["__brutalizer_attach_turn"], 5);

    // Aura recalc would clear aura_effects first; we test the contribution.
    state.getObject(unit).aura_effects.clear();
    Card* c = card_registry.get(kBrutalizer);
    c->applyPassiveAura(state, P1);
    int bonus = 0;
    for (auto& ae : state.getObject(unit).aura_effects) bonus += ae.might_bonus;
    EXPECT_EQ(bonus, 2) << "attached this turn -> +2 [M] aura";
}

TEST_F(AuditFix2Test, Brutalizer_NoAuraIfAttachedEarlierTurn) {
    auto unit = addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/0);
    auto gear = addGear(P1, kBrutalizer, /*at_bf=*/0);
    addRune(P1, Domain::Calm);
    state.turn.turn_number = 5;

    EffectExecutor exec(state, events, card_db);
    ASSERT_TRUE(runEquip(gear, unit, exec));

    // Advance the turn — no longer "attached this turn".
    state.turn.turn_number = 6;
    state.getObject(unit).aura_effects.clear();
    Card* c = card_registry.get(kBrutalizer);
    c->applyPassiveAura(state, P1);
    int bonus = 0;
    for (auto& ae : state.getObject(unit).aura_effects) bonus += ae.might_bonus;
    EXPECT_EQ(bonus, 0) << "not attached this turn -> no +2 [M]";
}

TEST_F(AuditFix2Test, Brutalizer_EquipFailsWithoutCalmRune) {
    auto unit = addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/0);
    auto gear = addGear(P1, kBrutalizer, /*at_bf=*/0);
    addRune(P1, Domain::Fury);  // wrong domain — no Calm rune to recycle

    EffectExecutor exec(state, events, card_db);
    EXPECT_FALSE(runEquip(gear, unit, exec));
    EXPECT_FALSE(state.getObject(gear).attached_to.has_value());
}

// ═══════════════════════════════════════════════════════════════════════════
// [435] Lucian, Merciless — Weaponmaster equip + first-conquer-ready.
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(AuditFix2Test, Lucian_DeclaresBothTriggers) {
    Card* c = card_registry.get(kLucianMerciless);
    ASSERT_NE(c, nullptr);
    EXPECT_TRUE(c->firesOn(TriggerType::WhenYouPlayMe));
    EXPECT_TRUE(c->firesOn(TriggerType::WhenIConquer));
}

TEST_F(AuditFix2Test, Lucian_WeaponmasterEquipsFreeFromAnotherUnit) {
    // Lucian on board; an Equipment gear attached to a DIFFERENT unit.
    auto lucian = addUnit(P1, kLucianMerciless, /*might=*/4, /*at_bf=*/0);
    auto other  = addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/0);
    auto gear   = addGear(P1, /*def_id=*/482 /*BF Sword: Equipment, +3M*/, /*at_bf=*/0);
    // Attach gear to `other` first.
    {
        auto& g = state.getObject(gear);
        auto& o = state.getObject(other);
        g.attached_to = other;
        o.attachments.push_back(gear);
        o.attachment_might_bonus += g.might_bonus;
        o.recomputeMight();
    }
    int gear_bonus = state.getObject(gear).might_bonus;
    ASSERT_GT(gear_bonus, 0);

    EffectExecutor exec(state, events, card_db);
    fireTriggerAs(kLucianMerciless, P1, lucian, TriggerType::WhenYouPlayMe, exec);

    EXPECT_EQ(state.getObject(gear).attached_to, std::optional<GameObjectId>(lucian));
    EXPECT_EQ(state.getObject(other).attachment_might_bonus, 0)
        << "gear detached from previous bearer";
    EXPECT_EQ(state.getObject(lucian).attachment_might_bonus, gear_bonus);
}

TEST_F(AuditFix2Test, Lucian_FirstConquerReadiesOncePerTurn) {
    auto lucian = addUnit(P1, kLucianMerciless, /*might=*/4, /*at_bf=*/0);
    state.getObject(lucian).is_exhausted = true;
    state.turn.turn_number = 3;

    EffectExecutor exec(state, events, card_db);
    fireTriggerAs(kLucianMerciless, P1, lucian, TriggerType::WhenIConquer, exec);
    EXPECT_FALSE(state.getObject(lucian).is_exhausted) << "first conquer readies";

    // Exhaust again; a SECOND conquer this same turn must NOT ready.
    state.getObject(lucian).is_exhausted = true;
    fireTriggerAs(kLucianMerciless, P1, lucian, TriggerType::WhenIConquer, exec);
    EXPECT_TRUE(state.getObject(lucian).is_exhausted)
        << "only the FIRST conquer each turn readies";
}

TEST_F(AuditFix2Test, Lucian_ConquerReadyResetsNextTurn) {
    auto lucian = addUnit(P1, kLucianMerciless, /*might=*/4, /*at_bf=*/0);
    state.getObject(lucian).is_exhausted = true;
    state.turn.turn_number = 3;

    EffectExecutor exec(state, events, card_db);
    fireTriggerAs(kLucianMerciless, P1, lucian, TriggerType::WhenIConquer, exec);
    ASSERT_FALSE(state.getObject(lucian).is_exhausted);

    // New turn: first conquer should ready again.
    state.turn.turn_number = 4;
    state.getObject(lucian).is_exhausted = true;
    fireTriggerAs(kLucianMerciless, P1, lucian, TriggerType::WhenIConquer, exec);
    EXPECT_FALSE(state.getObject(lucian).is_exhausted)
        << "first-conquer-ready resets each turn";
}

// ═══════════════════════════════════════════════════════════════════════════
// [471] Last Rites — equip (recycle 2 + [P]) ; conquer/hold play-from-trash.
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(AuditFix2Test, LastRites_EquipRecyclesTwoTrashAndCostsChaos) {
    auto unit = addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/0);
    auto gear = addGear(P1, kLastRites, /*at_bf=*/0);
    addToTrash(P1);
    addToTrash(P1);
    addRune(P1, Domain::Chaos);
    ASSERT_EQ(trashSize(P1), 2);

    EffectExecutor exec(state, events, card_db);
    EXPECT_TRUE(runEquip(gear, unit, exec));
    EXPECT_EQ(state.getObject(gear).attached_to, std::optional<GameObjectId>(unit));
    EXPECT_EQ(trashSize(P1), 0) << "2 trash cards recycled to deck";
    EXPECT_EQ(deckSize(P1), 2);
}

TEST_F(AuditFix2Test, LastRites_EquipFailsWithFewerThanTwoTrash) {
    auto unit = addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/0);
    auto gear = addGear(P1, kLastRites, /*at_bf=*/0);
    addToTrash(P1);             // only one
    addRune(P1, Domain::Chaos);

    EffectExecutor exec(state, events, card_db);
    EXPECT_FALSE(runEquip(gear, unit, exec));
    EXPECT_FALSE(state.getObject(gear).attached_to.has_value());
    EXPECT_EQ(trashSize(P1), 1) << "must not recycle when unaffordable";
}

TEST_F(AuditFix2Test, LastRites_EquipFailsWithoutChaosRune) {
    auto unit = addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/0);
    auto gear = addGear(P1, kLastRites, /*at_bf=*/0);
    addToTrash(P1);
    addToTrash(P1);
    addRune(P1, Domain::Fury);  // wrong domain

    EffectExecutor exec(state, events, card_db);
    EXPECT_FALSE(runEquip(gear, unit, exec));
    EXPECT_EQ(trashSize(P1), 2) << "no recycle on failed pre-check";
}

TEST_F(AuditFix2Test, LastRites_DeclaresConquerOrHoldTrigger) {
    Card* c = card_registry.get(kLastRites);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->equippedTriggerType(), TriggerType::WhenIConquerOrHold);
}

TEST_F(AuditFix2Test, LastRites_TriggerPlaysUnitFromTrash) {
    // A unit card sits in trash; onEquippedTrigger (yes-branch via direct call)
    // plays it onto the board for free (engine-gap: cost not paid).
    auto bearer = addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/0);
    auto gear = addGear(P1, kLastRites);
    auto unit_in_trash = addToTrash(P1, CardType::Unit);
    state.getObject(unit_in_trash).name = "TrashUnit";

    EffectExecutor exec(state, events, card_db);
    Card* c = card_registry.get(kLastRites);
    ASSERT_NE(c, nullptr);
    CardContext ctx{state, events, exec, P1, gear};
    // Direct call -> confirmOptional fallback = YES, pickTarget fallback = first.
    c->onEquippedTrigger(ctx, bearer, {});

    EXPECT_FALSE(inTrash(P1, unit_in_trash)) << "unit left trash";
    EXPECT_NE(state.getObject(unit_in_trash).zone, ZoneType::Trash);
    EXPECT_TRUE(state.getObject(unit_in_trash).isAtBase())
        << "playIgnoringCost lands the unit at controller's base";
}

TEST_F(AuditFix2Test, LastRites_TriggerNoOpWithNoTrashUnit) {
    auto bearer = addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/0);
    auto gear = addGear(P1, kLastRites);
    addToTrash(P1, CardType::Spell);  // a spell, NOT a unit

    EffectExecutor exec(state, events, card_db);
    Card* c = card_registry.get(kLastRites);
    CardContext ctx{state, events, exec, P1, gear};
    c->onEquippedTrigger(ctx, bearer, {});
    EXPECT_EQ(trashSize(P1), 1) << "no playable unit -> nothing happens";
}

// ═══════════════════════════════════════════════════════════════════════════
// [531] Seat of Power — draw 1 per OTHER battlefield you control.
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(AuditFix2Test, SeatOfPower_DeclaresConquerHereTrigger) {
    Card* c = card_registry.get(kSeatOfPower);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->triggerType(), TriggerType::WhenYouConquerHere);
}

TEST_F(AuditFix2Test, SeatOfPower_DrawsOnePerOtherControlledBF) {
    // 3 battlefields: SetUp creates 2, add a 3rd. Seat of Power is BF[0].
    auto bf3 = addBattlefield();  // id 2
    // Give P1 a draw library.
    for (int i = 0; i < 5; ++i) addToDeck(P1, kInvalidId);

    // Make a Seat-of-Power card object that backs battlefield 0.
    auto seat_obj = state.createObject();
    {
        auto& o = state.getObject(seat_obj);
        o.card_def_id = kSeatOfPower;
        o.card_type = CardType::Battlefield;
        o.name = "Seat of Power";
    }
    state.battlefields[0].card_object_id = seat_obj;
    state.battlefields[0].controller = P1;
    state.battlefields[1].controller = P1;   // other controlled BF
    state.battlefields[bf3].controller = P1; // other controlled BF

    int before = handSize(P1);
    EffectExecutor exec(state, events, card_db);
    fireTriggerAs(kSeatOfPower, P1, seat_obj, TriggerType::WhenYouConquerHere, exec);
    EXPECT_EQ(handSize(P1), before + 2) << "2 OTHER controlled BFs -> draw 2";
}

TEST_F(AuditFix2Test, SeatOfPower_NoOtherBFDrawsZero) {
    for (int i = 0; i < 3; ++i) addToDeck(P1, kInvalidId);
    auto seat_obj = state.createObject();
    {
        auto& o = state.getObject(seat_obj);
        o.card_def_id = kSeatOfPower;
        o.card_type = CardType::Battlefield;
        o.name = "Seat of Power";
    }
    state.battlefields[0].card_object_id = seat_obj;
    state.battlefields[0].controller = P1;
    state.battlefields[1].controller = P2;  // enemy-controlled, not counted

    int before = handSize(P1);
    EffectExecutor exec(state, events, card_db);
    fireTriggerAs(kSeatOfPower, P1, seat_obj, TriggerType::WhenYouConquerHere, exec);
    EXPECT_EQ(handSize(P1), before) << "no OTHER controlled BF -> draw 0";
}

// ═══════════════════════════════════════════════════════════════════════════
// [606] Flurry of Feathers — modal: counter a spell OR spawn 4 Bird tokens.
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(AuditFix2Test, FlurryOfFeathers_CounterModeRemovesTopSpell) {
    // Put a victim spell on the chain, then Flurry on top of it. Drive the
    // resolve picking mode 0 (counter).
    auto victim = pushSpellOnChain(P2, kInvalidId);
    auto flurry = pushSpellOnChain(P1, kFlurryOfFeathers);
    state.getObject(victim).owner = P2;

    EffectExecutor exec(state, events, card_db);
    // ChainManager would pop Flurry before onResolve; emulate by removing it
    // from the items vector so chain.items.back() is the victim.
    state.chain.items.pop_back();  // remove flurry
    ASSERT_EQ(state.chain.items.size(), 1u);

    driveResumable(kFlurryOfFeathers, P1, flurry, pickValue(0), exec);

    EXPECT_TRUE(state.chain.items.empty()) << "victim spell countered (popped)";
    EXPECT_TRUE(inTrash(P2, victim)) << "countered spell goes to trash";
}

TEST_F(AuditFix2Test, FlurryOfFeathers_BirdModeSpawnsFourDeflectTokens) {
    auto flurry = pushSpellOnChain(P1, kFlurryOfFeathers);
    auto victim = pushSpellOnChain(P2, kInvalidId);  // counter would be legal
    // Remove flurry & victim positioning is irrelevant for bird-mode.

    EffectExecutor exec(state, events, card_db);
    driveResumable(kFlurryOfFeathers, P1, flurry, pickValue(1), exec);

    int birds = 0;
    for (auto& [id, obj] : state.objects) {
        if (obj.isToken() && obj.name == "Bird") {
            ++birds;
            EXPECT_TRUE(obj.hasKeyword(Keyword::Deflect));
            EXPECT_EQ(obj.base_might, 1);
            EXPECT_EQ(obj.controller, P1);
        }
    }
    EXPECT_EQ(birds, 4);
    // victim remains since we chose bird mode.
    EXPECT_FALSE(inTrash(P2, victim));
}

TEST_F(AuditFix2Test, FlurryOfFeathers_NoSpellToCounterFallsToBirds) {
    // Empty chain (no victim). Counter mode is illegal -> only bird mode is
    // offered; direct invocation picks first legal mode = birds.
    auto flurry = pushSpellOnChain(P1, kFlurryOfFeathers);
    state.chain.items.pop_back();  // remove flurry, chain now empty
    ASSERT_TRUE(state.chain.items.empty());

    EffectExecutor exec(state, events, card_db);
    invokeOnResolve(flurry, kFlurryOfFeathers, P1, {}, exec);

    int birds = 0;
    for (auto& [id, obj] : state.objects)
        if (obj.isToken() && obj.name == "Bird") ++birds;
    EXPECT_EQ(birds, 4) << "no legal counter -> birds spawn";
}

// ═══════════════════════════════════════════════════════════════════════════
// [640] Sprite Fountain — play effect + Deathknell repeat.
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(AuditFix2Test, SpriteFountain_DeclaresPlayAndDeathTriggers) {
    Card* c = card_registry.get(kSpriteFountain);
    ASSERT_NE(c, nullptr);
    EXPECT_TRUE(c->firesOn(TriggerType::WhenYouPlayThis));
    EXPECT_TRUE(c->firesOn(TriggerType::WhenIDie));
}

TEST_F(AuditFix2Test, SpriteFountain_PlaySpawnsReadyTemporarySprite) {
    auto gear = addGear(P1, kSpriteFountain);
    EffectExecutor exec(state, events, card_db);
    fireTriggerAs(kSpriteFountain, P1, gear, TriggerType::WhenYouPlayThis, exec);

    int sprites = 0;
    for (auto& [id, obj] : state.objects) {
        if (obj.isToken() && obj.name == "Sprite") {
            ++sprites;
            EXPECT_EQ(obj.base_might, 3);
            EXPECT_TRUE(obj.hasKeyword(Keyword::Temporary));
            EXPECT_FALSE(obj.is_exhausted) << "enters ready";
            EXPECT_EQ(obj.controller, P1);
        }
    }
    EXPECT_EQ(sprites, 1);
}

TEST_F(AuditFix2Test, SpriteFountain_DeathknellRepeatsPlayEffect) {
    auto gear = addGear(P1, kSpriteFountain);
    EffectExecutor exec(state, events, card_db);
    // Death (Deathknell) fires the same spawn effect.
    fireTriggerAs(kSpriteFountain, P1, gear, TriggerType::WhenIDie, exec);

    int sprites = 0;
    for (auto& [id, obj] : state.objects)
        if (obj.isToken() && obj.name == "Sprite") ++sprites;
    EXPECT_EQ(sprites, 1) << "Deathknell repeat spawns a Sprite";
}

// ═══════════════════════════════════════════════════════════════════════════
// [657] Grim Resolve — +3 [M] this turn + win-combat XP rider.
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(AuditFix2Test, GrimResolve_GivesPlusThreeMight) {
    auto unit = addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/0);
    auto spell = pushSpellOnChain(P1, kGrimResolve);

    EffectExecutor exec(state, events, card_db);
    invokeOnResolve(spell, kGrimResolve, P1, {}, exec);
    EXPECT_EQ(state.getObject(unit).current_might, 6) << "+3 [M]";
}

TEST_F(AuditFix2Test, GrimResolve_ArmsWinCombatDelayedAbility) {
    auto unit = addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/0);
    auto spell = pushSpellOnChain(P1, kGrimResolve);
    state.turn.turn_number = 7;

    EffectExecutor exec(state, events, card_db);
    invokeOnResolve(spell, kGrimResolve, P1, {}, exec);

    ASSERT_EQ(state.delayed_abilities.size(), 1u);
    const auto& da = state.delayed_abilities.front();
    EXPECT_EQ(da.trigger, TriggerType::WhenIWinCombat);
    EXPECT_EQ(da.target_filter, unit) << "rider scoped to the buffed unit";
    EXPECT_EQ(da.card_def_id, kGrimResolve);
    EXPECT_EQ(da.expires_on_turn, 7) << "this turn only";
}

TEST_F(AuditFix2Test, GrimResolve_TriggerGrantsTwoXP) {
    auto unit = addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/0);
    setXp(P1, 0);

    EffectExecutor exec(state, events, card_db);
    // The delayed ability fires onTrigger (WhenIWinCombat) -> +2 XP.
    fireTriggerAs(kGrimResolve, P1, unit, TriggerType::WhenIWinCombat, exec);
    EXPECT_EQ(state.player(P1).xp, 2);
}

TEST_F(AuditFix2Test, GrimResolve_NoLegalTargetFizzles) {
    // No friendly units -> pickTarget fallback returns invalid -> no buff,
    // no delayed ability armed.
    auto spell = pushSpellOnChain(P1, kGrimResolve);
    EffectExecutor exec(state, events, card_db);
    invokeOnResolve(spell, kGrimResolve, P1, {}, exec);
    EXPECT_TRUE(state.delayed_abilities.empty());
}

// ═══════════════════════════════════════════════════════════════════════════
// [720] Shepherd's Heirloom — gain 1 XP on play + [Equip] Spend 1 XP.
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(AuditFix2Test, ShepherdsHeirloom_DeclaresPlayTriggerAndEquip) {
    Card* c = card_registry.get(kShepherdsHeirloom);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->triggerType(), TriggerType::WhenYouPlayThis);
    EXPECT_TRUE(c->hasEquipAbility());
}

TEST_F(AuditFix2Test, ShepherdsHeirloom_PlayGrantsOneXP) {
    auto gear = addGear(P1, kShepherdsHeirloom);
    setXp(P1, 4);
    EffectExecutor exec(state, events, card_db);
    fireTriggerAs(kShepherdsHeirloom, P1, gear, TriggerType::WhenYouPlayThis, exec);
    EXPECT_EQ(state.player(P1).xp, 5);
}

TEST_F(AuditFix2Test, ShepherdsHeirloom_EquipSpendsOneXP) {
    auto unit = addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/0);
    auto gear = addGear(P1, kShepherdsHeirloom, /*at_bf=*/0);
    setXp(P1, 2);
    int gear_bonus = state.getObject(gear).might_bonus;

    EffectExecutor exec(state, events, card_db);
    EXPECT_TRUE(runEquip(gear, unit, exec));
    EXPECT_EQ(state.player(P1).xp, 1) << "spent 1 XP";
    EXPECT_EQ(state.getObject(gear).attached_to, std::optional<GameObjectId>(unit));
    EXPECT_EQ(state.getObject(unit).attachment_might_bonus, gear_bonus);
}

TEST_F(AuditFix2Test, ShepherdsHeirloom_EquipFailsWithZeroXP) {
    auto unit = addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/0);
    auto gear = addGear(P1, kShepherdsHeirloom, /*at_bf=*/0);
    setXp(P1, 0);

    EffectExecutor exec(state, events, card_db);
    EXPECT_FALSE(runEquip(gear, unit, exec)) << "can't pay Spend 1 XP";
    EXPECT_FALSE(state.getObject(gear).attached_to.has_value());
    EXPECT_EQ(state.player(P1).xp, 0) << "no XP spent on failure";
}

// ═══════════════════════════════════════════════════════════════════════════
// [764] Black Flame Altar — Temporary units here gain [Shield].
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(AuditFix2Test, BlackFlameAltar_GrantsShieldToTemporaryUnitHere) {
    // Make BF[0] be a Black Flame Altar.
    auto altar_obj = state.createObject();
    {
        auto& o = state.getObject(altar_obj);
        o.card_def_id = kBlackFlameAltar;
        o.card_type = CardType::Battlefield;
        o.name = "Black Flame Altar";
    }
    state.battlefields[0].card_object_id = altar_obj;

    auto temp_unit = addUnit(P1, kInvalidId, /*might=*/2, /*at_bf=*/0);
    state.getObject(temp_unit).keywords.set(Keyword::Temporary);

    Card* c = card_registry.get(kBlackFlameAltar);
    ASSERT_NE(c, nullptr);
    state.getObject(temp_unit).aura_effects.clear();
    c->applyPassiveAura(state, P1);

    bool got_shield = false;
    for (auto& ae : state.getObject(temp_unit).aura_effects)
        if (ae.keyword == Keyword::Shield) { got_shield = true; EXPECT_EQ(ae.keyword_value, 1); }
    EXPECT_TRUE(got_shield) << "Temporary unit at this BF gets [Shield]";
}

TEST_F(AuditFix2Test, BlackFlameAltar_DoesNotGrantToNonTemporaryUnit) {
    auto altar_obj = state.createObject();
    {
        auto& o = state.getObject(altar_obj);
        o.card_def_id = kBlackFlameAltar;
        o.card_type = CardType::Battlefield;
        o.name = "Black Flame Altar";
    }
    state.battlefields[0].card_object_id = altar_obj;

    auto plain_unit = addUnit(P1, kInvalidId, /*might=*/2, /*at_bf=*/0);  // no Temporary

    Card* c = card_registry.get(kBlackFlameAltar);
    state.getObject(plain_unit).aura_effects.clear();
    c->applyPassiveAura(state, P1);

    bool got_shield = false;
    for (auto& ae : state.getObject(plain_unit).aura_effects)
        if (ae.keyword == Keyword::Shield) got_shield = true;
    EXPECT_FALSE(got_shield) << "non-Temporary unit must not gain Shield";
}

TEST_F(AuditFix2Test, BlackFlameAltar_DoesNotGrantToUnitAtOtherBF) {
    auto altar_obj = state.createObject();
    {
        auto& o = state.getObject(altar_obj);
        o.card_def_id = kBlackFlameAltar;
        o.card_type = CardType::Battlefield;
        o.name = "Black Flame Altar";
    }
    state.battlefields[0].card_object_id = altar_obj;

    // A Temporary unit at battlefield 1 (NOT the altar's BF).
    auto temp_elsewhere = addUnit(P1, kInvalidId, /*might=*/2, /*at_bf=*/1);
    state.getObject(temp_elsewhere).keywords.set(Keyword::Temporary);

    Card* c = card_registry.get(kBlackFlameAltar);
    state.getObject(temp_elsewhere).aura_effects.clear();
    c->applyPassiveAura(state, P1);

    bool got_shield = false;
    for (auto& ae : state.getObject(temp_elsewhere).aura_effects)
        if (ae.keyword == Keyword::Shield) got_shield = true;
    EXPECT_FALSE(got_shield) << "only units AT this BF are affected";
}

}  // namespace
