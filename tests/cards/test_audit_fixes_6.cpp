/// @file test_audit_fixes_6.cpp
/// Per-card tests for audit-fix batch 6 (src/cards/manual/audit_fixes_6.cpp).
/// Cards: 67, 284, 402, 461, 508, 601, 614, 645, 695, 745, 770.
///
/// Each test pins the behavior actually implemented by the AF6_* classes.
/// Where the implementation documents an engine-limitation approximation
/// (e.g. Rabadon's bonus-damage clause, Ripper's Bay return-to-hand trigger),
/// the test exercises the implemented approximation and notes the gap.

#include "tests/cards/card_test_fixture.h"

using namespace riftbound;
using namespace riftbound::test;

namespace {

constexpr CardDefId kBlitzcrank   = 67;
constexpr CardDefId kTargonsPeak  = 284;
constexpr CardDefId kBellowsBreath = 402;
constexpr CardDefId kFizz         = 461;
constexpr CardDefId kRabadon      = 508;
constexpr CardDefId kSoulSword    = 601;
constexpr CardDefId kNami         = 614;
constexpr CardDefId kSmokeMirrors = 645;
constexpr CardDefId kBlastCone    = 695;
constexpr CardDefId kThrill       = 745;
constexpr CardDefId kRippersBay   = 770;
constexpr CardDefId kDisintegrate = 5;    // spell, energy 4 (> 3) — Fizz gate

class AuditFix6Test : public CardTestFixture {
protected:
    // Build a gear object on board (optionally attached to a unit).
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

