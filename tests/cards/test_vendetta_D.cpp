/// @file test_vendetta_D.cpp
/// Vendetta batch D — 20 spells (Fury / Calm / Mind / Body).

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
namespace test_vendetta_D {  // file-unique: test sources are unity-built
namespace {

constexpr CardDefId kBrittleSteel     = 790;
constexpr CardDefId kRuthlessStrike   = 795;
constexpr CardDefId kConsumingCurse   = 797;
constexpr CardDefId kPerfectExecution = 799;
constexpr CardDefId kDecreeOfRage     = 802;
constexpr CardDefId kTwilightShroud   = 818;
constexpr CardDefId kResonatingStrike = 821;
constexpr CardDefId kSanction         = 822;
constexpr CardDefId kCrumblingSands   = 826;
constexpr CardDefId kDecreeOfFocus    = 827;
constexpr CardDefId kIterativeDesign  = 838;
constexpr CardDefId kMesmerize        = 839;
constexpr CardDefId kClairvoyance     = 843;
constexpr CardDefId kShockBlast       = 846;
constexpr CardDefId kDecreeOfInsight  = 848;
constexpr CardDefId kTemporalBreach   = 853;
constexpr CardDefId kGutturalRoar     = 859;
constexpr CardDefId kRampage          = 870;
constexpr CardDefId kDecreeOfStrength = 872;
constexpr CardDefId kWildClaw         = 876;

// Supporting cards.
constexpr CardDefId kBaccaiSandspinner = 788;   // unit, [6]
constexpr CardDefId kPunchingPoro      = 794;   // unit, [2]
constexpr CardDefId kRageAmplifier     = 805;   // gear, [4][R]
constexpr CardDefId kDredgeUp          = 836;   // Mind spell
constexpr CardDefId kOnslaught         = 868;   // Body spell

bool contains(const std::vector<GameObjectId>& v, GameObjectId id) {
    return std::find(v.begin(), v.end(), id) != v.end();
}

/// Pick the option that names `id`; otherwise the first option.
Intent pickObject(const std::vector<Intent>& legal, GameObjectId id) {
    for (const auto& i : legal)
        if (!i.chosen_objects.empty() && i.chosen_objects[0] == id) return i;
    return legal.empty() ? Intent{} : legal.front();
}

/// Pick the int-coded option `value`; otherwise the first option.
Intent pickValue(const std::vector<Intent>& legal, int value) {
    for (const auto& i : legal)
        if (i.chosen_value.has_value() && *i.chosen_value == value) return i;
    return legal.empty() ? Intent{} : legal.front();
}

bool isIntPrompt(const std::vector<Intent>& legal) {
    return !legal.empty() && legal.front().chosen_value.has_value();
}

class VendettaD : public CardTestFixture {
protected:
    GameObjectId addGear(PlayerId owner, CardDefId def_id = kInvalidId) {
        auto id = addUnit(owner, def_id);
        auto& obj = state.getObject(id);
        obj.card_type = CardType::Gear;
        obj.base_might = obj.current_might = 0;
        return id;
    }
    /// A card of `def_id` sitting in `owner`'s trash.
    GameObjectId addToTrash(PlayerId owner, CardDefId def_id) {
        auto id = addToHand(owner, def_id);
        auto& ps = state.player(owner);
        ps.hand.pop_back();
        state.getObject(id).zone = ZoneType::Trash;
        ps.trash.push_back(id);
        return id;
    }
    std::vector<GameObjectId> legalTargets(CardDefId def_id, PlayerId p) {
        return card_registry.get(def_id)->enumerateLegalTargets(state, p);
    }
    void resolve(CardDefId def_id, PlayerId p, EffectExecutor& exec, GameObjectId source) {
        invokeOnResolve(source, def_id, p, {}, exec);
    }
};

// ── 790 Brittle Steel ────────────────────────────────────────────────────

TEST_F(VendettaD, Vendetta_BrittleSteel_KillsAGearNotAUnit) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto gear = addGear(P2);
    auto unit = addUnit(P2, kInvalidId, 3, 0);
    auto spell = addToHand(P1, kBrittleSteel);

    EXPECT_EQ(legalTargets(kBrittleSteel, P1), std::vector<GameObjectId>{gear})
        << "only gear can be chosen";
    resolve(kBrittleSteel, P1, exec, spell);
    EXPECT_EQ(state.getObject(gear).zone, ZoneType::Trash);
    EXPECT_FALSE(state.getObject(gear).location.has_value());
    EXPECT_TRUE(state.getObject(unit).location.has_value());
}

TEST_F(VendettaD, Vendetta_BrittleSteel_NoGearNoPlayAndFlowCost) {
    const Card* c = card_registry.get(kBrittleSteel);
    ASSERT_NE(c, nullptr);
    addUnit(P2, kInvalidId, 3, 0);
    EXPECT_FALSE(c->hasLegalTargets(state, P1));
    auto f = c->flowCost();
    EXPECT_TRUE(f.valid);
    EXPECT_EQ(f.energy, 4);
    EXPECT_EQ(f.power, 1);
    EXPECT_EQ(f.power_domain, Domain::Fury);
}

// ── 795 Ruthless Strike ──────────────────────────────────────────────────

TEST_F(VendettaD, Vendetta_RuthlessStrike_ThreeWithoutAdditionalCost) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto at_bf = addUnit(P2, kInvalidId, 4, 0);
    addUnit(P2, kInvalidId, 4);   // in base — not a legal choice
    auto spell = addToHand(P1, kRuthlessStrike);

