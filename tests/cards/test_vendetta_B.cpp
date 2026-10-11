/// @file test_vendetta_B.cpp
/// Vendetta batch B: unit cards 837-885 (Empower units, gear-count units,
/// move / conquer / discard triggers, cost reducers).

#include "tests/cards/card_test_fixture.h"

#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/card_registry.h"
#include "core/events.h"
#include "core/game_state.h"
#include "engine/effect_executor.h"

#include <gtest/gtest.h>

#include <functional>
#include <vector>

namespace riftbound::test {
namespace test_vendetta_B {  // file-unique: test sources are unity-built
namespace {

constexpr CardDefId kIronBallista        = 17;    // gear, Energy cost 3
constexpr CardDefId kRageAmplifier       = 805;   // gear, Energy cost 4
constexpr CardDefId kGrumpyRockbear      = 837;
constexpr CardDefId kOtterpus            = 840;
constexpr CardDefId kAppliedResearchers  = 842;
constexpr CardDefId kCovertInformant     = 844;
constexpr CardDefId kPatchedPorobot      = 845;
constexpr CardDefId kSkyCrusier          = 847;
constexpr CardDefId kNasusGuardian       = 850;
constexpr CardDefId kPlazaGuardian       = 851;
constexpr CardDefId kSwainVisionary      = 852;
constexpr CardDefId kJayceInventor       = 855;
constexpr CardDefId kMelNewlyAwakened    = 856;
constexpr CardDefId kBrutalHunter        = 857;
constexpr CardDefId kFretfulFeline       = 858;
constexpr CardDefId kLegionMarauder      = 861;
constexpr CardDefId kRepairSpecialist    = 863;
constexpr CardDefId kBaccaiWitherclaw    = 865;
constexpr CardDefId kDameTheDespoiler    = 866;
constexpr CardDefId kNoxianDemolitionist = 867;
constexpr CardDefId kProfiteer           = 869;
constexpr CardDefId kAmbessaTheWolf      = 871;
constexpr CardDefId kGangplankNaval      = 873;
constexpr CardDefId kJayceHammer         = 875;
constexpr CardDefId kCorruptedDragon     = 878;
constexpr CardDefId kRenektonBrute       = 879;
constexpr CardDefId kKinkouLifeblade     = 880;
constexpr CardDefId kMaskMother          = 881;
constexpr CardDefId kShadowOrderDisciple = 882;
constexpr CardDefId kShadowbladeLurker   = 883;
constexpr CardDefId kSpiderling          = 884;
constexpr CardDefId kStargazer           = 885;

int auraMightSum(const GameObject& obj) {
    int n = 0;
    for (const auto& ae : obj.aura_effects) n += ae.might_bonus;
    return n;
}

/// Value of an aura-granted keyword on `obj`, or -1 when it isn't granted.
int auraKeywordValue(const GameObject& obj, Keyword kw) {
    for (const auto& ae : obj.aura_effects)
        if (ae.keyword == kw) return ae.keyword_value;
    return -1;
}

Intent pickNo(const std::vector<Intent>& legal) { return legal.at(0); }
Intent pickYes(const std::vector<Intent>& legal) { return legal.at(1); }

class VendettaB : public CardTestFixture {
protected:
    /// A gear on `owner`'s base. def_id may be kInvalidId (cost-0 test gear).
    GameObjectId addGear(PlayerId owner, CardDefId def_id = kInvalidId) {
        auto id = addUnit(owner, def_id);
        auto& g = state.getObject(id);
        g.card_type = CardType::Gear;
        if (def_id == kInvalidId) g.name = "TestGear";
        return id;
    }

    /// A rune sitting in `owner`'s rune deck (so channelRunes has one to take).
    GameObjectId addRuneToRuneDeck(PlayerId owner, Domain domain) {
        auto id = state.createObject();
        auto& obj = state.getObject(id);
        obj.owner = owner;
        obj.controller = owner;
        obj.card_type = CardType::Rune;
        obj.name = "Rune";
        obj.domains = {domain};
        obj.zone = ZoneType::RuneDeck;
        state.player(owner).rune_deck.push_back(id);
        return id;
    }

    GameObjectId addLegend(PlayerId owner) {
        auto id = state.createObject();
        auto& obj = state.getObject(id);
        obj.owner = owner;
        obj.controller = owner;
        obj.card_type = CardType::Legend;
        obj.name = "TestLegend";
        obj.zone = ZoneType::LegendZone;
        state.player(owner).legend_zone = id;
        return id;
    }

    /// Put a card into its owner's trash (as if discarded).
    GameObjectId addToTrash(PlayerId owner, CardDefId def_id) {
        auto id = addToHand(owner, def_id);
        auto& h = state.player(owner).hand;
        h.erase(std::remove(h.begin(), h.end(), id), h.end());
        state.getObject(id).zone = ZoneType::Trash;
        state.player(owner).trash.push_back(id);
        return id;
    }

    /// driveResumableTrigger with a triggering subject on the chain item.
    void driveTriggerWithSubject(CardDefId def_id, PlayerId controller, GameObjectId source,
                                 TriggerType firing, GameObjectId subject,
                                 std::function<Intent(const std::vector<Intent>&)> picker,
                                 EffectExecutor& exec) {
        ChainItem ri;
        ri.id = state.chain.allocateId();
        ri.controller = controller;
        ri.is_ability = true;
        ri.source = source;
        ri.card_def_id = def_id;
        ri.fired_trigger = firing;
        ri.triggering_subject = subject;
        state.chain.resuming = ri;

        Card* card = card_registry.get(def_id);
        ASSERT_NE(card, nullptr);
        CardContext ctx{state, events, exec, controller, source};
        ctx.firing_trigger = firing;
        card->onTrigger(ctx, {});
        while (exec.hasPendingChoice()) {
            auto pending = exec.consumePendingChoice();
            exec.recordChoice(picker(pending.legal));
            card->onTrigger(ctx, {});
        }
        state.chain.resuming.reset();
    }