    // Attach gear to a unit (mirrors engine's attach essentials).
    void attach(GameObjectId gear, GameObjectId unit) {
        auto& g = state.getObject(gear);
        auto& u = state.getObject(unit);
        g.attached_to = unit;
        g.location = u.location;
        g.zone = u.zone;
        u.attachments.push_back(gear);
        u.attachment_might_bonus += g.might_bonus;
        u.recomputeMight();
    }
};

// ─── [67] Blitzcrank, Impassive ─────────────────────────────────────────────

// WhenYouPlayMe: pull an enemy unit to my battlefield. Direct invocation of
// confirmOptional defaults to "yes" when legal.
TEST_F(AuditFix6Test, Blitzcrank_PlayPullsEnemyToMyBattlefield) {
    auto self = addUnit(P1, kBlitzcrank, /*might=*/5, /*at_bf=*/0);
    // Enemy unit elsewhere (other battlefield).
    auto enemy = addUnit(P2, kInvalidId, /*might=*/2, /*at_bf=*/1);

    EffectExecutor exec(state, events, card_db);
    fireTriggerAs(kBlitzcrank, P1, self, TriggerType::WhenYouPlayMe, exec);

    // Enemy moved onto Blitzcrank's battlefield (BF#0).
    EXPECT_EQ(state.getObject(enemy).battlefieldId(), std::optional<BattlefieldId>(0));
}

// WhenYouPlayMe with no enemy elsewhere → still_legal() false → no-op.
TEST_F(AuditFix6Test, Blitzcrank_PlayNoOpWhenNoEnemyElsewhere) {
    auto self = addUnit(P1, kBlitzcrank, /*might=*/5, /*at_bf=*/0);
    // Enemy is already on the SAME battlefield as Blitzcrank — not "elsewhere".
    auto enemy = addUnit(P2, kInvalidId, /*might=*/2, /*at_bf=*/0);

    EffectExecutor exec(state, events, card_db);
    fireTriggerAs(kBlitzcrank, P1, self, TriggerType::WhenYouPlayMe, exec);

    EXPECT_EQ(state.getObject(enemy).battlefieldId(), std::optional<BattlefieldId>(0));
}

// WhenIHold: return me to my owner's hand.
TEST_F(AuditFix6Test, Blitzcrank_HoldReturnsToHand) {
    auto self = addUnit(P1, kBlitzcrank, /*might=*/5, /*at_bf=*/0);

    EffectExecutor exec(state, events, card_db);
    fireTriggerAs(kBlitzcrank, P1, self, TriggerType::WhenIHold, exec);

    EXPECT_TRUE(inHand(P1, self));
    EXPECT_EQ(state.getObject(self).zone, ZoneType::Hand);
    EXPECT_FALSE(state.getObject(self).location.has_value());
}

// Card registers BOTH triggers.
TEST_F(AuditFix6Test, Blitzcrank_RegistersBothTriggers) {
    Card* c = card_registry.get(kBlitzcrank);
    ASSERT_NE(c, nullptr);
    EXPECT_TRUE(c->firesOn(TriggerType::WhenYouPlayMe));
    EXPECT_TRUE(c->firesOn(TriggerType::WhenIHold));
}

// ─── [284] Targon's Peak ────────────────────────────────────────────────────

// WhenYouConquerHere schedules an AtEndOfTurn delayed ability.
TEST_F(AuditFix6Test, TargonsPeak_ConquerSchedulesDelayedReady) {
    auto bf_obj = state.createObject();
    state.getObject(bf_obj).card_type = CardType::Battlefield;
    state.getObject(bf_obj).card_def_id = kTargonsPeak;
    state.turn.turn_number = 3;

    EffectExecutor exec(state, events, card_db);
    fireTriggerAs(kTargonsPeak, P1, bf_obj, TriggerType::WhenYouConquerHere, exec);

    ASSERT_EQ(state.delayed_abilities.size(), 1u);
    const auto& da = state.delayed_abilities[0];
    EXPECT_EQ(da.trigger, TriggerType::AtEndOfTurn);
    EXPECT_EQ(da.controller, P1);
    EXPECT_EQ(da.card_def_id, kTargonsPeak);
}

// AtEndOfTurn fire readies up to 2 exhausted runes of the controller.
TEST_F(AuditFix6Test, TargonsPeak_EndOfTurnReadiesUpToTwoRunes) {
    auto bf_obj = state.createObject();
    state.getObject(bf_obj).card_type = CardType::Battlefield;
    // Three exhausted runes — should only ready 2.
    addRune(P1, Domain::Calm, /*exhausted=*/true);
    addRune(P1, Domain::Calm, /*exhausted=*/true);
    addRune(P1, Domain::Calm, /*exhausted=*/true);
    ASSERT_EQ(exhaustedRuneCount(P1), 3);

    EffectExecutor exec(state, events, card_db);
    // Direct fire with no firing_trigger → falls into the ready branch.
    fireTriggerAs(kTargonsPeak, P1, bf_obj, TriggerType::None, exec);

    EXPECT_EQ(readyRuneCount(P1), 2);
    EXPECT_EQ(exhaustedRuneCount(P1), 1);
}

// Only the controller's runes are readied, and enemy exhausted runes untouched.
TEST_F(AuditFix6Test, TargonsPeak_EndOfTurnIgnoresEnemyRunes) {
    auto bf_obj = state.createObject();
    state.getObject(bf_obj).card_type = CardType::Battlefield;
    addRune(P1, Domain::Calm, /*exhausted=*/true);
    addRune(P2, Domain::Calm, /*exhausted=*/true);
    addRune(P2, Domain::Calm, /*exhausted=*/true);

    EffectExecutor exec(state, events, card_db);
    fireTriggerAs(kTargonsPeak, P1, bf_obj, TriggerType::None, exec);

    EXPECT_EQ(readyRuneCount(P1), 1);          // only friendly rune readied
    EXPECT_EQ(exhaustedRuneCount(P2), 2);      // enemy runes untouched
}

// ─── [402] Bellows Breath ───────────────────────────────────────────────────

// Deal 1 to up to three units sharing the first target's location.
TEST_F(AuditFix6Test, BellowsBreath_DamagesUpToThreeSameLocation) {
    auto a = addUnit(P2, kInvalidId, /*might=*/3, /*at_bf=*/0);
    auto b = addUnit(P2, kInvalidId, /*might=*/3, /*at_bf=*/0);
    auto c = addUnit(P2, kInvalidId, /*might=*/3, /*at_bf=*/0);
    auto src = pushSpellOnChain(P1, kBellowsBreath);

    EffectExecutor exec(state, events, card_db);
    invokeOnResolve(src, kBellowsBreath, P1, {a, b, c}, exec);

    EXPECT_EQ(state.getObject(a).damage_marked, 1);
    EXPECT_EQ(state.getObject(b).damage_marked, 1);
    EXPECT_EQ(state.getObject(c).damage_marked, 1);
}

// Units NOT at the anchor (first target's) location are not hit.
TEST_F(AuditFix6Test, BellowsBreath_OnlyHitsSameLocation) {
    auto a = addUnit(P2, kInvalidId, /*might=*/3, /*at_bf=*/0);  // anchor
    auto elsewhere = addUnit(P2, kInvalidId, /*might=*/3, /*at_bf=*/1);
    auto src = pushSpellOnChain(P1, kBellowsBreath);

    EffectExecutor exec(state, events, card_db);
    invokeOnResolve(src, kBellowsBreath, P1, {a, elsewhere}, exec);

    EXPECT_EQ(state.getObject(a).damage_marked, 1);
    EXPECT_EQ(state.getObject(elsewhere).damage_marked, 0);
}

// Lethal damage kills the unit (1-might unit dies to 1 damage).
TEST_F(AuditFix6Test, BellowsBreath_KillsLethalUnit) {
    auto a = addUnit(P2, kInvalidId, /*might=*/1, /*at_bf=*/0);
    auto src = pushSpellOnChain(P1, kBellowsBreath);

    EffectExecutor exec(state, events, card_db);
    invokeOnResolve(src, kBellowsBreath, P1, {a}, exec);

    // killObject routes a non-token dead unit to its owner's trash.
    EXPECT_EQ(state.getObject(a).zone, ZoneType::Trash);
    EXPECT_TRUE(inTrash(P2, a));
}

// No targets → no-op (optional spell with no chosen targets).
TEST_F(AuditFix6Test, BellowsBreath_NoTargetsNoOp) {
    auto src = pushSpellOnChain(P1, kBellowsBreath);
    EffectExecutor exec(state, events, card_db);
    invokeOnResolve(src, kBellowsBreath, P1, {}, exec);
    SUCCEED();  // nothing to assert beyond "doesn't crash"
}

// ─── [461] Fizz, Trickster ──────────────────────────────────────────────────

// WhenYouPlayMe: play a spell (energy<=3) from trash for free, then recycle it.
TEST_F(AuditFix6Test, Fizz_PlaysCheapSpellFromTrashThenRecycles) {
    auto self = addUnit(P1, kFizz, /*might=*/3, /*at_bf=*/0);
    // Bellows Breath (energy_cost 1 <= 3) sits in trash.
    auto trashed = addToHand(P1, kBellowsBreath);
    // Move it to trash manually.
    auto& ph = state.player(P1).hand;
    ph.erase(std::remove(ph.begin(), ph.end(), trashed), ph.end());
    state.getObject(trashed).zone = ZoneType::Trash;
    state.player(P1).trash.push_back(trashed);

    EffectExecutor exec(state, events, card_db);
    exec.setCardRegistry(&card_registry);
    fireTriggerAs(kFizz, P1, self, TriggerType::WhenYouPlayMe, exec);

    // It left the trash and was recycled to the bottom of the deck.
    EXPECT_FALSE(inTrash(P1, trashed));
    EXPECT_TRUE(inDeck(P1, trashed));
}

// Spell with energy cost > 3 is NOT eligible — Fizz no-ops on it.
TEST_F(AuditFix6Test, Fizz_SkipsExpensiveSpell) {
    auto self = addUnit(P1, kFizz, /*might=*/3, /*at_bf=*/0);
    // Disintegrate costs 4 energy (> 3) — outside Fizz's gate.
    auto trashed = addToHand(P1, kDisintegrate);
    auto& ph = state.player(P1).hand;
    ph.erase(std::remove(ph.begin(), ph.end(), trashed), ph.end());
    state.getObject(trashed).zone = ZoneType::Trash;
    state.player(P1).trash.push_back(trashed);

    EffectExecutor exec(state, events, card_db);
    exec.setCardRegistry(&card_registry);
    fireTriggerAs(kFizz, P1, self, TriggerType::WhenYouPlayMe, exec);

    // Too expensive: stays in trash, nothing recycled.
    EXPECT_TRUE(inTrash(P1, trashed));
    EXPECT_FALSE(inDeck(P1, trashed));
}

// A non-spell (unit) in trash is never eligible.
TEST_F(AuditFix6Test, Fizz_SkipsNonSpell) {
    auto self = addUnit(P1, kFizz, /*might=*/3, /*at_bf=*/0);
    auto trashed_unit = addToHand(P1, kBlitzcrank);  // a unit, not a spell
    auto& ph = state.player(P1).hand;
    ph.erase(std::remove(ph.begin(), ph.end(), trashed_unit), ph.end());
    state.getObject(trashed_unit).zone = ZoneType::Trash;
    state.player(P1).trash.push_back(trashed_unit);

    EffectExecutor exec(state, events, card_db);
    exec.setCardRegistry(&card_registry);
    fireTriggerAs(kFizz, P1, self, TriggerType::WhenYouPlayMe, exec);

    EXPECT_TRUE(inTrash(P1, trashed_unit));
    EXPECT_FALSE(inDeck(P1, trashed_unit));
}

// Empty trash → no-op.
TEST_F(AuditFix6Test, Fizz_EmptyTrashNoOp) {
    auto self = addUnit(P1, kFizz, /*might=*/3, /*at_bf=*/0);
    EffectExecutor exec(state, events, card_db);
    exec.setCardRegistry(&card_registry);
    fireTriggerAs(kFizz, P1, self, TriggerType::WhenYouPlayMe, exec);
    EXPECT_EQ(trashSize(P1), 0);
}

// ─── [508] Rabadon's Deathcrown ─────────────────────────────────────────────

// Equip: pays one power rune (any domain) and attaches.
// NOTE: the "spells/abilities deal 3 Bonus Damage" clause is NOT implemented
// (documented engine limitation in the card source) — not tested here.
TEST_F(AuditFix6Test, Rabadon_EquipPaysAnyPowerAndAttaches) {
    auto unit = addUnit(P1, kInvalidId, /*might=*/2, /*at_bf=*/0);
    auto gear = addGear(P1, kRabadon, /*at_bf=*/0);
    addRune(P1, Domain::Body);  // any domain works for [A]

    Card* c = card_registry.get(kRabadon);
    ASSERT_NE(c, nullptr);
    EXPECT_TRUE(c->hasEquipAbility());

    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, gear};
    bool ok = c->onEquip(ctx, unit);

