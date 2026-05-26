/// @file test_wave_a_batch3.cpp
/// Per-card unit tests for Wave-A batch-3 card implementations.
///
///   290 Vilemaw's Lair        — BF: "Units can't move from here to base"
///                               (engine-handled: blocks_move_to_base)
///   77  Zhonya's Hourglass     — friendly-death replacement (heal/exhaust/recall)
///   736 Shard of Undoing       — first friendly death in your Beginning Phase
///                               => each opponent kills one of their units
///   12  Noxus Hopeful          — [Legion] I cost [2] less
///   28  Draven, Showboat       — Might increased by your points (engine aura)
///   79  Leona, Zealot          — enter ready if opp near victory; stun -8M aura
///   307 Yi, Honed              — I enter ready (unconditional)
///   333 Void Drone             — cost [2] less played from non-hand zone
///   348 Rengar, Pouncing       — playable as Reaction to an attacking BF
///   416 Direwing               — enter ready if you control another Dragon
///   496 Xin Zhao, Vigilant     — enter ready if >=2 other units in base
///   597 Monch                  — cost [2] less + enter ready vs stunned enemy
///   378 Needlessly Large Yordle— hold-discount clause ESCALATED (no engine field)

#include "tests/cards/card_test_fixture.h"

using namespace riftbound;
using namespace riftbound::test;

namespace {

constexpr CardDefId kZhonyas      = 77;
constexpr CardDefId kShardUndoing = 736;
constexpr CardDefId kNoxusHopeful = 12;
constexpr CardDefId kLeonaZealot  = 79;
constexpr CardDefId kYiHoned      = 307;
constexpr CardDefId kVoidDrone    = 333;
constexpr CardDefId kRengar       = 348;
constexpr CardDefId kDirewing     = 416;
constexpr CardDefId kXinZhao      = 496;
constexpr CardDefId kMonch        = 597;
constexpr CardDefId kDravenShow   = 28;

class WaveABatch3Test : public CardTestFixture {
protected:
    // Stun an on-board unit (sets is_stunned).
    void stun(GameObjectId id) { state.getObject(id).is_stunned = true; }
};

// ── Zhonya's Hourglass (77): friendly-death replacement ─────────────────────

TEST_F(WaveABatch3Test, ZhonyasReplacesFriendlyDeath) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto gear = addUnit(P1, kInvalidId, 1, 0);     // placeholder object for gear
    auto& g = state.getObject(gear);
    g.card_type = CardType::Gear;
    g.card_def_id = kZhonyas;
    g.location = BaseLocation{P1};
    g.zone = ZoneType::Base;

    auto victim = addUnit(P1, kInvalidId, 5, 0);   // friendly unit at BF0
    auto& v = state.getObject(victim);
    v.damage_marked = 5;                            // lethally damaged

    Card* card = card_registry.get(kZhonyas);
    CardContext ctx{state, events, exec, P1, gear};
    bool applied = card->applyReplacement(ctx, victim);

    EXPECT_TRUE(applied);
    // Gear killed instead.
    EXPECT_EQ(state.getObject(gear).zone, ZoneType::Trash);
    // Victim healed (damage cleared), exhausted, recalled to base.
    EXPECT_EQ(state.getObject(victim).damage_marked, 0);
    EXPECT_TRUE(state.getObject(victim).is_exhausted);
    EXPECT_TRUE(state.getObject(victim).isAtBase());
}

TEST_F(WaveABatch3Test, ZhonyasIgnoresEnemyDeath) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto gear = addUnit(P1, kInvalidId, 1, 0);
    auto& g = state.getObject(gear);
    g.card_type = CardType::Gear;
    g.card_def_id = kZhonyas;

    auto enemy = addUnit(P2, kInvalidId, 5, 0);     // ENEMY unit
    Card* card = card_registry.get(kZhonyas);
    CardContext ctx{state, events, exec, P1, gear};
    EXPECT_FALSE(card->applyReplacement(ctx, enemy));
    EXPECT_NE(state.getObject(gear).zone, ZoneType::Trash);  // gear untouched
}

// ── Shard of Undoing (736): beginning-phase first-death trigger ─────────────

