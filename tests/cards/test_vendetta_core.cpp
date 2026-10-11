/// @file test_vendetta_core.cpp
/// Vendetta core mechanics: Empower / Disempower (CR 442-443, 827-828),
/// Burn (CR 441) and Flow, plus the first cards built on them.

#include "tests/cards/card_test_fixture.h"

#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/events.h"
#include "core/game_state.h"
#include "engine/effect_executor.h"

#include <gtest/gtest.h>

namespace riftbound::test {
namespace test_vendetta_core {  // file-unique: test sources are unity-built
namespace {

constexpr CardDefId kBaccaiSandspinner = 788;
constexpr CardDefId kPunchingPoro      = 794;
constexpr CardDefId kRageAmplifier     = 805;
constexpr CardDefId kApprenticeMage    = 834;
constexpr CardDefId kDredgeUp          = 836;
constexpr CardDefId kOnslaught         = 868;
constexpr CardDefId kSoulsReflection   = 938;

int auraMightSum(const GameObject& obj) {
    int n = 0;
    for (const auto& ae : obj.aura_effects) n += ae.might_bonus;
    return n;
}

// ── Empower / Disempower game actions ────────────────────────────────────

TEST_F(CardTestFixture, Vendetta_EmpowerIsBinary) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto unit = addUnit(P1, kPunchingPoro, 0, /*at_bf=*/0);

    EXPECT_TRUE(exec.empower(unit));
    EXPECT_TRUE(state.getObject(unit).isEmpowered());
    EXPECT_FALSE(exec.empower(unit)) << "CR 442.1.c: already Empowered -> nothing happens";
    EXPECT_EQ(state.getObject(unit).empowered, 1);

    EXPECT_TRUE(exec.disempower(unit));
    EXPECT_FALSE(state.getObject(unit).isEmpowered());
    EXPECT_FALSE(exec.disempower(unit)) << "CR 443.1.a.1: not Empowered -> does nothing";
}

TEST_F(CardTestFixture, Vendetta_EmpowerOnlyOnBoard) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto in_hand = addToHand(P1, kPunchingPoro);
    EXPECT_FALSE(exec.empower(in_hand)) << "Empowered is a board status (CR 442.2)";
}

// ── Empower keyword as an activated ability ──────────────────────────────

TEST_F(CardTestFixture, Vendetta_EmpowerAbilityDescriptor) {
    const Card* poro = card_registry.get(kPunchingPoro);
    ASSERT_NE(poro, nullptr);
    auto abs = poro->activatedAbilities();
    ASSERT_EQ(abs.size(), 1u);
    EXPECT_TRUE(abs[0].is_empower);
    EXPECT_TRUE(abs[0].cost.discard);
    EXPECT_EQ(abs[0].cost.discard_count, 1);
    EXPECT_EQ(poro->empowerAbilityIndex(), 0);
}

TEST_F(CardTestFixture, Vendetta_EmpowerOnlyIfNotEmpowered) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    const Card* poro = card_registry.get(kPunchingPoro);
    auto unit = addUnit(P1, kPunchingPoro, 0, 0);
    EXPECT_TRUE(poro->canActivateAbilityAt(state, P1, unit, 0));
    exec.empower(unit);
    EXPECT_FALSE(poro->canActivateAbilityAt(state, P1, unit, 0))
        << "CR 827.1.c.1: play only if not Empowered";
}

TEST_F(CardTestFixture, Vendetta_EmpoweredPassiveOnlyWhileEmpowered) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    const Card* poro = card_registry.get(kPunchingPoro);
    auto unit = addUnit(P1, kPunchingPoro, 0, 0);

    poro->applyPassiveAura(state, P1);
    EXPECT_EQ(auraMightSum(state.getObject(unit)), 0);

    state.getObject(unit).aura_effects.clear();
    exec.empower(unit);
    poro->applyPassiveAura(state, P1);
    EXPECT_EQ(auraMightSum(state.getObject(unit)), 1);
}

TEST_F(CardTestFixture, Vendetta_PassiveAppliesOncePerCopy) {
    // applyPassiveAura runs once per on-board copy; two Empowered copies must
    // each get +1, not +2.
    EffectExecutor exec(state, events, card_db, &card_registry);
    const Card* poro = card_registry.get(kPunchingPoro);
    auto a = addUnit(P1, kPunchingPoro, 0, 0);
    auto b = addUnit(P1, kPunchingPoro, 0, 0);
    exec.empower(a);
    exec.empower(b);
    poro->applyPassiveAura(state, P1);
    poro->applyPassiveAura(state, P1);
    EXPECT_EQ(auraMightSum(state.getObject(a)), 1);
    EXPECT_EQ(auraMightSum(state.getObject(b)), 1);
}