    /// Clear aura effects everywhere (what recalculateAuras does first).
    void clearAuras() {
        for (auto& [id, obj] : state.objects) {
            obj.aura_effects.clear();
            obj.immune_to_damage = false;
        }
        for (auto p : {P1, P2}) {
            auto& ps = state.player(p);
            ps.flow_cost_reduction = 0;
            ps.spell_discount_energy = 0;
            ps.spell_discount_power = 0;
            ps.spells_cant_be_countered = false;
            ps.extra_minus_might = 0;
        }
    }
};

// ── 837 Grumpy Rockbear ──────────────────────────────────────────────────

TEST_F(VendettaB, Vendetta_GrumpyRockbear_EmpowerCostsLessPerRune) {
    const Card* c = card_registry.get(kGrumpyRockbear);
    ASSERT_NE(c, nullptr);
    const int idx = c->empowerAbilityIndex();
    ASSERT_GE(idx, 0);
    EXPECT_EQ(c->activatedAbilities()[idx].cost.energy, 12);
    EXPECT_EQ(c->activationCostReduction(state, P1, idx), 0);
    for (int i = 0; i < 5; ++i) addRune(P1, Domain::Mind);
    addRune(P2, Domain::Mind);   // the opponent's runes don't count
    EXPECT_EQ(c->activationCostReduction(state, P1, idx), 5);
    EXPECT_EQ(c->activationCostReduction(state, P1, idx + 1), 0) << "only the Empower ability";
}

TEST_F(VendettaB, Vendetta_GrumpyRockbear_DeflectAndShieldOnlyWhileEmpowered) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    const Card* c = card_registry.get(kGrumpyRockbear);
    auto unit = addUnit(P1, kGrumpyRockbear, 0, 0);
    c->applyPassiveAura(state, P1);
    EXPECT_EQ(auraKeywordValue(state.getObject(unit), Keyword::Shield), -1);

    clearAuras();
    exec.empower(unit);
    c->applyPassiveAura(state, P1);
    EXPECT_EQ(auraKeywordValue(state.getObject(unit), Keyword::Shield), 3);
    EXPECT_EQ(auraKeywordValue(state.getObject(unit), Keyword::Deflect), 1);
}

// ── 840 Otterpus ─────────────────────────────────────────────────────────

TEST_F(VendettaB, Vendetta_Otterpus_EarlyScoreDrawsInsteadWhileOnBoard) {
    const Card* c = card_registry.get(kOtterpus);
    ASSERT_NE(c, nullptr);
    addToHand(P1, kOtterpus);                         // not on the board
    c->applyPassiveAura(state, P1);
    EXPECT_FALSE(state.turn.early_score_draws_instead);

    addUnit(P2, kOtterpus, 0, 0);                     // either player's Otterpus
    c->applyPassiveAura(state, P2);
    EXPECT_TRUE(state.turn.early_score_draws_instead) << "replaces every player's early point";
}

// ── 884 Spiderling ───────────────────────────────────────────────────────

TEST_F(VendettaB, Vendetta_Spiderling_AnyNumberOfCopies) {
    EXPECT_TRUE(card_db.get(kSpiderling).unlimited_copies);
    EXPECT_TRUE(card_db.get(kSpiderling).keywords.has(Keyword::Hidden));
    EXPECT_FALSE(card_db.get(kOtterpus).unlimited_copies) << "the 3-copy cap is the default";
}

// ── 842 Applied Researchers ──────────────────────────────────────────────

TEST_F(VendettaB, Vendetta_AppliedResearchers_SpellDiscountWhileEmpowered) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    const Card* c = card_registry.get(kAppliedResearchers);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->activatedAbilities().at(c->empowerAbilityIndex()).cost.energy, 3);
    auto unit = addUnit(P1, kAppliedResearchers, 0, 0);

    c->applyPassiveAura(state, P1);
    EXPECT_EQ(state.player(P1).spell_discount_energy, 0) << "not Empowered";

    clearAuras();
    exec.empower(unit);
    c->applyPassiveAura(state, P1);
    EXPECT_EQ(state.player(P1).spell_discount_energy, 1);
    EXPECT_EQ(state.player(P1).spell_discount_power, 1);
    EXPECT_EQ(state.player(P2).spell_discount_energy, 0) << "only YOUR spells";
}

// ── 844 Covert Informant ─────────────────────────────────────────────────

TEST_F(VendettaB, Vendetta_CovertInformant_DrawsOnMoveOnlyWhileEmpowered) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    const Card* c = card_registry.get(kCovertInformant);
    ASSERT_NE(c, nullptr);
    EXPECT_TRUE(c->firesOn(TriggerType::WhenIMove));
    addToDeck(P1, kStargazer);
    addToDeck(P1, kStargazer);
    auto unit = addUnit(P1, kCovertInformant, 0, 0);

    fireTriggerAs(kCovertInformant, P1, unit, TriggerType::WhenIMove, exec);
    EXPECT_EQ(handSize(P1), 0) << "not Empowered -> no draw";

    exec.empower(unit);
    fireTriggerAs(kCovertInformant, P1, unit, TriggerType::WhenIMove, exec);
    EXPECT_EQ(handSize(P1), 1);
}

// ── 845 Patched Porobot ──────────────────────────────────────────────────

TEST_F(VendettaB, Vendetta_PatchedPorobot_DrawsWithThreeGear) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    addToDeck(P1, kStargazer);
    addToDeck(P1, kStargazer);
    auto unit = addUnit(P1, kPatchedPorobot);
    addGear(P1);
    addGear(P1);
    addGear(P2);   // not mine
    fireTriggerAs(kPatchedPorobot, P1, unit, TriggerType::WhenYouPlayMe, exec);
    EXPECT_EQ(handSize(P1), 0) << "only 2 gear";

    addGear(P1);
    fireTriggerAs(kPatchedPorobot, P1, unit, TriggerType::WhenYouPlayMe, exec);
    EXPECT_EQ(handSize(P1), 1);
}

// ── 847 Sky Crusier ──────────────────────────────────────────────────────

TEST_F(VendettaB, Vendetta_SkyCrusier_CostAndTargets) {
    const Card* c = card_registry.get(kSkyCrusier);
    ASSERT_NE(c, nullptr);
    auto abs = c->activatedAbilities();
    ASSERT_EQ(abs.size(), 1u);
    EXPECT_TRUE(abs[0].cost.exhaust);
    EXPECT_EQ(abs[0].cost.energy, 1);
    EXPECT_TRUE(abs[0].cost.discard);
    EXPECT_EQ(abs[0].cost.discard_count, 1);
    EXPECT_EQ(abs[0].cost.discard_type_filter, 1 + static_cast<int>(CardType::Gear));

    auto at_bf   = addUnit(P2, kInvalidId, 3, 0);
    auto at_base = addUnit(P2, kInvalidId, 3);
    auto legal = c->enumerateLegalTargets(state, P1, 0);
    EXPECT_NE(std::find(legal.begin(), legal.end(), at_bf), legal.end());
    EXPECT_EQ(std::find(legal.begin(), legal.end(), at_base), legal.end())
        << "only units at a battlefield";
}