    EXPECT_EQ(legalTargets(kRuthlessStrike, P1), std::vector<GameObjectId>{at_bf});
    resolve(kRuthlessStrike, P1, exec, spell);
    EXPECT_EQ(state.getObject(at_bf).damage_marked, 3);
    EXPECT_TRUE(state.getObject(at_bf).location.has_value());
}

TEST_F(VendettaD, Vendetta_RuthlessStrike_FiveWhenAdditionalCostPaid) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto at_bf = addUnit(P2, kInvalidId, 4, 0);
    auto spell = addToHand(P1, kRuthlessStrike);
    auto oac = card_registry.get(kRuthlessStrike)->optionalAdditionalCost();
    ASSERT_TRUE(oac.valid);
    EXPECT_EQ(oac.discard_cards, 1);
    state.getObject(spell).card_counters[oac.paid_flag] = 1;

    resolve(kRuthlessStrike, P1, exec, spell);
    EXPECT_EQ(state.getObject(at_bf).zone, ZoneType::Trash) << "5 damage kills a 4 [M] unit";
    EXPECT_EQ(state.getObject(spell).card_counters.count(oac.paid_flag), 0u)
        << "the payment is consumed by this resolution";
}

// ── 797 Consuming Curse ──────────────────────────────────────────────────

TEST_F(VendettaD, Vendetta_ConsumingCurse_BonusDamagePerCopyInTrash) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto target = addUnit(P2, kInvalidId, 9, 0);
    auto spell = addToHand(P1, kConsumingCurse);

    resolve(kConsumingCurse, P1, exec, spell);
    EXPECT_EQ(state.getObject(target).damage_marked, 2) << "no copies in trash";

    state.getObject(target).damage_marked = 0;
    addToTrash(P1, kConsumingCurse);
    addToTrash(P1, kConsumingCurse);
    addToTrash(P1, kDredgeUp);            // a different name doesn't count
    addToTrash(P2, kConsumingCurse);      // the opponent's trash doesn't count
    resolve(kConsumingCurse, P1, exec, spell);
    EXPECT_EQ(state.getObject(target).damage_marked, 4);
}

// ── 799 Perfect Execution ────────────────────────────────────────────────

TEST_F(VendettaD, Vendetta_PerfectExecution_ReadiesAndGivesAssault3) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto unit = addUnit(P1, kInvalidId, 2, 0);
    state.getObject(unit).is_exhausted = true;
    auto spell = addToHand(P1, kPerfectExecution);

    resolve(kPerfectExecution, P1, exec, spell);
    const auto& u = state.getObject(unit);
    EXPECT_FALSE(u.is_exhausted);
    EXPECT_TRUE(u.hasKeyword(Keyword::Assault));
    EXPECT_EQ(u.assault_value, 3);
    EXPECT_EQ(u.temp_assault_value, 3) << "this turn only";
    EXPECT_EQ(u.current_might, 2) << "Assault only counts while attacking";

    auto f = card_registry.get(kPerfectExecution)->flowCost();
    EXPECT_TRUE(f.valid);
    EXPECT_EQ(f.energy, 3);
    EXPECT_EQ(f.power, 1);
    EXPECT_EQ(f.power_domain, Domain::Fury);
}

// ── 802 Decree of Rage ───────────────────────────────────────────────────

TEST_F(VendettaD, Vendetta_DecreeOfRage_OnlyEnemyCalmUnits) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto enemy_calm = addUnit(P2, kInvalidId, 5, 0);
    state.getObject(enemy_calm).domains = {Domain::Calm};
    auto enemy_fury = addUnit(P2, kInvalidId, 5, 0);
    state.getObject(enemy_fury).domains = {Domain::Fury};
    auto my_calm = addUnit(P1, kInvalidId, 5, 0);
    state.getObject(my_calm).domains = {Domain::Calm};
    auto spell = addToHand(P1, kDecreeOfRage);

    EXPECT_EQ(legalTargets(kDecreeOfRage, P1), std::vector<GameObjectId>{enemy_calm});
    resolve(kDecreeOfRage, P1, exec, spell);
    EXPECT_EQ(state.getObject(enemy_calm).damage_marked, 4);
    EXPECT_EQ(state.getObject(enemy_fury).damage_marked, 0);
    EXPECT_EQ(state.getObject(my_calm).damage_marked, 0);
}

TEST_F(VendettaD, Vendetta_DecreeOfRage_UnplayableWithoutEnemyCalmUnit) {
    auto enemy_fury = addUnit(P2, kInvalidId, 5, 0);
    state.getObject(enemy_fury).domains = {Domain::Fury};
    EXPECT_FALSE(card_registry.get(kDecreeOfRage)->hasLegalTargets(state, P1));
}