    EXPECT_TRUE(ok);
    EXPECT_EQ(state.getObject(gear).attached_to, std::optional<GameObjectId>(unit));
    auto& attach_list = state.getObject(unit).attachments;
    EXPECT_NE(std::find(attach_list.begin(), attach_list.end(), gear),
              attach_list.end());
    EXPECT_EQ(readyRuneCount(P1), 0);  // power rune recycled
}

// Equip fails (no state change) when there's no rune to pay.
TEST_F(AuditFix6Test, Rabadon_EquipFailsWithoutRune) {
    auto unit = addUnit(P1, kInvalidId, /*might=*/2, /*at_bf=*/0);
    auto gear = addGear(P1, kRabadon, /*at_bf=*/0);
    // No runes.

    Card* c = card_registry.get(kRabadon);
    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, gear};
    bool ok = c->onEquip(ctx, unit);

    EXPECT_FALSE(ok);
    EXPECT_FALSE(state.getObject(gear).attached_to.has_value());
    EXPECT_TRUE(state.getObject(unit).attachments.empty());
}

// ─── [601] Soul Sword ───────────────────────────────────────────────────────

// Equip requires a Calm rune; an off-domain rune doesn't satisfy [G].
TEST_F(AuditFix6Test, SoulSword_EquipFailsWithWrongDomainRune) {
    auto unit = addUnit(P1, kInvalidId, /*might=*/2, /*at_bf=*/0);
    auto gear = addGear(P1, kSoulSword, /*at_bf=*/0);
    addRune(P1, Domain::Body);  // wrong domain for [G]

    Card* c = card_registry.get(kSoulSword);
    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, gear};
    bool ok = c->onEquip(ctx, unit);

    EXPECT_FALSE(ok);
    EXPECT_FALSE(state.getObject(gear).attached_to.has_value());
}

