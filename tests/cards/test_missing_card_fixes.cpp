/// @file test_missing_card_fixes.cpp
/// Per-card tests for the previously-MISSING cards implemented during the
/// pre-release card-text audit. Each card had an empty/wrong body before;
/// these tests pin the now-implemented behavior.
///
///   91  Pit Crew         — ready self when controller plays a gear
///   395 Dropboarder      — ready self on play if controlling 2+ gear
///   538 Seal of Focus    — activated [E]: add [G] power
///   573 Fresh Beans      — exhaust to draw 1 on unit-play during showdown
///   509 Shurelya's Req.  — ready your units on play; grant [Ganking] aura
///   650 Gutter Palace    — beginning-phase win condition + Bird-token ability
///   581 Blighted Battleaxe — end-of-turn unattach + self-damage if no conquer
///   374 Guardian Angel   — death-replacement: kill gear, heal/recall bearer
///   353 Skyfall          — crossesHoldConquerTriggers() property

#include "tests/cards/card_test_fixture.h"
#include "engine/trigger_manager.h"

using namespace riftbound;
using namespace riftbound::test;

namespace {

constexpr CardDefId kPitCrew           = 91;
constexpr CardDefId kDropboarder       = 395;
constexpr CardDefId kSealOfFocus       = 538;
constexpr CardDefId kFreshBeans        = 573;
constexpr CardDefId kShurelyasRequiem  = 509;
constexpr CardDefId kGutterPalace      = 650;
constexpr CardDefId kBlightedBattleaxe = 581;
constexpr CardDefId kGuardianAngel     = 374;
constexpr CardDefId kSkyfallAreion     = 353;

class MissingFixTest : public CardTestFixture {
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

    // Attach `gear` to `unit` (mirrors GameEngine::attachGearToUnit essentials).
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

    void fireTrigger(CardDefId def_id, GameObjectId source, PlayerId controller,
                     EffectExecutor& exec) {
        Card* card = card_registry.get(def_id);
        ASSERT_NE(card, nullptr);
        CardContext ctx{state, events, exec, controller, source};
        card->onTrigger(ctx, {});
    }
};

// ── 538 Seal of Focus — activated ability adds [G] ──────────────────────────
TEST_F(MissingFixTest, SealOfFocus_ActivatedAddsCalmPower) {
    Card* c = card_registry.get(kSealOfFocus);
    ASSERT_NE(c, nullptr);
    EXPECT_TRUE(c->hasActivatedAbility());
    EXPECT_TRUE(c->isReactionAbility());
    EXPECT_TRUE(c->getActivationCost().exhaust);

    auto gear = addGear(P1, kSealOfFocus);
    EffectExecutor exec(state, events, card_db);
    int before = state.player(P1).rune_pool.totalPower();
    CardContext ctx{state, events, exec, P1, gear};
    c->onActivate(ctx, {});
    EXPECT_EQ(state.player(P1).rune_pool.totalPower(), before + 1);
}

// ── 395 Dropboarder — readies self only with 2+ gear ────────────────────────
TEST_F(MissingFixTest, Dropboarder_TriggerType) {
    Card* c = card_registry.get(kDropboarder);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->triggerType(), TriggerType::WhenYouPlayMe);
}

TEST_F(MissingFixTest, Dropboarder_ReadiesWhenTwoGear) {
    auto dropboarder = addUnit(P1, kDropboarder, /*might=*/3, /*at_bf=*/0);
    state.getObject(dropboarder).is_exhausted = true;   // enters exhausted
    addGear(P1, kInvalidId, 0);
    addGear(P1, kInvalidId, 0);

    EffectExecutor exec(state, events, card_db);
    fireTrigger(kDropboarder, dropboarder, P1, exec);
    EXPECT_FALSE(state.getObject(dropboarder).is_exhausted)
        << "should ready with 2 gear controlled";
}

TEST_F(MissingFixTest, Dropboarder_StaysExhaustedWithOneGear) {
    auto dropboarder = addUnit(P1, kDropboarder, /*might=*/3, /*at_bf=*/0);
    state.getObject(dropboarder).is_exhausted = true;
    addGear(P1, kInvalidId, 0);   // only one gear

    EffectExecutor exec(state, events, card_db);
    fireTrigger(kDropboarder, dropboarder, P1, exec);
    EXPECT_TRUE(state.getObject(dropboarder).is_exhausted)
        << "should NOT ready with only 1 gear";
}

// ── 91 Pit Crew — WhenYouPlayAGear readies self ─────────────────────────────
TEST_F(MissingFixTest, PitCrew_TriggerTypeIsPlayAGear) {
    Card* c = card_registry.get(kPitCrew);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->triggerType(), TriggerType::WhenYouPlayAGear);
}