TEST_F(VendettaD, Vendetta_DecreeOfRage_CantBeCountered) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    EXPECT_TRUE(card_registry.get(kDecreeOfRage)->cantBeCountered());
    EXPECT_FALSE(card_registry.get(kOnslaught)->cantBeCountered());

    auto ordinary = pushSpellOnChain(P1, kOnslaught);
    auto decree = pushSpellOnChain(P1, kDecreeOfRage);
    auto counter = pushSpellOnChain(P2, kCrumblingSands);
    state.chain.items.pop_back();                     // the counterspell is resolving
    CardContext ctx{state, events, exec, P2, counter};
    ctx.registry = &card_registry;

    counterChainTop(ctx);
    ASSERT_EQ(state.chain.items.size(), 2u) << "Decree of Rage stays on the chain";
    EXPECT_EQ(state.chain.items.back().source, decree);

    state.chain.items.pop_back();
    counterChainTop(ctx);
    EXPECT_TRUE(state.chain.items.empty()) << "an ordinary spell is countered";
    EXPECT_TRUE(inTrash(P1, ordinary));
}

// ── 818 Twilight Shroud ──────────────────────────────────────────────────

TEST_F(VendettaD, Vendetta_TwilightShroud_FriendlyUnitGetsPlusOne) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto mine = addUnit(P1, kInvalidId, 2, 0);
    auto theirs = addUnit(P2, kInvalidId, 2, 0);
    auto spell = addToHand(P1, kTwilightShroud);

    EXPECT_EQ(legalTargets(kTwilightShroud, P1), std::vector<GameObjectId>{mine});
    resolve(kTwilightShroud, P1, exec, spell);
    EXPECT_EQ(state.getObject(mine).current_might, 3);
    EXPECT_EQ(state.getObject(mine).temp_might_bonus, 1);
    EXPECT_EQ(state.getObject(theirs).current_might, 2);
    // "It can't be chosen by enemy spells and abilities this turn."
    EXPECT_TRUE(state.getObject(mine).untargetable_by_enemy_this_turn);
    EXPECT_TRUE(state.getObject(mine).untargetable_by_enemy);
    EXPECT_FALSE(state.getObject(theirs).untargetable_by_enemy_this_turn);
    EXPECT_TRUE(legalTargets(kRuthlessStrike, P2) == std::vector<GameObjectId>{theirs})
        << "the opponent can no longer choose it";

    auto f = card_registry.get(kTwilightShroud)->flowCost();
    EXPECT_TRUE(f.valid);
    EXPECT_EQ(f.energy, 2);
    EXPECT_EQ(f.power, 0);
}

// ── 821 Resonating Strike ────────────────────────────────────────────────

TEST_F(VendettaD, Vendetta_ResonatingStrike_MovesUnitToControlledBattlefield) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    state.battlefields[1].controller = P1;
    auto already_there = addUnit(P1, kInvalidId, 2, 1);
    auto in_base = addUnit(P1, kInvalidId, 2);
    auto spell = addToHand(P1, kResonatingStrike);

    resolve(kResonatingStrike, P1, exec, spell);
    const auto& moved = state.getObject(in_base);
    ASSERT_TRUE(moved.battlefieldId().has_value());
    EXPECT_EQ(*moved.battlefieldId(), BattlefieldId::fromRaw(1));
    EXPECT_EQ(moved.current_might, 4);
    EXPECT_EQ(state.getObject(already_there).current_might, 2)
        << "a unit already at that battlefield can't be the one chosen";
}

TEST_F(VendettaD, Vendetta_ResonatingStrike_NeedsControlledBattlefieldAndUnitElsewhere) {
    const Card* c = card_registry.get(kResonatingStrike);
    ASSERT_NE(c, nullptr);
    auto unit = addUnit(P1, kInvalidId, 2, 1);
    EXPECT_FALSE(c->hasLegalTargets(state, P1)) << "no battlefield controlled";
    state.battlefields[1].controller = P1;
    EXPECT_FALSE(c->hasLegalTargets(state, P1)) << "only unit is already there";
    state.battlefields[1].controller = P2;
    state.battlefields[0].controller = P1;
    EXPECT_TRUE(c->hasLegalTargets(state, P1));
    (void)unit;
}

// ── 822 Sanction ─────────────────────────────────────────────────────────

TEST_F(VendettaD, Vendetta_Sanction_EmpowerThenDisempowerAtEndOfTurn) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto unit = addUnit(P1, kInvalidId, 2, 0);
    auto spell = addToHand(P1, kSanction);

    int first_prompt_options = 0;
    int prompts = 0;
    driveResumable(kSanction, P1, spell, [&](const std::vector<Intent>& legal) {
        if (prompts++ == 0) first_prompt_options = static_cast<int>(legal.size());
        return isIntPrompt(legal) ? pickValue(legal, 0) : pickObject(legal, unit);
    }, exec);

    EXPECT_EQ(first_prompt_options, 1) << "nothing is Empowered, so only the first mode is offered";
    EXPECT_TRUE(state.getObject(unit).isEmpowered());
    // "Disempower it at end of turn": a delayed ability (finding G13; it was
    // a flag the Expiration Step flipped). ability_index 0 = disempower.
    ASSERT_EQ(state.delayed_abilities.size(), 1u);
    EXPECT_EQ(state.delayed_abilities[0].trigger, TriggerType::AtEndOfTurn);
    EXPECT_EQ(state.delayed_abilities[0].subject, unit);
    EXPECT_EQ(state.delayed_abilities[0].ability_index, 0);
}