TEST_F(VendettaB, Vendetta_SkyCrusier_Deals4) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    Card* c = card_registry.get(kSkyCrusier);
    auto cruiser = addUnit(P1, kSkyCrusier);
    auto big   = addUnit(P2, kInvalidId, 6, 0);
    auto small = addUnit(P2, kInvalidId, 4, 1);
    CardContext ctx{state, events, exec, P1, cruiser};

    c->onActivate(ctx, 0, {big});
    EXPECT_EQ(state.getObject(big).damage_marked, 4);
    EXPECT_TRUE(state.getObject(big).location.has_value()) << "6 [M] survives 4";

    c->onActivate(ctx, 0, {small});
    EXPECT_FALSE(state.getObject(small).location.has_value()) << "4 [M] dies to 4";
}

// ── 850 Nasus, Guardian of Knowledge ─────────────────────────────────────

TEST_F(VendettaB, Vendetta_NasusGuardian_ChannelsOncePerTurn) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    const Card* c = card_registry.get(kNasusGuardian);
    ASSERT_NE(c, nullptr);
    EXPECT_TRUE(c->firesOn(TriggerType::WhenAnEnemyUnitDiesHere));
    auto nasus = addUnit(P1, kNasusGuardian, 0, 0);
    for (int i = 0; i < 3; ++i) addRuneToRuneDeck(P1, Domain::Mind);
    state.turn.turn_number = 4;

    fireTriggerAs(kNasusGuardian, P1, nasus, TriggerType::WhenAnEnemyUnitDiesHere, exec);
    EXPECT_EQ(exhaustedRuneCount(P1), 1);
    EXPECT_EQ(readyRuneCount(P1), 0) << "channeled exhausted";

    fireTriggerAs(kNasusGuardian, P1, nasus, TriggerType::WhenAnEnemyUnitDiesHere, exec);
    EXPECT_EQ(exhaustedRuneCount(P1), 1) << "once each turn";

    state.turn.turn_number = 5;
    fireTriggerAs(kNasusGuardian, P1, nasus, TriggerType::WhenAnEnemyUnitDiesHere, exec);
    EXPECT_EQ(exhaustedRuneCount(P1), 2) << "again on a later turn";
}

// ── 851 Plaza Guardian ───────────────────────────────────────────────────

TEST_F(VendettaB, Vendetta_PlazaGuardian_CostsLessPerGear) {
    const Card* c = card_registry.get(kPlazaGuardian);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->selfCostReduction(state, P1), 0);
    addGear(P1);
    addGear(P1);
    addGear(P2);
    EXPECT_EQ(c->selfCostReduction(state, P1), 2);
    for (int i = 0; i < 12; ++i) addGear(P1);
    EXPECT_EQ(c->selfCostReduction(state, P1), 10) << "never more than the printed [10]";
    EXPECT_TRUE(card_db.get(kPlazaGuardian).keywords.has(Keyword::Deflect));
}

// ── 852 Swain, Visionary ─────────────────────────────────────────────────

TEST_F(VendettaB, Vendetta_SwainVisionary_ScoresOnlyWithUnitGearAndSpell) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    const Card* c = card_registry.get(kSwainVisionary);
    ASSERT_NE(c, nullptr);
    EXPECT_TRUE(c->firesOn(TriggerType::WhenIConquer));
    auto swain = addUnit(P1, kSwainVisionary, 0, 0);
    auto& ps = state.player(P1);
    ps.played_nontoken_unit_this_turn = true;
    ps.played_spell_this_turn = true;

    fireTriggerAs(kSwainVisionary, P1, swain, TriggerType::WhenIConquer, exec);
    EXPECT_EQ(ps.score, 0) << "no non-token gear played";

    ps.played_nontoken_gear_this_turn = true;
    fireTriggerAs(kSwainVisionary, P1, swain, TriggerType::WhenIConquer, exec);
    EXPECT_EQ(ps.score, 1);
}

// ── 855 Jayce, Brilliant Inventor ────────────────────────────────────────

TEST_F(VendettaB, Vendetta_JayceInventor_ReadiesOnPlayAndMayDecline) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto jayce = addUnit(P1, kJayceInventor);
    state.getObject(jayce).is_exhausted = true;
    auto other = addUnit(P1, kInvalidId, 2);
    state.getObject(other).is_exhausted = true;

    driveResumableTrigger(kJayceInventor, P1, jayce, pickNo, exec, TriggerType::WhenYouPlayMe);
    EXPECT_TRUE(state.getObject(other).is_exhausted) << "declined";

    driveResumableTrigger(kJayceInventor, P1, jayce,
        [](const std::vector<Intent>& legal) { return legal.back(); },
        exec, TriggerType::WhenYouPlayMe);
    EXPECT_FALSE(state.getObject(other).is_exhausted);
    EXPECT_TRUE(state.getObject(jayce).is_exhausted) << "something besides me";
}

TEST_F(VendettaB, Vendetta_JayceInventor_FirstNonTokenGearEachTurn) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    const Card* c = card_registry.get(kJayceInventor);
    ASSERT_NE(c, nullptr);
    EXPECT_TRUE(c->firesOn(TriggerType::WhenYouPlayACard));
    auto jayce = addUnit(P1, kJayceInventor);
    auto other = addUnit(P1, kInvalidId, 2);
    // "the first time ... each turn" is the trigger's condition, so each
    // play is announced to Jayce through the engine; the agent says yes.
    (void)exec;
    auto playsCard = [&](GameObjectId played) {
        EnginePathAgent agent(EnginePathAgent::choosing(other, EnginePathAgent::May::Yes));
        fireTriggerThroughEngine(jayce, TriggerType::WhenYouPlayACard, &agent, played);
    };
    state.turn.turn_number = 3;

    // A token gear and a unit don't count.
    auto token_gear = addGear(P1);
    state.getObject(token_gear).super_type = SuperType::Token;
    state.getObject(other).is_exhausted = true;
    playsCard(token_gear);
    playsCard(other);
    EXPECT_TRUE(state.getObject(other).is_exhausted);

    // First non-token gear this turn readies.
    auto gear1 = addGear(P1, kIronBallista);
    playsCard(gear1);
    EXPECT_FALSE(state.getObject(other).is_exhausted);

    // Second one the same turn does not.
    state.getObject(other).is_exhausted = true;
    auto gear2 = addGear(P1, kRageAmplifier);
    playsCard(gear2);
    EXPECT_TRUE(state.getObject(other).is_exhausted) << "only the first each turn";

    // Next turn it works again.
    state.turn.turn_number = 5;
    playsCard(gear2);
    EXPECT_FALSE(state.getObject(other).is_exhausted);
}

// ── 856 Mel, Newly Awakened ──────────────────────────────────────────────

TEST_F(VendettaB, Vendetta_Mel_DrawsOnPlay) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    addToDeck(P1, kStargazer);
    auto mel = addUnit(P1, kMelNewlyAwakened);
    fireTriggerAs(kMelNewlyAwakened, P1, mel, TriggerType::WhenYouPlayMe, exec);
    EXPECT_EQ(handSize(P1), 1);
}

