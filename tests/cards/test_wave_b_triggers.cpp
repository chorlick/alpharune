/// @file test_wave_b_triggers.cpp
/// Wave B declared-trigger cluster.
///   Prize of Progress (398) — WhenYouActivateAGearAbility (gear's resolved
///   activated ability emits "gear_ability_used").

#include "tests/cards/card_test_fixture.h"
#include "engine/trigger_manager.h"
#include "engine/chain_manager.h"
#include <gtest/gtest.h>

namespace riftbound::test {
namespace {

using TriggerClusterTest = CardTestFixture;
constexpr CardDefId kPrizeOfProgress = 398;

TEST_F(TriggerClusterTest, PrizeOfProgress_EnqueuesOnGearAbility) {
    addUnit(P1, kPrizeOfProgress, 3, 0);
    auto gear = addUnit(P1, 1, 0, 0);  // a P1-controlled object (the "gear")
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    size_t before = state.chain.items.size();
    events.emit(ObjectStateChangedEvent{gear, "gear_ability_used"});
    EXPECT_GT(state.chain.items.size(), before)
        << "a gear ability use should enqueue Prize of Progress";
}

TEST_F(TriggerClusterTest, PrizeOfProgress_SelfBuffsOnTrigger) {
    auto prize = addUnit(P1, kPrizeOfProgress, 3, 0);
    int before = state.getObject(prize).current_might;
    EffectExecutor exec(state, events, card_db);
    fireTriggerAs(kPrizeOfProgress, P1, prize,
                  TriggerType::WhenYouActivateAGearAbility, exec);
    EXPECT_EQ(state.getObject(prize).current_might, before + 1);
}

// ── Stealthy Pursuer (177): WhenAFriendlyUnitMovesFromMyLocation ──
constexpr CardDefId kStealthyPursuer = 177;

TEST_F(TriggerClusterTest, StealthyPursuer_EnqueuesWithMoverSubject) {
    auto stealthy = addUnit(P1, kStealthyPursuer, 3, /*at_bf=*/0);
    auto mover = addUnit(P1, 1, 4, /*at_bf=*/0);   // shares Stealthy's location
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    size_t before = state.chain.items.size();
    events.emit(UnitMovedEvent{mover, P1, BattlefieldLocation{0},
                               BattlefieldLocation{1}, true});
    ASSERT_GT(state.chain.items.size(), before)
        << "a friendly unit leaving Stealthy's location should enqueue it";
    EXPECT_EQ(state.chain.items.back().triggering_subject, mover);
    EXPECT_EQ(state.chain.items.back().source, stealthy);
}

TEST_F(TriggerClusterTest, StealthyPursuer_FollowsTheMover) {
    auto stealthy = addUnit(P1, kStealthyPursuer, 3, /*at_bf=*/0);
    auto mover = addUnit(P1, 1, 4, /*at_bf=*/1);    // already moved to bf 1
    EffectExecutor exec(state, events, card_db);
    // Drive the resumable confirmOptional with a "yes", subject = mover.
    ChainItem ri; ri.id = state.chain.allocateId(); ri.controller = P1;
    ri.is_ability = true; ri.source = stealthy; ri.card_def_id = kStealthyPursuer;
    ri.fired_trigger = TriggerType::WhenAFriendlyUnitMovesFromMyLocation;
    ri.triggering_subject = mover;
    state.chain.resuming = ri;
    Card* card = card_registry.get(kStealthyPursuer);
    CardContext ctx{state, events, exec, P1, stealthy};
    ctx.firing_trigger = TriggerType::WhenAFriendlyUnitMovesFromMyLocation;
    card->onTrigger(ctx, {});
    while (exec.hasPendingChoice()) {
        auto p = exec.consumePendingChoice();
        exec.recordChoice(p.legal.back());   // "yes" (confirmOptional publishes [no, yes])
        card->onTrigger(ctx, {});
    }
    state.chain.resuming.reset();
    auto bf = state.getObject(stealthy).battlefieldId();
    ASSERT_TRUE(bf.has_value());
    EXPECT_EQ(*bf, 1u) << "Stealthy Pursuer follows the moved unit to bf 1";
}

// ── Group A trigger-dispatch cards ─────────────────────────────────────────
constexpr CardDefId kFioraWorthy   = 500;
constexpr CardDefId kGrandDuelist  = 519;
constexpr CardDefId kKarmaChanneler = 235;
constexpr CardDefId kMaskOfForesight = 60;
constexpr CardDefId kLoyalPup      = 447;
constexpr CardDefId kApheliosExalted = 372;

// Drive a resumable onTrigger that reads triggering_subject, using `picker` to
// choose from each published choice set (confirmOptional publishes [no, yes]).
static void driveTriggerWithSubject(
        GameState& state, EventBus& events, CardRegistry& card_registry,
        CardDefId def_id, PlayerId controller,
        GameObjectId source, GameObjectId subject, TriggerType firing,
        EffectExecutor& exec,
        std::function<Intent(const std::vector<Intent>&)> picker) {
    ChainItem ri; ri.id = state.chain.allocateId(); ri.controller = controller;
    ri.is_ability = true; ri.source = source; ri.card_def_id = def_id;
    ri.fired_trigger = firing; ri.triggering_subject = subject;
    state.chain.resuming = ri;
    Card* card = card_registry.get(def_id);
    CardContext ctx{state, events, exec, controller, source};
    ctx.firing_trigger = firing;
    card->onTrigger(ctx, {});
    while (exec.hasPendingChoice()) {
        auto p = exec.consumePendingChoice();
        exec.recordChoice(picker(p.legal));
        card->onTrigger(ctx, {});
    }
    state.chain.resuming.reset();
}

TEST_F(TriggerClusterTest, FioraWorthy_EnqueuesOnBecameMighty) {
    addUnit(P1, kFioraWorthy, 3, /*at_bf=*/0);
    auto unit = addUnit(P1, 1, 5, /*at_bf=*/0);
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    size_t before = state.chain.items.size();
    events.emit(ObjectStateChangedEvent{unit, "became_mighty"});
    ASSERT_GT(state.chain.items.size(), before);
    EXPECT_EQ(state.chain.items.back().triggering_subject, unit);
}

TEST_F(TriggerClusterTest, FioraWorthy_PaysYToReady) {
    auto fiora = addUnit(P1, kFioraWorthy, 3, /*at_bf=*/0);
    auto unit = addUnit(P1, 1, 5, /*at_bf=*/0);
    state.getObject(unit).is_exhausted = true;
    addRune(P1, Domain::Order);  // [Y] to pay
    EffectExecutor exec(state, events, card_db);
    driveTriggerWithSubject(state, events, card_registry, kFioraWorthy, P1, fiora, unit,
        TriggerType::WhenAUnitBecomesMighty, exec,
        [](const std::vector<Intent>& l){ return l.back(); });  // "yes"
    EXPECT_FALSE(state.getObject(unit).is_exhausted)
        << "Fiora readies the Mighty unit after paying [Y]";
}

TEST_F(TriggerClusterTest, FioraWorthy_DeclineLeavesExhausted) {
    auto fiora = addUnit(P1, kFioraWorthy, 3, /*at_bf=*/0);
    auto unit = addUnit(P1, 1, 5, /*at_bf=*/0);
    state.getObject(unit).is_exhausted = true;
    addRune(P1, Domain::Order);
    EffectExecutor exec(state, events, card_db);
    driveTriggerWithSubject(state, events, card_registry, kFioraWorthy, P1, fiora, unit,
        TriggerType::WhenAUnitBecomesMighty, exec,
        [](const std::vector<Intent>& l){ return l.front(); });  // "no"
    EXPECT_TRUE(state.getObject(unit).is_exhausted);
}

TEST_F(TriggerClusterTest, GrandDuelist_ExhaustsSelfOnBecameMighty) {
    auto legend = addUnit(P1, kGrandDuelist, 0, /*at_bf=*/-1);
    state.getObject(legend).card_type = CardType::Legend;
    EffectExecutor exec(state, events, card_db);
    driveTriggerWithSubject(state, events, card_registry, kGrandDuelist, P1, legend, kInvalidId,
        TriggerType::WhenAUnitBecomesMighty, exec,
        [](const std::vector<Intent>& l){ return l.back(); });  // "yes"
    EXPECT_TRUE(state.getObject(legend).is_exhausted)
        << "Grand Duelist exhausts itself to channel";
}

TEST_F(TriggerClusterTest, KarmaChanneler_EnqueuesOnRecycleMain) {
    addUnit(P1, kKarmaChanneler, 6, /*at_bf=*/0);
    auto card = addToDeck(P1, 1);  // a P1-owned card object
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    size_t before = state.chain.items.size();
    events.emit(ObjectStateChangedEvent{card, "recycled_main"});
    EXPECT_GT(state.chain.items.size(), before);
}

TEST_F(TriggerClusterTest, KarmaChanneler_BuffsFriendlyUnit) {
    auto karma = addUnit(P1, kKarmaChanneler, 6, /*at_bf=*/0);
    auto ally = addUnit(P1, 1, 3, /*at_bf=*/0);
    int before = state.getObject(ally).buff_count;
    EffectExecutor exec(state, events, card_db);
    driveTriggerWithSubject(state, events, card_registry, kKarmaChanneler, P1, karma, kInvalidId,
        TriggerType::WhenYouRecycle, exec,
        [&](const std::vector<Intent>& l){
            for (auto& i : l) if (!i.chosen_objects.empty() &&
                                  i.chosen_objects[0] == ally) return i;
            return l.front();
        });
    EXPECT_GT(state.getObject(ally).buff_count, before);
}

TEST_F(TriggerClusterTest, MaskOfForesight_BuffsLoneUnit) {
    auto mask = addUnit(P1, kMaskOfForesight, 0, /*at_bf=*/0);
    state.getObject(mask).card_type = CardType::Gear;
    auto lone = addUnit(P1, 1, 3, /*at_bf=*/0);
    int before = state.getObject(lone).current_might;
    EffectExecutor exec(state, events, card_db);
    driveTriggerWithSubject(state, events, card_registry, kMaskOfForesight, P1, mask, lone,
        TriggerType::WhenAUnitAttacksOrDefendsAlone, exec,
        [](const std::vector<Intent>& l){ return l.front(); });
    EXPECT_EQ(state.getObject(lone).current_might, before + 1);
}

TEST_F(TriggerClusterTest, MaskOfForesight_EnqueuesOnSoloCombat) {
    addUnit(P1, kMaskOfForesight, 0, /*at_bf=*/-1);  // free-standing gear at base
    auto lone = addUnit(P1, 1, 3, /*at_bf=*/0);
    state.getObject(lone).combat_designation = CombatDesignation::Attacker;
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    size_t before = state.chain.items.size();
    events.emit(CombatStartedEvent{0, P1, P2});
    EXPECT_GT(state.chain.items.size(), before)
        << "a lone attacker should enqueue Mask of Foresight";
}

TEST_F(TriggerClusterTest, LoyalPup_MovesToDefendedBattlefield) {
    auto pup = addUnit(P1, kLoyalPup, 3, /*at_bf=*/-1);  // at base
    state.getObject(pup).card_counters["__defend_bf"] = 1;
    EffectExecutor exec(state, events, card_db);
    driveTriggerWithSubject(state, events, card_registry, kLoyalPup, P1, pup, kInvalidId,
        TriggerType::WhenYouDefendAtABattlefield, exec,
        [](const std::vector<Intent>& l){ return l.back(); });  // "yes"
    auto bf = state.getObject(pup).battlefieldId();
    ASSERT_TRUE(bf.has_value());
    EXPECT_EQ(*bf, 1u);
}

TEST_F(TriggerClusterTest, ApheliosExalted_EnqueuesOnEquip) {
    auto aph = addUnit(P1, kApheliosExalted, 4, /*at_bf=*/0);
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    size_t before = state.chain.items.size();
    events.emit(ObjectStateChangedEvent{aph, "equipped"});
    EXPECT_GT(state.chain.items.size(), before);
}

TEST_F(TriggerClusterTest, ApheliosExalted_BuffModeBuffsAlly) {
    auto aph = addUnit(P1, kApheliosExalted, 4, /*at_bf=*/0);
    auto ally = addUnit(P1, 1, 3, /*at_bf=*/0);
    int before = state.getObject(ally).buff_count;
    EffectExecutor exec(state, events, card_db);
    // pickMode encodes the mode in chosen_value; choose mode 2 (Buff). pickTarget
    // then publishes one MakeChoice per friendly unit (chosen_objects).
    driveTriggerWithSubject(state, events, card_registry, kApheliosExalted, P1, aph, kInvalidId,
        TriggerType::WhenEquipmentAttachedToMe, exec,
        [&](const std::vector<Intent>& l)->Intent{
            for (auto& i : l) if (!i.chosen_objects.empty() &&
                                  i.chosen_objects[0] == ally) return i;  // buff target
            for (auto& i : l) if (i.chosen_value == 2) return i;          // mode 2
            return l.back();
        });
    EXPECT_GT(state.getObject(ally).buff_count, before);
}

}  // namespace
}  // namespace riftbound::test
