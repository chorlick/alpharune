/// @file test_wave_a_batch4.cpp
/// Per-card tests for Wave A batch 4. One focused test (or small cluster) per
/// card. Cards whose entire printed text is an engine keyword / engine-handled
/// clause are exercised at the keyword-presence / engine-hook level.

#include "tests/cards/card_test_fixture.h"

#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/events.h"
#include "core/game_state.h"
#include "engine/effect_executor.h"

#include <gtest/gtest.h>

namespace riftbound::test {
namespace {

// Card def ids.
constexpr CardDefId kCaptainFarron      = 15;
constexpr CardDefId kWielderOfWater     = 55;
constexpr CardDefId kGemcraftSeer       = 100;
constexpr CardDefId kSaiScout           = 174;
constexpr CardDefId kGarenCommander     = 311;
constexpr CardDefId kMightOfDemacia     = 321;
constexpr CardDefId kBatteringRam       = 335;
constexpr CardDefId kRekSaiBreacher     = 352;
constexpr CardDefId kTiannaCrownguard   = 383;
constexpr CardDefId kTemporalPortal     = 401;
constexpr CardDefId kLaurentBladekeeper = 418;
constexpr CardDefId kForgottenMonument  = 523;
constexpr CardDefId kArenaKingpin       = 557;
constexpr CardDefId kLeBlanc            = 652;

// Sum the might bonus contributed by all aura effects on a unit.
int auraMight(const GameObject& o) {
    int b = 0;
    for (auto& ae : o.aura_effects) b += ae.might_bonus;
    return b;
}
bool auraGrantsKeyword(const GameObject& o, Keyword kw) {
    for (auto& ae : o.aura_effects) if (ae.keyword == kw) return true;
    return false;
}

// ─── [15] Captain Farron — "Other friendly units here have [Assault]." ──────

TEST_F(CardTestFixture, CaptainFarron_GrantsAssaultToOthersHere) {
    auto farron = addUnit(P1, kCaptainFarron, 5, /*at_bf=*/0);
    auto ally   = addUnit(P1, kInvalidId, 3, /*at_bf=*/0);    // same BF
    auto far    = addUnit(P1, kInvalidId, 3, /*at_bf=*/1);    // other BF
    auto enemy  = addUnit(P2, kInvalidId, 3, /*at_bf=*/0);    // enemy here

    card_registry.get(kCaptainFarron)->applyPassiveAura(state, P1);

    EXPECT_TRUE(auraGrantsKeyword(state.getObject(ally), Keyword::Assault));
    EXPECT_FALSE(auraGrantsKeyword(state.getObject(far), Keyword::Assault))
        << "not at my battlefield";
    EXPECT_FALSE(auraGrantsKeyword(state.getObject(enemy), Keyword::Assault))
        << "enemies excluded";
    EXPECT_FALSE(auraGrantsKeyword(state.getObject(farron), Keyword::Assault))
        << "'other' excludes myself";
}

TEST_F(CardTestFixture, CaptainFarron_AssaultMightOnlyWhileAttacking) {
    auto farron = addUnit(P1, kCaptainFarron, 5, /*at_bf=*/0);
    (void)farron;
    auto ally = addUnit(P1, kInvalidId, 3, /*at_bf=*/0);
    state.getObject(ally).combat_designation = CombatDesignation::Attacker;

    card_registry.get(kCaptainFarron)->applyPassiveAura(state, P1);
    EXPECT_EQ(auraMight(state.getObject(ally)), 1) << "+1 [M] while attacking";

    // Re-run with the ally not attacking → keyword present, no might bump.
    state.getObject(ally).aura_effects.clear();
    state.getObject(ally).combat_designation = CombatDesignation::None;
    card_registry.get(kCaptainFarron)->applyPassiveAura(state, P1);
    EXPECT_EQ(auraMight(state.getObject(ally)), 0);
}

// ─── [55] Wielder of Water — "attacking or defending alone, +2 [M]." ────────

TEST_F(CardTestFixture, WielderOfWater_AloneAttackerGetsPlus2) {
    auto w = addUnit(P1, kWielderOfWater, 2, /*at_bf=*/0);
    state.getObject(w).combat_designation = CombatDesignation::Attacker;
    card_registry.get(kWielderOfWater)->applyPassiveAura(state, P1);
    EXPECT_EQ(auraMight(state.getObject(w)), 2);
}

TEST_F(CardTestFixture, WielderOfWater_NotAloneNoBonus) {
    auto w = addUnit(P1, kWielderOfWater, 2, /*at_bf=*/0);
    auto buddy = addUnit(P1, kInvalidId, 3, /*at_bf=*/0);
    state.getObject(w).combat_designation = CombatDesignation::Attacker;
    state.getObject(buddy).combat_designation = CombatDesignation::Attacker;
    card_registry.get(kWielderOfWater)->applyPassiveAura(state, P1);
    EXPECT_EQ(auraMight(state.getObject(w)), 0)
        << "another friendly attacker here -> not alone";
}

TEST_F(CardTestFixture, WielderOfWater_NotInCombatNoBonus) {
    auto w = addUnit(P1, kWielderOfWater, 2, /*at_bf=*/0);
    state.getObject(w).combat_designation = CombatDesignation::None;
    card_registry.get(kWielderOfWater)->applyPassiveAura(state, P1);
    EXPECT_EQ(auraMight(state.getObject(w)), 0);
}

// ─── [100] Gemcraft Seer — "Other friendly units have [Vision]." ────────────

TEST_F(CardTestFixture, GemcraftSeer_GrantsVisionToOtherFriendlies) {
    auto seer  = addUnit(P1, kGemcraftSeer, 3, /*at_bf=*/0);
    auto ally  = addUnit(P1, kInvalidId, 3, /*at_bf=*/1);  // any location
    auto enemy = addUnit(P2, kInvalidId, 3, /*at_bf=*/0);

    card_registry.get(kGemcraftSeer)->applyPassiveAura(state, P1);

    EXPECT_TRUE(auraGrantsKeyword(state.getObject(ally), Keyword::Vision));
    EXPECT_FALSE(auraGrantsKeyword(state.getObject(seer), Keyword::Vision))
        << "'other' excludes the seer";
    EXPECT_FALSE(auraGrantsKeyword(state.getObject(enemy), Keyword::Vision));
}

// ─── [174] Sai Scout — Vision + "play me to an open battlefield" (engine). ──

TEST_F(CardTestFixture, SaiScout_HasVisionKeyword) {
    const auto& def = card_db.get(kSaiScout);
    EXPECT_TRUE(def.keywords.has(Keyword::Vision));
    // The "play me to an open battlefield" clause is matched by the engine's
    // action generator on the ability text — verify the text carries it.
    EXPECT_NE(def.ability_text.find("play me to an open battlefield"),
              std::string::npos);
}

// ─── [311] Garen, Commander — "Other friendly units have +1 [M] here." ──────

TEST_F(CardTestFixture, GarenCommander_BuffsOthersHere) {
    auto garen = addUnit(P1, kGarenCommander, 5, /*at_bf=*/0);
    auto ally  = addUnit(P1, kInvalidId, 3, /*at_bf=*/0);   // here
    auto far   = addUnit(P1, kInvalidId, 3, /*at_bf=*/1);   // elsewhere

    card_registry.get(kGarenCommander)->applyPassiveAura(state, P1);

    EXPECT_EQ(auraMight(state.getObject(ally)), 1);
    EXPECT_EQ(auraMight(state.getObject(far)), 0) << "only 'here'";
    EXPECT_EQ(auraMight(state.getObject(garen)), 0) << "'other' excludes self";
}

TEST_F(CardTestFixture, GarenCommander_InactiveWhileAtBase) {
    auto garen = addUnit(P1, kGarenCommander, 5, /*at_bf=*/-1);  // base
    (void)garen;
    auto ally  = addUnit(P1, kInvalidId, 3, /*at_bf=*/-1);       // base
    card_registry.get(kGarenCommander)->applyPassiveAura(state, P1);
    EXPECT_EQ(auraMight(state.getObject(ally)), 0)
        << "'here' = a battlefield; Garen at base grants nothing";
}

// ─── [321] Might of Demacia — conquer w/ 4+ units here -> draw 2. ────────────

TEST_F(CardTestFixture, MightOfDemacia_DrawsTwoOnConquerWith4Units) {
    auto legend = addUnit(P1, kMightOfDemacia, 0, /*at_bf=*/-1);
    state.getObject(legend).card_type = CardType::Legend;
    for (int i = 0; i < 4; ++i) addUnit(P1, kInvalidId, 2, /*at_bf=*/0);
    for (int i = 0; i < 3; ++i) addToDeck(P1, kInvalidId);
    state.player(P1).battlefields_scored_this_turn.insert(0);

    EffectExecutor exec(state, events, card_db);
    int hand_before = handSize(P1);
    fireTriggerAs(kMightOfDemacia, P1, legend, TriggerType::WhenIConquer, exec);
    EXPECT_EQ(handSize(P1), hand_before + 2);
}

TEST_F(CardTestFixture, MightOfDemacia_NoDrawUnderFourUnits) {
    auto legend = addUnit(P1, kMightOfDemacia, 0, /*at_bf=*/-1);
    state.getObject(legend).card_type = CardType::Legend;
    for (int i = 0; i < 3; ++i) addUnit(P1, kInvalidId, 2, /*at_bf=*/0);  // only 3
    for (int i = 0; i < 3; ++i) addToDeck(P1, kInvalidId);
    state.player(P1).battlefields_scored_this_turn.insert(0);

    EffectExecutor exec(state, events, card_db);
    int hand_before = handSize(P1);
    fireTriggerAs(kMightOfDemacia, P1, legend, TriggerType::WhenIConquer, exec);
    EXPECT_EQ(handSize(P1), hand_before) << "fewer than 4 units -> no draw";
}

// ─── [335] Battering Ram — cost -1 per card played, min [1]. ────────────────

TEST_F(CardTestFixture, BatteringRam_CostReductionScalesAndFloors) {
    Card* c = card_registry.get(kBatteringRam);
    // printed energy cost = 5.
    state.player(P1).cards_played_this_turn = 0;
    EXPECT_EQ(c->selfCostReduction(state, P1), 0);
    state.player(P1).cards_played_this_turn = 2;
    EXPECT_EQ(c->selfCostReduction(state, P1), 2);     // 5 - 2 = 3
    state.player(P1).cards_played_this_turn = 4;
    EXPECT_EQ(c->selfCostReduction(state, P1), 4);     // 5 - 4 = 1 (floor)
    state.player(P1).cards_played_this_turn = 10;
    EXPECT_EQ(c->selfCostReduction(state, P1), 4)
        << "net energy floored at 1 -> reduction capped at 4";
}

// ─── [352] Rek'Sai, Breacher — engine-handled clauses. ──────────────────────

TEST_F(CardTestFixture, RekSaiBreacher_HasAccelerateAndAssault) {
    const auto& def = card_db.get(kRekSaiBreacher);
    EXPECT_TRUE(def.keywords.has(Keyword::Accelerate));
    EXPECT_TRUE(def.keywords.has(Keyword::Assault));
}

// ─── [383] Tianna Crownguard — Deflect present; scoring-prevention escalated.

TEST_F(CardTestFixture, TiannaCrownguard_HasDeflect) {
    const auto& def = card_db.get(kTiannaCrownguard);
    EXPECT_TRUE(def.keywords.has(Keyword::Deflect));
    EXPECT_EQ(def.deflect_value, 1);
}

// ─── [401] Temporal Portal — activation cost modeled; effect escalated. ─────

TEST_F(CardTestFixture, TemporalPortal_ActivationCostIsPowerPlusExhaust) {
    Card* c = card_registry.get(kTemporalPortal);
    EXPECT_TRUE(c->hasActivatedAbility());
    auto cost = c->getActivationCost();
    EXPECT_TRUE(cost.exhaust);
    EXPECT_EQ(cost.power, 1);
}

// ─── [418] Laurent Bladekeeper — Ganking keyword. ───────────────────────────

TEST_F(CardTestFixture, LaurentBladekeeper_HasGanking) {
    const auto& def = card_db.get(kLaurentBladekeeper);
    EXPECT_TRUE(def.keywords.has(Keyword::Ganking))
        << "entire printed ability is the [Ganking] keyword";
}

// ─── [523] Forgotten Monument — turn-gated scoring. ─────────────────────────

TEST_F(CardTestFixture, ForgottenMonument_ReportsMinTurnThree) {
    auto* bfc = dynamic_cast<BattlefieldCard*>(card_registry.get(kForgottenMonument));
    ASSERT_NE(bfc, nullptr);
    EXPECT_EQ(bfc->minTurnToScore(), 3);
}

// ─── [557] Arena Kingpin — enters ready + [E]: +3 [M] this turn. ────────────

TEST_F(CardTestFixture, ArenaKingpin_EntersReady) {
    EXPECT_TRUE(card_registry.get(kArenaKingpin)->entersReadyOnPlay());
}

TEST_F(CardTestFixture, ArenaKingpin_AbilityGivesPlus3ThisTurn) {
    auto kp     = addUnit(P1, kArenaKingpin, 3, /*at_bf=*/0);
    auto target = addUnit(P1, kInvalidId, 4, /*at_bf=*/0);

    Card* c = card_registry.get(kArenaKingpin);
    EXPECT_TRUE(c->hasActivatedAbility());
    EXPECT_TRUE(c->getActivationCost().exhaust);
    EXPECT_EQ(c->getTargetRequirements().count, 1);

    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, kp};
    int before = state.getObject(target).current_might;  // 4
    c->onActivate(ctx, {target});
    EXPECT_EQ(state.getObject(target).temp_might_bonus, 3);
    EXPECT_EQ(state.getObject(target).current_might, before + 3);
}

// ─── [652] LeBlanc — Backline present; Temporary-suppression escalated. ─────

TEST_F(CardTestFixture, LeBlanc_HasBacklineAndTemporary) {
    const auto& def = card_db.get(kLeBlanc);
    EXPECT_TRUE(def.keywords.has(Keyword::Backline));
    EXPECT_TRUE(def.keywords.has(Keyword::Temporary));
}

}  // namespace
}  // namespace riftbound::test