TEST_F(VendettaB, Vendetta_Mel_EmpoweredFlags) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    const Card* c = card_registry.get(kMelNewlyAwakened);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->activatedAbilities().at(c->empowerAbilityIndex()).cost.energy, 3);
    auto mel = addUnit(P1, kMelNewlyAwakened, 0, 0);

    c->applyPassiveAura(state, P1);
    EXPECT_FALSE(state.player(P1).spells_cant_be_countered);
    EXPECT_EQ(state.player(P1).extra_minus_might, 0);

    clearAuras();
    exec.empower(mel);
    c->applyPassiveAura(state, P1);
    EXPECT_TRUE(state.player(P1).spells_cant_be_countered);
    EXPECT_EQ(state.player(P1).extra_minus_might, 1);
    EXPECT_FALSE(state.player(P2).spells_cant_be_countered);
}

TEST_F(VendettaB, Vendetta_Mel_EmpoweredExtraMinusMightAndUncounterable) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    const Card* c = card_registry.get(kMelNewlyAwakened);
    ASSERT_NE(c, nullptr);
    auto mel = addUnit(P1, kMelNewlyAwakened, 0, 0);
    auto victim = addUnit(P2, kInvalidId, 9, 0);
    exec.empower(mel);
    c->applyPassiveAura(state, P1);

    // A resolving spell Mel's controller controls gives an additional -1 [M].
    ChainItem mine;
    mine.id = state.chain.allocateId();
    mine.controller = P1;
    mine.is_spell = true;
    state.chain.resuming = mine;
    exec.giveTemporaryMight(victim, -2);
    EXPECT_EQ(state.getObject(victim).temp_might_bonus, -3);
    exec.giveTemporaryMight(victim, 2);
    EXPECT_EQ(state.getObject(victim).temp_might_bonus, -1) << "+[M] is untouched";

    // The opponent's -[M] effects get nothing extra.
    ChainItem theirs = mine;
    theirs.controller = P2;
    state.chain.resuming = theirs;
    exec.giveTemporaryMight(victim, -2);
    EXPECT_EQ(state.getObject(victim).temp_might_bonus, -3);
    state.chain.resuming.reset();

    // Mel's controller's spells can't be countered; the opponent's still can.
    auto p2_spell = pushSpellOnChain(P2, kInvalidId);
    auto p1_spell = pushSpellOnChain(P1, kInvalidId);
    CardContext p2_ctx{state, events, exec, P2, p2_spell};
    p2_ctx.registry = &card_registry;
    counterChainTop(p2_ctx);
    ASSERT_EQ(state.chain.items.size(), 2u);
    EXPECT_EQ(state.chain.items.back().source, p1_spell);
    state.chain.items.pop_back();
    CardContext p1_ctx{state, events, exec, P1, p1_spell};
    p1_ctx.registry = &card_registry;
    counterChainTop(p1_ctx);
    EXPECT_TRUE(state.chain.items.empty());
}

// ── 857 / 861 / 880 simple Empowered stat units ──────────────────────────

TEST_F(VendettaB, Vendetta_BrutalHunter_EmpoweredMightAndGanking) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    const Card* c = card_registry.get(kBrutalHunter);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->activatedAbilities().at(c->empowerAbilityIndex()).cost.energy, 3);
    auto unit = addUnit(P1, kBrutalHunter, 0, 0);
    c->applyPassiveAura(state, P1);
    EXPECT_EQ(auraMightSum(state.getObject(unit)), 0);
    EXPECT_EQ(auraKeywordValue(state.getObject(unit), Keyword::Ganking), -1);

    clearAuras();
    exec.empower(unit);
    c->applyPassiveAura(state, P1);
    EXPECT_EQ(auraMightSum(state.getObject(unit)), 2);
    EXPECT_NE(auraKeywordValue(state.getObject(unit), Keyword::Ganking), -1);
}

TEST_F(VendettaB, Vendetta_LegionMarauder_EmpoweredPlusOne) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    const Card* c = card_registry.get(kLegionMarauder);
    ASSERT_NE(c, nullptr);
    const auto cost = c->activatedAbilities().at(c->empowerAbilityIndex()).cost;
    EXPECT_EQ(cost.energy, 1);
    EXPECT_EQ(cost.power, 0);
    auto unit = addUnit(P1, kLegionMarauder, 0, 0);
    c->applyPassiveAura(state, P1);
    EXPECT_EQ(auraMightSum(state.getObject(unit)), 0);
    clearAuras();
    exec.empower(unit);
    c->applyPassiveAura(state, P1);
    EXPECT_EQ(auraMightSum(state.getObject(unit)), 1);
}

TEST_F(VendettaB, Vendetta_KinkouLifeblade_EmpoweredMightAndGanking) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    const Card* c = card_registry.get(kKinkouLifeblade);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->activatedAbilities().at(c->empowerAbilityIndex()).cost.energy, 2);
    auto unit = addUnit(P1, kKinkouLifeblade, 0, 0);
    c->applyPassiveAura(state, P1);
    EXPECT_EQ(auraMightSum(state.getObject(unit)), 0);
    clearAuras();
    exec.empower(unit);
    c->applyPassiveAura(state, P1);
    EXPECT_EQ(auraMightSum(state.getObject(unit)), 1);
    EXPECT_NE(auraKeywordValue(state.getObject(unit), Keyword::Ganking), -1);
}

// ── 858 Fretful Feline ───────────────────────────────────────────────────

TEST_F(VendettaB, Vendetta_FretfulFeline_PlusTwoWhenReadied) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    const Card* c = card_registry.get(kFretfulFeline);
    ASSERT_NE(c, nullptr);
    EXPECT_TRUE(c->firesOn(TriggerType::WhenIBecomeReady)) << "effect readies and the Awaken phase";
    EXPECT_FALSE(c->firesOn(TriggerType::WhenIAmReadied));
    auto cat = addUnit(P1, kFretfulFeline, 0, 0);
    fireTriggerAs(kFretfulFeline, P1, cat, TriggerType::WhenIBecomeReady, exec);
    EXPECT_EQ(state.getObject(cat).temp_might_bonus, 2);
    EXPECT_EQ(state.getObject(cat).current_might, 7);
}

// ── 863 Repair Specialist ────────────────────────────────────────────────