TEST_F(VendettaD, Vendetta_Sanction_DisempowerThenEmpowerAtEndOfTurn) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto plain = addUnit(P1, kInvalidId, 2, 0);
    auto empowered = addUnit(P2, kInvalidId, 2, 0);
    exec.empower(empowered);
    auto spell = addToHand(P1, kSanction);

    std::vector<GameObjectId> offered;
    driveResumable(kSanction, P1, spell, [&](const std::vector<Intent>& legal) {
        if (isIntPrompt(legal)) return pickValue(legal, 1);
        for (const auto& i : legal) offered.push_back(i.chosen_objects.at(0));
        return pickObject(legal, empowered);
    }, exec);

    EXPECT_EQ(offered, std::vector<GameObjectId>{empowered})
        << "the second mode only chooses Empowered units";
    EXPECT_FALSE(state.getObject(empowered).isEmpowered());
    // "Empower it at end of turn": ability_index 1 (finding G13).
    ASSERT_EQ(state.delayed_abilities.size(), 1u);
    EXPECT_EQ(state.delayed_abilities[0].subject, empowered);
    EXPECT_EQ(state.delayed_abilities[0].ability_index, 1);
    EXPECT_FALSE(state.getObject(plain).isEmpowered());
}

// ── 826 Crumbling Sands ──────────────────────────────────────────────────

TEST_F(VendettaD, Vendetta_CrumblingSands_CountersWhenOpponentHasAnotherSpell) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto first = pushSpellOnChain(P2, kDredgeUp);
    auto second = pushSpellOnChain(P2, kOnslaught);
    state.player(P2).spells_played_this_turn = 2;
    auto spell = addToHand(P1, kCrumblingSands);

    resolve(kCrumblingSands, P1, exec, spell);
    ASSERT_EQ(state.chain.items.size(), 1u);
    EXPECT_EQ(state.chain.items.back().source, first);
    EXPECT_TRUE(inTrash(P2, second));
}

TEST_F(VendettaD, Vendetta_CrumblingSands_NoCounterForOpponentsOnlySpell) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    const Card* c = card_registry.get(kCrumblingSands);
    ASSERT_NE(c, nullptr);
    EXPECT_FALSE(c->hasLegalTargets(state, P1)) << "no spell on the chain";

    auto only = pushSpellOnChain(P2, kOnslaught);
    state.player(P2).spells_played_this_turn = 1;   // the target itself
    EXPECT_TRUE(c->hasLegalTargets(state, P1));
    auto spell = addToHand(P1, kCrumblingSands);
    resolve(kCrumblingSands, P1, exec, spell);
    ASSERT_EQ(state.chain.items.size(), 1u) << "the opponent has played no OTHER spell";
    EXPECT_EQ(state.chain.items.back().source, only);
    EXPECT_FALSE(inTrash(P2, only));

    // An earlier spell of theirs that already left the chain still counts.
    state.player(P2).spells_played_this_turn = 2;
    resolve(kCrumblingSands, P1, exec, spell);
    EXPECT_TRUE(state.chain.items.empty());
    EXPECT_TRUE(inTrash(P2, only));
}

TEST_F(VendettaD, Vendetta_CrumblingSands_OwnSpellCounteredIfOpponentPlayedASpell) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto mine = pushSpellOnChain(P1, kOnslaught);
    auto spell = addToHand(P1, kCrumblingSands);

    resolve(kCrumblingSands, P1, exec, spell);
    EXPECT_EQ(state.chain.items.size(), 1u) << "the opponent has played no spell";

    state.player(P2).spells_played_this_turn = 1;
    resolve(kCrumblingSands, P1, exec, spell);
    EXPECT_TRUE(state.chain.items.empty());
    EXPECT_TRUE(inTrash(P1, mine));
}

// ── 827 Decree of Focus ──────────────────────────────────────────────────

TEST_F(VendettaD, Vendetta_DecreeOfFocus_UnitInCombatWithEnemyFuryUnit) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto defender = addUnit(P1, kInvalidId, 2, 0);
    state.getObject(defender).combat_designation = CombatDesignation::Defender;
    addUnit(P1, kInvalidId, 2, 1);   // not in combat
    auto attacker = addUnit(P2, kInvalidId, 3, 0);
    state.getObject(attacker).combat_designation = CombatDesignation::Attacker;
    auto spell = addToHand(P1, kDecreeOfFocus);

    EXPECT_TRUE(legalTargets(kDecreeOfFocus, P1).empty()) << "the attacker isn't Fury";
    state.getObject(attacker).domains = {Domain::Fury};
    EXPECT_EQ(legalTargets(kDecreeOfFocus, P1), std::vector<GameObjectId>{defender});

    resolve(kDecreeOfFocus, P1, exec, spell);
    EXPECT_EQ(state.getObject(defender).temp_might_bonus, 4);
}

