/// @file test_vendetta_A.cpp
/// Vendetta batch A: Fury / Calm units (ids 789-835).

#include "tests/cards/card_test_fixture.h"

#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/card_registry.h"
#include "core/events.h"
#include "core/game_state.h"
#include "engine/effect_executor.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

namespace riftbound::test {
namespace test_vendetta_A {  // file-unique: test sources are unity-built
namespace {

constexpr CardDefId kBladeTwirler      = 789;
constexpr CardDefId kDuneSurfer        = 791;
constexpr CardDefId kForsakenBaccai    = 792;
constexpr CardDefId kOasisRaider       = 793;
constexpr CardDefId kBaccaiReaper      = 796;
constexpr CardDefId kShadowAssassin    = 800;
constexpr CardDefId kShadowFiend       = 801;
constexpr CardDefId kEclipseDragon     = 803;
constexpr CardDefId kMorgana           = 804;
constexpr CardDefId kRenekton          = 806;
constexpr CardDefId kTwilightReveler   = 807;
constexpr CardDefId kAkaliDeadlyWeapon = 808;
constexpr CardDefId kZedFromTheShadows = 810;
constexpr CardDefId kAffectionatePoro  = 811;
constexpr CardDefId kHierophant        = 812;
constexpr CardDefId kFieldMusicians    = 813;
constexpr CardDefId kMournfulWitness   = 815;
constexpr CardDefId kOlPoro            = 816;
constexpr CardDefId kSereneAscetic     = 817;
constexpr CardDefId kFrostcoatMother   = 819;
constexpr CardDefId kPakaaProtector    = 820;
constexpr CardDefId kSandstoneChimera  = 823;
constexpr CardDefId kBarbara           = 824;
constexpr CardDefId kAkaliSilent       = 825;
constexpr CardDefId kRiven             = 828;
constexpr CardDefId kShen              = 829;
constexpr CardDefId kSteelPaws         = 830;
constexpr CardDefId kAstralHeron       = 831;
constexpr CardDefId kNasusAscended     = 833;
constexpr CardDefId kCloudDrake        = 835;
// Fillers.
constexpr CardDefId kDredgeUp          = 836;   // a spell
constexpr CardDefId kPunchingPoro      = 794;   // a unit

int auraMightSum(const GameObject& obj) {
    int n = 0;
    for (const auto& ae : obj.aura_effects) n += ae.might_bonus;
    return n;
}

int auraKeywordValue(const GameObject& obj, Keyword kw) {
    for (const auto& ae : obj.aura_effects)
        if (ae.keyword == kw && ae.keyword_value != kAuraOnceMarker) return ae.keyword_value;
    return -1;  // not granted
}

Intent pickValue(const std::vector<Intent>& legal, int value) {
    for (const auto& i : legal)
        if (i.chosen_value.has_value() && *i.chosen_value == value) return i;
    return legal.empty() ? Intent{} : legal.front();
}

class VendettaA : public CardTestFixture {
protected:
    void runes(PlayerId p, int n, Domain d = Domain::Fury) {
        for (int i = 0; i < n; ++i) addRune(p, d);
    }
    GameObjectId addToTrash(PlayerId p, CardDefId def) {
        auto id = addToHand(p, def);
        auto& h = state.player(p).hand;
        h.erase(std::remove(h.begin(), h.end(), id), h.end());
        state.getObject(id).zone = ZoneType::Trash;
        state.player(p).trash.push_back(id);
        return id;
    }
    GameObjectId addGear(PlayerId p, bool equipment = false) {
        auto id = addUnit(p, kInvalidId);
        auto& g = state.getObject(id);
        g.card_type = CardType::Gear;
        g.name = "TestGear";
        if (equipment) g.tags = {"Equipment"};
        withEquipmentCard(id);   // CR 821.1.c.4: see the fixture
        return id;
    }
    void clearAuras() {
        for (auto& [id, obj] : state.objects) {
            obj.aura_effects.clear();
            obj.immune_to_damage = false;
            obj.prevent_enemy_spell_ability_damage = false;
            obj.untargetable_by_enemy = false;
        }
        state.turn.channel_phase_cap = 0;
        for (auto& bf : state.battlefields) bf.ignore_tank[0] = bf.ignore_tank[1] = false;
    }
    void aura(CardDefId def, PlayerId p) {
        const Card* c = card_registry.get(def);
        ASSERT_NE(c, nullptr);
        c->applyPassiveAura(state, p);
    }
    const ActivatedAbility empowerOf(CardDefId def) {
        const Card* c = card_registry.get(def);
        EXPECT_NE(c, nullptr);
        const int idx = c->empowerAbilityIndex();
        EXPECT_GE(idx, 0);
        auto abs = c->activatedAbilities();
        EXPECT_TRUE(abs.at(idx).is_empower);
        return abs.at(idx);
    }
};

// ── 789 Blade Twirler ────────────────────────────────────────────────────

TEST_F(VendettaA, Vendetta_BladeTwirler_FirstMoveBurnsChosenPlayerOncePerTurn) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto unit = addUnit(P1, kBladeTwirler, 0, 0);
    for (int i = 0; i < 3; ++i) { addToDeck(P1, kDredgeUp); addToDeck(P2, kDredgeUp); }

    // "The first time I move each turn" is the trigger's condition (the move
    // count the engine keeps), so the moves go through the engine.
    (void)exec;
    // Mode 0 = the opponent.
    state.getObject(unit).moves_this_turn = 1;
    EnginePathAgent them(EnginePathAgent::script({0}));
    fireTriggerThroughEngine(unit, TriggerType::WhenIMove, &them);
    EXPECT_EQ(trashSize(P2), 1);
    EXPECT_EQ(trashSize(P1), 0);

    // Second move the same turn: not "the first time".
    state.getObject(unit).moves_this_turn = 2;
    EnginePathAgent again(EnginePathAgent::script({0}));
    fireTriggerThroughEngine(unit, TriggerType::WhenIMove, &again);
    EXPECT_EQ(trashSize(P2), 1);