TEST_F(WaveABatch3Test, ShardKillsOpponentUnitInBeginningPhase) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto gear = addUnit(P1, kInvalidId, 1, 0);
    auto& g = state.getObject(gear);
    g.card_type = CardType::Gear;
    g.card_def_id = kShardUndoing;
    g.location = BaseLocation{P1};

    // P2 has two units; rational pick = lowest might (the 2M one).
    auto weak   = addUnit(P2, kInvalidId, 2, 1);
    auto strong = addUnit(P2, kInvalidId, 7, 1);

    state.turn.turn_player = P1;
    state.turn.phase = TurnPhase::BeginningStep;
    state.turn.turn_number = 4;

    fireTriggerAs(kShardUndoing, P1, gear, TriggerType::WhenAFriendlyUnitDies, exec);

    EXPECT_EQ(state.getObject(weak).zone, ZoneType::Trash);     // lowest might killed
    EXPECT_NE(state.getObject(strong).zone, ZoneType::Trash);   // stronger survives
}

TEST_F(WaveABatch3Test, ShardOnlyFiresOncePerTurn) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto gear = addUnit(P1, kInvalidId, 1, 0);
    auto& g = state.getObject(gear);
    g.card_type = CardType::Gear;
    g.card_def_id = kShardUndoing;

    auto u1 = addUnit(P2, kInvalidId, 2, 1);
    auto u2 = addUnit(P2, kInvalidId, 3, 1);

    state.turn.turn_player = P1;
    state.turn.phase = TurnPhase::BeginningStep;
    state.turn.turn_number = 4;

    fireTriggerAs(kShardUndoing, P1, gear, TriggerType::WhenAFriendlyUnitDies, exec);
    fireTriggerAs(kShardUndoing, P1, gear, TriggerType::WhenAFriendlyUnitDies, exec);

    int dead = (state.getObject(u1).zone == ZoneType::Trash ? 1 : 0) +
               (state.getObject(u2).zone == ZoneType::Trash ? 1 : 0);
    EXPECT_EQ(dead, 1);  // only the first death this turn triggers a kill
}

TEST_F(WaveABatch3Test, ShardSilentOutsideBeginningPhase) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto gear = addUnit(P1, kInvalidId, 1, 0);
    auto& g = state.getObject(gear);
    g.card_type = CardType::Gear;
    g.card_def_id = kShardUndoing;

    auto u = addUnit(P2, kInvalidId, 2, 1);
    state.turn.turn_player = P1;
    state.turn.phase = TurnPhase::MainPhase;   // NOT a beginning-phase step
    state.turn.turn_number = 4;

    fireTriggerAs(kShardUndoing, P1, gear, TriggerType::WhenAFriendlyUnitDies, exec);
    EXPECT_NE(state.getObject(u).zone, ZoneType::Trash);
}

TEST_F(WaveABatch3Test, ShardSilentOnOpponentTurn) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto gear = addUnit(P1, kInvalidId, 1, 0);
    auto& g = state.getObject(gear);
    g.card_type = CardType::Gear;
    g.card_def_id = kShardUndoing;

    auto u = addUnit(P2, kInvalidId, 2, 1);
    state.turn.turn_player = P2;               // not the gear controller's turn
    state.turn.phase = TurnPhase::BeginningStep;
    state.turn.turn_number = 4;

    fireTriggerAs(kShardUndoing, P1, gear, TriggerType::WhenAFriendlyUnitDies, exec);
    EXPECT_NE(state.getObject(u).zone, ZoneType::Trash);
}

// ── Noxus Hopeful (12): Legion cost reduction ───────────────────────────────

TEST_F(WaveABatch3Test, NoxusHopefulLegionDiscount) {
    Card* card = card_registry.get(kNoxusHopeful);
    state.player(P1).cards_played_this_turn = 0;
    EXPECT_EQ(card->selfCostReduction(state, P1), 0);
    state.player(P1).cards_played_this_turn = 1;
    EXPECT_EQ(card->selfCostReduction(state, P1), 2);   // [Legion]: 2 less
}

// ── Void Drone (333): cheaper from non-hand ─────────────────────────────────

TEST_F(WaveABatch3Test, VoidDroneDiscountFromNonHand) {
    Card* card = card_registry.get(kVoidDrone);
    state.player(P1).current_play_source = Intent::PlaySource::Hand;
    EXPECT_EQ(card->selfCostReduction(state, P1), 0);
    state.player(P1).current_play_source = Intent::PlaySource::Trash;
    EXPECT_EQ(card->selfCostReduction(state, P1), 2);
    state.player(P1).current_play_source = Intent::PlaySource::Hidden;
    EXPECT_EQ(card->selfCostReduction(state, P1), 2);
}