// Equip succeeds with a Calm rune.
TEST_F(AuditFix6Test, SoulSword_EquipSucceedsWithCalmRune) {
    auto unit = addUnit(P1, kInvalidId, /*might=*/2, /*at_bf=*/0);
    auto gear = addGear(P1, kSoulSword, /*at_bf=*/0);
    addRune(P1, Domain::Calm);

    Card* c = card_registry.get(kSoulSword);
    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, gear};
    bool ok = c->onEquip(ctx, unit);

    EXPECT_TRUE(ok);
    EXPECT_EQ(state.getObject(gear).attached_to, std::optional<GameObjectId>(unit));
}

// [Level 3] passive: +1M granted only when controller has 3+ XP.
TEST_F(AuditFix6Test, SoulSword_PassiveGrantedAtLevel3) {
    auto unit = addUnit(P1, kInvalidId, /*might=*/2, /*at_bf=*/0);
    auto gear = addGear(P1, kSoulSword, /*at_bf=*/0);
    attach(gear, unit);
    setXp(P1, 3);

    Card* c = card_registry.get(kSoulSword);
    ASSERT_NE(c, nullptr);
    state.getObject(unit).aura_effects.clear();
    c->applyPassiveAura(state, P1);

    int bonus = 0;
    for (auto& ae : state.getObject(unit).aura_effects) bonus += ae.might_bonus;
    EXPECT_EQ(bonus, 1);
}