    // Next turn it works again, and the controller may choose themself.
    state.turn.turn_number++;
    state.getObject(unit).moves_this_turn = 1;
    EnginePathAgent me(EnginePathAgent::script({1}));
    fireTriggerThroughEngine(unit, TriggerType::WhenIMove, &me);
    EXPECT_EQ(trashSize(P1), 1);
    EXPECT_EQ(trashSize(P2), 1);
}

// ── 791 Dune Surfer ──────────────────────────────────────────────────────

TEST_F(VendettaA, Vendetta_DuneSurfer_IgnoreTankOnlyAtMyBattlefieldForMe) {
    auto unit = addUnit(P1, kDuneSurfer, 0, 0);
    aura(kDuneSurfer, P1);
    EXPECT_TRUE(state.battlefields[0].ignore_tank[playerIndex(P1)]);
    EXPECT_FALSE(state.battlefields[0].ignore_tank[playerIndex(P2)]);
    EXPECT_FALSE(state.battlefields[1].ignore_tank[playerIndex(P1)]);

    clearAuras();
    state.getObject(unit).location = BaseLocation{P1};
    state.getObject(unit).zone = ZoneType::Base;
    aura(kDuneSurfer, P1);
    EXPECT_FALSE(state.battlefields[0].ignore_tank[playerIndex(P1)]) << "not 'here' from base";
}

// ── 792 Forsaken Baccai / 793 Oasis Raider ───────────────────────────────

TEST_F(VendettaA, Vendetta_ForsakenBaccai_BuffOnlyWithFewerRunes) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto unit = addUnit(P1, kForsakenBaccai, 0, 0);
    runes(P1, 2);
    runes(P2, 2);
    fireTriggerAs(kForsakenBaccai, P1, unit, TriggerType::AtStartOfBeginning, exec);
    EXPECT_EQ(state.getObject(unit).current_might, 2) << "equal runes is not fewer";

    runes(P2, 1);
    fireTriggerAs(kForsakenBaccai, P1, unit, TriggerType::AtStartOfBeginning, exec);
    EXPECT_EQ(state.getObject(unit).current_might, 3);
    EXPECT_EQ(state.getObject(unit).temp_might_bonus, 1) << "this turn only";
}

TEST_F(VendettaA, Vendetta_OasisRaider_MightAndGankingWithFewerRunes) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    EXPECT_TRUE(card_registry.get(kOasisRaider)->firesOn(TriggerType::AtStartOfBeginning));
    auto unit = addUnit(P1, kOasisRaider, 0, 0);
    runes(P1, 3);
    runes(P2, 3);
    fireTriggerAs(kOasisRaider, P1, unit, TriggerType::AtStartOfBeginning, exec);
    EXPECT_EQ(state.getObject(unit).current_might, 4);
    EXPECT_FALSE(state.getObject(unit).hasKeyword(Keyword::Ganking));

    runes(P2, 1);
    fireTriggerAs(kOasisRaider, P1, unit, TriggerType::AtStartOfBeginning, exec);
    EXPECT_EQ(state.getObject(unit).current_might, 6);
    EXPECT_TRUE(state.getObject(unit).hasKeyword(Keyword::Ganking));
    EXPECT_TRUE(state.getObject(unit).temp_keywords.has(Keyword::Ganking)) << "this turn only";
}

// ── 796 Baccai Reaper ────────────────────────────────────────────────────

TEST_F(VendettaA, Vendetta_BaccaiReaper_PayFuryForAssault) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto unit = addUnit(P1, kBaccaiReaper, 0, 0);
    state.getObject(unit).combat_designation = CombatDesignation::Attacker;
    addRune(P1, Domain::Fury);

    // Decline: nothing paid, nothing gained.
    driveResumableTrigger(kBaccaiReaper, P1, unit,
        [](const std::vector<Intent>& legal) { return pickValue(legal, 0); },
        exec, TriggerType::WhenIAttack);
    EXPECT_EQ(state.getObject(unit).assault_value, 0);
    EXPECT_EQ(readyRuneCount(P1), 1);

    // Accept: one Fury power paid, [Assault 2] this turn.
    driveResumableTrigger(kBaccaiReaper, P1, unit,
        [](const std::vector<Intent>& legal) { return pickValue(legal, 1); },
        exec, TriggerType::WhenIAttack);
    EXPECT_EQ(state.getObject(unit).assault_value, 2);
    EXPECT_EQ(state.getObject(unit).temp_assault_value, 2);
    EXPECT_EQ(state.getObject(unit).current_might, 6) << "4 + Assault 2 while attacking";
    EXPECT_EQ(readyRuneCount(P1), 0);
}

TEST_F(VendettaA, Vendetta_BaccaiReaper_NotOfferedWithoutFuryPower) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto unit = addUnit(P1, kBaccaiReaper, 0, 0);
    addRune(P1, Domain::Calm);  // wrong domain
    int prompts = 0;
    driveResumableTrigger(kBaccaiReaper, P1, unit,
        [&](const std::vector<Intent>& legal) { ++prompts; return pickValue(legal, 1); },
        exec, TriggerType::WhenIAttack);
    EXPECT_EQ(prompts, 0);
    EXPECT_EQ(state.getObject(unit).assault_value, 0);
    EXPECT_EQ(readyRuneCount(P1), 1);
}

// ── 800 Shadow Assassin ──────────────────────────────────────────────────

TEST_F(VendettaA, Vendetta_ShadowAssassin_EntersReadyWithNamesakeInTrash) {
    const Card* c = card_registry.get(kShadowAssassin);
    ASSERT_NE(c, nullptr);
    addToTrash(P1, kPunchingPoro);
    addToTrash(P2, kShadowAssassin);  // the opponent's trash doesn't count
    EXPECT_FALSE(c->entersReadyOnPlay(state, P1));
    addToTrash(P1, kShadowAssassin);
    EXPECT_TRUE(c->entersReadyOnPlay(state, P1));
}

// ── 801 Shadow Fiend ─────────────────────────────────────────────────────