TEST_F(VendettaB, Vendetta_RepairSpecialist_AssaultEqualsGearCount) {
    const Card* c = card_registry.get(kRepairSpecialist);
    ASSERT_NE(c, nullptr);
    auto unit = addUnit(P1, kRepairSpecialist, 0, 0);
    c->applyPassiveAura(state, P1);
    EXPECT_EQ(auraKeywordValue(state.getObject(unit), Keyword::Assault), -1) << "no gear";

    clearAuras();
    addGear(P1);
    addGear(P1);
    addGear(P1);
    addGear(P2);
    c->applyPassiveAura(state, P1);
    EXPECT_EQ(auraKeywordValue(state.getObject(unit), Keyword::Assault), 3);
    EXPECT_EQ(auraMightSum(state.getObject(unit)), 0) << "Assault only counts while attacking";

    clearAuras();
    state.getObject(unit).combat_designation = CombatDesignation::Attacker;
    c->applyPassiveAura(state, P1);
    // The +3 [M] while attacking is the engine's aura fold of keyword_value
    // (CR 807) — AurasCostsTest.AuraKeywordHelper_*; the effect carries none.
    EXPECT_EQ(auraKeywordValue(state.getObject(unit), Keyword::Assault), 3);
    EXPECT_EQ(auraMightSum(state.getObject(unit)), 0);
}

// ── 865 Baccai Witherclaw ────────────────────────────────────────────────

TEST_F(VendettaB, Vendetta_BaccaiWitherclaw_EmpowerCostAndMight) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    const Card* c = card_registry.get(kBaccaiWitherclaw);
    ASSERT_NE(c, nullptr);
    const auto cost = c->activatedAbilities().at(c->empowerAbilityIndex()).cost;
    EXPECT_EQ(cost.energy, 1);
    EXPECT_EQ(cost.power, 2);
    EXPECT_TRUE(cost.power_any_domain);

    auto unit = addUnit(P1, kBaccaiWitherclaw, 0, 0);
    exec.empower(unit);
    c->applyPassiveAura(state, P1);
    EXPECT_EQ(auraMightSum(state.getObject(unit)), 2);
}

TEST_F(VendettaB, Vendetta_BaccaiWitherclaw_DeathknellOnlyIfEmpowered) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    const Card* c = card_registry.get(kBaccaiWitherclaw);
    EXPECT_TRUE(c->firesOn(TriggerType::WhenIDie));
    for (int i = 0; i < 4; ++i) addRuneToRuneDeck(P1, Domain::Fury);
    auto unit = addUnit(P1, kBaccaiWitherclaw, 0, 0);

    fireTriggerAs(kBaccaiWitherclaw, P1, unit, TriggerType::WhenIDie, exec);
    EXPECT_EQ(exhaustedRuneCount(P1), 0) << "died while not Empowered";

    exec.empower(unit);
    fireTriggerAs(kBaccaiWitherclaw, P1, unit, TriggerType::WhenIDie, exec);
    EXPECT_EQ(exhaustedRuneCount(P1), 2);
    EXPECT_EQ(readyRuneCount(P1), 0);
}

TEST_F(VendettaB, Vendetta_BaccaiWitherclaw_DeathknellUsesLastKnownEmpowered) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    for (int i = 0; i < 4; ++i) addRuneToRuneDeck(P1, Domain::Fury);
    auto unit = addUnit(P1, kBaccaiWitherclaw, 0, 0);
    exec.empower(unit);
    exec.killObject(unit);
    EXPECT_GT(state.getObject(unit).card_counters["__died_empowered"], 0);
    state.getObject(unit).empowered = 0;   // what the aura pass does once it is off the board
    fireTriggerAs(kBaccaiWitherclaw, P1, unit, TriggerType::WhenIDie, exec);
    EXPECT_EQ(exhaustedRuneCount(P1), 2);

    // Dying again while not Empowered overwrites the snapshot.
    auto plain = addUnit(P1, kBaccaiWitherclaw, 0, 0);
    exec.killObject(plain);
    fireTriggerAs(kBaccaiWitherclaw, P1, plain, TriggerType::WhenIDie, exec);
    EXPECT_EQ(exhaustedRuneCount(P1), 2);
}

// ── 866 Dame the Despoiler ───────────────────────────────────────────────

TEST_F(VendettaB, Vendetta_Dame_MatchesBiggestUnitHereThenPlusOne) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    const Card* c = card_registry.get(kDameTheDespoiler);
    ASSERT_NE(c, nullptr);
    EXPECT_TRUE(c->firesOn(TriggerType::WhenIAttackOrDefend));
    const auto cost = c->activatedAbilities().at(c->empowerAbilityIndex()).cost;
    EXPECT_EQ(cost.energy, 5);
    EXPECT_EQ(cost.power, 1);
    EXPECT_EQ(cost.power_domain, Domain::Body);

    auto dame  = addUnit(P1, kDameTheDespoiler, 0, 0);   // 5 [M]
    auto giant = addUnit(P2, kInvalidId, 9, 0);
    addUnit(P2, kInvalidId, 20, 1);                       // elsewhere: not "here"
    auto pick_giant = [giant](const std::vector<Intent>& legal) {
        for (const auto& i : legal)
            if (!i.chosen_objects.empty() && i.chosen_objects[0] == giant) return i;
        ADD_FAILURE() << "giant not offered";
        return legal.front();
    };

    driveResumableTrigger(kDameTheDespoiler, P1, dame, pick_giant, exec,
                          TriggerType::WhenIAttackOrDefend);
    EXPECT_EQ(state.getObject(dame).current_might, 5) << "not Empowered -> nothing";

    exec.empower(dame);
    driveResumableTrigger(kDameTheDespoiler, P1, dame, pick_giant, exec,
                          TriggerType::WhenIAttackOrDefend);
    EXPECT_EQ(state.getObject(dame).current_might, 10) << "raised to 9, then +1";
}

TEST_F(VendettaB, Vendetta_Dame_SmallerUnitOnlyGivesPlusOne) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto dame  = addUnit(P1, kDameTheDespoiler, 0, 0);   // 5 [M]
    auto small = addUnit(P2, kInvalidId, 2, 0);
    exec.empower(dame);
    driveResumableTrigger(kDameTheDespoiler, P1, dame,
        [small](const std::vector<Intent>& legal) {
            for (const auto& i : legal)
                if (!i.chosen_objects.empty() && i.chosen_objects[0] == small) return i;
            return legal.front();
        }, exec, TriggerType::WhenIAttackOrDefend);
    EXPECT_EQ(state.getObject(dame).current_might, 6) << "Might is never lowered";
}

// ── 867 Noxian Demolitionist ─────────────────────────────────────────────