TEST_F(VendettaD, Vendetta_DecreeOfFocus_UnitChosenByEnemyFurySpell) {
    auto chosen = addUnit(P1, kInvalidId, 2, 0);
    auto bystander = addUnit(P1, kInvalidId, 2, 0);
    auto enemy_spell = pushSpellOnChain(P2, kOnslaught, {chosen});

    EXPECT_TRUE(legalTargets(kDecreeOfFocus, P1).empty()) << "the spell isn't Fury";
    state.getObject(enemy_spell).domains = {Domain::Fury};
    EXPECT_EQ(legalTargets(kDecreeOfFocus, P1), std::vector<GameObjectId>{chosen});

    // A friendly Fury spell choosing the unit doesn't count.
    state.chain.items.back().controller = P1;
    EXPECT_TRUE(legalTargets(kDecreeOfFocus, P1).empty());
    (void)bystander;
}

// ── 838 Iterative Design ─────────────────────────────────────────────────

TEST_F(VendettaD, Vendetta_IterativeDesign_PlaysMechTokenToChosenLocation) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    state.battlefields[1].controller = P1;
    auto spell = addToHand(P1, kIterativeDesign);
    const auto before = state.objects.size();

    int options = 0;
    driveResumable(kIterativeDesign, P1, spell, [&](const std::vector<Intent>& legal) {
        options = static_cast<int>(legal.size());
        return pickValue(legal, 2);   // 0 = base, 1 + battlefield index
    }, exec);
    EXPECT_EQ(options, 2) << "base + the one battlefield P1 controls";
    ASSERT_EQ(state.objects.size(), before + 1);

    const GameObject* mech = nullptr;
    for (const auto& [id, obj] : state.objects)
        if (obj.isToken() && obj.name == "Mech") mech = &obj;
    ASSERT_NE(mech, nullptr);
    EXPECT_TRUE(mech->isUnit());
    EXPECT_EQ(mech->current_might, 3);
    EXPECT_EQ(mech->controller, P1);
    EXPECT_TRUE(mech->is_exhausted);
    EXPECT_EQ(mech->tags, std::vector<std::string>{"Mech"});
    ASSERT_TRUE(mech->battlefieldId().has_value());
    EXPECT_EQ(*mech->battlefieldId(), BattlefieldId::fromRaw(1));
}

TEST_F(VendettaD, Vendetta_IterativeDesign_DefaultsToBaseAndFlowCost) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto spell = addToHand(P1, kIterativeDesign);
    resolve(kIterativeDesign, P1, exec, spell);
    int mechs = 0;
    for (const auto& [id, obj] : state.objects)
        if (obj.isToken() && obj.name == "Mech" && obj.isAtBase()) ++mechs;
    EXPECT_EQ(mechs, 1);

    auto f = card_registry.get(kIterativeDesign)->flowCost();
    EXPECT_TRUE(f.valid);
    EXPECT_EQ(f.energy, 2);
    EXPECT_EQ(f.power, 1);
    EXPECT_EQ(f.power_domain, Domain::Mind);
}

// ── 839 Mesmerize ────────────────────────────────────────────────────────

TEST_F(VendettaD, Vendetta_Mesmerize_ReturnsFriendlyUnitToHand) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto mine = addUnit(P1, kPunchingPoro, 0, 0);
    auto theirs = addUnit(P2, kInvalidId, 3, 0);
    auto spell = addToHand(P1, kMesmerize);

    std::vector<GameObjectId> offered;
    driveResumable(kMesmerize, P1, spell, [&](const std::vector<Intent>& legal) {
        if (isIntPrompt(legal)) return pickValue(legal, 0);
        for (const auto& i : legal) offered.push_back(i.chosen_objects.at(0));
        return pickObject(legal, mine);
    }, exec);

    EXPECT_EQ(offered, std::vector<GameObjectId>{mine}) << "only friendly units for this mode";
    EXPECT_TRUE(inHand(P1, mine));
    EXPECT_FALSE(state.getObject(mine).location.has_value());
    EXPECT_EQ(state.getObject(theirs).current_might, 3);
}

TEST_F(VendettaD, Vendetta_Mesmerize_GivesEnemyUnitMinusTwo) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto mine = addUnit(P1, kInvalidId, 3, 0);
    auto theirs = addUnit(P2, kInvalidId, 3, 0);
    auto spell = addToHand(P1, kMesmerize);

    std::vector<GameObjectId> offered;
    driveResumable(kMesmerize, P1, spell, [&](const std::vector<Intent>& legal) {
        if (isIntPrompt(legal)) return pickValue(legal, 1);
        for (const auto& i : legal) offered.push_back(i.chosen_objects.at(0));
        return pickObject(legal, theirs);
    }, exec);

    EXPECT_EQ(offered, std::vector<GameObjectId>{theirs}) << "only enemy units for this mode";
    EXPECT_EQ(state.getObject(theirs).current_might, 1);
    EXPECT_EQ(state.getObject(theirs).temp_might_bonus, -2);
    EXPECT_EQ(state.getObject(mine).current_might, 3);
    EXPECT_TRUE(state.getObject(mine).location.has_value());
}