TEST_F(VendettaA, Vendetta_ShadowFiend_EmpowerCostAndAssault) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto cost = empowerOf(kShadowFiend).cost;
    EXPECT_EQ(cost.energy, 2);
    EXPECT_EQ(cost.power, 1);
    EXPECT_EQ(cost.power_domain, Domain::Fury);
    EXPECT_FALSE(cost.power_any_domain);

    auto unit = addUnit(P1, kShadowFiend, 0, 0);
    aura(kShadowFiend, P1);
    EXPECT_EQ(auraKeywordValue(state.getObject(unit), Keyword::Assault), -1);

    clearAuras();
    exec.empower(unit);
    aura(kShadowFiend, P1);
    EXPECT_EQ(auraKeywordValue(state.getObject(unit), Keyword::Assault), 3);
}

// ── 803 Eclipse Dragon ───────────────────────────────────────────────────

TEST_F(VendettaA, Vendetta_EclipseDragon_DrawsOnMoveWithFourOrFewerRunes) {
    // CR 383.2.a.1: the printed condition is part of the trigger condition, so it is
    // judged when the event happens (Card::triggerCondition) — the trigger goes
    // through the engine, which is what asks it; false = nothing on the chain.
    EffectExecutor exec(state, events, card_db, &card_registry);
    EXPECT_TRUE(card_db.get(kEclipseDragon).keywords.has(Keyword::Accelerate));
    auto unit = addUnit(P1, kEclipseDragon, 0, 0);
    for (int i = 0; i < 3; ++i) addToDeck(P1, kDredgeUp);
    runes(P1, 4);
    fireTriggerThroughEngine(unit, TriggerType::WhenIMove);
    EXPECT_EQ(handSize(P1), 1);
    runes(P1, 1);
    fireTriggerThroughEngine(unit, TriggerType::WhenIMove);
    EXPECT_EQ(handSize(P1), 1) << "5 runes: no draw";
}

// ── 804 Morgana, Vindictive ──────────────────────────────────────────────

TEST_F(VendettaA, Vendetta_Morgana_DealsDamageEqualToMarkedDamage) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto morgana = addUnit(P1, kMorgana, 0, 0);
    auto hurt = addUnit(P2, kInvalidId, 6, 0);
    auto fresh = addUnit(P2, kInvalidId, 6, 0);
    state.getObject(hurt).damage_marked = 2;

    // Through the engine: the controller chooses the unit (a trigger is never
    // handed targets — plans/07 item 7.3).
    EnginePathAgent pick_hurt(EnginePathAgent::choosing(hurt));
    fireTriggerThroughEngine(morgana, TriggerType::WhenYouPlayMe, &pick_hurt);
    EXPECT_EQ(pick_hurt.prompts.size(), 1u);
    EXPECT_EQ(state.getObject(hurt).damage_marked, 4);
    EnginePathAgent pick_fresh(EnginePathAgent::choosing(fresh));
    fireTriggerThroughEngine(morgana, TriggerType::WhenYouPlayMe, &pick_fresh);
    EXPECT_EQ(state.getObject(fresh).damage_marked, 0) << "no damage marked -> deals 0";
}

TEST_F(VendettaA, Vendetta_Morgana_ChoosesUnitAtResolution) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto morgana = addUnit(P1, kMorgana, 0, 0);
    auto a = addUnit(P2, kInvalidId, 6, 0);
    auto b = addUnit(P2, kInvalidId, 6, 1);
    state.getObject(a).damage_marked = 1;
    state.getObject(b).damage_marked = 3;
    driveResumableTrigger(kMorgana, P1, morgana,
        [&](const std::vector<Intent>& legal) {
            for (const auto& i : legal)
                if (!i.chosen_objects.empty() && i.chosen_objects[0] == b) return i;
            return legal.front();
        }, exec, TriggerType::WhenYouPlayMe);
    EXPECT_EQ(state.getObject(b).damage_marked, 6);
    EXPECT_EQ(state.getObject(a).damage_marked, 1);
}

// ── 806 Renekton, Rage Fueled ────────────────────────────────────────────

TEST_F(VendettaA, Vendetta_Renekton_DamagesAllEnemiesHereWithFewRunes) {
    // CR 383.2.a.1: the printed condition is part of the trigger condition, so it is
    // judged when the event happens (Card::triggerCondition) — the trigger goes
    // through the engine, which is what asks it; false = nothing on the chain.
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto ren = addUnit(P1, kRenekton, 0, 0);
    auto friendly = addUnit(P1, kInvalidId, 5, 0);
    auto e1 = addUnit(P2, kInvalidId, 5, 0);
    auto e2 = addUnit(P2, kInvalidId, 5, 0);
    auto elsewhere = addUnit(P2, kInvalidId, 5, 1);

    runes(P1, 5);
    fireTriggerThroughEngine(ren, TriggerType::WhenIAttack);
    EXPECT_EQ(state.getObject(e1).damage_marked, 0) << "5 runes: condition fails";

    // Drop to 4 runes.
    for (auto& [id, obj] : state.objects)
        if (obj.isRune() && obj.controller == P1) { obj.location = std::nullopt; break; }
    ASSERT_EQ(runesControlled(state, P1), 4);
    fireTriggerThroughEngine(ren, TriggerType::WhenIAttack);
    EXPECT_EQ(state.getObject(e1).damage_marked, 2);
    EXPECT_EQ(state.getObject(e2).damage_marked, 2);
    EXPECT_EQ(state.getObject(elsewhere).damage_marked, 0);
    EXPECT_EQ(state.getObject(friendly).damage_marked, 0);
    EXPECT_EQ(state.getObject(ren).damage_marked, 0);
}

// ── 807 Twilight Reveler ─────────────────────────────────────────────────

TEST_F(VendettaA, Vendetta_TwilightReveler_ReadiesAnotherFriendlyUnit) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto reveler = addUnit(P1, kTwilightReveler, 0, 0);
    state.getObject(reveler).is_exhausted = true;

    // Alone: nothing to choose, and it can't ready itself.
    fireTriggerAs(kTwilightReveler, P1, reveler, TriggerType::WhenIAttack, exec);
    EXPECT_TRUE(state.getObject(reveler).is_exhausted);

    auto friendly = addUnit(P1, kInvalidId, 2);
    auto enemy = addUnit(P2, kInvalidId, 2, 0);
    state.getObject(friendly).is_exhausted = true;
    state.getObject(enemy).is_exhausted = true;
    bool offered_bad = false;
    driveResumableTrigger(kTwilightReveler, P1, reveler,
        [&](const std::vector<Intent>& legal) {
            for (const auto& i : legal)
                for (auto t : i.chosen_objects)
                    if (t == reveler || t == enemy) offered_bad = true;
            return legal.front();
        }, exec, TriggerType::WhenIAttack);
    EXPECT_FALSE(offered_bad);
    EXPECT_FALSE(state.getObject(friendly).is_exhausted);
    EXPECT_TRUE(state.getObject(reveler).is_exhausted);
    EXPECT_TRUE(state.getObject(enemy).is_exhausted);
}