TEST_F(VendettaB, Vendetta_NoxianDemolitionist_KillsGearWithinMight) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    const Card* c = card_registry.get(kNoxianDemolitionist);
    ASSERT_NE(c, nullptr);
    EXPECT_TRUE(c->firesOn(TriggerType::WhenIConquer));
    auto demo   = addUnit(P1, kNoxianDemolitionist, 0, 0);     // 1 [M]
    auto costly = addGear(P2, kIronBallista);                  // Energy cost 3
    auto yes = [](const std::vector<Intent>& legal) { return legal.back(); };

    driveResumableTrigger(kNoxianDemolitionist, P1, demo, yes, exec, TriggerType::WhenIConquer);
    EXPECT_TRUE(state.getObject(costly).location.has_value()) << "cost 3 > 1 [M]";

    // Raise Might to 3: now the cost-3 gear is legal.
    exec.giveTemporaryMight(demo, 2);
    driveResumableTrigger(kNoxianDemolitionist, P1, demo, pickNo, exec, TriggerType::WhenIConquer);
    EXPECT_TRUE(state.getObject(costly).location.has_value()) << "you may: declined";

    driveResumableTrigger(kNoxianDemolitionist, P1, demo, yes, exec, TriggerType::WhenIConquer);
    EXPECT_FALSE(state.getObject(costly).location.has_value());
}

// ── 869 Profiteer ────────────────────────────────────────────────────────

TEST_F(VendettaB, Vendetta_Profiteer_MovesEmpowerFromOneThingToAnother) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto profiteer = addUnit(P1, kProfiteer);
    auto donor     = addUnit(P1, kLegionMarauder, 0, 0);
    auto legend    = addLegend(P1);
    exec.empower(donor);

    // yes, then the legend as the recipient (the donor is the only possible cost).
    int step = 0;
    driveResumableTrigger(kProfiteer, P1, profiteer,
        [&](const std::vector<Intent>& legal) {
            ++step;
            if (step == 1) return legal.at(1);                 // yes
            // The only Empowered thing is the cost without a prompt.
            const GameObjectId want = legend;
            for (const auto& i : legal)
                if (!i.chosen_objects.empty() && i.chosen_objects[0] == want) return i;
            ADD_FAILURE() << "wanted object not offered at step " << step;
            return legal.front();
        }, exec, TriggerType::WhenYouPlayMe);
    EXPECT_FALSE(state.getObject(donor).isEmpowered());
    EXPECT_TRUE(state.getObject(legend).isEmpowered());
    EXPECT_FALSE(state.getObject(profiteer).isEmpowered());
}

TEST_F(VendettaB, Vendetta_Profiteer_NothingEmpoweredOrDeclinedDoesNothing) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto profiteer = addUnit(P1, kProfiteer);
    auto other     = addUnit(P1, kLegionMarauder, 0, 0);
    auto theirs    = addUnit(P2, kLegionMarauder, 0, 0);
    exec.empower(theirs);   // not something I control

    int prompts = 0;
    driveResumableTrigger(kProfiteer, P1, profiteer,
        [&](const std::vector<Intent>& legal) { ++prompts; return legal.back(); },
        exec, TriggerType::WhenYouPlayMe);
    EXPECT_EQ(prompts, 0) << "nothing of mine is Empowered -> not offered";
    EXPECT_FALSE(state.getObject(other).isEmpowered());
    EXPECT_TRUE(state.getObject(theirs).isEmpowered());

    exec.empower(other);
    driveResumableTrigger(kProfiteer, P1, profiteer, pickNo, exec, TriggerType::WhenYouPlayMe);
    EXPECT_TRUE(state.getObject(other).isEmpowered()) << "declined";
}

// ── 871 Ambessa, The Wolf ────────────────────────────────────────────────

TEST_F(VendettaB, Vendetta_Ambessa_EmpoweredImmuneOutsideCombat) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    const Card* c = card_registry.get(kAmbessaTheWolf);
    ASSERT_NE(c, nullptr);
    const auto cost = c->activatedAbilities().at(c->empowerAbilityIndex()).cost;
    EXPECT_EQ(cost.energy, 3);
    EXPECT_EQ(cost.power, 1);
    EXPECT_EQ(cost.power_domain, Domain::Body);
    auto unit = addUnit(P1, kAmbessaTheWolf, 0, 0);
    auto src  = addUnit(P2, kInvalidId, 3, 0);

    c->applyPassiveAura(state, P1);
    EXPECT_FALSE(state.getObject(unit).immune_to_damage) << "not Empowered";
    EXPECT_EQ(auraMightSum(state.getObject(unit)), 0);

    clearAuras();
    exec.empower(unit);
    c->applyPassiveAura(state, P1);
    EXPECT_EQ(auraMightSum(state.getObject(unit)), 3);
    EXPECT_TRUE(state.getObject(unit).immune_to_damage);
    exec.dealDamage(unit, 2, src);
    EXPECT_EQ(state.getObject(unit).damage_marked, 0);

    clearAuras();
    state.getObject(unit).combat_designation = CombatDesignation::Defender;
    c->applyPassiveAura(state, P1);
    EXPECT_FALSE(state.getObject(unit).immune_to_damage) << "in combat -> can be damaged";
    exec.dealDamage(unit, 2, src);
    EXPECT_EQ(state.getObject(unit).damage_marked, 2);
}

// ── 873 Gangplank, Naval (Empower cost only; replacement is an engine gap) ─

TEST_F(VendettaB, Vendetta_GangplankNaval_EmpowerCost) {
    const Card* c = card_registry.get(kGangplankNaval);
    ASSERT_NE(c, nullptr);
    const int idx = c->empowerAbilityIndex();
    ASSERT_GE(idx, 0);
    const auto cost = c->activatedAbilities()[idx].cost;
    EXPECT_EQ(cost.energy, 0);
    EXPECT_EQ(cost.power, 2);
    EXPECT_EQ(cost.power_domain, Domain::Body);
    EXPECT_FALSE(cost.power_any_domain);
}

// ── 875 Jayce, Hammer in Hand ────────────────────────────────────────────

TEST_F(VendettaB, Vendetta_JayceHammer_NoBaseKeywords) {
    const auto& def = card_db.get(kJayceHammer);
    EXPECT_FALSE(def.keywords.has(Keyword::Assault));
    EXPECT_FALSE(def.keywords.has(Keyword::Deflect));
    EXPECT_FALSE(def.keywords.has(Keyword::Ganking));
    // "When I become ready" covers effect readies and the Awaken phase.
    EXPECT_TRUE(card_registry.get(kJayceHammer)->firesOn(TriggerType::WhenIBecomeReady));
    EXPECT_FALSE(card_registry.get(kJayceHammer)->firesOn(TriggerType::WhenIAmReadied));
}