TEST_F(CardTestFixture, Vendetta_BaccaiSandspinner_CheaperWithFewRunes) {
    const Card* c = card_registry.get(kBaccaiSandspinner);
    ASSERT_NE(c, nullptr);
    const int idx = c->empowerAbilityIndex();
    ASSERT_GE(idx, 0);
    EXPECT_EQ(c->activatedAbilities()[idx].cost.energy, 5);
    for (int i = 0; i < 4; ++i) addRune(P1, Domain::Fury);
    EXPECT_EQ(c->activationCostReduction(state, P1, idx), 3) << "4 or fewer runes";
    addRune(P1, Domain::Fury);
    EXPECT_EQ(c->activationCostReduction(state, P1, idx), 0) << "5 runes";
}

TEST_F(CardTestFixture, Vendetta_RageAmplifier_BuffsUnitsMoreWhenEmpowered) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    const Card* amp = card_registry.get(kRageAmplifier);
    ASSERT_NE(amp, nullptr);
    auto gear = addUnit(P1, kRageAmplifier);         // placed in base
    state.getObject(gear).card_type = CardType::Gear;
    auto mine   = addUnit(P1, kInvalidId, 2, 0);
    auto theirs = addUnit(P2, kInvalidId, 2, 0);

    amp->applyPassiveAura(state, P1);
    EXPECT_EQ(auraMightSum(state.getObject(mine)), 1);
    EXPECT_EQ(auraMightSum(state.getObject(theirs)), 0);

    for (auto id : {gear, mine}) state.getObject(id).aura_effects.clear();
    exec.empower(gear);
    amp->applyPassiveAura(state, P1);
    EXPECT_EQ(auraMightSum(state.getObject(mine)), 2);
}

TEST_F(CardTestFixture, Vendetta_SoulsReflection_DisempowerCost) {
    const Card* legend = card_registry.get(kSoulsReflection);
    ASSERT_NE(legend, nullptr);
    auto abs = legend->activatedAbilities();
    ASSERT_EQ(abs.size(), 1u);
    EXPECT_TRUE(abs[0].cost.disempower_self);
    EXPECT_TRUE(abs[0].cost.exhaust);
    EXPECT_TRUE(legend->firesOn(TriggerType::WhenYouEmpower));
}

TEST_F(CardTestFixture, Vendetta_ApprenticeMage_TriggersOnBecomingEmpowered) {
    const Card* mage = card_registry.get(kApprenticeMage);
    ASSERT_NE(mage, nullptr);
    EXPECT_TRUE(mage->firesOn(TriggerType::WhenIBecomeEmpowered));
    EXPECT_EQ(mage->activatedAbilities().at(0).cost.energy, 2);
}

// ── Burn ─────────────────────────────────────────────────────────────────

TEST_F(CardTestFixture, Vendetta_BurnMovesTopCardsToTrash) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto bottom = addToDeck(P1, kDredgeUp);
    auto mid    = addToDeck(P1, kDredgeUp);
    auto top    = addToDeck(P1, kDredgeUp);

    auto burned = exec.burn(P1, 2);
    ASSERT_EQ(burned.size(), 2u);
    EXPECT_EQ(burned[0], top);
    EXPECT_EQ(burned[1], mid);
    EXPECT_TRUE(inTrash(P1, top));
    EXPECT_TRUE(inTrash(P1, mid));
    EXPECT_TRUE(inDeck(P1, bottom));
    EXPECT_EQ(state.player(P1).cards_burned_this_turn, 2);
}

TEST_F(CardTestFixture, Vendetta_BurnPastEmptyDeckBurnsOut) {
    // CR 441.4: burn what you can, burn out, then burn the rest.
    EffectExecutor exec(state, events, card_db, &card_registry);
    addToDeck(P1, kDredgeUp);
    const int before = state.player(P2).score;

    auto burned = exec.burn(P1, 2);
    EXPECT_EQ(burned.size(), 2u);
    EXPECT_EQ(state.player(P2).score, before + 1) << "burn out gives the opponent a point";
}

// ── Flow ─────────────────────────────────────────────────────────────────

TEST_F(CardTestFixture, Vendetta_FlowCosts) {
    const Card* dredge = card_registry.get(kDredgeUp);
    ASSERT_NE(dredge, nullptr);
    auto f = dredge->flowCost();
    EXPECT_TRUE(f.valid);
    EXPECT_EQ(f.energy, 2);
    EXPECT_EQ(f.power, 0);
    EXPECT_TRUE(card_db.get(kDredgeUp).keywords.has(Keyword::Flow));

    EXPECT_EQ(card_registry.get(kOnslaught)->flowCost().energy, 4);
    EXPECT_FALSE(card_registry.get(kPunchingPoro)->flowCost().valid);
}

TEST_F(CardTestFixture, Vendetta_DredgeUpDraws) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    addToDeck(P1, kOnslaught);
    auto spell = addToHand(P1, kDredgeUp);
    const int before = handSize(P1);
    invokeOnResolve(spell, kDredgeUp, P1, {}, exec);
    EXPECT_EQ(handSize(P1), before + 1);
}

}  // namespace
}  // namespace test_vendetta_core
}  // namespace riftbound::test