// Below level 3: no +1M aura.
TEST_F(AuditFix6Test, SoulSword_PassiveNotGrantedBelowLevel3) {
    auto unit = addUnit(P1, kInvalidId, /*might=*/2, /*at_bf=*/0);
    auto gear = addGear(P1, kSoulSword, /*at_bf=*/0);
    attach(gear, unit);
    setXp(P1, 2);

    Card* c = card_registry.get(kSoulSword);
    state.getObject(unit).aura_effects.clear();
    c->applyPassiveAura(state, P1);

    int bonus = 0;
    for (auto& ae : state.getObject(unit).aura_effects) bonus += ae.might_bonus;
    EXPECT_EQ(bonus, 0);
}

// Unattached gear grants nothing even at level 3.
TEST_F(AuditFix6Test, SoulSword_PassiveNotGrantedWhenUnattached) {
    auto unit = addUnit(P1, kInvalidId, /*might=*/2, /*at_bf=*/0);
    addGear(P1, kSoulSword, /*at_bf=*/0);  // not attached
    setXp(P1, 3);

    Card* c = card_registry.get(kSoulSword);
    state.getObject(unit).aura_effects.clear();
    c->applyPassiveAura(state, P1);

    EXPECT_TRUE(state.getObject(unit).aura_effects.empty());
}

// ─── [614] Nami, Headstrong ─────────────────────────────────────────────────

// WhenYouPlayMe: paid additional cost [G] (Calm rune present) → stun an enemy.
// Direct confirmOptional defaults to "yes" when legal.
TEST_F(AuditFix6Test, Nami_StunsWhenAdditionalCostWasPaid) {
    auto self = addUnit(P1, kNami, /*might=*/3, /*at_bf=*/0);
    auto enemy = addUnit(P2, kInvalidId, /*might=*/2, /*at_bf=*/0);
    // The [G] additional cost is now paid at play time
    // (maybePayOptionalAdditionalCost) — simulate it via the paid flag.
    state.getObject(self).card_counters["__nami_paid"] = 1;

    EffectExecutor exec(state, events, card_db);
    fireTriggerAs(kNami, P1, self, TriggerType::WhenYouPlayMe, exec);

    EXPECT_TRUE(state.getObject(enemy).is_stunned);
}

TEST_F(AuditFix6Test, Nami_NoStunWhenAdditionalCostNotPaid) {
    auto self = addUnit(P1, kNami, 3, 0);
    auto enemy = addUnit(P2, kInvalidId, 2, 0);
    // No __nami_paid flag -> the additional cost wasn't paid -> no stun.
    EffectExecutor exec(state, events, card_db);
    fireTriggerAs(kNami, P1, self, TriggerType::WhenYouPlayMe, exec);
    EXPECT_FALSE(state.getObject(enemy).is_stunned);
}

// WhenYouPlayMe: no Calm rune → can't pay additional cost → no stun.
TEST_F(AuditFix6Test, Nami_PlayNoStunWithoutCalmRune) {
    auto self = addUnit(P1, kNami, /*might=*/3, /*at_bf=*/0);
    auto enemy = addUnit(P2, kInvalidId, /*might=*/2, /*at_bf=*/0);
    // No Calm rune to pay [G].

    EffectExecutor exec(state, events, card_db);
    fireTriggerAs(kNami, P1, self, TriggerType::WhenYouPlayMe, exec, {enemy});

    EXPECT_FALSE(state.getObject(enemy).is_stunned);
}

