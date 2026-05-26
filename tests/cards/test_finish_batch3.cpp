/// @file test_finish_batch3.cpp
/// Per-card tests for "finish batch 3" cards. Every card in this batch is
/// fully blocked on an UNWIRED engine primitive (see the ESCALATE comment in
/// each card .cpp), so these tests verify what IS true and load-bearing today:
///   - each card is registered in the CardRegistry under its def id,
///   - its CardDef carries the printed cost / might / keywords that the engine
///     already honors (e.g. LeBlanc's [Backline]),
///   - its ability_text matches the printed text the engine's substring hooks
///     key on (e.g. Miss Fortune's engine-handled "play me to an open
///     battlefield" self-clause).
///
/// These tests deliberately do NOT assert the escalated effects (aura-granted
/// abilities, becomes-Mighty / attacks-alone / defend-at-BF triggers,
/// discard-as-additional-cost, reveal replacement, trigger suppression,
/// spells-have-Repeat) because no primitive exists to implement them — see the
/// task report's ESCALATED list. Fabricating a passing test for an
/// unimplemented effect is forbidden.

#include "tests/cards/card_test_fixture.h"

using namespace riftbound;
using namespace riftbound::test;

namespace {

constexpr CardDefId kForgeOfFluft      = 522;
constexpr CardDefId kGardensBecoming   = 769;
constexpr CardDefId kMaskOfForesight   = 60;
constexpr CardDefId kBrazenBuccaneer   = 2;
constexpr CardDefId kMageseekerWarden  = 70;
constexpr CardDefId kMissFortuneBucc   = 193;
constexpr CardDefId kVoidHatchling     = 341;
constexpr CardDefId kLoyalPup          = 447;
constexpr CardDefId kFioraWorthy       = 500;
constexpr CardDefId kLeBlancEverywhere = 652;
constexpr CardDefId kSyndraTrans       = 708;

class FinishBatch3 : public CardTestFixture {};

// ── Forge of the Fluft (522) — ESCALATED (aura-granted activated ability) ──
TEST_F(FinishBatch3, ForgeOfFluftRegisteredAsBattlefield) {
    Card* c = card_registry.get(kForgeOfFluft);
    ASSERT_NE(c, nullptr);
    const auto& d = c->def();
    EXPECT_EQ(d.card_type, CardType::Battlefield);
    EXPECT_NE(d.ability_text.find("friendly legends have"), std::string::npos);
    // No aura-granted-ability mechanism: card grants no activated ability.
    EXPECT_TRUE(c->activatedAbilities().empty());
}

// ── Gardens of Becoming (769) — ESCALATED (aura-granted activated ability) ──
TEST_F(FinishBatch3, GardensOfBecomingRegisteredAsBattlefield) {
    Card* c = card_registry.get(kGardensBecoming);
    ASSERT_NE(c, nullptr);
    const auto& d = c->def();
    EXPECT_EQ(d.card_type, CardType::Battlefield);
    EXPECT_NE(d.ability_text.find("Units here have"), std::string::npos);
    EXPECT_TRUE(c->activatedAbilities().empty());
}

// ── Mask of Foresight (60) — ESCALATED (WhenAUnitAttacksOrDefendsAlone) ──
TEST_F(FinishBatch3, MaskOfForesightDefAndNoWiredTrigger) {
    Card* c = card_registry.get(kMaskOfForesight);
    ASSERT_NE(c, nullptr);
    const auto& d = c->def();
    EXPECT_EQ(d.card_type, CardType::Gear);
    EXPECT_EQ(d.energy_cost, 2);
    // The only trigger it could use is unwired; the card declares none, so it
    // never fires a phantom effect.
    EXPECT_TRUE(c->triggerTypes().empty());
}

// ── Brazen Buccaneer (2) — ESCALATED (discard-as-additional-cost) ──
TEST_F(FinishBatch3, BrazenBuccaneerHasNoDiscardCostHook) {
    Card* c = card_registry.get(kBrazenBuccaneer);
    ASSERT_NE(c, nullptr);
    const auto& d = c->def();
    EXPECT_EQ(d.energy_cost, 6);
    EXPECT_EQ(d.might, 5);
    // optionalAdditionalCost only models energy/power; the card declares none,
    // so it never silently mis-applies a cost reduction.
    EXPECT_FALSE(c->optionalAdditionalCost().valid);
    EXPECT_EQ(c->selfCostReduction(state, P1), 0);
}

// ── Mageseeker Warden (70) — ESCALATED (play-narrowing + ready-suppression) ──
TEST_F(FinishBatch3, MageseekerWardenDefOnly) {
    Card* c = card_registry.get(kMageseekerWarden);
    ASSERT_NE(c, nullptr);
    const auto& d = c->def();
    EXPECT_EQ(d.energy_cost, 6);
    EXPECT_EQ(d.power_cost, 1);
    EXPECT_EQ(d.might, 5);
    // getPlayLocations is never consulted by the engine and the card declares
    // no override, so it imposes no (incorrect) restriction on itself.
    EXPECT_TRUE(c->getPlayLocations(state, P1).empty());
}

// ── Miss Fortune, Buccaneer (193) — self-clause ENGINE-HANDLED, grant ESC ──
TEST_F(FinishBatch3, MissFortuneSelfClauseTextMatchesEngineHook) {
    Card* c = card_registry.get(kMissFortuneBucc);
    ASSERT_NE(c, nullptr);
    const auto& d = c->def();
    EXPECT_EQ(d.super_type, SuperType::Champion);
    // The engine's GameEngine::generateMainPhaseActions keys the open-BF play
    // allowance on this exact substring; verify it is present so the
    // engine-handled self-clause keeps working.
    EXPECT_NE(d.ability_text.find("play me to an open battlefield"),
              std::string::npos);
}

// ── Void Hatchling (341) — ESCALATED (reveal replacement hook) ──
TEST_F(FinishBatch3, VoidHatchlingNoReplacementHook) {
    Card* c = card_registry.get(kVoidHatchling);
    ASSERT_NE(c, nullptr);
    const auto& d = c->def();
    EXPECT_EQ(d.energy_cost, 2);
    EXPECT_EQ(d.might, 2);
    // Replacement surface intercepts only killUnit; there is no reveal hook,
    // so the card must NOT advertise a replacement effect.
    EXPECT_FALSE(c->hasReplacementEffect());
}

// ── Loyal Pup (447) — ESCALATED (WhenYouDefendAtABattlefield) ──
TEST_F(FinishBatch3, LoyalPupNoWiredTrigger) {
    Card* c = card_registry.get(kLoyalPup);
    ASSERT_NE(c, nullptr);
    const auto& d = c->def();
    EXPECT_EQ(d.energy_cost, 3);
    EXPECT_EQ(d.might, 3);
    EXPECT_TRUE(c->triggerTypes().empty());
}

// ── Fiora, Worthy (500) — ESCALATED (WhenAUnitBecomesMighty) ──
TEST_F(FinishBatch3, FioraWorthyNoWiredTrigger) {
    Card* c = card_registry.get(kFioraWorthy);
    ASSERT_NE(c, nullptr);
    const auto& d = c->def();
    EXPECT_EQ(d.super_type, SuperType::Champion);
    EXPECT_EQ(d.might, 3);
    EXPECT_TRUE(c->triggerTypes().empty());
}

// ── LeBlanc, Everywhere at Once (652) — Backline ENGINE-HANDLED, supp ESC ──
TEST_F(FinishBatch3, LeBlancHasBacklineAndTemporaryKeywords) {
    Card* c = card_registry.get(kLeBlancEverywhere);
    ASSERT_NE(c, nullptr);
    const auto& d = c->def();
    EXPECT_EQ(d.super_type, SuperType::Champion);
    // Clause 1 ([Backline]) is engine-handled via the keyword; verify it (and
    // the [Temporary] supertype the suppression clause references) are set.
    EXPECT_TRUE(d.keywords.has(Keyword::Backline));
    EXPECT_TRUE(d.keywords.has(Keyword::Temporary));
    // The suppression clause (clause 2) is escalated -> no trigger declared.
    EXPECT_TRUE(c->triggerTypes().empty());
}

// ── Syndra, Transcendent (708) — ESCALATED (spells-have-Repeat field) ──
TEST_F(FinishBatch3, SyndraTranscendentDefOnly) {
    Card* c = card_registry.get(kSyndraTrans);
    ASSERT_NE(c, nullptr);
    const auto& d = c->def();
    EXPECT_EQ(d.super_type, SuperType::Champion);
    EXPECT_EQ(d.energy_cost, 6);
    EXPECT_EQ(d.might, 6);
    // No continuous "spells have Repeat" field to set; applyPassiveAura is the
    // default no-op (it cannot grant Repeat). Verify it does not throw / mutate
    // a player into an inconsistent state when called.
    EXPECT_NO_THROW({ c->applyPassiveAura(state, P1); });
}

}  // namespace