TEST_F(VendettaB, Vendetta_JayceHammer_ChoosesOneKeywordWhenReadied) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto pick_mode = [](int m) {
        return [m](const std::vector<Intent>& legal) {
            for (const auto& i : legal)
                if (i.chosen_value.has_value() && *i.chosen_value == m) return i;
            return legal.front();
        };
    };
    auto a = addUnit(P1, kJayceHammer, 0, 0);
    driveResumableTrigger(kJayceHammer, P1, a, pick_mode(0), exec, TriggerType::WhenIBecomeReady);
    EXPECT_TRUE(state.getObject(a).keywords.has(Keyword::Assault));
    EXPECT_EQ(state.getObject(a).temp_assault_value, 2);
    EXPECT_FALSE(state.getObject(a).keywords.has(Keyword::Ganking));
    EXPECT_FALSE(state.getObject(a).keywords.has(Keyword::Deflect));

    auto b = addUnit(P1, kJayceHammer, 0, 0);
    driveResumableTrigger(kJayceHammer, P1, b, pick_mode(1), exec, TriggerType::WhenIBecomeReady);
    EXPECT_TRUE(state.getObject(b).keywords.has(Keyword::Deflect));
    // "this turn": the grant is not written into the printed deflect_value
    // (it never expired from there); what an opponent must pay is [A][A].
    EXPECT_EQ(exec.deflectCostFor(P2, b), 2);
    EXPECT_EQ(state.getObject(b).deflect_value, 0);
    EXPECT_FALSE(state.getObject(b).keywords.has(Keyword::Assault));

    auto g = addUnit(P1, kJayceHammer, 0, 0);
    driveResumableTrigger(kJayceHammer, P1, g, pick_mode(2), exec, TriggerType::WhenIBecomeReady);
    EXPECT_TRUE(state.getObject(g).keywords.has(Keyword::Ganking));
    EXPECT_TRUE(state.getObject(g).temp_keywords.has(Keyword::Ganking)) << "this turn only";
    EXPECT_FALSE(state.getObject(g).keywords.has(Keyword::Assault));
}

// ── 878 Corrupted Dragon ─────────────────────────────────────────────────

TEST_F(VendettaB, Vendetta_CorruptedDragon_EntersReadyUnlessNearVictory) {
    const Card* c = card_registry.get(kCorruptedDragon);
    ASSERT_NE(c, nullptr);
    const int victory = state.mode.victory_score;
    setScore(P1, victory - 4);
    EXPECT_TRUE(c->entersReadyOnPlay(state, P1));
    setScore(P1, victory - 3);
    EXPECT_FALSE(c->entersReadyOnPlay(state, P1)) << "within 3 points";
}

TEST_F(VendettaB, Vendetta_CorruptedDragon_MovesChosenSmallEnemiesToBase) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    const Card* c = card_registry.get(kCorruptedDragon);
    EXPECT_TRUE(c->firesOn(TriggerType::WhenIAttack));
    auto dragon = addUnit(P1, kCorruptedDragon, 0, 0);
    auto small1 = addUnit(P2, kInvalidId, 5, 0);
    auto small2 = addUnit(P2, kInvalidId, 2, 0);
    auto big    = addUnit(P2, kInvalidId, 6, 0);
    auto away   = addUnit(P2, kInvalidId, 1, 1);
    auto mine   = addUnit(P1, kInvalidId, 1, 0);

    // Take every unit offered: only the two <=5 [M] enemies here are offered.
    bool offered_illegal = false;
    driveResumableTrigger(kCorruptedDragon, P1, dragon,
        [&](const std::vector<Intent>& legal) {
            for (const auto& i : legal)
                if (!i.chosen_objects.empty() &&
                    i.chosen_objects[0] != small1 && i.chosen_objects[0] != small2)
                    offered_illegal = true;
            return legal.front();
        }, exec, TriggerType::WhenIAttack);
    EXPECT_FALSE(offered_illegal);
    EXPECT_TRUE(state.getObject(small1).isAtBase());
    EXPECT_TRUE(state.getObject(small2).isAtBase());
    EXPECT_TRUE(state.getObject(big).isAtBattlefield()) << "6 [M] is too big";
    EXPECT_TRUE(state.getObject(away).isAtBattlefield()) << "not here";
    EXPECT_TRUE(state.getObject(mine).isAtBattlefield()) << "enemy units only";
    EXPECT_TRUE(state.getObject(dragon).isAtBattlefield());
}

TEST_F(VendettaB, Vendetta_CorruptedDragon_MayStopEarly) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto dragon = addUnit(P1, kCorruptedDragon, 0, 0);
    auto small1 = addUnit(P2, kInvalidId, 3, 0);
    auto small2 = addUnit(P2, kInvalidId, 2, 0);

    // "done" is the trailing choice with no chosen object.
    driveResumableTrigger(kCorruptedDragon, P1, dragon,
        [](const std::vector<Intent>& legal) { return legal.back(); },
        exec, TriggerType::WhenIAttack);
    EXPECT_TRUE(state.getObject(small1).isAtBattlefield());
    EXPECT_TRUE(state.getObject(small2).isAtBattlefield());

    // Move exactly one, then stop.
    int calls = 0;
    driveResumableTrigger(kCorruptedDragon, P1, dragon,
        [&](const std::vector<Intent>& legal) {
            return ++calls == 1 ? legal.front() : legal.back();
        }, exec, TriggerType::WhenIAttack);
    const int at_base = static_cast<int>(state.getObject(small1).isAtBase()) +
                        static_cast<int>(state.getObject(small2).isAtBase());
    EXPECT_EQ(at_base, 1);
}

// ── 879 Renekton, Brute ──────────────────────────────────────────────────

TEST_F(VendettaB, Vendetta_RenektonBrute_PayOneForPlusOne) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    Card* c = card_registry.get(kRenektonBrute);
    ASSERT_NE(c, nullptr);
    auto abs = c->activatedAbilities();
    ASSERT_EQ(abs.size(), 1u) << "no printed [Empower]";
    EXPECT_EQ(abs[0].cost.energy, 1);
    EXPECT_FALSE(abs[0].cost.exhaust);
    EXPECT_FALSE(abs[0].is_empower);

    auto unit = addUnit(P1, kRenektonBrute, 0, 0);
    CardContext ctx{state, events, exec, P1, unit};
    c->onActivate(ctx, 0, {});
    c->onActivate(ctx, 0, {});
    EXPECT_EQ(state.getObject(unit).temp_might_bonus, 2);
    EXPECT_EQ(state.getObject(unit).current_might, 6);
}

TEST_F(VendettaB, Vendetta_RenektonBrute_DeflectAndGankingWhileEmpowered) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    const Card* c = card_registry.get(kRenektonBrute);
    auto unit = addUnit(P1, kRenektonBrute, 0, 0);
    c->applyPassiveAura(state, P1);
    EXPECT_EQ(auraKeywordValue(state.getObject(unit), Keyword::Deflect), -1);

    clearAuras();
    exec.empower(unit);
    c->applyPassiveAura(state, P1);
    EXPECT_EQ(auraKeywordValue(state.getObject(unit), Keyword::Deflect), 1);
    EXPECT_NE(auraKeywordValue(state.getObject(unit), Keyword::Ganking), -1);
}