// ── 808 Akali, Deadly Weapon ─────────────────────────────────────────────

TEST_F(VendettaA, Vendetta_AkaliDeadlyWeapon_MoveDamageScalesWithEmpowered) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto cost = empowerOf(kAkaliDeadlyWeapon).cost;
    EXPECT_EQ(cost.energy, 2);
    EXPECT_EQ(cost.power, 1);
    EXPECT_EQ(cost.power_domain, Domain::Fury);

    auto akali = addUnit(P1, kAkaliDeadlyWeapon, 0, 0);
    auto enemy = addUnit(P2, kInvalidId, 9, 0);
    auto far_enemy = addUnit(P2, kInvalidId, 9, 1);
    auto target_enemy = [&](const std::vector<Intent>& legal) {
        for (const auto& i : legal)
            if (!i.chosen_objects.empty() && i.chosen_objects[0] == enemy) return i;
        for (const auto& i : legal)
            for (auto t : i.chosen_objects) EXPECT_NE(t, far_enemy);
        return pickValue(legal, 1);  // "yes" on the may-prompt
    };

    // The move that triggered it travels with the trigger (ChainItem::triggering_move).
    AnnouncedMove moved(state, LocationId{BaseLocation{P1}}, LocationId{BattlefieldLocation{0}});
    driveResumableTrigger(kAkaliDeadlyWeapon, P1, akali, target_enemy, exec, TriggerType::WhenIMove);
    EXPECT_EQ(state.getObject(enemy).damage_marked, 1);

    exec.empower(akali);
    driveResumableTrigger(kAkaliDeadlyWeapon, P1, akali, target_enemy, exec, TriggerType::WhenIMove);
    EXPECT_EQ(state.getObject(enemy).damage_marked, 3) << "Empowered: 2 instead";
    EXPECT_EQ(state.getObject(far_enemy).damage_marked, 0);

    // "you may": declining deals nothing.
    driveResumableTrigger(kAkaliDeadlyWeapon, P1, akali,
        [](const std::vector<Intent>& legal) { return pickValue(legal, 0); },
        exec, TriggerType::WhenIMove);
    EXPECT_EQ(state.getObject(enemy).damage_marked, 3);
}

TEST_F(VendettaA, Vendetta_AkaliDeadlyWeapon_MayHitTheBattlefieldSheMovedFrom) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto akali = addUnit(P1, kAkaliDeadlyWeapon, 0, 0);
    auto left_behind = addUnit(P2, kInvalidId, 9, 1);
    auto pick_left_behind = [&](const std::vector<Intent>& legal) {
        for (const auto& i : legal)
            if (!i.chosen_objects.empty() && i.chosen_objects[0] == left_behind) return i;
        return pickValue(legal, 1);  // "yes" on the may-prompt
    };

    // Moved from a base: nothing at battlefield 1 can be chosen.
    {
        AnnouncedMove moved(state, LocationId{BaseLocation{P1}}, LocationId{BattlefieldLocation{0}});
        driveResumableTrigger(kAkaliDeadlyWeapon, P1, akali, pick_left_behind, exec,
                              TriggerType::WhenIMove);
    }
    EXPECT_EQ(state.getObject(left_behind).damage_marked, 0);

    // Moved from battlefield 1 to battlefield 0: units at 1 are legal too.
    {
        AnnouncedMove moved(state, LocationId{BattlefieldLocation{1}}, LocationId{BattlefieldLocation{0}});
        driveResumableTrigger(kAkaliDeadlyWeapon, P1, akali, pick_left_behind, exec,
                              TriggerType::WhenIMove);
    }
    EXPECT_EQ(state.getObject(left_behind).damage_marked, 1);
}

TEST_F(VendettaA, Vendetta_AkaliDeadlyWeapon_EmpoweredMight) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto akali = addUnit(P1, kAkaliDeadlyWeapon, 0, 0);
    aura(kAkaliDeadlyWeapon, P1);
    EXPECT_EQ(auraMightSum(state.getObject(akali)), 0);
    clearAuras();
    exec.empower(akali);
    aura(kAkaliDeadlyWeapon, P1);
    EXPECT_EQ(auraMightSum(state.getObject(akali)), 1);
}

// ── 810 Zed, From the Shadows ────────────────────────────────────────────

TEST_F(VendettaA, Vendetta_Zed_ShadowCloneOnlyIfAdditionalCostPaid) {
    // CR 383.2.a.1: the printed condition is part of the trigger condition, so it is
    // judged when the event happens (Card::triggerCondition) — the trigger goes
    // through the engine, which is what asks it; false = nothing on the chain.
    EffectExecutor exec(state, events, card_db, &card_registry);
    const Card* c = card_registry.get(kZedFromTheShadows);
    ASSERT_NE(c, nullptr);
    auto oac = c->optionalAdditionalCost();
    ASSERT_TRUE(oac.valid);
    EXPECT_EQ(oac.discard_cards, 1);
    EXPECT_EQ(oac.reduce_energy, 0);
    const std::string flag = oac.paid_flag;
    ASSERT_FALSE(flag.empty());

    auto count_clones = [&]() {
        int n = 0;
        for (const auto& [id, obj] : state.objects)
            if (obj.name == "Shadow Clone" && obj.isUnit() && obj.isToken() &&
                obj.controller == P1 && obj.location.has_value()) {
                EXPECT_EQ(obj.base_might, 0);
                ++n;
            }
        return n;
    };

    auto zed = addUnit(P1, kZedFromTheShadows, 0, 0);
    fireTriggerThroughEngine(zed, TriggerType::WhenYouPlayMe);
    EXPECT_EQ(count_clones(), 0) << "additional cost not paid";

    state.getObject(zed).card_counters[flag] = 1;
    fireTriggerThroughEngine(zed, TriggerType::WhenYouPlayMe);
    EXPECT_EQ(count_clones(), 1);
    EXPECT_EQ(state.getObject(zed).card_counters[flag], 0) << "payment consumed";
}