// ── 843 Clairvoyance ─────────────────────────────────────────────────────

TEST_F(VendettaD, Vendetta_Clairvoyance_PredictThenDrawTwo) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    std::vector<GameObjectId> deck;
    for (int i = 0; i < 8; ++i) deck.push_back(addToDeck(P1, kDredgeUp));
    auto spell = addToHand(P1, kClairvoyance);
    const int hand_before = handSize(P1);

    resolve(kClairvoyance, P1, exec, spell);
    EXPECT_EQ(handSize(P1), hand_before + 2);
    EXPECT_EQ(deckSize(P1), 6);
    // No agent installed: Predict keeps everything in order, so the two drawn
    // cards are the two that were on top.
    EXPECT_TRUE(inHand(P1, deck[7]));
    EXPECT_TRUE(inHand(P1, deck[6]));
}

// ── 846 Shock Blast ──────────────────────────────────────────────────────

TEST_F(VendettaD, Vendetta_ShockBlast_CostsTwoLessWithSomethingEmpowered) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    const Card* c = card_registry.get(kShockBlast);
    ASSERT_NE(c, nullptr);
    auto mine = addUnit(P1, kInvalidId, 2);
    auto theirs = addUnit(P2, kInvalidId, 2, 0);

    EXPECT_EQ(c->selfCostReduction(state, P1), 0);
    exec.empower(theirs);
    EXPECT_EQ(c->selfCostReduction(state, P1), 0) << "an opponent's Empowered unit doesn't count";
    exec.empower(mine);
    EXPECT_EQ(c->selfCostReduction(state, P1), 2);
}

TEST_F(VendettaD, Vendetta_ShockBlast_DealsFourToUnitAtBattlefield) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto at_bf = addUnit(P2, kInvalidId, 6, 0);
    addUnit(P2, kInvalidId, 6);   // in base
    auto spell = addToHand(P1, kShockBlast);

    EXPECT_EQ(legalTargets(kShockBlast, P1), std::vector<GameObjectId>{at_bf});
    resolve(kShockBlast, P1, exec, spell);
    EXPECT_EQ(state.getObject(at_bf).damage_marked, 4);
}

// ── 848 Decree of Insight ────────────────────────────────────────────────

TEST_F(VendettaD, Vendetta_DecreeOfInsight_MinusFiveToEnemyBodyUnit) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto enemy_body = addUnit(P2, kInvalidId, 6, 0);
    state.getObject(enemy_body).domains = {Domain::Body};
    auto enemy_mind = addUnit(P2, kInvalidId, 6, 0);
    state.getObject(enemy_mind).domains = {Domain::Mind};
    auto my_body = addUnit(P1, kInvalidId, 6, 0);
    state.getObject(my_body).domains = {Domain::Body};
    auto spell = addToHand(P1, kDecreeOfInsight);

    EXPECT_EQ(legalTargets(kDecreeOfInsight, P1), std::vector<GameObjectId>{enemy_body});
    resolve(kDecreeOfInsight, P1, exec, spell);
    EXPECT_EQ(state.getObject(enemy_body).current_might, 1);
    EXPECT_EQ(state.getObject(enemy_mind).current_might, 6);
    EXPECT_EQ(state.getObject(my_body).current_might, 6);
}

// ── 853 Temporal Breach ──────────────────────────────────────────────────

TEST_F(VendettaD, Vendetta_TemporalBreach_OwnerReplaysUnitAtSameLocation) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto unit = addUnit(P2, kPunchingPoro, 0, 1);
    {
        auto& u = state.getObject(unit);
        u.damage_marked = 1;
        u.buff_count = 2;
        u.is_exhausted = false;
        u.recomputeMight();
    }
    exec.empower(unit);
    auto spell = addToHand(P1, kTemporalBreach);

    resolve(kTemporalBreach, P1, exec, spell);
    const auto& u = state.getObject(unit);
    ASSERT_TRUE(u.battlefieldId().has_value());
    EXPECT_EQ(*u.battlefieldId(), BattlefieldId::fromRaw(1)) << "played back to the same location";
    EXPECT_EQ(u.controller, P2) << "its owner plays it";
    EXPECT_TRUE(u.is_exhausted) << "units enter exhausted";
    EXPECT_EQ(u.damage_marked, 0);
    EXPECT_EQ(u.buff_count, 0) << "it is a new object";
    EXPECT_FALSE(u.isEmpowered());
    EXPECT_FALSE(contains(state.player(P2).banishment, unit));
}

TEST_F(VendettaD, Vendetta_TemporalBreach_BanishedTokenCeasesToExist) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto token = addUnit(P2, kInvalidId, 3, 0);
    state.getObject(token).super_type = SuperType::Token;
    auto spell = addToHand(P1, kTemporalBreach);

    resolve(kTemporalBreach, P1, exec, spell);
    EXPECT_FALSE(state.getObject(token).location.has_value());
    EXPECT_EQ(state.getObject(token).zone, ZoneType::Banishment);
    EXPECT_FALSE(contains(state.player(P2).banishment, token));
}

// ── 859 Guttural Roar ────────────────────────────────────────────────────