// ── 881 Mask Mother ──────────────────────────────────────────────────────

TEST_F(VendettaB, Vendetta_MaskMother_PayOneToBuffWhenDiscarded) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    const Card* c = card_registry.get(kMaskMother);
    ASSERT_NE(c, nullptr);
    EXPECT_TRUE(c->firesOn(TriggerType::WhenIAmDiscarded));
    auto mother = addToTrash(P1, kMaskMother);
    auto ally   = addUnit(P1, kInvalidId, 3, 0);
    auto enemy  = addUnit(P2, kInvalidId, 3, 0);
    auto& ps = state.player(P1);
    bool enemy_offered = false;
    auto yes_ally = [&](const std::vector<Intent>& legal) {
        for (const auto& i : legal)
            if (!i.chosen_objects.empty() && i.chosen_objects[0] == enemy) enemy_offered = true;
        return legal.back();
    };

    int prompts = 0;
    driveResumableTrigger(kMaskMother, P1, mother,
        [&](const std::vector<Intent>& legal) { ++prompts; return legal.back(); },
        exec, TriggerType::WhenIAmDiscarded);
    EXPECT_EQ(prompts, 0) << "no energy -> not offered";
    EXPECT_EQ(state.getObject(ally).temp_might_bonus, 0);

    ps.rune_pool.energy = 2;
    driveResumableTrigger(kMaskMother, P1, mother, pickNo, exec, TriggerType::WhenIAmDiscarded);
    EXPECT_EQ(ps.rune_pool.energy, 2) << "declined: nothing paid";
    EXPECT_EQ(state.getObject(ally).temp_might_bonus, 0);

    driveResumableTrigger(kMaskMother, P1, mother, yes_ally, exec, TriggerType::WhenIAmDiscarded);
    EXPECT_EQ(ps.rune_pool.energy, 1) << "paid exactly [1]";
    EXPECT_EQ(state.getObject(ally).temp_might_bonus, 2);
    EXPECT_FALSE(enemy_offered) << "friendly units only";
    EXPECT_EQ(state.getObject(enemy).temp_might_bonus, 0);

    // With an empty pool the [1] comes from a ready rune.
    ps.rune_pool.energy = 0;
    addRune(P1, Domain::Chaos);
    driveResumableTrigger(kMaskMother, P1, mother, yes_ally, exec, TriggerType::WhenIAmDiscarded);
    EXPECT_EQ(readyRuneCount(P1), 0);
    EXPECT_EQ(exhaustedRuneCount(P1), 1);
    EXPECT_EQ(state.getObject(ally).temp_might_bonus, 4);
}

// ── 882 Shadow Order Disciple ────────────────────────────────────────────

TEST_F(VendettaB, Vendetta_ShadowOrderDisciple_BurnForMight) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    const Card* c = card_registry.get(kShadowOrderDisciple);
    ASSERT_NE(c, nullptr);
    EXPECT_TRUE(c->firesOn(TriggerType::WhenIMove));
    auto unit = addUnit(P1, kShadowOrderDisciple, 0, 0);
    addToDeck(P1, kStargazer);
    auto top = addToDeck(P1, kStargazer);

    driveResumableTrigger(kShadowOrderDisciple, P1, unit, pickNo, exec, TriggerType::WhenIMove);
    EXPECT_EQ(deckSize(P1), 2) << "declined";
    EXPECT_EQ(state.getObject(unit).temp_might_bonus, 0);

    driveResumableTrigger(kShadowOrderDisciple, P1, unit, pickYes, exec, TriggerType::WhenIMove);
    EXPECT_TRUE(inTrash(P1, top));
    EXPECT_EQ(deckSize(P1), 1);
    EXPECT_EQ(state.player(P1).cards_burned_this_turn, 1);
    EXPECT_EQ(state.getObject(unit).temp_might_bonus, 1);
}

TEST_F(VendettaB, Vendetta_ShadowOrderDisciple_EmptyDeckNotOffered) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto unit = addUnit(P1, kShadowOrderDisciple, 0, 0);
    int prompts = 0;
    driveResumableTrigger(kShadowOrderDisciple, P1, unit,
        [&](const std::vector<Intent>& legal) { ++prompts; return legal.back(); },
        exec, TriggerType::WhenIMove);
    EXPECT_EQ(prompts, 0);
    EXPECT_EQ(state.getObject(unit).temp_might_bonus, 0);
}

// ── 883 Shadowblade Lurker ───────────────────────────────────────────────

TEST_F(VendettaB, Vendetta_ShadowbladeLurker_CostsTwoLessPerCopyInTrash) {
    const Card* c = card_registry.get(kShadowbladeLurker);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->selfCostReduction(state, P1), 0);
    addToTrash(P1, kShadowbladeLurker);
    addToTrash(P1, kStargazer);            // different name
    addToTrash(P2, kShadowbladeLurker);    // the opponent's trash
    EXPECT_EQ(c->selfCostReduction(state, P1), 2);
    addToTrash(P1, kShadowbladeLurker);
    EXPECT_EQ(c->selfCostReduction(state, P1), 4);
    addToTrash(P1, kShadowbladeLurker);
    EXPECT_EQ(c->selfCostReduction(state, P1), 5) << "capped at the printed [5]";
}

// ── 885 Stargazer ────────────────────────────────────────────────────────

TEST_F(VendettaB, Vendetta_Stargazer_FlowDiscountWhileOnBoard) {
    const Card* c = card_registry.get(kStargazer);
    ASSERT_NE(c, nullptr);
    addToHand(P1, kStargazer);
    c->applyPassiveAura(state, P1);
    EXPECT_EQ(state.player(P1).flow_cost_reduction, 0) << "not on the board";

    addUnit(P1, kStargazer, 0, 0);
    c->applyPassiveAura(state, P1);
    c->applyPassiveAura(state, P1);   // one call per on-board copy per pass
    EXPECT_EQ(state.player(P1).flow_cost_reduction, 2) << "applied once per copy";
    EXPECT_EQ(state.player(P2).flow_cost_reduction, 0);

    clearAuras();
    addUnit(P1, kStargazer);
    c->applyPassiveAura(state, P1);
    c->applyPassiveAura(state, P1);
    EXPECT_EQ(state.player(P1).flow_cost_reduction, 4) << "two Stargazers stack";
}

}  // namespace
}  // namespace test_vendetta_B
}  // namespace riftbound::test