TEST_F(MissingFixTest, PitCrew_ReadiesSelfOnGearPlay) {
    auto pit = addUnit(P1, kPitCrew, /*might=*/2, /*at_bf=*/0);
    state.getObject(pit).is_exhausted = true;
    EffectExecutor exec(state, events, card_db);
    fireTrigger(kPitCrew, pit, P1, exec);
    EXPECT_FALSE(state.getObject(pit).is_exhausted);
}

// ── 573 Fresh Beans — exhaust to draw, gated on showdown ────────────────────
TEST_F(MissingFixTest, FreshBeans_DrawsDuringShowdownOnYes) {
    auto gear = addGear(P1, kFreshBeans, /*at_bf=*/0);
    addToDeck(P1, kInvalidId);   // a card to draw
    state.battlefields[0].showdown_in_progress = true;

    EffectExecutor exec(state, events, card_db);
    int hand_before = handSize(P1);
    // confirmOptional publishes [no, yes]; pick yes (legal.back()).
    driveResumableTrigger(kFreshBeans, P1, gear,
        [](const std::vector<Intent>& legal) { return legal.back(); }, exec);
    EXPECT_EQ(handSize(P1), hand_before + 1);
    EXPECT_TRUE(state.getObject(gear).is_exhausted);
}

TEST_F(MissingFixTest, FreshBeans_NoOpOutsideShowdown) {
    auto gear = addGear(P1, kFreshBeans, /*at_bf=*/0);
    addToDeck(P1, kInvalidId);
    // No showdown in progress.
    EffectExecutor exec(state, events, card_db);
    int hand_before = handSize(P1);
    fireTrigger(kFreshBeans, gear, P1, exec);
    EXPECT_EQ(handSize(P1), hand_before) << "no draw outside a showdown";
    EXPECT_FALSE(state.getObject(gear).is_exhausted);
}

// ── 509 Shurelya's Requiem — ready your units on play ───────────────────────
TEST_F(MissingFixTest, ShurelyasRequiem_ReadiesYourUnitsOnPlay) {
    auto u1 = addUnit(P1, kInvalidId, 3, /*at_bf=*/0);
    auto u2 = addUnit(P1, kInvalidId, 3, /*at_bf=*/1);
    auto enemy = addUnit(P2, kInvalidId, 3, /*at_bf=*/0);
    state.getObject(u1).is_exhausted = true;
    state.getObject(u2).is_exhausted = true;
    state.getObject(enemy).is_exhausted = true;
    auto gear = addGear(P1, kShurelyasRequiem, /*at_bf=*/0);

    EffectExecutor exec(state, events, card_db);
    fireTrigger(kShurelyasRequiem, gear, P1, exec);
    EXPECT_FALSE(state.getObject(u1).is_exhausted);
    EXPECT_FALSE(state.getObject(u2).is_exhausted);
    EXPECT_TRUE(state.getObject(enemy).is_exhausted) << "enemy units untouched";
}

// ── 650 Gutter Palace — win condition + Bird-token ability ──────────────────
TEST_F(MissingFixTest, GutterPalace_WinsWith4Hand4Units) {
    auto gear = addGear(P1, kGutterPalace);
    for (int i = 0; i < 4; ++i) addToHand(P1, kInvalidId);
    addUnit(P1, kInvalidId, 1, /*at_bf=*/0);
    addUnit(P1, kInvalidId, 1, /*at_bf=*/0);
    addUnit(P1, kInvalidId, 1, /*at_bf=*/1);
    addUnit(P1, kInvalidId, 1, /*at_bf=*/1);

    EffectExecutor exec(state, events, card_db);
    fireTrigger(kGutterPalace, gear, P1, exec);
    EXPECT_TRUE(state.game_over);
    EXPECT_EQ(state.winner, P1);
}

TEST_F(MissingFixTest, GutterPalace_NoWinWhenCountsWrong) {
    auto gear = addGear(P1, kGutterPalace);
    for (int i = 0; i < 3; ++i) addToHand(P1, kInvalidId);  // only 3 cards
    addUnit(P1, kInvalidId, 1, /*at_bf=*/0);
    addUnit(P1, kInvalidId, 1, /*at_bf=*/0);
    addUnit(P1, kInvalidId, 1, /*at_bf=*/1);
    addUnit(P1, kInvalidId, 1, /*at_bf=*/1);

    EffectExecutor exec(state, events, card_db);
    fireTrigger(kGutterPalace, gear, P1, exec);
    EXPECT_FALSE(state.game_over);
}