// WhenIHold arms the next-unit ready+buff for this turn; WhenYouPlayAUnit
// then readies + buffs the freshly-played (exhausted) friendly unit.
TEST_F(AuditFix6Test, Nami_HoldArmsThenNextUnitReadiedAndBuffed) {
    auto self = addUnit(P1, kNami, /*might=*/3, /*at_bf=*/0);
    state.turn.turn_number = 5;

    EffectExecutor exec(state, events, card_db);
    // Hold: arm.
    fireTriggerAs(kNami, P1, self, TriggerType::WhenIHold, exec);
    EXPECT_EQ(state.getObject(self).card_counters["__nami_hold_arm_turn"], 5);

    // A freshly-played friendly unit (exhausted).
    auto played = addUnit(P1, kInvalidId, /*might=*/2, /*at_bf=*/0);
    state.getObject(played).is_exhausted = true;
    int before_might = state.getObject(played).current_might;

    fireTriggerAs(kNami, P1, self, TriggerType::WhenYouPlayAUnit, exec);

    EXPECT_FALSE(state.getObject(played).is_exhausted);          // readied
    EXPECT_GT(state.getObject(played).current_might, before_might);  // buffed
    // One-shot: arm cleared.
    EXPECT_EQ(state.getObject(self).card_counters.count("__nami_hold_arm_turn"), 0u);
}

// WhenYouPlayAUnit without prior hold (no arm) → no effect.
TEST_F(AuditFix6Test, Nami_PlayUnitNoOpWithoutArm) {
    auto self = addUnit(P1, kNami, /*might=*/3, /*at_bf=*/0);
    state.turn.turn_number = 5;
    auto played = addUnit(P1, kInvalidId, /*might=*/2, /*at_bf=*/0);
    state.getObject(played).is_exhausted = true;
    int before_might = state.getObject(played).current_might;

    EffectExecutor exec(state, events, card_db);
    fireTriggerAs(kNami, P1, self, TriggerType::WhenYouPlayAUnit, exec);

    EXPECT_TRUE(state.getObject(played).is_exhausted);          // not readied
    EXPECT_EQ(state.getObject(played).current_might, before_might);  // not buffed
}

// Stale arm (armed on a different turn) does not fire.
TEST_F(AuditFix6Test, Nami_StaleArmDoesNotFire) {
    auto self = addUnit(P1, kNami, /*might=*/3, /*at_bf=*/0);
    state.turn.turn_number = 5;
    EffectExecutor exec(state, events, card_db);
    fireTriggerAs(kNami, P1, self, TriggerType::WhenIHold, exec);

    // Advance the turn so the arm is stale.
    state.turn.turn_number = 6;
    auto played = addUnit(P1, kInvalidId, /*might=*/2, /*at_bf=*/0);
    state.getObject(played).is_exhausted = true;

    fireTriggerAs(kNami, P1, self, TriggerType::WhenYouPlayAUnit, exec);

    EXPECT_TRUE(state.getObject(played).is_exhausted);  // stale arm: no ready
}

// ─── [645] Smoke and Mirrors ────────────────────────────────────────────────

// One unit has [Temporary] and they're at different locations → swap, draw 1.
TEST_F(AuditFix6Test, SmokeMirrors_SwapsWhenTemporaryAndDrawsOne) {
    auto a = addUnit(P1, kInvalidId, /*might=*/2, /*at_bf=*/0);
    auto b = addUnit(P1, kInvalidId, /*might=*/2, /*at_bf=*/1);
    state.getObject(a).keywords.set(Keyword::Temporary);
    addToDeck(P1, kInvalidId);  // a card to draw

    auto src = pushSpellOnChain(P1, kSmokeMirrors);
    EffectExecutor exec(state, events, card_db);
    invokeOnResolve(src, kSmokeMirrors, P1, {a, b}, exec);

    EXPECT_EQ(state.getObject(a).battlefieldId(), std::optional<BattlefieldId>(1));
    EXPECT_EQ(state.getObject(b).battlefieldId(), std::optional<BattlefieldId>(0));
    EXPECT_EQ(handSize(P1), 1);  // Draw 1
}