TEST_F(VendettaD, Vendetta_GutturalRoar_PlusTwoOrPlusFourIfEmpowered) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto unit = addUnit(P1, kInvalidId, 2, 0);
    auto spell = addToHand(P1, kGutturalRoar);

    resolve(kGutturalRoar, P1, exec, spell);
    EXPECT_EQ(state.getObject(unit).temp_might_bonus, 2);

    state.getObject(unit).temp_might_bonus = 0;
    exec.empower(unit);
    resolve(kGutturalRoar, P1, exec, spell);
    EXPECT_EQ(state.getObject(unit).temp_might_bonus, 4) << "+4 instead of, not on top of, +2";
}

// ── 870 Rampage ──────────────────────────────────────────────────────────

TEST_F(VendettaD, Vendetta_Rampage_UnitsDealMightToEachOther) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto mine = addUnit(P1, kInvalidId, 3, 0);
    auto theirs = addUnit(P2, kInvalidId, 4, 0);
    auto spell = addToHand(P1, kRampage);

    resolve(kRampage, P1, exec, spell);
    EXPECT_EQ(state.getObject(mine).zone, ZoneType::Trash) << "took 4 at 3 [M]";
    EXPECT_EQ(state.getObject(theirs).damage_marked, 3);
    EXPECT_TRUE(state.getObject(theirs).location.has_value());
}

TEST_F(VendettaD, Vendetta_Rampage_PaidAdditionalCostGivesPlusTwoFirst) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    const Card* c = card_registry.get(kRampage);
    ASSERT_NE(c, nullptr);
    auto oac = c->optionalAdditionalCost();
    ASSERT_TRUE(oac.valid);
    EXPECT_EQ(oac.energy, 0);
    EXPECT_EQ(oac.power, 1);
    EXPECT_EQ(oac.power_domain, Domain::Body);
    EXPECT_FALSE(oac.any_domain);

    auto mine = addUnit(P1, kInvalidId, 3, 0);
    auto theirs = addUnit(P2, kInvalidId, 4, 0);
    auto spell = addToHand(P1, kRampage);
    state.getObject(spell).card_counters[oac.paid_flag] = 1;

    resolve(kRampage, P1, exec, spell);
    EXPECT_EQ(state.getObject(theirs).zone, ZoneType::Trash) << "took 5 at 4 [M]";
    EXPECT_TRUE(state.getObject(mine).location.has_value()) << "took 4 at 5 [M]";
    EXPECT_EQ(state.getObject(mine).damage_marked, 4);
    EXPECT_EQ(state.getObject(mine).temp_might_bonus, 2);
}

TEST_F(VendettaD, Vendetta_Rampage_NeedsAFriendlyAndAnEnemyUnit) {
    const Card* c = card_registry.get(kRampage);
    ASSERT_NE(c, nullptr);
    addUnit(P1, kInvalidId, 3, 0);
    EXPECT_FALSE(c->hasLegalTargets(state, P1));
    addUnit(P2, kInvalidId, 3, 0);
    EXPECT_TRUE(c->hasLegalTargets(state, P1));
}

// ── 872 Decree of Strength ───────────────────────────────────────────────

TEST_F(VendettaD, Vendetta_DecreeOfStrength_RecyclesAMindCardFromOpponentsHand) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto body_card = addToHand(P2, kOnslaught);
    auto mind_card = addToHand(P2, kDredgeUp);
    addToDeck(P2, kOnslaught);
    auto spell = addToHand(P1, kDecreeOfStrength);

    std::vector<GameObjectId> offered;
    driveResumable(kDecreeOfStrength, P1, spell, [&](const std::vector<Intent>& legal) {
        for (const auto& i : legal) offered.push_back(i.chosen_objects.at(0));
        return legal.front();
    }, exec);

    EXPECT_EQ(offered, std::vector<GameObjectId>{mind_card}) << "only Mind cards can be chosen";
    EXPECT_FALSE(inHand(P2, mind_card));
    EXPECT_TRUE(inHand(P2, body_card));
    ASSERT_EQ(deckSize(P2), 2);
    EXPECT_EQ(state.player(P2).main_deck.front(), mind_card) << "recycled to the bottom";
}

TEST_F(VendettaD, Vendetta_DecreeOfStrength_NoMindCardNothingRecycled) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto body_card = addToHand(P2, kOnslaught);
    auto spell = addToHand(P1, kDecreeOfStrength);
    resolve(kDecreeOfStrength, P1, exec, spell);
    EXPECT_TRUE(inHand(P2, body_card));
    EXPECT_EQ(deckSize(P2), 0);
}

// ── 876 Wild Claw ────────────────────────────────────────────────────────