TEST_F(MissingFixTest, GutterPalace_ActivatedMakesBirdToken) {
    Card* c = card_registry.get(kGutterPalace);
    ASSERT_NE(c, nullptr);
    EXPECT_TRUE(c->hasActivatedAbility());
    auto cost = c->getActivationCost();
    EXPECT_TRUE(cost.exhaust);
    EXPECT_TRUE(cost.discard);

    auto gear = addGear(P1, kGutterPalace, /*at_bf=*/0);
    EffectExecutor exec(state, events, card_db);
    size_t before = state.objects.size();
    CardContext ctx{state, events, exec, P1, gear};
    c->onActivate(ctx, {});
    EXPECT_EQ(state.objects.size(), before + 1) << "one Bird token created";
    // The new token has Deflect.
    bool found_bird = false;
    for (auto& [id, obj] : state.objects) {
        if (obj.name == "Bird") {
            found_bird = true;
            EXPECT_TRUE(obj.keywords.has(Keyword::Deflect));
            EXPECT_EQ(obj.base_might, 1);
        }
    }
    EXPECT_TRUE(found_bird);
}

// ── 581 Blighted Battleaxe — end-of-turn self-punish without conquer ────────
TEST_F(MissingFixTest, BlightedBattleaxe_TriggerType) {
    Card* c = card_registry.get(kBlightedBattleaxe);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->triggerType(), TriggerType::AtEndOfTurn);
}

TEST_F(MissingFixTest, BlightedBattleaxe_UnattachesAndDamagesIfNoConquer) {
    auto unit = addUnit(P1, kInvalidId, /*might=*/5, /*at_bf=*/0);
    auto gear = addGear(P1, kBlightedBattleaxe, /*at_bf=*/0);
    attach(gear, unit);
    // No __conquered_turn marker → didn't conquer.
    EffectExecutor exec(state, events, card_db);
    fireTrigger(kBlightedBattleaxe, gear, P1, exec);
    EXPECT_FALSE(state.getObject(gear).attached_to.has_value())
        << "gear should unattach";
    EXPECT_EQ(state.getObject(unit).damage_marked, 4) << "deal 4 to bearer";
}

TEST_F(MissingFixTest, BlightedBattleaxe_NoPunishIfConqueredThisTurn) {
    auto unit = addUnit(P1, kInvalidId, /*might=*/5, /*at_bf=*/0);
    auto gear = addGear(P1, kBlightedBattleaxe, /*at_bf=*/0);
    attach(gear, unit);
    state.turn.turn_number = 7;
    state.getObject(unit).card_counters["__conquered_turn"] = 7;  // conquered

    EffectExecutor exec(state, events, card_db);
    fireTrigger(kBlightedBattleaxe, gear, P1, exec);
    EXPECT_TRUE(state.getObject(gear).attached_to.has_value())
        << "gear stays attached when bearer conquered";
    EXPECT_EQ(state.getObject(unit).damage_marked, 0);
}

// ── 374 Guardian Angel — death replacement ──────────────────────────────────
TEST_F(MissingFixTest, GuardianAngel_HasReplacementEffect) {
    Card* c = card_registry.get(kGuardianAngel);
    ASSERT_NE(c, nullptr);
    EXPECT_TRUE(c->hasReplacementEffect());
}

TEST_F(MissingFixTest, GuardianAngel_ProtectsBearer) {
    auto unit = addUnit(P1, kInvalidId, /*might=*/4, /*at_bf=*/0);
    state.getObject(unit).damage_marked = 4;   // lethal
    auto gear = addGear(P1, kGuardianAngel, /*at_bf=*/0);
    attach(gear, unit);

    EffectExecutor exec(state, events, card_db);
    Card* c = card_registry.get(kGuardianAngel);
    CardContext ctx{state, events, exec, P1, gear};
    bool replaced = c->applyReplacement(ctx, unit);
    EXPECT_TRUE(replaced);
    // Bearer healed, exhausted, recalled to base.
    EXPECT_EQ(state.getObject(unit).damage_marked, 0);
    EXPECT_TRUE(state.getObject(unit).is_exhausted);
    EXPECT_EQ(state.getObject(unit).zone, ZoneType::Base);
    // Guardian Angel itself is trashed.
    EXPECT_EQ(state.getObject(gear).zone, ZoneType::Trash);
}

TEST_F(MissingFixTest, GuardianAngel_DoesNotProtectOtherUnits) {
    auto bearer = addUnit(P1, kInvalidId, 4, /*at_bf=*/0);
    auto other  = addUnit(P1, kInvalidId, 4, /*at_bf=*/0);
    auto gear = addGear(P1, kGuardianAngel, /*at_bf=*/0);
    attach(gear, bearer);

    EffectExecutor exec(state, events, card_db);
    Card* c = card_registry.get(kGuardianAngel);
    CardContext ctx{state, events, exec, P1, gear};
    EXPECT_FALSE(c->applyReplacement(ctx, other))
        << "Guardian Angel only protects the unit it's attached to";
}

// ── 353 Skyfall of Areion — cross-fire property ─────────────────────────────
TEST_F(MissingFixTest, Skyfall_DeclaresCrossfireProperty) {
    Card* c = card_registry.get(kSkyfallAreion);
    ASSERT_NE(c, nullptr);
    EXPECT_TRUE(c->crossesHoldConquerTriggers());
}

}  // namespace