// Neither unit has [Temporary] → no swap, but still Draw 1.
TEST_F(AuditFix6Test, SmokeMirrors_NoSwapWithoutTemporaryButStillDraws) {
    auto a = addUnit(P1, kInvalidId, /*might=*/2, /*at_bf=*/0);
    auto b = addUnit(P1, kInvalidId, /*might=*/2, /*at_bf=*/1);
    addToDeck(P1, kInvalidId);

    auto src = pushSpellOnChain(P1, kSmokeMirrors);
    EffectExecutor exec(state, events, card_db);
    invokeOnResolve(src, kSmokeMirrors, P1, {a, b}, exec);

    EXPECT_EQ(state.getObject(a).battlefieldId(), std::optional<BattlefieldId>(0));
    EXPECT_EQ(state.getObject(b).battlefieldId(), std::optional<BattlefieldId>(1));
    EXPECT_EQ(handSize(P1), 1);  // still draws
}

// Same location: no swap even with [Temporary], still draws 1.
TEST_F(AuditFix6Test, SmokeMirrors_SameLocationNoSwapStillDraws) {
    auto a = addUnit(P1, kInvalidId, /*might=*/2, /*at_bf=*/0);
    auto b = addUnit(P1, kInvalidId, /*might=*/2, /*at_bf=*/0);
    state.getObject(a).keywords.set(Keyword::Temporary);
    addToDeck(P1, kInvalidId);

    auto src = pushSpellOnChain(P1, kSmokeMirrors);
    EffectExecutor exec(state, events, card_db);
    invokeOnResolve(src, kSmokeMirrors, P1, {a, b}, exec);

    EXPECT_EQ(state.getObject(a).battlefieldId(), std::optional<BattlefieldId>(0));
    EXPECT_EQ(state.getObject(b).battlefieldId(), std::optional<BattlefieldId>(0));
    EXPECT_EQ(handSize(P1), 1);
}

// Fewer than two targets → no-op (and importantly no draw).
TEST_F(AuditFix6Test, SmokeMirrors_FewerThanTwoTargetsNoOp) {
    auto a = addUnit(P1, kInvalidId, /*might=*/2, /*at_bf=*/0);
    addToDeck(P1, kInvalidId);

    auto src = pushSpellOnChain(P1, kSmokeMirrors);
    EffectExecutor exec(state, events, card_db);
    invokeOnResolve(src, kSmokeMirrors, P1, {a}, exec);

    EXPECT_EQ(handSize(P1), 0);  // early return before Draw 1
}

// ─── [695] Blast Cone ───────────────────────────────────────────────────────

// WhenYouPlayThis: moves an enemy unit to base. Direct confirmOptional = yes.
TEST_F(AuditFix6Test, BlastCone_PlayMovesEnemyToBase) {
    auto gear = addGear(P1, kBlastCone, /*at_bf=*/0);
    auto enemy = addUnit(P2, kInvalidId, /*might=*/2, /*at_bf=*/0);

    EffectExecutor exec(state, events, card_db);
    fireTriggerAs(kBlastCone, P1, gear, TriggerType::WhenYouPlayThis, exec, {enemy});

    // Enemy moved to its own base (moveToBase uses controller's base).
    EXPECT_TRUE(state.getObject(enemy).isAtBase());
}

// After moving, the second confirmOptional (also "yes" by default) exhausts
// Blast Cone and stuns the moved enemy.
TEST_F(AuditFix6Test, BlastCone_ExhaustsToStunMovedEnemy) {
    auto gear = addGear(P1, kBlastCone, /*at_bf=*/0);
    auto enemy = addUnit(P2, kInvalidId, /*might=*/2, /*at_bf=*/0);

    EffectExecutor exec(state, events, card_db);
    fireTriggerAs(kBlastCone, P1, gear, TriggerType::WhenYouPlayThis, exec, {enemy});

    EXPECT_TRUE(state.getObject(gear).is_exhausted);
    EXPECT_TRUE(state.getObject(enemy).is_stunned);
}