TEST_F(VendettaD, Vendetta_WildClaw_PlaysUnitFromTopFiveAndEmpowersIt) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto below = addToDeck(P1, kDredgeUp);             // sixth from the top — not looked at
    auto poro = addToDeck(P1, kPunchingPoro);
    for (int i = 0; i < 4; ++i) addToDeck(P1, kDredgeUp);
    auto spell = addToHand(P1, kWildClaw);

    std::vector<GameObjectId> offered;
    int prompts = 0;
    // Through the engine: the text after "do this" is a reflexive trigger (CR
    // 387-389), a chain item of its own that only the engine queues and resolves.
    {
        EnginePathAgent engine_agent([&](const std::vector<Intent>& legal) {
            if (prompts++ == 0) {
                for (const auto& i : legal)
                    if (!i.chosen_objects.empty()) offered.push_back(i.chosen_objects[0]);
                return pickObject(legal, poro);
            }
            return pickValue(legal, 1);   // "Empower it" — yes
        });
        playThroughEngine(spell, &engine_agent);
    }

    EXPECT_EQ(offered, std::vector<GameObjectId>{poro}) << "spells aren't unit or gear";
    EXPECT_EQ(prompts, 2) << "no battlefield controlled, so no location prompt";
    const auto& p = state.getObject(poro);
    EXPECT_TRUE(p.isAtBase());
    EXPECT_TRUE(p.isEmpowered());
    EXPECT_FALSE(contains(state.player(P1).banishment, poro));
    EXPECT_EQ(deckSize(P1), 5) << "the other four were recycled";
    EXPECT_EQ(state.player(P1).main_deck.back(), below) << "recycled cards go to the bottom";
}

TEST_F(VendettaD, Vendetta_WildClaw_MayDeclineAndMayPlayToControlledBattlefield) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto poro = addToDeck(P1, kPunchingPoro);
    auto spell = addToHand(P1, kWildClaw);

    // Through the engine: the text after "do this" is a reflexive trigger (CR
    // 387-389), a chain item of its own that only the engine queues and resolves.
    {
        EnginePathAgent engine_agent([&](const std::vector<Intent>& legal) {
            return pickValue(legal, 0);   // play nothing
        });
        playThroughEngine(spell, &engine_agent);
    }
    EXPECT_TRUE(inDeck(P1, poro));
    EXPECT_FALSE(state.getObject(poro).location.has_value());

    state.battlefields[1].controller = P1;
    int prompts = 0;
    auto second_spell = addToHand(P1, kWildClaw);     // the first one was played above
    {
        EnginePathAgent engine_agent([&](const std::vector<Intent>& legal) {
            switch (prompts++) {
                case 0: return pickObject(legal, poro);
                case 1: return pickValue(legal, 2);   // battlefield index 1
                default: return pickValue(legal, 0);  // don't Empower (the reflexive trigger's "may")
            }
        });
        playThroughEngine(second_spell, &engine_agent);
    }
    EXPECT_EQ(prompts, 3);
    ASSERT_TRUE(state.getObject(poro).battlefieldId().has_value());
    EXPECT_EQ(*state.getObject(poro).battlefieldId(), BattlefieldId::fromRaw(1));
    EXPECT_FALSE(state.getObject(poro).isEmpowered());
}

TEST_F(VendettaD, Vendetta_WildClaw_OnlyReducesEnergyByFive) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto big = addToDeck(P1, kBaccaiSandspinner);      // [6] -> [1] left to pay
    auto spell = addToHand(P1, kWildClaw);

    int prompts = 0;
    driveResumable(kWildClaw, P1, spell, [&](const std::vector<Intent>& legal) {
        ++prompts;
        return pickObject(legal, big);
    }, exec);
    EXPECT_EQ(prompts, 0) << "can't pay the remaining [1], so it isn't offered";
    EXPECT_TRUE(inDeck(P1, big));

    auto rune = addRune(P1, Domain::Body);
    prompts = 0;
    driveResumable(kWildClaw, P1, spell, [&](const std::vector<Intent>& legal) {
        return prompts++ == 0 ? pickObject(legal, big) : pickValue(legal, 0);
    }, exec);
    EXPECT_TRUE(state.getObject(big).isAtBase());
    EXPECT_TRUE(state.getObject(rune).is_exhausted) << "the remaining [1] was paid";
}

TEST_F(VendettaD, Vendetta_WildClaw_PowerCostIsNotReduced) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto gear = addToDeck(P1, kRageAmplifier);         // [4][R] -> [R] left to pay
    auto off_domain = addRune(P1, Domain::Body);
    auto spell = addToHand(P1, kWildClaw);

    int prompts = 0;
    driveResumable(kWildClaw, P1, spell, [&](const std::vector<Intent>& legal) {
        ++prompts;
        return pickObject(legal, gear);
    }, exec);
    EXPECT_EQ(prompts, 0) << "no Fury rune to pay [R]";

    auto fury = addRune(P1, Domain::Fury);
    prompts = 0;
    driveResumable(kWildClaw, P1, spell, [&](const std::vector<Intent>& legal) {
        return prompts++ == 0 ? pickObject(legal, gear) : pickValue(legal, 0);
    }, exec);
    EXPECT_TRUE(state.getObject(gear).isAtBase());
    EXPECT_FALSE(state.getObject(fury).location.has_value()) << "recycled for [R]";
    EXPECT_TRUE(contains(state.player(P1).rune_deck, fury));
    EXPECT_TRUE(state.getObject(off_domain).location.has_value());
    EXPECT_FALSE(state.getObject(off_domain).is_exhausted) << "no Energy was left to pay";
}

}  // namespace
}  // namespace test_vendetta_D
}  // namespace riftbound::test