// ── 811 Affectionate Poro ────────────────────────────────────────────────

TEST_F(VendettaA, Vendetta_AffectionatePoro_DrawsOnlyIfUndamagedThisTurn) {
    // CR 383.2.a.1: the printed condition is part of the trigger condition, so it is
    // judged when the event happens (Card::triggerCondition) — the trigger goes
    // through the engine, which is what asks it; false = nothing on the chain.
    EffectExecutor exec(state, events, card_db, &card_registry);
    EXPECT_TRUE(card_registry.get(kAffectionatePoro)->firesOn(TriggerType::WhenACombatIWasInEnds));
    auto poro = addUnit(P1, kAffectionatePoro, 5, 0);
    auto enemy = addUnit(P2, kInvalidId, 1, 0);
    for (int i = 0; i < 4; ++i) addToDeck(P1, kDredgeUp);

    // Dealt damage this turn, then healed by combat cleanup: still no draw.
    exec.dealDamage(poro, 1, enemy);
    EXPECT_EQ(state.getObject(poro).damage_taken_this_turn, 1);
    state.getObject(poro).damage_marked = 0;
    fireTriggerThroughEngine(poro, TriggerType::WhenACombatIWasInEnds);
    EXPECT_EQ(handSize(P1), 0);

    // A later turn (tracker reset at end of turn) with no damage: draw.
    state.getObject(poro).damage_taken_this_turn = 0;
    fireTriggerThroughEngine(poro, TriggerType::WhenACombatIWasInEnds);
    EXPECT_EQ(handSize(P1), 1);
}

// ── 812 Esteemed Hierophant ──────────────────────────────────────────────

TEST_F(VendettaA, Vendetta_EsteemedHierophant_PreventsEnemySpellDamageAtSevenRunes) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto unit = addUnit(P1, kHierophant, 9, 0);
    auto enemy_spell = pushSpellOnChain(P2);
    auto friendly_spell = pushSpellOnChain(P1);
    runes(P1, 6);
    aura(kHierophant, P1);
    EXPECT_FALSE(state.getObject(unit).prevent_enemy_spell_ability_damage) << "6 runes";
    exec.dealDamage(unit, 2, enemy_spell);
    EXPECT_EQ(state.getObject(unit).damage_marked, 2);

    clearAuras();
    runes(P1, 1);
    aura(kHierophant, P1);
    EXPECT_TRUE(state.getObject(unit).prevent_enemy_spell_ability_damage) << "7 runes";
    EXPECT_FALSE(state.getObject(unit).immune_to_damage) << "not a blanket immunity";
    exec.dealDamage(unit, 2, enemy_spell);
    EXPECT_EQ(state.getObject(unit).damage_marked, 2) << "enemy spell damage prevented";
    exec.dealDamage(unit, 1, friendly_spell);
    EXPECT_EQ(state.getObject(unit).damage_marked, 3) << "friendly spell damage is not";

    // Still protected while in combat.
    clearAuras();
    state.getObject(unit).combat_designation = CombatDesignation::Defender;
    aura(kHierophant, P1);
    EXPECT_TRUE(state.getObject(unit).prevent_enemy_spell_ability_damage);
}

// ── 813 Field Musicians ──────────────────────────────────────────────────

TEST_F(VendettaA, Vendetta_FieldMusicians_GivesAUnitThreeMightThisTurn) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto musicians = addUnit(P1, kFieldMusicians, 0, 0);
    auto friendly = addUnit(P1, kInvalidId, 2, 0);
    auto other = addUnit(P1, kInvalidId, 2, 1);
    driveResumableTrigger(kFieldMusicians, P1, musicians,
        [&](const std::vector<Intent>& legal) {
            for (const auto& i : legal)
                if (!i.chosen_objects.empty() && i.chosen_objects[0] == friendly) return i;
            return legal.front();
        }, exec, TriggerType::WhenYouPlayMe);
    EXPECT_EQ(state.getObject(friendly).current_might, 5);
    EXPECT_EQ(state.getObject(friendly).temp_might_bonus, 3);
    EXPECT_EQ(state.getObject(other).current_might, 2);
    EXPECT_EQ(state.getObject(musicians).current_might, 3);
}

// ── 815 Mournful Witness ─────────────────────────────────────────────────

TEST_F(VendettaA, Vendetta_MournfulWitness_EmpowersWhenCombatEnds) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    EXPECT_TRUE(card_registry.get(kMournfulWitness)->firesOn(TriggerType::WhenACombatIWasInEnds));
    EXPECT_FALSE(card_registry.get(kMournfulWitness)->empowerSpec().valid) << "no [Empower] keyword";
    auto unit = addUnit(P1, kMournfulWitness, 0, 0);
    aura(kMournfulWitness, P1);
    EXPECT_EQ(auraMightSum(state.getObject(unit)), 0);

    fireTriggerAs(kMournfulWitness, P1, unit, TriggerType::WhenACombatIWasInEnds, exec);
    EXPECT_TRUE(state.getObject(unit).isEmpowered());
    fireTriggerAs(kMournfulWitness, P1, unit, TriggerType::WhenACombatIWasInEnds, exec);
    EXPECT_EQ(state.getObject(unit).empowered, 1) << "already Empowered: nothing more";

    clearAuras();
    aura(kMournfulWitness, P1);
    EXPECT_EQ(auraMightSum(state.getObject(unit)), 2);
}

// ── 816 Ol' Poro ─────────────────────────────────────────────────────────