// ── Yi, Honed (307): unconditional enter ready ──────────────────────────────

TEST_F(WaveABatch3Test, YiHonedEntersReady) {
    Card* card = card_registry.get(kYiHoned);
    EXPECT_TRUE(card->entersReadyOnPlay());
    EXPECT_TRUE(card->entersReadyOnPlay(state, P1));
}

// ── Direwing (416): enter ready iff another Dragon ──────────────────────────

TEST_F(WaveABatch3Test, DirewingReadyWithAnotherDragon) {
    Card* card = card_registry.get(kDirewing);
    // No other dragon -> not ready.
    EXPECT_FALSE(card->entersReadyOnPlay(state, P1));
    // A second Direwing (same def) does NOT count ("another Dragon").
    addUnit(P1, kDirewing, 7, 0);
    EXPECT_FALSE(card->entersReadyOnPlay(state, P1));
    // A different Dragon-tagged unit DOES count.
    auto other = addUnit(P1, kInvalidId, 4, 0);
    state.getObject(other).tags = {"Dragon"};
    EXPECT_TRUE(card->entersReadyOnPlay(state, P1));
}

// ── Xin Zhao, Vigilant (496): enter ready iff >=2 other base units ──────────

TEST_F(WaveABatch3Test, XinZhaoReadyWithTwoBaseUnits) {
    Card* card = card_registry.get(kXinZhao);
    EXPECT_FALSE(card->entersReadyOnPlay(state, P1));
    addUnit(P1, kInvalidId, 3, -1);   // base unit #1
    EXPECT_FALSE(card->entersReadyOnPlay(state, P1));
    addUnit(P1, kInvalidId, 3, -1);   // base unit #2
    EXPECT_TRUE(card->entersReadyOnPlay(state, P1));
}

TEST_F(WaveABatch3Test, XinZhaoBattlefieldUnitsDoNotCount) {
    Card* card = card_registry.get(kXinZhao);
    addUnit(P1, kInvalidId, 3, 0);    // at battlefield, not base
    addUnit(P1, kInvalidId, 3, 0);
    EXPECT_FALSE(card->entersReadyOnPlay(state, P1));
}

// ── Leona, Zealot (79): enter ready when opp near victory ───────────────────

TEST_F(WaveABatch3Test, LeonaReadyWhenOpponentNearVictory) {
    Card* card = card_registry.get(kLeonaZealot);
    // Victory score default 8; "within 3" => opp score >= 5.
    setScore(P2, 4);
    EXPECT_FALSE(card->entersReadyOnPlay(state, P1));
    setScore(P2, 5);
    EXPECT_TRUE(card->entersReadyOnPlay(state, P1));
}

// ── Monch (597): discount + ready vs stunned enemy ──────────────────────────

TEST_F(WaveABatch3Test, MonchVsStunnedEnemy) {
    Card* card = card_registry.get(kMonch);
    // No stunned enemy: no discount, not ready.
    auto e = addUnit(P2, kInvalidId, 4, 0);
    EXPECT_EQ(card->selfCostReduction(state, P1), 0);
    EXPECT_FALSE(card->entersReadyOnPlay(state, P1));
    // Stun the enemy: discount + ready.
    stun(e);
    EXPECT_EQ(card->selfCostReduction(state, P1), 2);
    EXPECT_TRUE(card->entersReadyOnPlay(state, P1));
}

// ── Rengar, Pouncing (348): reaction-to-attack flag ─────────────────────────

TEST_F(WaveABatch3Test, RengarPlayableAsReactionToAttack) {
    Card* card = card_registry.get(kRengar);
    EXPECT_TRUE(card->playableAsReactionToAttack());
}

// ── Draven, Showboat (28): Might-from-points is engine-handled ──────────────
// The aura ("My Might is increased by your points") is applied in
// GameEngine::recalculateAuras via ability-text match; the Card itself adds
// no per-instance behavior, so we assert the card registers and its printed
// text carries the clause the engine matches on.

TEST_F(WaveABatch3Test, DravenShowboatRegistered) {
    Card* card = card_registry.get(kDravenShow);
    ASSERT_NE(card, nullptr);
    std::string t = card->def().ability_text;
    std::transform(t.begin(), t.end(), t.begin(), ::tolower);
    EXPECT_NE(t.find("my might is increased by your points"), std::string::npos);
}

}  // namespace