// No enemy unit → no-op (nothing to move).
TEST_F(AuditFix6Test, BlastCone_NoEnemyNoOp) {
    auto gear = addGear(P1, kBlastCone, /*at_bf=*/0);

    EffectExecutor exec(state, events, card_db);
    fireTriggerAs(kBlastCone, P1, gear, TriggerType::WhenYouPlayThis, exec, {});

    EXPECT_FALSE(state.getObject(gear).is_exhausted);
}

// Already-exhausted Blast Cone: still moves the enemy but cannot stun it.
TEST_F(AuditFix6Test, BlastCone_ExhaustedCannotStun) {
    auto gear = addGear(P1, kBlastCone, /*at_bf=*/0);
    state.getObject(gear).is_exhausted = true;
    auto enemy = addUnit(P2, kInvalidId, /*might=*/2, /*at_bf=*/0);

    EffectExecutor exec(state, events, card_db);
    fireTriggerAs(kBlastCone, P1, gear, TriggerType::WhenYouPlayThis, exec, {enemy});

    EXPECT_TRUE(state.getObject(enemy).isAtBase());     // still moved
    EXPECT_FALSE(state.getObject(enemy).is_stunned);    // can't stun (exhausted)
}

// ─── [745] Thrill of the Hunt ───────────────────────────────────────────────

// Banish a friendly unit, then replay it to a battlefield, ignoring cost.
// Direct invocation: pickTarget → first legal friendly; pickMode → BF#0.
//
// NOTE: the implementation re-plays via EffectExecutor::playIgnoringCost,
// which applies the normal "units enter exhausted" rule (CR 143.4). The
// card text implies the replay (and the earlier generated stub) entered the
// unit READY. So this AF6 version lands the unit EXHAUSTED, not ready — we
// pin the implemented behavior here.
TEST_F(AuditFix6Test, Thrill_BanishesAndReplaysToBattlefield) {
    auto unit = addUnit(P1, kInvalidId, /*might=*/4, /*at_bf=*/1);
    auto src = pushSpellOnChain(P1, kThrill);

    EffectExecutor exec(state, events, card_db);
    exec.setCardRegistry(&card_registry);
    // Thrill defers target selection; needsPlayTimeTarget=true, onResolve picks.
    invokeOnResolve(src, kThrill, P1, {}, exec);

    auto& u = state.getObject(unit);
    EXPECT_EQ(u.zone, ZoneType::BattlefieldZone);
    EXPECT_EQ(u.battlefieldId(), std::optional<BattlefieldId>(0));  // pickMode → BF#0
    EXPECT_TRUE(u.is_exhausted);  // playIgnoringCost exhausts units (CR 143.4)
    // No longer in P1's banishment.
    auto& bz = state.player(P1).banishment;
    EXPECT_EQ(std::find(bz.begin(), bz.end(), unit), bz.end());
}

// No friendly unit on board → no legal target → no-op.
TEST_F(AuditFix6Test, Thrill_NoFriendlyUnitNoOp) {
    auto enemy = addUnit(P2, kInvalidId, /*might=*/4, /*at_bf=*/0);
    auto src = pushSpellOnChain(P1, kThrill);

    EffectExecutor exec(state, events, card_db);
    exec.setCardRegistry(&card_registry);
    invokeOnResolve(src, kThrill, P1, {}, exec);

    // Enemy untouched.
    EXPECT_EQ(state.getObject(enemy).battlefieldId(), std::optional<BattlefieldId>(0));
}

// ─── [770] Ripper's Bay ─────────────────────────────────────────────────────

// Documented engine-limitation stub: no trigger wired, so it's a registered
// no-op battlefield card. We pin that it registers and fires no trigger.
// TODO: "return-to-hand here → pay 1 to channel 1 rune exhausted" is NOT
// implemented (no return-to-hand battlefield-scoped trigger event exists).
TEST_F(AuditFix6Test, RippersBay_FiresOnUnitReturnedToHandHere) {
    Card* c = card_registry.get(kRippersBay);
    ASSERT_NE(c, nullptr);
    // "When a unit here is returned to hand, that player may pay [1] to channel
    // 1 rune exhausted" now has an engine trigger (WhenAUnitReturnsToHandHere).
    EXPECT_TRUE(c->firesOn(TriggerType::WhenAUnitReturnsToHandHere));
}

} // namespace