TEST_F(VendettaA, Vendetta_OlPoro_UnplayableOnFirstThreeTurns) {
    const Card* c = card_registry.get(kOlPoro);
    ASSERT_NE(c, nullptr);
    state.turn.turn_player = P1;
    for (int t = 1; t <= 3; ++t) {
        state.player(P1).turns_taken = t;
        EXPECT_FALSE(c->canBePlayed(state, P1)) << "turn " << t;
    }
    state.player(P1).turns_taken = 4;
    EXPECT_TRUE(c->canBePlayed(state, P1));

    // Not "your" turn: the restriction doesn't apply.
    state.turn.turn_player = P2;
    state.player(P1).turns_taken = 2;
    EXPECT_TRUE(c->canBePlayed(state, P1));
}

// ── 817 Serene Ascetic ───────────────────────────────────────────────────

TEST_F(VendettaA, Vendetta_SereneAscetic_DeflectAndShieldWhileEmpowered) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto cost = empowerOf(kSereneAscetic).cost;
    EXPECT_EQ(cost.energy, 3);
    EXPECT_EQ(cost.power, 0);

    auto unit = addUnit(P1, kSereneAscetic, 0, 0);
    aura(kSereneAscetic, P1);
    EXPECT_EQ(auraKeywordValue(state.getObject(unit), Keyword::Deflect), -1);
    EXPECT_EQ(auraKeywordValue(state.getObject(unit), Keyword::Shield), -1);

    clearAuras();
    exec.empower(unit);
    state.getObject(unit).combat_designation = CombatDesignation::Defender;
    aura(kSereneAscetic, P1);
    EXPECT_EQ(auraKeywordValue(state.getObject(unit), Keyword::Deflect), 1);
    EXPECT_EQ(auraKeywordValue(state.getObject(unit), Keyword::Shield), 3);
    // The +3 [M] while defending is the engine's aura fold of keyword_value
    // (CR 814) — AurasCostsTest.AuraKeywordHelper_*; the effect carries none.
    EXPECT_EQ(auraMightSum(state.getObject(unit)), 0);
}

// ── 819 Frostcoat Mother ─────────────────────────────────────────────────

TEST_F(VendettaA, Vendetta_FrostcoatMother_EmpowerCheaperPerRune) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    const Card* c = card_registry.get(kFrostcoatMother);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(empowerOf(kFrostcoatMother).cost.energy, 12);
    const int idx = c->empowerAbilityIndex();
    EXPECT_EQ(c->activationCostReduction(state, P1, idx), 0);
    runes(P1, 5);
    runes(P2, 3);  // the opponent's runes don't count
    EXPECT_EQ(c->activationCostReduction(state, P1, idx), 5);
    EXPECT_EQ(c->activationCostReduction(state, P1, idx + 1), 0) << "only the Empower ability";

    auto unit = addUnit(P1, kFrostcoatMother, 0, 0);
    aura(kFrostcoatMother, P1);
    EXPECT_EQ(auraMightSum(state.getObject(unit)), 0);
    clearAuras();
    exec.empower(unit);
    aura(kFrostcoatMother, P1);
    EXPECT_EQ(auraMightSum(state.getObject(unit)), 3);
}

// ── 820 Pakaa Protector ──────────────────────────────────────────────────

TEST_F(VendettaA, Vendetta_PakaaProtector_UnitIsDrawnOtherwiseTrashedForMight) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto pakaa = addUnit(P1, kPakaaProtector, 0, 0);
    auto spell = addToDeck(P1, kDredgeUp);
    auto unit_card = addToDeck(P1, kPunchingPoro);  // top

    fireTriggerAs(kPakaaProtector, P1, pakaa, TriggerType::WhenIMove, exec);
    EXPECT_TRUE(inHand(P1, unit_card));
    EXPECT_EQ(state.getObject(pakaa).current_might, 4) << "unit revealed: no bonus";

    fireTriggerAs(kPakaaProtector, P1, pakaa, TriggerType::WhenIMove, exec);
    EXPECT_TRUE(inTrash(P1, spell));
    EXPECT_FALSE(inHand(P1, spell));
    EXPECT_EQ(state.getObject(spell).zone, ZoneType::Trash);
    EXPECT_EQ(state.getObject(pakaa).current_might, 6);
    EXPECT_EQ(state.getObject(pakaa).temp_might_bonus, 2);

    // Empty deck: nothing to reveal, nothing happens.
    fireTriggerAs(kPakaaProtector, P1, pakaa, TriggerType::WhenIMove, exec);
    EXPECT_EQ(state.getObject(pakaa).current_might, 6);
}

// ── 823 Sandstone Chimera ────────────────────────────────────────────────

TEST_F(VendettaA, Vendetta_SandstoneChimera_ChannelCapOnlyAtBattlefield) {
    auto unit = addUnit(P1, kSandstoneChimera);  // base
    aura(kSandstoneChimera, P1);
    EXPECT_EQ(state.turn.channel_phase_cap, 0);

    state.getObject(unit).location = BattlefieldLocation{0};
    state.getObject(unit).zone = ZoneType::BattlefieldZone;
    aura(kSandstoneChimera, P1);
    EXPECT_EQ(state.turn.channel_phase_cap, 1);
}

// ── 824 Tomb-Raider Barbara ──────────────────────────────────────────────

TEST_F(VendettaA, Vendetta_Barbara_KillsEnemyGearAtSevenRunes) {
    auto barbara = addUnit(P1, kBarbara, 0, 0);
    auto my_gear = addGear(P1);
    auto gear = addGear(P2);

    // On the engine path: "if you control 7 or more runes" is the trigger
    // condition (Card::triggerCondition, asked by TriggerManager::queueTrigger),
    // so with 6 runes no ability goes on the chain and nothing is asked.
    runes(P1, 6);
    EnginePathAgent six(EnginePathAgent::script({}));
    fireTriggerThroughEngine(barbara, TriggerType::WhenYouPlayMe, &six);
    EXPECT_TRUE(six.prompts.empty()) << "6 runes: the ability does not trigger";
    EXPECT_TRUE(state.getObject(gear).location.has_value()) << "6 runes: no effect";

    runes(P1, 1);
    EnginePathAgent seven(EnginePathAgent::script({static_cast<int>(gear.raw())}));
    fireTriggerThroughEngine(barbara, TriggerType::WhenYouPlayMe, &seven);
    ASSERT_EQ(seven.prompts.size(), 1u) << "\"choose an enemy gear\"";
    EXPECT_TRUE(seven.wasOffered(gear));
    EXPECT_FALSE(seven.wasOffered(my_gear)) << "\"an enemy gear\"";
    EXPECT_FALSE(state.getObject(gear).location.has_value());
    EXPECT_EQ(state.getObject(gear).zone, ZoneType::Trash);
    EXPECT_TRUE(state.getObject(my_gear).location.has_value()) << "friendly gear untouched";
}

TEST_F(VendettaA, Vendetta_Barbara_DisempowersEmpoweredGearInstead) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto barbara = addUnit(P1, kBarbara, 0, 0);
    auto gear = addGear(P2);
    ASSERT_TRUE(exec.empower(gear));
    runes(P1, 7);
    fireTriggerAs(kBarbara, P1, barbara, TriggerType::WhenYouPlayMe, exec);
    EXPECT_FALSE(state.getObject(gear).isEmpowered());
    EXPECT_TRUE(state.getObject(gear).location.has_value()) << "disempowered, not killed";
}

// ── 825 Akali, Silent ────────────────────────────────────────────────────

TEST_F(VendettaA, Vendetta_AkaliSilent_UnchoosableUnlessInCombat) {
    auto akali = addUnit(P1, kAkaliSilent, 0, 0);
    auto any = [](const GameObject&) { return true; };
    auto contains = [&](const std::vector<GameObjectId>& v) {
        return std::find(v.begin(), v.end(), akali) != v.end();
    };

    aura(kAkaliSilent, P1);
    EXPECT_TRUE(state.getObject(akali).untargetable_by_enemy);
    EXPECT_FALSE(contains(chooseableUnits(state, P2, any)));
    EXPECT_TRUE(contains(chooseableUnits(state, P1, any))) << "friendly effects may choose me";

    clearAuras();
    state.getObject(akali).combat_designation = CombatDesignation::Attacker;
    aura(kAkaliSilent, P1);
    EXPECT_FALSE(state.getObject(akali).untargetable_by_enemy);
    EXPECT_TRUE(contains(chooseableUnits(state, P2, any)));
}

TEST_F(VendettaA, Vendetta_AkaliSilent_MightOnMoveToBattlefield) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    const Card* c = card_registry.get(kAkaliSilent);
    EXPECT_TRUE(c->firesOn(TriggerType::WhenIMoveToFB));
    EXPECT_FALSE(c->firesOn(TriggerType::WhenIMove)) << "moving to base doesn't count";
    auto akali = addUnit(P1, kAkaliSilent, 0, 0);
    fireTriggerAs(kAkaliSilent, P1, akali, TriggerType::WhenIMoveToFB, exec);
    EXPECT_EQ(state.getObject(akali).current_might, 6);
    EXPECT_EQ(state.getObject(akali).temp_might_bonus, 2);
}

// ── 828 Riven, Shattered ─────────────────────────────────────────────────

// Through the engine (plans/09 item 9.N6; these were direct fireTriggerAs
// calls): "When I attack, choose an enemy unit here. Deal 2 to it for each
// Equipment attached to me." The enemy unit is chosen at finalization.
TEST_F(VendettaA, Vendetta_Riven_AttackDamagePerAttachedEquipment) {
    const Card* c = card_registry.get(kRiven);
    ASSERT_NE(c, nullptr);
    EXPECT_TRUE(c->firesOn(TriggerType::WhenYouPlayMe)) << "[Weaponmaster]";
    EXPECT_TRUE(c->firesOn(TriggerType::WhenIAttack));

    auto riven = addUnit(P1, kRiven, 0, 0);
    auto enemy = addUnit(P2, kInvalidId, 9, 0);
    auto far_enemy = addUnit(P2, kInvalidId, 9, 1);

    EnginePathAgent bare(EnginePathAgent::choosing(enemy));
    fireTriggerThroughEngine(riven, TriggerType::WhenIAttack, &bare);
    EXPECT_EQ(state.getObject(enemy).damage_marked, 0) << "no Equipment attached";
    EXPECT_FALSE(state.chain.exists());

    auto sword = addGear(P1, /*equipment=*/true);
    auto shield = addGear(P1, /*equipment=*/true);
    auto trinket = addGear(P1, /*equipment=*/false);
    for (auto g : {sword, shield, trinket}) {
        state.getObject(g).attached_to = riven;
        state.getObject(g).location = state.getObject(riven).location;   // CR 719.3
        state.getObject(g).zone = state.getObject(riven).zone;
        state.getObject(riven).attachments.push_back(g);
    }
    EnginePathAgent agent(EnginePathAgent::choosing(enemy));
    fireTriggerThroughEngine(riven, TriggerType::WhenIAttack, &agent);
    EXPECT_FALSE(agent.wasOffered(far_enemy)) << "only an enemy unit here";
    EXPECT_EQ(state.getObject(enemy).damage_marked, 4) << "2 per Equipment (2 of 3 gear)";
    EXPECT_EQ(state.getObject(far_enemy).damage_marked, 0) << "only an enemy unit here";
    EXPECT_FALSE(state.chain.exists());
}

// The play trigger through the engine: [Weaponmaster] is a "may", so the
// agent accepts it and chooses the Equipment (the default agent declines).
// The test Equipment has no card behind it, so there is no Equip cost to fund.
TEST_F(VendettaA, Vendetta_Riven_WeaponmasterEquipsOnPlay) {
    auto riven = addUnit(P1, kRiven, 0, 0);
    auto sword = addGear(P1, /*equipment=*/true);
    auto enemy = addUnit(P2, kInvalidId, 9, 0);
    EnginePathAgent yes(EnginePathAgent::choosing(sword, EnginePathAgent::May::Yes));
    fireTriggerThroughEngine(riven, TriggerType::WhenYouPlayMe, &yes);
    ASSERT_EQ(yes.prompts.size(), 2u) << "the may, then which Equipment";
    ASSERT_TRUE(state.getObject(sword).attached_to.has_value());
    EXPECT_EQ(*state.getObject(sword).attached_to, riven);
    EXPECT_EQ(state.getObject(riven).attachments.size(), 1u);
    EXPECT_EQ(state.getObject(enemy).damage_marked, 0) << "play trigger is not the attack trigger";
    EXPECT_FALSE(state.chain.exists());
}

// ── 829 Shen, Scourge of Shadows ─────────────────────────────────────────

TEST_F(VendettaA, Vendetta_Shen_DrawsOnHoldWithExactlyOneOtherFriendlyHere) {
    // CR 383.2.a.1: the printed condition is part of the trigger condition, so it is
    // judged when the event happens (Card::triggerCondition) — the trigger goes
    // through the engine, which is what asks it; false = nothing on the chain.
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto shen = addUnit(P1, kShen, 0, 0);
    for (int i = 0; i < 4; ++i) addToDeck(P1, kDredgeUp);
    addUnit(P2, kInvalidId, 2, 0);   // enemy here: irrelevant
    addUnit(P1, kInvalidId, 2, 1);   // friendly elsewhere: irrelevant

    fireTriggerThroughEngine(shen, TriggerType::WhenIHold);
    EXPECT_EQ(handSize(P1), 0) << "zero others";

    addUnit(P1, kInvalidId, 2, 0);
    fireTriggerThroughEngine(shen, TriggerType::WhenIHold);
    EXPECT_EQ(handSize(P1), 1) << "exactly one other";

    addUnit(P1, kInvalidId, 2, 0);
    fireTriggerThroughEngine(shen, TriggerType::WhenIHold);
    EXPECT_EQ(handSize(P1), 1) << "two others";
}

// ── 830 Steel Paws ───────────────────────────────────────────────────────

TEST_F(VendettaA, Vendetta_SteelPaws_EmpoweredSevenMight) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    EXPECT_EQ(empowerOf(kSteelPaws).cost.energy, 7);
    EXPECT_EQ(card_db.get(kSteelPaws).deflect_value, 1);
    auto unit = addUnit(P1, kSteelPaws, 0, 0);
    aura(kSteelPaws, P1);
    EXPECT_EQ(auraMightSum(state.getObject(unit)), 0);
    clearAuras();
    exec.empower(unit);
    aura(kSteelPaws, P1);
    EXPECT_EQ(auraMightSum(state.getObject(unit)), 7);
}

// ── 831 Astral Heron (ENGINE-GAP: unimplemented) ─────────────────────────

TEST_F(VendettaA, Vendetta_AstralHeron_FirstCardAtBattlefieldDiscountsNextCard) {
    // CR 383.2.a.1: the printed condition is part of the trigger condition, so it is
    // judged when the event happens (Card::triggerCondition) — the trigger goes
    // through the engine, which is what asks it; false = nothing on the chain.
    EffectExecutor exec(state, events, card_db, &card_registry);
    EXPECT_TRUE(card_registry.get(kAstralHeron)->firesOn(TriggerType::WhenYouPlayACard));
    auto heron = addUnit(P1, kAstralHeron, 0, 0);
    auto& mods = state.player(P1).cost_modifiers;

    // "When you play your first card each turn": judged from the played
    // card's play ordinal (the trigger's subject, stamped by
    // TriggerManager::onCardPlayed), so the trigger goes through the engine.
    auto played = [&](int ordinal) {
        auto id = addUnit(P1, kInvalidId, 1, -1);
        state.getObject(id).card_counters["__play_ordinal"] = ordinal;
        state.getObject(id).card_counters["__play_ordinal_turn"] = state.turn.turn_number + 1;
        return id;
    };
    // Second card this turn: not "your first card".
    state.player(P1).cards_played_this_turn = 2;
    fireTriggerThroughEngine(heron, TriggerType::WhenYouPlayACard, nullptr, played(2));
    EXPECT_TRUE(mods.empty());

    // A new turn, and its first card.
    state.turn.turn_number++;
    state.player(P1).cards_played_this_turn = 1;
    fireTriggerThroughEngine(heron, TriggerType::WhenYouPlayACard, nullptr, played(1));
    ASSERT_EQ(mods.size(), 1u);
    EXPECT_EQ(mods[0].energy_reduction, 2);
    EXPECT_EQ(mods[0].power_reduction, 2);
    EXPECT_TRUE(mods[0].next_card_only) << "consumed by the next card of any type";
    EXPECT_FALSE(mods[0].next_spell_only);
    EXPECT_FALSE(mods[0].next_unit_only);

    // "if I'm at a battlefield": nothing from base.
    mods.clear();
    auto home = addUnit(P1, kAstralHeron);
    state.turn.turn_number++;   // a new turn, and again its first card
    fireTriggerThroughEngine(home, TriggerType::WhenYouPlayACard, nullptr, played(1));
    EXPECT_TRUE(mods.empty());
}

// ── 833 Nasus, Ascended ──────────────────────────────────────────────────

TEST_F(VendettaA, Vendetta_NasusAscended_ScoresOnConquerOnlyWhileEmpowered) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    EXPECT_EQ(empowerOf(kNasusAscended).cost.energy, 8);
    EXPECT_EQ(card_db.get(kNasusAscended).deflect_value, 2);
    auto nasus = addUnit(P1, kNasusAscended, 0, 0);
    setScore(P1, 3);

    fireTriggerAs(kNasusAscended, P1, nasus, TriggerType::WhenIConquer, exec);
    EXPECT_EQ(state.player(P1).score, 3) << "not Empowered";

    exec.empower(nasus);
    fireTriggerAs(kNasusAscended, P1, nasus, TriggerType::WhenIConquer, exec);
    EXPECT_EQ(state.player(P1).score, 4);
    EXPECT_EQ(state.player(P2).score, 0);
}

// ── 835 Cloud Drake ──────────────────────────────────────────────────────

TEST_F(VendettaA, Vendetta_CloudDrake_DrawsOnPlay) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto drake = addUnit(P1, kCloudDrake, 0, 0);
    addToDeck(P1, kDredgeUp);
    addToDeck(P1, kDredgeUp);
    fireTriggerAs(kCloudDrake, P1, drake, TriggerType::WhenYouPlayMe, exec);
    EXPECT_EQ(handSize(P1), 1);
    EXPECT_EQ(handSize(P2), 0);
}

}  // namespace
}  // namespace test_vendetta_A
}  // namespace riftbound::test
