/// @file test_rules_compliance_cards2.cpp
/// Card-level rules-compliance fixes, second batch (docs/rules-audit-2026-10.md),
/// built on the engine hooks added in "Rules compliance, batch 6". Each test
/// names the rule (rules/core-rules.md) it enforces.
///
///   A. Points from effects go through EffectExecutor::scorePoints (CR 194.1.c):
///      "can't gain points" is honoured and the game ends at the Victory Score
///   B. Last-known information for "When I die" (CR 383.2.c.3.b)
///   C. Vendetta cards on the new hooks: selfPowerCostReduction, onUnattached,
///      replaceHostileEffect, ignoresDeflect, recalculateContinuousEffects,
///      ChainItem::chosen_objects, chose_enemy_unit_this_turn
///   D. "Ready it" / "a Mech" / gear tokens to base (CR 149.2)
///   E. Controller's choices: Arise, Deceiver, Jax, Hall of Legends
///   F. Hostile Takeover's recall (CR 457-458), "activate" (CR 383.4.g),
///      countering what "chooses" a unit (Repulse, Not So Fast)

#include "tests/cards/card_test_fixture.h"

#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/events.h"
#include "core/game_state.h"
#include "core/intent.h"
#include "engine/effect_executor.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace riftbound::test {
namespace test_rules_compliance_cards2 {  // file-unique: test sources are unity-built
namespace {

constexpr CardDefId kLegionRearguard  = 10;    // 2 [M] unit, no abilities
constexpr CardDefId kTryndamere       = 34;
constexpr CardDefId kAhriAlluring     = 66;
constexpr CardDefId kKogMawCaustic    = 190;
constexpr CardDefId kYasuoWindrider   = 205;
constexpr CardDefId kReckonersArena   = 281;
constexpr CardDefId kArenasGreatest   = 285;
constexpr CardDefId kGold             = 326;
constexpr CardDefId kSkyfallOfAreion  = 353;   // "[Equip] [1][R]"
constexpr CardDefId kNotSoFast        = 368;
constexpr CardDefId kProductionSurge  = 399;
constexpr CardDefId kWorldAtlas       = 408;
constexpr CardDefId kRenataMastermind = 410;
constexpr CardDefId kTrinityForce     = 437;   // "[Equip] [O]"
constexpr CardDefId kJaxUnrelenting   = 440;
constexpr CardDefId kCull             = 455;
constexpr CardDefId kJaeMedarda       = 463;
constexpr CardDefId kDravenAudacious  = 469;
constexpr CardDefId kUnsungHero       = 488;
constexpr CardDefId kArise            = 513;
constexpr CardDefId kHostileTakeover  = 516;
constexpr CardDefId kHallOfLegends    = 524;
constexpr CardDefId kPowerNexus       = 528;
constexpr CardDefId kTreasureHoard    = 534;
constexpr CardDefId kYasuoWindrider2  = 546;
constexpr CardDefId kBladeDancer      = 555;
constexpr CardDefId kRepulse          = 668;
constexpr CardDefId kArachnoidHorror  = 679;   // "When I conquer or hold, gain 2 XP"
constexpr CardDefId kPykeReturned     = 707;
constexpr CardDefId kShepherdsHeirloom = 720;  // "[Equip] — Spend 1 XP"
constexpr CardDefId kIvernFriend      = 739;
constexpr CardDefId kDeceiver         = 756;
constexpr CardDefId kPunchingPoro     = 794;
constexpr CardDefId kDecreeOfRage     = 802;
constexpr CardDefId kDecreeOfFocus    = 827;
constexpr CardDefId kNasusAscended    = 833;
constexpr CardDefId kDecreeOfInsight  = 848;
constexpr CardDefId kSwainVisionary   = 852;
constexpr CardDefId kBottledConstellation = 854;
constexpr CardDefId kBaccaiWitherclaw = 865;
constexpr CardDefId kGangplankNaval   = 873;
constexpr CardDefId kKeeperOfLaw      = 906;
constexpr CardDefId kHungryWolf       = 912;
constexpr CardDefId kLacerate         = 914;
constexpr CardDefId kNoxianEmissary   = 915;
constexpr CardDefId kShadySpectacles  = 924;
constexpr CardDefId kShenKinkou       = 925;
constexpr CardDefId kRebuttal         = 939;

using Picker = std::function<Intent(const std::vector<Intent>&)>;

class RulesCards2Test : public CardTestFixture {
protected:
    // ── pickers ──
    /// "Yes" to a yes/no prompt; first option otherwise.
    static Intent yes(const std::vector<Intent>& legal) {
        for (const auto& i : legal)
            if (i.chosen_value.has_value() && *i.chosen_value == 1) return i;
        return legal.empty() ? Intent{} : legal.front();
    }
    /// "No" / "none" / "stop" (chosen_value 0); first option otherwise.
    static Intent no(const std::vector<Intent>& legal) {
        for (const auto& i : legal)
            if (i.chosen_value.has_value() && *i.chosen_value == 0) return i;
        return legal.empty() ? Intent{} : legal.front();
    }
    /// Counts prompts and records every object offered.
    struct Seen {
        int prompts = 0;
        std::vector<GameObjectId> offered;
        bool wasOffered(GameObjectId id) const {
            return std::find(offered.begin(), offered.end(), id) != offered.end();
        }
    };
    /// "Yes" to yes/no prompts; for object prompts pick `want` (first option
    /// when `want` isn't offered).
    static Picker pickObj(GameObjectId want, Seen* seen = nullptr) {
        return [want, seen](const std::vector<Intent>& legal) -> Intent {
            if (seen) {
                ++seen->prompts;
                for (const auto& i : legal)
                    if (!i.chosen_objects.empty()) seen->offered.push_back(i.chosen_objects[0]);
            }
            for (const auto& i : legal)
                if (i.chosen_value.has_value() && *i.chosen_value == 1) return i;
            for (const auto& i : legal)
                if (!i.chosen_objects.empty() && i.chosen_objects[0] == want) return i;
            return legal.empty() ? Intent{} : legal.front();
        };
    }
    static Picker watch(Seen& seen, Picker inner) {
        return [&seen, inner](const std::vector<Intent>& legal) -> Intent {
            ++seen.prompts;
            for (const auto& i : legal)
                if (!i.chosen_objects.empty()) seen.offered.push_back(i.chosen_objects[0]);
            return inner(legal);
        };
    }

    // ── object construction ──
    GameObjectId addGear(PlayerId owner, CardDefId def_id,
                         std::vector<std::string> tags = {}, int might_bonus = 0,
                         int at_bf = -1) {
        auto id = state.createObject();
        auto& obj = state.getObject(id);
        obj.owner = owner;
        obj.controller = owner;
        obj.card_def_id = def_id;
        obj.card_type = CardType::Gear;
        obj.name = def_id != kInvalidId ? card_db.get(def_id).name : "TestGear";
        if (def_id != kInvalidId) {
            obj.tags = card_db.get(def_id).tags;
            obj.might_bonus = card_db.get(def_id).might_bonus;
        }
        for (auto& t : tags) obj.tags.push_back(t);
        if (might_bonus != 0) obj.might_bonus = might_bonus;
        if (at_bf >= 0) {
            obj.zone = ZoneType::BattlefieldZone;
            obj.location = BattlefieldLocation{static_cast<BattlefieldId>(at_bf)};
        } else {
            obj.zone = ZoneType::Base;
            obj.location = BaseLocation{owner};
        }
        return id;
    }
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
    GameObjectId installBattlefieldCard(int bf, CardDefId def_id) {
        auto id = state.createObject();
        auto& obj = state.getObject(id);
        obj.card_def_id = def_id;
        obj.card_type = CardType::Battlefield;
        obj.name = card_db.get(def_id).name;
        obj.zone = ZoneType::BattlefieldZone;
        state.battlefields[bf].card_object_id = id;
        return id;
    }
    GameObjectId addLegend(PlayerId owner, CardDefId def_id, bool exhausted) {
        auto id = state.createObject();
        auto& obj = state.getObject(id);
        obj.owner = owner;
        obj.controller = owner;
        obj.card_def_id = def_id;
        obj.card_type = CardType::Legend;
        obj.name = card_db.get(def_id).name;
        obj.zone = ZoneType::LegendZone;
        obj.is_exhausted = exhausted;
        state.player(owner).legend_zone = id;
        return id;
    }
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
    const GameObject* findOnBoard(PlayerId p, const std::string& name) {
        for (const auto& [id, obj] : state.objects)
            if (obj.name == name && obj.controller == p && obj.location.has_value()) return &obj;
        return nullptr;
    }
    int countOnBoard(PlayerId p, const std::string& name) {
        int n = 0;
        for (const auto& [id, obj] : state.objects)
            if (obj.name == name && obj.controller == p && obj.location.has_value()) ++n;
        return n;
    }
    int runesInBase(PlayerId p) { return readyRuneCount(p) + exhaustedRuneCount(p); }
    bool onBoard(GameObjectId id) { return state.getObject(id).location.has_value(); }
    /// Every Gold gear token `p` controls is exhausted, the real Gold card, and
    /// in `p`'s base (CR 149.2, 187.5).
    void expectGoldInBase(PlayerId p, int count) {
        int n = 0;
        for (const auto& [id, obj] : state.objects) {
            if (obj.name != "Gold" || obj.controller != p || !obj.location.has_value()) continue;
            ++n;
            EXPECT_TRUE(obj.isGear());
            EXPECT_TRUE(obj.isToken());
            EXPECT_TRUE(obj.is_exhausted) << "\"play a Gold gear token exhausted\"";
            EXPECT_EQ(obj.card_def_id, kGold) << "CR 187.5";
            EXPECT_TRUE(obj.isAtBase()) << "CR 149.2: gear is played to its controller's base";
            EXPECT_FALSE(obj.battlefieldId().has_value());
        }
        EXPECT_EQ(n, count);
    }

    // ── drivers ──
    /// Resolve a spell through the resume loop (registry set, play-time
    /// targets passed on every entry).
    void driveSpell(CardDefId def_id, PlayerId controller, GameObjectId source,
                    const std::vector<GameObjectId>& targets, const Picker& picker,
                    EffectExecutor& exec) {
        ChainItem ri;
        ri.id = state.chain.allocateId();
        ri.controller = controller;
        ri.is_spell = true;
        ri.source = source;
        ri.card_def_id = def_id;
        ri.targets = targets;
        ri.chosen_objects = targets;
        state.chain.resuming = ri;
        Card* card = card_registry.get(def_id);
        ASSERT_NE(card, nullptr);
        CardContext ctx{state, events, exec, controller, source};
        ctx.registry = &card_registry;
        card->onResolve(ctx, targets);
        int guard = 0;
        while (exec.hasPendingChoice()) {
            // ChainManager::stepResolve allows 16 entries per resolution.
            ASSERT_LT(++guard, 16) << "more choices than the chain's resume loop allows";
            auto pending = exec.consumePendingChoice();
            exec.recordChoice(picker(pending.legal));
            card->onResolve(ctx, targets);
        }
        state.chain.resuming.reset();
    }
    /// Resolve a triggered ability through the resume loop. `subject` is what
    /// TriggerManager::fireTrigger records as ChainItem::triggering_subject.
    void driveTrigger(CardDefId def_id, PlayerId controller, GameObjectId source,
                      TriggerType firing, const Picker& picker, EffectExecutor& exec,
                      GameObjectId subject = kInvalidId) {
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
        ctx.registry = &card_registry;
        card->onTrigger(ctx, {});
        int guard = 0;
        while (exec.hasPendingChoice()) {
            ASSERT_LT(++guard, 16) << "more choices than the chain's resume loop allows";
            auto pending = exec.consumePendingChoice();
            exec.recordChoice(picker(pending.legal));
            card->onTrigger(ctx, {});
        }
        state.chain.resuming.reset();
    }
    /// Take the top chain item off the chain and make it the resolving one.
    void beginResolvingTop() {
        ASSERT_FALSE(state.chain.items.empty());
        ChainItem item = state.chain.items.back();
        state.chain.items.pop_back();
        state.chain.resuming = item;
    }

    // ── A. scoring ──
    /// `who` is told to score 1 point by an effect: nothing while "can't gain
    /// points" holds; at 7 points the point is gained and the game ends at
    /// once (CR 194.1.c, 194.2; EffectExecutor::scorePoints).
    void checkScoring(PlayerId who, const std::function<void()>& fire_blocked,
                      const std::function<void()>& fire_open) {
        auto& ps = state.player(who);
        ps.cannot_gain_points = true;
        fire_blocked();
        EXPECT_EQ(ps.score, 0) << "\"can't gain points\" must stop a point from an effect";
        EXPECT_FALSE(state.game_over);

        ps.cannot_gain_points = false;
        setScore(who, 7);
        fire_open();
        EXPECT_EQ(ps.score, 8);
        EXPECT_TRUE(state.game_over) << "reaching the Victory Score by an effect ends the game";
        EXPECT_EQ(state.winner, who);
    }
    void checkScoring(PlayerId who, const std::function<void()>& fire) {
        checkScoring(who, fire, fire);
    }
};

// ═══ A. Points from effects (CR 194.1.c) ════════════════════════════════════

TEST_F(RulesCards2Test, RulesCards2_CR194_AhriAlluring_HoldPointGoesThroughScorePoints) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto ahri = addUnit(P1, kAhriAlluring, 0, /*at_bf=*/0);
    checkScoring(P1, [&] { fireTriggerAs(kAhriAlluring, P1, ahri, TriggerType::WhenIHold, exec); });
}

TEST_F(RulesCards2Test, RulesCards2_CR194_Tryndamere_ConquerPointGoesThroughScorePoints) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto t = addUnit(P1, kTryndamere, 0, /*at_bf=*/0);
    checkScoring(P1, [&] { fireTriggerAs(kTryndamere, P1, t, TriggerType::WhenIConquer, exec); });
}

TEST_F(RulesCards2Test, RulesCards2_CR194_YasuoWindrider_BothPrintings_ThirdMoveScores) {
    for (CardDefId def : {kYasuoWindrider, kYasuoWindrider2}) {
        SCOPED_TRACE(def);
        state.game_over = false;
        state.winner = PlayerId::None;
        setScore(P1, 0);
        EffectExecutor exec(state, events, card_db, &card_registry);
        auto blocked = addUnit(P1, def, 0, /*at_bf=*/0);
        auto open = addUnit(P1, def, 0, /*at_bf=*/0);
        auto moveThrice = [&](GameObjectId y) {
            for (int i = 0; i < 3; ++i)
                fireTriggerAs(def, P1, y, TriggerType::WhenIMove, exec);
        };
        checkScoring(P1, [&] { moveThrice(blocked); }, [&] { moveThrice(open); });
    }
}

TEST_F(RulesCards2Test, RulesCards2_CR194_TrinityForce_HoldPointGoesThroughScorePoints) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto unit = addUnit(P1, kInvalidId, 3, /*at_bf=*/0);
    auto gear = addGear(P1, kTrinityForce);
    attach(gear, unit);
    Card* card = card_registry.get(kTrinityForce);
    ASSERT_NE(card, nullptr);
    checkScoring(P1, [&] {
        CardContext ctx{state, events, exec, P1, gear};
        card->onEquippedTrigger(ctx, unit, {});
    });
}

TEST_F(RulesCards2Test, RulesCards2_CR194_RenataMastermind_ScoreAbilityGoesThroughScorePoints) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto renata = addUnit(P1, kRenataMastermind, 0, /*at_bf=*/0);
    Card* card = card_registry.get(kRenataMastermind);
    ASSERT_NE(card, nullptr);
    checkScoring(P1, [&] {
        CardContext ctx{state, events, exec, P1, renata};
        card->onActivate(ctx, /*ability_index=*/1, {});
    });
}

TEST_F(RulesCards2Test, RulesCards2_CR194_SwainVisionary_ConquerPointGoesThroughScorePoints) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto swain = addUnit(P1, kSwainVisionary, 0, /*at_bf=*/0);
    auto& ps = state.player(P1);
    ps.played_nontoken_unit_this_turn = true;
    ps.played_nontoken_gear_this_turn = true;
    ps.played_spell_this_turn = true;
    checkScoring(P1, [&] {
        fireTriggerAs(kSwainVisionary, P1, swain, TriggerType::WhenIConquer, exec);
    });
}

TEST_F(RulesCards2Test, RulesCards2_CR194_IvernFriendToAll_PointGoesThroughScorePoints) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto ivern = addUnit(P1, kIvernFriend, 0, /*at_bf=*/0);
    for (const char* tag : {"Bird", "Cat", "Dog", "Poro"}) {
        auto u = addUnit(P1, kInvalidId, 1, /*at_bf=*/0);
        state.getObject(u).tags = {tag};
    }
    checkScoring(P1, [&] {
        fireTriggerAs(kIvernFriend, P1, ivern, TriggerType::WhenIConquerOrHold, exec);
    });
}

TEST_F(RulesCards2Test, RulesCards2_CR194_NasusAscended_EmpoweredConquerPointGoesThroughScorePoints) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto nasus = addUnit(P1, kNasusAscended, 0, /*at_bf=*/0);
    state.getObject(nasus).empowered = 1;
    checkScoring(P1, [&] {
        fireTriggerAs(kNasusAscended, P1, nasus, TriggerType::WhenIConquer, exec);
    });
}

TEST_F(RulesCards2Test, RulesCards2_CR194_Shen_HoldPointGoesThroughScorePoints) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto shen = addUnit(P1, kShenKinkou, 0, /*at_bf=*/0);
    addUnit(P1, kInvalidId, 2, /*at_bf=*/0);     // exactly one other unit here
    checkScoring(P1, [&] {
        fireTriggerAs(kShenKinkou, P1, shen, TriggerType::WhenIHold, exec);
    });
}

TEST_F(RulesCards2Test, RulesCards2_CR194_BottledConstellation_PointGoesThroughScorePoints) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto bottle = addGear(P1, kBottledConstellation);
    auto fire = [&] {
        for (int i = 0; i < 3; ++i) addUnit(P1, kInvalidId, 1, /*at_bf=*/0);
        driveTrigger(kBottledConstellation, P1, bottle, TriggerType::AtStartOfMain,
                     &RulesCards2Test::yes, exec);
    };
    checkScoring(P1, fire);
    EXPECT_TRUE(onBoard(bottle)) << "\"3 OTHER friendly units and/or gear\"";
}

TEST_F(RulesCards2Test, RulesCards2_CR194_ArenasGreatest_PointGoesThroughScorePoints) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto arena = installBattlefieldCard(0, kArenasGreatest);

    state.player(P1).cannot_gain_points = true;
    fireTriggerAs(kArenasGreatest, P1, arena, TriggerType::AtStartOfBeginning, exec);
    EXPECT_EQ(state.player(P1).score, 0) << "\"can't gain points\" stops the point";

    setScore(P2, 7);
    fireTriggerAs(kArenasGreatest, P2, arena, TriggerType::AtStartOfBeginning, exec);
    EXPECT_EQ(state.player(P2).score, 8);
    EXPECT_TRUE(state.game_over);
    EXPECT_EQ(state.winner, P2);
}

TEST_F(RulesCards2Test, RulesCards2_CR164_PowerNexus_PaysFourPowerByRecyclingRunes) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto nexus = installBattlefieldCard(0, kPowerNexus);
    addRune(P1, Domain::Fury);
    addRune(P1, Domain::Calm);
    addRune(P1, Domain::Mind, /*exhausted=*/true);
    addRune(P1, Domain::Body, /*exhausted=*/true);
    ASSERT_EQ(state.player(P1).rune_pool.totalPower(), 0);
    setScore(P1, 7);

    driveTrigger(kPowerNexus, P1, nexus, TriggerType::WhenYouHoldHere,
                 &RulesCards2Test::yes, exec);

    EXPECT_EQ(runesInBase(P1), 0)
        << "CR 164.2.b: [A][A][A][A] paid by recycling runes, ready or exhausted";
    EXPECT_EQ(state.player(P1).rune_deck.size(), 4u);
    EXPECT_EQ(state.player(P1).score, 8);
    EXPECT_TRUE(state.game_over) << "CR 194.1.c: the point goes through scorePoints";
}

TEST_F(RulesCards2Test, RulesCards2_CR445_PowerNexus_NotOfferedWhenFourPowerCantBePaid) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto nexus = installBattlefieldCard(0, kPowerNexus);
    for (int i = 0; i < 3; ++i) addRune(P1, Domain::Fury);
    Seen seen;

    driveTrigger(kPowerNexus, P1, nexus, TriggerType::WhenYouHoldHere,
                 watch(seen, &RulesCards2Test::yes), exec);

    EXPECT_EQ(seen.prompts, 0) << "CR 445.2: only [A][A][A] available -> no payment offered";
    EXPECT_EQ(runesInBase(P1), 3);
    EXPECT_EQ(state.player(P1).score, 0);
}

TEST_F(RulesCards2Test, RulesCards2_CR194_PowerNexus_CantGainPointsStillLetsNoPointThrough) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto nexus = installBattlefieldCard(0, kPowerNexus);
    state.player(P1).rune_pool.universal_power = 4;
    state.player(P1).cannot_gain_points = true;

    driveTrigger(kPowerNexus, P1, nexus, TriggerType::WhenYouHoldHere,
                 &RulesCards2Test::yes, exec);

    EXPECT_EQ(state.player(P1).score, 0);
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 0) << "floating power is spent first";
}

TEST_F(RulesCards2Test, RulesCards2_CR194_Draven_FirstCombatWinEachTurnScoresThroughScorePoints) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto blocked = addUnit(P1, kDravenAudacious, 0, /*at_bf=*/0);
    auto open = addUnit(P1, kDravenAudacious, 0, /*at_bf=*/0);
    checkScoring(P1,
        [&] { fireTriggerAs(kDravenAudacious, P1, blocked, TriggerType::WhenIWinCombat, exec); },
        [&] { fireTriggerAs(kDravenAudacious, P1, open, TriggerType::WhenIWinCombat, exec); });
}

// ═══ B. Last-known information (CR 383.2.c.3.b) ═════════════════════════════

TEST_F(RulesCards2Test, RulesCards2_CR383_Draven_DiedInCombat_OpponentGainsThePoint) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto draven = addUnit(P1, kDravenAudacious, 0, /*at_bf=*/0);
    state.battlefields[0].combat_in_progress = true;
    setScore(P2, 7);

    exec.killObject(draven);          // snapshots last_known before he leaves
    ASSERT_TRUE(state.getObject(draven).last_known.valid);
    state.battlefields[0].combat_in_progress = false;   // the combat is over by resolution
    fireTriggerAs(kDravenAudacious, P1, draven, TriggerType::WhenIDie, exec);

    EXPECT_EQ(state.player(P2).score, 8) << "\"They gain 1 point\"";
    EXPECT_TRUE(state.game_over) << "CR 194.1.c: through scorePoints";
    EXPECT_EQ(state.winner, P2);
}

TEST_F(RulesCards2Test, RulesCards2_CR383_Draven_DiedAsAnAttacker_CountsAsInCombat) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto draven = addUnit(P1, kDravenAudacious, 0, /*at_bf=*/0);
    state.getObject(draven).combat_designation = CombatDesignation::Attacker;

    exec.killObject(draven);          // the designation is cleared by the kill
    ASSERT_EQ(state.getObject(draven).combat_designation, CombatDesignation::None);
    fireTriggerAs(kDravenAudacious, P1, draven, TriggerType::WhenIDie, exec);

    EXPECT_EQ(state.player(P2).score, 1);
}

TEST_F(RulesCards2Test, RulesCards2_CR383_Draven_DiedAtABattlefieldOutsideCombat_NoPoint) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto draven = addUnit(P1, kDravenAudacious, 0, /*at_bf=*/0);

    exec.killObject(draven);          // e.g. a spell in the Main Phase: no combat here
    ASSERT_TRUE(state.getObject(draven).last_location.has_value());
    fireTriggerAs(kDravenAudacious, P1, draven, TriggerType::WhenIDie, exec);

    EXPECT_EQ(state.player(P2).score, 0)
        << "\"When I die IN COMBAT\": dying at a battlefield is not enough";
}

TEST_F(RulesCards2Test, RulesCards2_CR383_Draven_BlockedOpponentGainsNothing) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto draven = addUnit(P1, kDravenAudacious, 0, /*at_bf=*/0);
    state.battlefields[0].combat_in_progress = true;
    state.player(P2).cannot_gain_points = true;

    exec.killObject(draven);
    fireTriggerAs(kDravenAudacious, P1, draven, TriggerType::WhenIDie, exec);

    EXPECT_EQ(state.player(P2).score, 0);
}

TEST_F(RulesCards2Test, RulesCards2_CR383_UnsungHero_WasMightyReadsMightFromBeforeDeath) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    addToDeck(P1, kInvalidId);
    addToDeck(P1, kInvalidId);
    auto hero = addUnit(P1, kUnsungHero, 0, /*at_bf=*/0);     // printed 2 [M]
    exec.giveTemporaryMight(hero, 3);                          // 5 [M]: Mighty
    ASSERT_EQ(state.getObject(hero).current_might, 5);

    exec.killObject(hero);
    // What is left of it in the trash is the printed card again.
    auto& dead = state.getObject(hero);
    dead.temp_might_bonus = 0;
    dead.recomputeMight();
    ASSERT_LT(dead.current_might, 5);
    fireTriggerAs(kUnsungHero, P1, hero, TriggerType::WhenIDie, exec);

    EXPECT_EQ(handSize(P1), 2) << "\"If I WAS [Mighty], draw 2\"";
}

TEST_F(RulesCards2Test, RulesCards2_CR383_UnsungHero_NotMightyAtDeath_NoDraw) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    addToDeck(P1, kInvalidId);
    addToDeck(P1, kInvalidId);
    auto hero = addUnit(P1, kUnsungHero, 0, /*at_bf=*/0);

    exec.killObject(hero);            // died with 2 [M]
    // Whatever happens to the object afterwards is irrelevant.
    state.getObject(hero).current_might = 9;
    fireTriggerAs(kUnsungHero, P1, hero, TriggerType::WhenIDie, exec);

    EXPECT_EQ(handSize(P1), 0);
}

TEST_F(RulesCards2Test, RulesCards2_CR383_NoxianEmissary_ReadsLastKnownEmpowered) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto emissary = addUnit(P1, kNoxianEmissary, 0, /*at_bf=*/0);
    state.getObject(emissary).empowered = 1;

    exec.killObject(emissary);
    auto& dead = state.getObject(emissary);
    dead.empowered = 0;                               // cleared once off the board
    dead.card_counters.erase("__died_empowered");     // not what the card reads any more
    fireTriggerAs(kNoxianEmissary, P1, emissary, TriggerType::WhenIDie, exec);

    EXPECT_EQ(countOnBoard(P1, "Recruit"), 2) << "died while Empowered";
}

TEST_F(RulesCards2Test, RulesCards2_CR383_NoxianEmissary_NotEmpoweredAtDeath_NoTokens) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto emissary = addUnit(P1, kNoxianEmissary, 0, /*at_bf=*/0);

    exec.killObject(emissary);
    state.getObject(emissary).empowered = 1;          // too late: it is already dead
    fireTriggerAs(kNoxianEmissary, P1, emissary, TriggerType::WhenIDie, exec);

    EXPECT_EQ(countOnBoard(P1, "Recruit"), 0);
}

TEST_F(RulesCards2Test, RulesCards2_CR383_BaccaiWitherclaw_ReadsLastKnownEmpowered) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    for (int i = 0; i < 3; ++i) addRuneToRuneDeck(P1, Domain::Body);
    auto baccai = addUnit(P1, kBaccaiWitherclaw, 0, /*at_bf=*/0);
    state.getObject(baccai).empowered = 1;

    exec.killObject(baccai);
    auto& dead = state.getObject(baccai);
    dead.empowered = 0;
    dead.card_counters.erase("__died_empowered");
    fireTriggerAs(kBaccaiWitherclaw, P1, baccai, TriggerType::WhenIDie, exec);

    EXPECT_EQ(exhaustedRuneCount(P1), 2) << "died while Empowered -> channel 2 runes exhausted";
    EXPECT_EQ(readyRuneCount(P1), 0);
}

TEST_F(RulesCards2Test, RulesCards2_CR383_KogMaw_DealsOnlyToUnitsAtTheBattlefieldItDiedAt) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto kog = addUnit(P1, kKogMawCaustic, 0, /*at_bf=*/0);
    auto enemy_here = addUnit(P2, kInvalidId, 6, /*at_bf=*/0);
    auto friend_here = addUnit(P1, kInvalidId, 6, /*at_bf=*/0);
    auto enemy_elsewhere = addUnit(P2, kInvalidId, 6, /*at_bf=*/1);
    auto enemy_in_base = addUnit(P2, kInvalidId, 6);

    exec.killObject(kog);
    fireTriggerAs(kKogMawCaustic, P1, kog, TriggerType::WhenIDie, exec);

    EXPECT_EQ(state.getObject(enemy_here).damage_marked, 4);
    EXPECT_EQ(state.getObject(friend_here).damage_marked, 4) << "\"ALL units at my battlefield\"";
    EXPECT_EQ(state.getObject(enemy_elsewhere).damage_marked, 0)
        << "\"at MY battlefield\": not the other battlefield";
    EXPECT_EQ(state.getObject(enemy_in_base).damage_marked, 0);
}

TEST_F(RulesCards2Test, RulesCards2_CR383_KogMaw_DiedInBase_DealsNothing) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto kog = addUnit(P1, kKogMawCaustic, 0);                 // in base
    auto enemy = addUnit(P2, kInvalidId, 6, /*at_bf=*/0);

    exec.killObject(kog);
    fireTriggerAs(kKogMawCaustic, P1, kog, TriggerType::WhenIDie, exec);

    EXPECT_EQ(state.getObject(enemy).damage_marked, 0) << "no \"my battlefield\" in base";
}

// ═══ C. Vendetta cards on the new hooks ═════════════════════════════════════

TEST_F(RulesCards2Test, RulesCards2_KeeperOfLaw_PowerHalfOfTheDiscount) {
    const Card* card = card_registry.get(kKeeperOfLaw);
    ASSERT_NE(card, nullptr);
    EXPECT_EQ(card->selfPowerCostReduction(state, P1), 0);
    EXPECT_EQ(card->selfCostReduction(state, P1), 0);

    state.battlefields[0].controller = P1;
    addUnit(P1, kInvalidId, 2, /*at_bf=*/0);
    EXPECT_EQ(card->selfPowerCostReduction(state, P1), 0) << "one unit there: not \"exactly two\"";
    addUnit(P1, kInvalidId, 2, /*at_bf=*/0);
    EXPECT_EQ(card->selfCostReduction(state, P1), 2) << "\"I cost [2][Y] less\": the [2]";
    EXPECT_EQ(card->selfPowerCostReduction(state, P1), 1) << "\"I cost [2][Y] less\": the [Y]";
    EXPECT_EQ(card->selfPowerCostReduction(state, P2), 0) << "P2 doesn't control that battlefield";
    addUnit(P1, kInvalidId, 2, /*at_bf=*/0);
    EXPECT_EQ(card->selfPowerCostReduction(state, P1), 0) << "three units there";
}

TEST_F(RulesCards2Test, RulesCards2_CR446_ShadySpectacles_UnattachingEndsTheCopy_PlainToken) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    Card* card = card_registry.get(kShadySpectacles);
    ASSERT_NE(card, nullptr);
    auto bearer = addUnit(P1, kInvalidId, 5, /*at_bf=*/0);     // a plain token: no card def
    state.getObject(bearer).is_token = true;
    state.getObject(bearer).tags = {"Recruit"};
    auto model = addUnit(P1, kPunchingPoro, 0, /*at_bf=*/0);
    auto gear = addGear(P1, kShadySpectacles);
    addRune(P1, Domain::Fury);
    addRune(P1, Domain::Order);
    CardContext ctx{state, events, exec, P1, gear};
    ASSERT_TRUE(card->onEquip(ctx, bearer));
    ASSERT_EQ(state.getObject(bearer).card_def_id, kPunchingPoro);

    exec.unattachGear(gear);          // runs Card::onUnattached

    const auto& b = state.getObject(bearer);
    EXPECT_EQ(b.card_def_id, kInvalidId)
        << "audit item 4: the token's own (absent) card def is put back";
    EXPECT_EQ(b.name, "TestUnit");
    EXPECT_EQ(b.base_might, 5);
    EXPECT_EQ(b.current_might, 5);
    EXPECT_EQ(b.tags, std::vector<std::string>{"Recruit"});
    EXPECT_TRUE(b.isToken()) << "CR 185.1";
    EXPECT_TRUE(b.card_counters.find("__spectacles_copies") == b.card_counters.end());
    EXPECT_EQ(state.getObject(model).card_def_id, kPunchingPoro);
}

TEST_F(RulesCards2Test, RulesCards2_CR446_ShadySpectacles_UnattachingEndsTheCopy_Card) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    Card* card = card_registry.get(kShadySpectacles);
    auto bearer = addUnit(P1, kLegionRearguard, 0, /*at_bf=*/0);
    const GameObject before = state.getObject(bearer);
    addUnit(P1, kArachnoidHorror, 0, /*at_bf=*/0);
    auto gear = addGear(P1, kShadySpectacles);
    addRune(P1, Domain::Fury);
    addRune(P1, Domain::Order);
    CardContext ctx{state, events, exec, P1, gear};
    ASSERT_TRUE(card->onEquip(ctx, bearer));
    ASSERT_EQ(state.getObject(bearer).card_def_id, kArachnoidHorror);
    ASSERT_EQ(state.getObject(bearer).base_might, card_db.get(kArachnoidHorror).might);

    exec.unattachGear(gear);

    const auto& b = state.getObject(bearer);
    EXPECT_EQ(b.card_def_id, kLegionRearguard);
    EXPECT_EQ(b.name, before.name);
    EXPECT_EQ(b.base_might, before.base_might);
    EXPECT_EQ(b.tags, before.tags);
    EXPECT_EQ(b.domains, before.domains);
    EXPECT_EQ(b.keywords.bits, before.keywords.bits);
    EXPECT_EQ(b.super_type, before.super_type);
    EXPECT_FALSE(b.isToken()) << "CR 185.1.b: a card never becomes a token";
}

TEST_F(RulesCards2Test, RulesCards2_CR446_ShadySpectacles_StaleCopyEndsWhenReEquipped) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    Card* card = card_registry.get(kShadySpectacles);
    auto first = addUnit(P1, kInvalidId, 5, /*at_bf=*/0);
    auto second = addUnit(P1, kLegionRearguard, 0, /*at_bf=*/0);
    auto gear = addGear(P1, kShadySpectacles);
    for (int i = 0; i < 2; ++i) { addRune(P1, Domain::Fury); addRune(P1, Domain::Order); }
    CardContext ctx{state, events, exec, P1, gear};
    ASSERT_TRUE(card->onEquip(ctx, first));
    ASSERT_EQ(state.getObject(first).card_def_id, kLegionRearguard);
    // The gear comes off by a path that does not run onUnattached.
    state.getObject(gear).attached_to = std::nullopt;
    state.getObject(first).attachments.clear();
    state.getObject(first).attachment_might_bonus = 0;

    ASSERT_TRUE(card->onEquip(ctx, second));

    EXPECT_EQ(state.getObject(first).card_def_id, kInvalidId)
        << "the copy lasts only \"for as long as this is attached to it\"";
    EXPECT_EQ(state.getObject(first).base_might, 5);
    EXPECT_TRUE(state.getObject(gear).attached_to == std::optional<GameObjectId>(second));
}

TEST_F(RulesCards2Test, RulesCards2_GangplankNaval_EmpoweredReplacesHostileEffectsOfWhatChoseHim) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto gp = addUnit(P1, kGangplankNaval, 0, /*at_bf=*/0);
    state.getObject(gp).empowered = 1;
    auto enemy_spell = pushSpellOnChain(P2, kInvalidId, {});
    state.chain.items.back().chosen_objects = {gp};     // chosen at resolution
    beginResolvingTop();

    exec.stunUnitBy(gp, enemy_spell);
    EXPECT_FALSE(state.getObject(gp).is_stunned) << "\"would stun me\" is replaced";
    EXPECT_EQ(state.getObject(gp).temp_might_bonus, 3) << "\"give me +3 [M] instead\"";

    exec.giveTemporaryMight(gp, -4);
    EXPECT_EQ(state.getObject(gp).temp_might_bonus, 6) << "\"would give me -[M]\" is replaced";

    exec.bounceToHand(gp);
    EXPECT_TRUE(onBoard(gp)) << "\"would return me to hand\" is replaced";
    EXPECT_FALSE(inHand(P1, gp));
    EXPECT_EQ(state.getObject(gp).temp_might_bonus, 9);
}

TEST_F(RulesCards2Test, RulesCards2_GangplankNaval_NoReplacementUnlessEmpoweredAndChosen) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    // Not Empowered, chosen.
    auto plain = addUnit(P1, kGangplankNaval, 0, /*at_bf=*/0);
    // Empowered, but the resolving spell did not choose him.
    auto unchosen = addUnit(P1, kGangplankNaval, 0, /*at_bf=*/0);
    state.getObject(unchosen).empowered = 1;
    auto enemy_spell = pushSpellOnChain(P2, kInvalidId, {plain});
    beginResolvingTop();

    exec.stunUnitBy(plain, enemy_spell);
    exec.stunUnitBy(unchosen, enemy_spell);

    EXPECT_TRUE(state.getObject(plain).is_stunned) << "\"[Empowered][>]\": only while Empowered";
    EXPECT_EQ(state.getObject(plain).temp_might_bonus, 0);
    EXPECT_TRUE(state.getObject(unchosen).is_stunned)
        << "\"a spell or ability that CHOOSES me\": this one didn't";
    EXPECT_EQ(state.getObject(unchosen).temp_might_bonus, 0);
}

TEST_F(RulesCards2Test, RulesCards2_CR809_DecreeOfInsight_IgnoresDeflect) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    ASSERT_TRUE(card_registry.get(kDecreeOfInsight)->ignoresDeflect());
    auto body = addUnit(P2, kInvalidId, 7, /*at_bf=*/0);
    state.getObject(body).domains = {Domain::Body};
    state.getObject(body).keywords.set(Keyword::Deflect);
    state.getObject(body).deflect_value = 1;
    auto rune = addRune(P1, Domain::Mind, /*exhausted=*/true);   // could pay a Deflect [A]
    auto spell = addToHand(P1, kDecreeOfInsight);
    Seen seen;

    driveSpell(kDecreeOfInsight, P1, spell, {}, pickObj(body, &seen), exec);

    EXPECT_TRUE(seen.wasOffered(body));
    EXPECT_EQ(state.getObject(body).temp_might_bonus, -5);
    EXPECT_TRUE(onBoard(rune)) << "\"Ignore [Deflect] while paying this spell's cost\": no [A] paid";
}

TEST_F(RulesCards2Test, RulesCards2_CR809_DecreeOfInsight_ChoosesADeflectUnitWithNothingToPay) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto body = addUnit(P2, kInvalidId, 7, /*at_bf=*/0);
    state.getObject(body).domains = {Domain::Body};
    state.getObject(body).keywords.set(Keyword::Deflect);
    state.getObject(body).deflect_value = 1;
    auto calm = addUnit(P2, kInvalidId, 7, /*at_bf=*/0);
    state.getObject(calm).domains = {Domain::Calm};
    state.getObject(calm).keywords.set(Keyword::Deflect);
    state.getObject(calm).deflect_value = 1;
    ASSERT_EQ(exec.payableAnyPower(P1), 0);

    auto insight = addToHand(P1, kDecreeOfInsight);
    driveSpell(kDecreeOfInsight, P1, insight, {}, pickObj(body), exec);
    EXPECT_EQ(state.getObject(body).temp_might_bonus, -5);

    // Control: a spell that does NOT ignore Deflect can't choose such a unit.
    auto rage = addToHand(P1, kDecreeOfRage);
    Seen seen;
    driveSpell(kDecreeOfRage, P1, rage, {}, pickObj(calm, &seen), exec);
    EXPECT_FALSE(seen.wasOffered(calm)) << "CR 809: the [A] can't be paid";
    EXPECT_EQ(state.getObject(calm).damage_marked, 0);
}

TEST_F(RulesCards2Test, RulesCards2_Lacerate_RecomputesContinuousEffectsBeforeTheMightCheck) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    // 2 [M] printed, "[Empowered][>] I have +2 [M]" as a continuous effect.
    auto unit = addUnit(P2, kInvalidId, 2, /*at_bf=*/0);
    auto& u = state.getObject(unit);
    u.empowered = 1;
    u.aura_might_bonus = 2;
    u.recomputeMight();
    ASSERT_EQ(u.current_might, 4);
    int recalcs = 0;
    exec.setAuraRecalc([&] {          // what GameEngine::recalculateAuras does for this unit
        ++recalcs;
        auto& o = state.getObject(unit);
        o.aura_might_bonus = o.isEmpowered() ? 2 : 0;
        o.recomputeMight();
    });
    auto spell = addToHand(P1, kLacerate);

    driveSpell(kLacerate, P1, spell, {}, pickObj(unit), exec);

    EXPECT_GE(recalcs, 1);
    EXPECT_FALSE(onBoard(unit))
        << "disempowered -> 2 [M] -> \"kill it if it has 3 [M] or less\"";
}

TEST_F(RulesCards2Test, RulesCards2_Lacerate_FourMightAfterDisempowerSurvives) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto unit = addUnit(P2, kInvalidId, 4, /*at_bf=*/0);
    auto& u = state.getObject(unit);
    u.empowered = 1;
    u.aura_might_bonus = 2;
    u.recomputeMight();
    exec.setAuraRecalc([&] {
        auto& o = state.getObject(unit);
        o.aura_might_bonus = o.isEmpowered() ? 2 : 0;
        o.recomputeMight();
    });
    auto spell = addToHand(P1, kLacerate);

    driveSpell(kLacerate, P1, spell, {}, pickObj(unit), exec);

    EXPECT_TRUE(onBoard(unit));
    EXPECT_FALSE(state.getObject(unit).isEmpowered());
    EXPECT_EQ(state.getObject(unit).current_might, 4);
}

TEST_F(RulesCards2Test, RulesCards2_DecreeOfFocus_SeesWhatAnEnemyFurySpellChose) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    const Card* card = card_registry.get(kDecreeOfFocus);
    ASSERT_NE(card, nullptr);
    auto mine = addUnit(P1, kInvalidId, 3, /*at_bf=*/0);
    auto other = addUnit(P1, kInvalidId, 3, /*at_bf=*/0);
    auto fury = pushSpellOnChain(P2, kInvalidId, {});
    state.getObject(fury).domains = {Domain::Fury};
    EXPECT_TRUE(card->enumerateLegalTargets(state, P1).empty());

    // Only in chosen_objects (not in the play-time targets).
    state.chain.items.back().chosen_objects = {mine};
    auto legal = card->enumerateLegalTargets(state, P1);
    ASSERT_EQ(legal.size(), 1u);
    EXPECT_EQ(legal[0], mine);
    (void)other;

    auto spell = addToHand(P1, kDecreeOfFocus);
    // Decree of Focus resolves above the Fury spell, which stays on the chain.
    ChainItem ri;
    ri.id = state.chain.allocateId();
    ri.controller = P1;
    ri.is_spell = true;
    ri.source = spell;
    ri.card_def_id = kDecreeOfFocus;
    state.chain.resuming = ri;
    Card* c = card_registry.get(kDecreeOfFocus);
    CardContext ctx{state, events, exec, P1, spell};
    ctx.registry = &card_registry;
    c->onResolve(ctx, {});
    while (exec.hasPendingChoice()) {
        auto pending = exec.consumePendingChoice();
        exec.recordChoice(pickObj(mine)(pending.legal));
        c->onResolve(ctx, {});
    }
    state.chain.resuming.reset();

    EXPECT_EQ(state.getObject(mine).temp_might_bonus, 4);
    EXPECT_EQ(state.getObject(other).temp_might_bonus, 0);
}

TEST_F(RulesCards2Test, RulesCards2_DecreeOfFocus_NonFuryOrOwnSpellDoesNotQualify) {
    const Card* card = card_registry.get(kDecreeOfFocus);
    auto mine = addUnit(P1, kInvalidId, 3, /*at_bf=*/0);
    auto calm = pushSpellOnChain(P2, kInvalidId, {});
    state.getObject(calm).domains = {Domain::Calm};
    state.chain.items.back().chosen_objects = {mine};
    auto own = pushSpellOnChain(P1, kInvalidId, {});
    state.getObject(own).domains = {Domain::Fury};
    state.chain.items.back().chosen_objects = {mine};

    EXPECT_TRUE(card->enumerateLegalTargets(state, P1).empty())
        << "\"being chosen by an ENEMY FURY spell\"";
}

TEST_F(RulesCards2Test, RulesCards2_HungryWolf_GateAndOncePerTurnWhileOnTheChain) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    Card* card = card_registry.get(kHungryWolf);
    ASSERT_NE(card, nullptr);
    auto wolf = addUnit(P1, kHungryWolf, 0, /*at_bf=*/0);
    state.getObject(wolf).is_exhausted = true;

    EXPECT_FALSE(card->canActivateAbility(state, P1))
        << "\"Use only if you've chosen an enemy unit this turn\"";
    state.player(P1).chose_enemy_unit_this_turn = true;
    EXPECT_TRUE(card->canActivateAbility(state, P1));
    EXPECT_TRUE(card->canActivateAbilityAt(state, P1, wolf, 0));

    // Activated: the ability waits on the chain.
    ChainItem item;
    item.id = state.chain.allocateId();
    item.source = wolf;
    item.card_def_id = kHungryWolf;
    item.controller = P1;
    item.is_ability = true;
    item.is_activated_ability = true;
    item.ability_index = 0;
    state.chain.items.push_back(item);
    state.getObject(wolf).card_counters["__ability_used_turn_0"] = state.turn.turn_number + 1;   // as GameEngine does on activation
    EXPECT_FALSE(card->canActivateAbilityAt(state, P1, wolf, 0))
        << "\"only once each turn\": not a second time while the first is on the chain";

    // It resolves.
    beginResolvingTop();
    EXPECT_FALSE(card->canActivateAbilityAt(state, P1, wolf, 0)) << "nor while it resolves";
    CardContext ctx{state, events, exec, P1, wolf};
    card->onActivate(ctx, {});
    state.chain.resuming.reset();

    EXPECT_FALSE(state.getObject(wolf).is_exhausted) << "\"Ready me\"";
    EXPECT_EQ(state.getObject(wolf).temp_might_bonus, 1) << "\"+1 [M] this turn\"";
    EXPECT_FALSE(card->canActivateAbilityAt(state, P1, wolf, 0)) << "used this turn";
    state.turn.turn_number++;
    EXPECT_TRUE(card->canActivateAbilityAt(state, P1, wolf, 0)) << "a new turn";
}

TEST_F(RulesCards2Test, RulesCards2_HungryWolf_AnotherWolfsActivationDoesNotBlockThisOne) {
    Card* card = card_registry.get(kHungryWolf);
    auto wolf = addUnit(P1, kHungryWolf, 0, /*at_bf=*/0);
    auto other = addUnit(P1, kHungryWolf, 0, /*at_bf=*/0);
    ChainItem item;
    item.id = state.chain.allocateId();
    item.source = other;
    item.card_def_id = kHungryWolf;
    item.controller = P1;
    item.is_ability = true;
    item.is_activated_ability = true;
    state.chain.items.push_back(item);
    state.getObject(other).card_counters["__ability_used_turn_0"] = state.turn.turn_number + 1;   // as GameEngine does on activation

    EXPECT_TRUE(card->canActivateAbilityAt(state, P1, wolf, 0)) << "once each turn PER wolf";
    EXPECT_FALSE(card->canActivateAbilityAt(state, P1, other, 0));
}

TEST_F(RulesCards2Test, RulesCards2_CR164_Rebuttal_PaysTheAnyPowerByRecyclingAnExhaustedRune) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto enemy_spell = pushSpellOnChain(P2, kInvalidId, {});
    auto rune = addRune(P1, Domain::Chaos, /*exhausted=*/true);
    auto rebuttal = addToHand(P1, kRebuttal);

    driveSpell(kRebuttal, P1, rebuttal, {}, &RulesCards2Test::yes, exec);

    ASSERT_EQ(state.chain.items.size(), 1u) << "paid: gained control instead of countering";
    EXPECT_EQ(state.chain.items.back().controller, P1);
    EXPECT_EQ(state.getObject(enemy_spell).controller, P1);
    EXPECT_FALSE(state.getObject(rune).location.has_value())
        << "CR 164.2.b: an exhausted rune can be recycled for [A]";
    EXPECT_EQ(state.player(P1).rune_deck.size(), 1u);
}

TEST_F(RulesCards2Test, RulesCards2_JaeMedarda_DrawsWhenASpellOfMineChoseHer) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto jae = addUnit(P1, kJaeMedarda, 0, /*at_bf=*/0);
    addToDeck(P1, kInvalidId);
    addToDeck(P1, kInvalidId);

    // "with a spell" of mine is the ability's trigger condition, decided as
    // the choice happens (plans/cards/batch-05.md, 463): it used to be
    // re-derived from the chain when the trigger resolved.
    const Card* card = card_registry.get(kJaeMedarda);
    const auto chosen = TriggerType::WhenYouChooseAFriendlyUnit;

    // An ENEMY spell choosing her is not "you choose me".
    pushSpellOnChain(P2, kInvalidId, {});
    state.chain.items.back().chosen_objects = {jae};
    EXPECT_FALSE(card->triggerCondition(state, jae, P1, chosen, jae));

    // My spell, with her only in chosen_objects.
    pushSpellOnChain(P1, kInvalidId, {});
    state.chain.items.back().chosen_objects = {jae};
    ASSERT_TRUE(card->triggerCondition(state, jae, P1, chosen, jae));
    fireTriggerAs(kJaeMedarda, P1, jae, chosen, exec);
    EXPECT_EQ(handSize(P1), 1);
}

// ═══ D. "Ready it", "a Mech", gear tokens (CR 149.2) ════════════════════════

TEST_F(RulesCards2Test, RulesCards2_BladeDancer_ReadiesTheUnitThatWasChosen) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto legend = addLegend(P1, kBladeDancer, /*exhausted=*/false);
    auto bystander = addUnit(P1, kInvalidId, 3, /*at_bf=*/0);   // lower id, also exhausted
    auto chosen = addUnit(P1, kInvalidId, 3, /*at_bf=*/0);
    state.getObject(bystander).is_exhausted = true;
    state.getObject(chosen).is_exhausted = true;
    addRune(P1, Domain::Chaos);
    Seen seen;

    driveTrigger(kBladeDancer, P1, legend, TriggerType::WhenYouChooseAFriendlyUnit,
                 watch(seen, &RulesCards2Test::yes), exec, /*subject=*/chosen);

    EXPECT_FALSE(state.getObject(chosen).is_exhausted) << "\"to ready IT\"";
    EXPECT_TRUE(state.getObject(bystander).is_exhausted)
        << "another exhausted friendly unit is not \"it\"";
    EXPECT_TRUE(seen.offered.empty()) << "there is no unit to pick: \"it\" is the chosen one";
    EXPECT_EQ(seen.prompts, 1) << "only the \"you may\"";
    EXPECT_TRUE(state.getObject(legend).is_exhausted) << "\"exhaust me\"";
    EXPECT_EQ(runesInBase(P1), 0) << "\"pay [A]\", once";
}

TEST_F(RulesCards2Test, RulesCards2_BladeDancer_NothingWithoutAChosenUnit) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto legend = addLegend(P1, kBladeDancer, /*exhausted=*/false);
    auto unit = addUnit(P1, kInvalidId, 3, /*at_bf=*/0);
    state.getObject(unit).is_exhausted = true;
    addRune(P1, Domain::Chaos);
    Seen seen;

    driveTrigger(kBladeDancer, P1, legend, TriggerType::WhenYouChooseAFriendlyUnit,
                 watch(seen, &RulesCards2Test::yes), exec, /*subject=*/kInvalidId);

    EXPECT_EQ(seen.prompts, 0);
    EXPECT_TRUE(state.getObject(unit).is_exhausted);
    EXPECT_FALSE(state.getObject(legend).is_exhausted);
    EXPECT_EQ(runesInBase(P1), 1);
}

TEST_F(RulesCards2Test, RulesCards2_BladeDancer_AnEnemyUnitIsNotAFriendlyUnit) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto legend = addLegend(P1, kBladeDancer, /*exhausted=*/false);
    auto enemy = addUnit(P2, kInvalidId, 3, /*at_bf=*/0);
    state.getObject(enemy).is_exhausted = true;
    addRune(P1, Domain::Chaos);

    driveTrigger(kBladeDancer, P1, legend, TriggerType::WhenYouChooseAFriendlyUnit,
                 &RulesCards2Test::yes, exec, /*subject=*/enemy);

    EXPECT_TRUE(state.getObject(enemy).is_exhausted);
    EXPECT_FALSE(state.getObject(legend).is_exhausted);
}

TEST_F(RulesCards2Test, RulesCards2_CR187_4_ProductionSurge_AMechIsAUnitWithTheMechTag) {
    const Card* card = card_registry.get(kProductionSurge);
    ASSERT_NE(card, nullptr);
    auto impostor = addUnit(P1, kInvalidId, 3);
    state.getObject(impostor).name = "Mechanical Tinkerer";      // name only, no tag
    EXPECT_EQ(card->selfCostReduction(state, P1), 0)
        << "a name containing \"Mech\" does not make a Mech";

    auto mech = addUnit(P1, kInvalidId, 3);
    state.getObject(mech).name = "Bubble Bot";
    state.getObject(mech).tags = {"Mech"};
    EXPECT_EQ(card->selfCostReduction(state, P1), 2) << "the Mech TAG does";
    EXPECT_EQ(card->selfCostReduction(state, P2), 0) << "\"if YOU control a Mech\"";
}

TEST_F(RulesCards2Test, RulesCards2_CR149_2_TreasureHoard_GoldTokenGoesToBase) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto bf_card = installBattlefieldCard(0, kTreasureHoard);
    addRune(P1, Domain::Fury);

    driveTrigger(kTreasureHoard, P1, bf_card, TriggerType::WhenYouConquerHere,
                 &RulesCards2Test::yes, exec);

    expectGoldInBase(P1, 1);
}

TEST_F(RulesCards2Test, RulesCards2_CR149_2_WorldAtlas_GoldTokensGoToBase) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto unit = addUnit(P1, kInvalidId, 3, /*at_bf=*/1);
    auto atlas = addGear(P1, kWorldAtlas);
    attach(atlas, unit);
    Card* card = card_registry.get(kWorldAtlas);
    ASSERT_NE(card, nullptr);
    CardContext ctx{state, events, exec, P1, atlas};

    card->onEquippedTrigger(ctx, unit, {});

    expectGoldInBase(P1, 2);
}

TEST_F(RulesCards2Test, RulesCards2_CR149_2_Cull_GoldTokenGoesToBase) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto unit = addUnit(P1, kInvalidId, 3, /*at_bf=*/1);
    auto cull = addGear(P1, kCull);
    attach(cull, unit);
    Card* card = card_registry.get(kCull);
    ASSERT_NE(card, nullptr);
    CardContext ctx{state, events, exec, P1, cull};

    card->onEquippedTrigger(ctx, unit, {});

    expectGoldInBase(P1, 1);
}

TEST_F(RulesCards2Test, RulesCards2_CR149_2_PykeReturned_GoldTokenGoesToBase) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto pyke = addUnit(P1, kPykeReturned, 0, /*at_bf=*/0);

    fireTriggerAs(kPykeReturned, P1, pyke, TriggerType::WhenAnEnemyUnitDies, exec);
    expectGoldInBase(P1, 1);

    fireTriggerAs(kPykeReturned, P1, pyke, TriggerType::WhenAnEnemyUnitDies, exec);
    expectGoldInBase(P1, 1);          // "Once each turn"
}

// ═══ E. Controller's choices ════════════════════════════════════════════════

TEST_F(RulesCards2Test, RulesCards2_Arise_ControllerChoosesWhichTwoTokensToReady) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    for (int i = 0; i < 3; ++i) addGear(P1, kInvalidId, {"Equipment"});
    auto spell = addToHand(P1, kArise);
    int prompts = 0;
    std::vector<GameObjectId> picked;
    // Always take the highest-id token offered.
    Picker highest = [&](const std::vector<Intent>& legal) -> Intent {
        ++prompts;
        const Intent* best = nullptr;
        for (const auto& i : legal)
            if (!i.chosen_objects.empty() &&
                (!best || i.chosen_objects[0] > best->chosen_objects[0])) best = &i;
        if (!best) return legal.front();
        picked.push_back(best->chosen_objects[0]);
        return *best;
    };

    driveSpell(kArise, P1, spell, {}, highest, exec);

    ASSERT_EQ(countOnBoard(P1, "Sand Soldier"), 3) << "one per Equipment";
    EXPECT_EQ(prompts, 2) << "\"Ready up to two of them\"";
    ASSERT_EQ(picked.size(), 2u);
    EXPECT_NE(picked[0], picked[1]);
    int ready = 0;
    GameObjectId lowest = kInvalidId;
    for (const auto& [id, obj] : state.objects) {
        if (obj.name != "Sand Soldier") continue;
        if (lowest == kInvalidId || id < lowest) lowest = id;
        if (!obj.is_exhausted) ++ready;
    }
    EXPECT_EQ(ready, 2);
    EXPECT_FALSE(state.getObject(picked[0]).is_exhausted);
    EXPECT_FALSE(state.getObject(picked[1]).is_exhausted);
    EXPECT_TRUE(state.getObject(lowest).is_exhausted)
        << "the two readied are the controller's picks, not simply the first two";
}

TEST_F(RulesCards2Test, RulesCards2_Arise_UpToTwoMayBeNone) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    for (int i = 0; i < 3; ++i) addGear(P1, kInvalidId, {"Equipment"});
    auto spell = addToHand(P1, kArise);
    Seen seen;

    driveSpell(kArise, P1, spell, {}, watch(seen, &RulesCards2Test::no), exec);

    EXPECT_EQ(seen.prompts, 1);
    ASSERT_EQ(countOnBoard(P1, "Sand Soldier"), 3);
    for (const auto& [id, obj] : state.objects)
        if (obj.name == "Sand Soldier") { EXPECT_TRUE(obj.is_exhausted) << "\"up to two\": none"; }
}

TEST_F(RulesCards2Test, RulesCards2_Arise_OneTokenOnePick) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    addGear(P1, kInvalidId, {"Equipment"});
    auto spell = addToHand(P1, kArise);
    Seen seen;

    driveSpell(kArise, P1, spell, {}, watch(seen, &RulesCards2Test::yes), exec);

    EXPECT_EQ(seen.prompts, 1) << "nothing left to ready after the only token";
    ASSERT_EQ(countOnBoard(P1, "Sand Soldier"), 1);
    EXPECT_FALSE(findOnBoard(P1, "Sand Soldier")->is_exhausted);
}

TEST_F(RulesCards2Test, RulesCards2_Deceiver_ControllerChoosesTheUnitToCopy) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto legend = addLegend(P1, kDeceiver, /*exhausted=*/false);
    state.battlefields[0].controller = P1;
    auto big = addUnit(P1, kArachnoidHorror, 0, /*at_bf=*/0);      // 6 [M], lower id
    auto small = addUnit(P2, kLegionRearguard, 0, /*at_bf=*/0);    // 2 [M], an enemy's
    auto elsewhere = addUnit(P1, kPunchingPoro, 0, /*at_bf=*/1);
    auto card_in_hand = addToHand(P1, kInvalidId);
    Seen seen;
    // The conquered battlefield's card: a conquer / hold trigger's subject.
    auto scored_bf = state.createObject();
    state.getObject(scored_bf).card_type = CardType::Battlefield;
    state.battlefields[0].card_object_id = scored_bf;

    driveTrigger(kDeceiver, P1, legend, TriggerType::WhenIConquerOrHold,
                 pickObj(small, &seen), exec, scored_bf);

    EXPECT_TRUE(state.getObject(legend).is_exhausted);
    EXPECT_TRUE(inTrash(P1, card_in_hand));
    EXPECT_TRUE(seen.wasOffered(big));
    EXPECT_TRUE(seen.wasOffered(small)) << "\"another unit there\": any controller's";
    EXPECT_FALSE(seen.wasOffered(elsewhere)) << "\"THERE\"";
    const GameObject* token = nullptr;
    for (const auto& [id, obj] : state.objects)
        if (obj.isToken() && obj.isUnit() && obj.controller == P1 && obj.location.has_value())
            token = &obj;
    ASSERT_NE(token, nullptr);
    EXPECT_EQ(token->card_def_id, kLegionRearguard)
        << "the controller's pick, not the highest-Might unit";
    EXPECT_TRUE(token->keywords.has(Keyword::Temporary)) << "\"Give it [Temporary]\"";
    EXPECT_FALSE(token->is_exhausted) << "\"a READY Reflection unit token\"";
    EXPECT_EQ(token->battlefieldId().value_or(kInvalidId), 0u);
}

TEST_F(RulesCards2Test, RulesCards2_CR383_3_Deceiver_YouMayDecline) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto legend = addLegend(P1, kDeceiver, /*exhausted=*/false);
    state.battlefields[0].controller = P1;
    addUnit(P1, kArachnoidHorror, 0, /*at_bf=*/0);
    auto card_in_hand = addToHand(P1, kInvalidId);
    Seen seen;

    driveTrigger(kDeceiver, P1, legend, TriggerType::WhenIConquerOrHold,
                 watch(seen, &RulesCards2Test::no), exec);

    EXPECT_EQ(seen.prompts, 1) << "\"you MAY discard 1 and exhaust me\"";
    EXPECT_FALSE(state.getObject(legend).is_exhausted);
    EXPECT_TRUE(inHand(P1, card_in_hand));
    for (const auto& [id, obj] : state.objects) EXPECT_FALSE(obj.isToken());
}

TEST_F(RulesCards2Test, RulesCards2_Jax_WeaponmasterChoosesWhichEquipment) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto jax = addUnit(P1, kJaxUnrelenting, 0, /*at_bf=*/0);
    auto first = addGear(P1, kInvalidId, {"Equipment"}, /*might_bonus=*/1);
    auto second = addGear(P1, kInvalidId, {"Equipment"}, /*might_bonus=*/2);
    auto not_equipment = addGear(P1, kInvalidId, {}, /*might_bonus=*/5);
    Seen seen;

    driveTrigger(kJaxUnrelenting, P1, jax, TriggerType::WhenYouPlayMe,
                 pickObj(second, &seen), exec);

    EXPECT_TRUE(seen.wasOffered(first));
    EXPECT_TRUE(seen.wasOffered(second));
    EXPECT_FALSE(seen.wasOffered(not_equipment)) << "\"one of your Equipment\"";
    EXPECT_TRUE(state.getObject(second).attached_to == std::optional<GameObjectId>(jax));
    EXPECT_FALSE(state.getObject(first).attached_to.has_value())
        << "the controller's pick, not the first Equipment found";
    EXPECT_EQ(state.getObject(jax).attachment_might_bonus, 2);
}

TEST_F(RulesCards2Test, RulesCards2_Jax_WeaponmasterIsOptional) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto jax = addUnit(P1, kJaxUnrelenting, 0, /*at_bf=*/0);
    auto gear = addGear(P1, kInvalidId, {"Equipment"}, /*might_bonus=*/2);

    driveTrigger(kJaxUnrelenting, P1, jax, TriggerType::WhenYouPlayMe,
                 &RulesCards2Test::no, exec);

    EXPECT_FALSE(state.getObject(gear).attached_to.has_value()) << "\"you MAY [Equip]\"";
    EXPECT_TRUE(state.getObject(jax).attachments.empty());
}

TEST_F(RulesCards2Test, RulesCards2_Jax_WeaponmasterTakesAnEquipmentOffAnotherUnit) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto jax = addUnit(P1, kJaxUnrelenting, 0, /*at_bf=*/0);
    auto other = addUnit(P1, kInvalidId, 3, /*at_bf=*/1);
    auto gear = addGear(P1, kInvalidId, {"Equipment"}, /*might_bonus=*/2);
    attach(gear, other);
    ASSERT_EQ(state.getObject(other).current_might, 5);

    driveTrigger(kJaxUnrelenting, P1, jax, TriggerType::WhenYouPlayMe, pickObj(gear), exec);

    EXPECT_TRUE(state.getObject(gear).attached_to == std::optional<GameObjectId>(jax))
        << "\"even if it's already attached\"";
    EXPECT_TRUE(state.getObject(other).attachments.empty());
    EXPECT_EQ(state.getObject(other).current_might, 3);
    EXPECT_EQ(state.getObject(gear).battlefieldId().value_or(kInvalidId), 0u);
}

TEST_F(RulesCards2Test, RulesCards2_Jax_WeaponmasterEquipCostsOnePowerLess) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto jax = addUnit(P1, kJaxUnrelenting, 0, /*at_bf=*/0);
    auto trinity = addGear(P1, kTrinityForce);        // "[Equip] [O]" -> nothing left to pay
    Seen seen;
    ASSERT_EQ(runesInBase(P1), 0);

    driveTrigger(kJaxUnrelenting, P1, jax, TriggerType::WhenYouPlayMe,
                 pickObj(trinity, &seen), exec);

    EXPECT_TRUE(seen.wasOffered(trinity));
    EXPECT_TRUE(state.getObject(trinity).attached_to == std::optional<GameObjectId>(jax))
        << "[O] for [A] less is free";
}

TEST_F(RulesCards2Test, RulesCards2_Jax_WeaponmasterStillChargesThePrintedEnergy) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto jax = addUnit(P1, kJaxUnrelenting, 0, /*at_bf=*/0);
    auto skyfall = addGear(P1, kSkyfallOfAreion);     // "[Equip] [1][R]" -> [1]
    Seen seen;

    // Nothing to pay the [1] with: not offered (CR 445.2).
    driveTrigger(kJaxUnrelenting, P1, jax, TriggerType::WhenYouPlayMe,
                 pickObj(skyfall, &seen), exec);
    EXPECT_EQ(seen.prompts, 0);
    EXPECT_FALSE(state.getObject(skyfall).attached_to.has_value());

    // With one ready rune: the [1] is paid, the [R] is the "[A] less".
    auto rune = addRune(P1, Domain::Body);
    driveTrigger(kJaxUnrelenting, P1, jax, TriggerType::WhenYouPlayMe,
                 pickObj(skyfall, &seen), exec);
    EXPECT_TRUE(state.getObject(skyfall).attached_to == std::optional<GameObjectId>(jax));
    EXPECT_TRUE(state.getObject(rune).is_exhausted) << "the printed [1] is still paid";
    EXPECT_TRUE(onBoard(rune)) << "no rune recycled: the Power was the discount";
}

// Corrected (sweep-costs, 7.7): the Equip cost is the gear's declaration
// (Card::equipCost), so a non-resource cost is charged too — the Equipment is
// offered while that cost can be paid and never attached for free.
TEST_F(RulesCards2Test, RulesCards2_Jax_WeaponmasterChargesANonResourceEquipCost) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto jax = addUnit(P1, kJaxUnrelenting, 0, /*at_bf=*/0);
    auto heirloom = addGear(P1, kShepherdsHeirloom);  // "[Equip] — Spend 1 XP"
    Seen seen;

    // No XP: the cost cannot be paid, so it is not offered (CR 821.1.c.5).
    setXp(P1, 0);
    driveTrigger(kJaxUnrelenting, P1, jax, TriggerType::WhenYouPlayMe,
                 pickObj(heirloom, &seen), exec);
    EXPECT_FALSE(seen.wasOffered(heirloom)) << "never attached for free";
    EXPECT_FALSE(state.getObject(heirloom).attached_to.has_value());

    setXp(P1, 5);
    driveTrigger(kJaxUnrelenting, P1, jax, TriggerType::WhenYouPlayMe,
                 pickObj(heirloom, &seen), exec);
    EXPECT_TRUE(seen.wasOffered(heirloom));
    EXPECT_TRUE(state.getObject(heirloom).attached_to == std::optional<GameObjectId>(jax));
    EXPECT_EQ(state.player(P1).xp, 4) << "the XP is the Equip cost: no Power in it to reduce";
}

TEST_F(RulesCards2Test, RulesCards2_CR383_Jax_AttachTriggerIsItsOwnAbility) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    Card* card = card_registry.get(kJaxUnrelenting);
    ASSERT_NE(card, nullptr);
    EXPECT_TRUE(card->firesOn(TriggerType::WhenYouPlayMe));
    EXPECT_TRUE(card->firesOn(TriggerType::WhenEquipmentAttachedToMe))
        << "\"When you attach an Equipment to me\": every attach, not only on play";
    auto jax = addUnit(P1, kJaxUnrelenting, 0, /*at_bf=*/0);
    addToDeck(P1, kInvalidId);
    addToDeck(P1, kInvalidId);
    auto rune = addRune(P1, Domain::Fury);

    // An attach long after the play (no Weaponmaster involved).
    driveTrigger(kJaxUnrelenting, P1, jax, TriggerType::WhenEquipmentAttachedToMe,
                 &RulesCards2Test::yes, exec);
    EXPECT_EQ(handSize(P1), 1) << "\"you may pay [1] to draw 1\"";
    EXPECT_TRUE(state.getObject(rune).is_exhausted);

    // Declined: nothing paid, nothing drawn.
    addRune(P1, Domain::Fury);
    driveTrigger(kJaxUnrelenting, P1, jax, TriggerType::WhenEquipmentAttachedToMe,
                 &RulesCards2Test::no, exec);
    EXPECT_EQ(handSize(P1), 1);
    EXPECT_EQ(readyRuneCount(P1), 1);
}

TEST_F(RulesCards2Test, RulesCards2_CR383_3_HallOfLegends_OfferedEvenWhileTheLegendIsReady) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto bf_card = installBattlefieldCard(0, kHallOfLegends);
    auto legend = addLegend(P1, kBladeDancer, /*exhausted=*/false);
    auto rune = addRune(P1, Domain::Calm);
    Seen seen;

    driveTrigger(kHallOfLegends, P1, bf_card, TriggerType::WhenYouConquerHere,
                 watch(seen, &RulesCards2Test::yes), exec);

    EXPECT_EQ(seen.prompts, 1)
        << "CR 383.3.b / 445.2: the cost may be paid whatever state the legend is in";
    EXPECT_TRUE(state.getObject(rune).is_exhausted);
    EXPECT_FALSE(state.getObject(legend).is_exhausted);
}

TEST_F(RulesCards2Test, RulesCards2_CR445_HallOfLegends_NotOfferedWithNothingToPayWith) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto bf_card = installBattlefieldCard(0, kHallOfLegends);
    auto legend = addLegend(P1, kBladeDancer, /*exhausted=*/true);
    Seen seen;

    driveTrigger(kHallOfLegends, P1, bf_card, TriggerType::WhenYouConquerHere,
                 watch(seen, &RulesCards2Test::yes), exec);

    EXPECT_EQ(seen.prompts, 0);
    EXPECT_TRUE(state.getObject(legend).is_exhausted);
}

// ═══ F. Recall, "activate", countering what "chooses" a unit ═══════════════

TEST_F(RulesCards2Test, RulesCards2_CR457_HostileTakeover_LosesControlAndRecallsAtEndOfTurn) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto enemy = addUnit(P2, kInvalidId, 4, /*at_bf=*/0);
    state.getObject(enemy).is_exhausted = true;
    state.getObject(enemy).damage_marked = 1;
    auto gear = addGear(P2, kInvalidId, {"Equipment"}, /*might_bonus=*/1);
    attach(gear, enemy);
    auto spell = addToHand(P1, kHostileTakeover);

    driveSpell(kHostileTakeover, P1, spell, {}, pickObj(enemy), exec);

    EXPECT_EQ(state.getObject(enemy).controller, P1) << "\"Take control\"";
    EXPECT_FALSE(state.getObject(enemy).is_exhausted) << "\"Ready it\"";
    ASSERT_EQ(state.delayed_abilities.size(), 1u);
    EXPECT_EQ(state.delayed_abilities[0].trigger, TriggerType::AtEndOfTurn);
    EXPECT_EQ(state.delayed_abilities[0].controller, P1);
    EXPECT_EQ(state.delayed_abilities[0].card_def_id, kHostileTakeover);

    // End of turn: the delayed ability resolves.
    fireTriggerAs(kHostileTakeover, P1, spell, TriggerType::AtEndOfTurn, exec);

    const auto& u = state.getObject(enemy);
    EXPECT_EQ(u.controller, P2) << "\"Lose control of that unit\"";
    EXPECT_FALSE(u.control_reverts_eot);
    EXPECT_TRUE(u.isAtBase()) << "\"and recall it\" (CR 457)";
    EXPECT_TRUE(*u.location == LocationId{BaseLocation{P2}}) << "to ITS controller's base";
    EXPECT_EQ(u.damage_marked, 1) << "CR 460.1: a recall leaves damage alone";
    EXPECT_TRUE(*state.getObject(gear).location == LocationId{BaseLocation{P2}})
        << "attached gear goes with it";
}

TEST_F(RulesCards2Test, RulesCards2_CR457_HostileTakeover_NothingToRecallIfTheUnitLeft) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto enemy = addUnit(P2, kInvalidId, 4, /*at_bf=*/0);
    auto spell = addToHand(P1, kHostileTakeover);
    driveSpell(kHostileTakeover, P1, spell, {}, pickObj(enemy), exec);

    exec.killObject(enemy);
    fireTriggerAs(kHostileTakeover, P1, spell, TriggerType::AtEndOfTurn, exec);

    EXPECT_FALSE(onBoard(enemy)) << "a dead unit is not brought back to base";
}

TEST_F(RulesCards2Test, RulesCards2_CR383_4_g_ReckonersArena_PutsConquerEffectsOnTheChain) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto arena = installBattlefieldCard(0, kReckonersArena);
    auto horror = addUnit(P1, kArachnoidHorror, 0, /*at_bf=*/0);   // "conquer or hold": +2 XP
    addUnit(P1, kInvalidId, 2, /*at_bf=*/0);                       // no conquer effect
    auto elsewhere = addUnit(P1, kTryndamere, 0, /*at_bf=*/1);     // a conquer effect, not here
    auto enemy = addUnit(P2, kArachnoidHorror, 0, /*at_bf=*/0);    // not the holder's
    CardContext ctx{state, events, exec, P1, arena};
    ctx.firing_trigger = TriggerType::WhenYouHoldHere;
    ctx.registry = &card_registry;

    card_registry.get(kReckonersArena)->onTrigger(ctx, {});

    EXPECT_EQ(state.player(P1).xp, 0)
        << "CR 383.4.g.1: \"placed on the chain as if it had just triggered\", not run inline";
    ASSERT_EQ(state.chain.items.size(), 1u);
    const ChainItem queued = state.chain.items.back();
    EXPECT_EQ(queued.source, horror);
    EXPECT_NE(queued.source, elsewhere);
    EXPECT_NE(queued.source, enemy);
    EXPECT_EQ(queued.controller, P1);
    EXPECT_EQ(queued.card_def_id, kArachnoidHorror);
    EXPECT_TRUE(queued.is_ability);
    EXPECT_FALSE(queued.is_activated_ability);
    EXPECT_EQ(queued.fired_trigger, TriggerType::WhenIConquerOrHold);
    EXPECT_EQ(queued.status, ChainItemStatus::Pending);
    EXPECT_EQ(state.turn.oc_state, OpenClosedState::Closed);

    // It resolves like any triggered ability, with its own resume slots.
    beginResolvingTop();
    CardContext unit_ctx{state, events, exec, queued.controller, queued.source};
    unit_ctx.firing_trigger = queued.fired_trigger;
    unit_ctx.registry = &card_registry;
    card_registry.get(queued.card_def_id)->onTrigger(unit_ctx, {});
    state.chain.resuming.reset();
    EXPECT_EQ(state.player(P1).xp, 2);
    EXPECT_EQ(state.player(P2).xp, 0);
}

TEST_F(RulesCards2Test, RulesCards2_CR383_4_g_ReckonersArena_IncludesConquerEffectsFromEquipment) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto arena = installBattlefieldCard(0, kReckonersArena);
    auto unit = addUnit(P1, kInvalidId, 3, /*at_bf=*/0);
    auto cull = addGear(P1, kCull);                   // "When I conquer, play a Gold ..."
    attach(cull, unit);
    CardContext ctx{state, events, exec, P1, arena};
    ctx.firing_trigger = TriggerType::WhenYouHoldHere;
    ctx.registry = &card_registry;

    card_registry.get(kReckonersArena)->onTrigger(ctx, {});

    expectGoldInBase(P1, 1);
    EXPECT_TRUE(state.chain.items.empty()) << "the unit itself has no conquer ability";
}

TEST_F(RulesCards2Test, RulesCards2_Repulse_NotWhenAnotherFriendlyUnitIsChosenToo) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    Card* card = card_registry.get(kRepulse);
    ASSERT_NE(card, nullptr);
    auto a = addUnit(P1, kInvalidId, 3, /*at_bf=*/0);
    auto b = addUnit(P1, kInvalidId, 3, /*at_bf=*/0);
    auto enemy = addUnit(P2, kInvalidId, 3, /*at_bf=*/0);
    pushSpellOnChain(P2, kInvalidId, {a, b});
    EXPECT_FALSE(card->hasLegalTargets(state, P1)) << "\"it and NO OTHER friendly unit\"";

    auto repulse = addToHand(P1, kRepulse);
    invokeOnResolve(repulse, kRepulse, P1, {}, exec);
    EXPECT_EQ(state.chain.items.size(), 1u) << "not countered";

    // One friendly unit plus an enemy unit is fine.
    state.chain.items.back().targets = {a, enemy};
    state.chain.items.back().chosen_objects = {a, enemy};
    EXPECT_TRUE(card->hasLegalTargets(state, P1));
}

TEST_F(RulesCards2Test, RulesCards2_Repulse_TheUnitMustBeAtABattlefield) {
    Card* card = card_registry.get(kRepulse);
    auto in_base = addUnit(P1, kInvalidId, 3);
    pushSpellOnChain(P2, kInvalidId, {in_base});

    EXPECT_FALSE(card->hasLegalTargets(state, P1)) << "\"a friendly unit AT A BATTLEFIELD\"";
}

TEST_F(RulesCards2Test, RulesCards2_Repulse_ReadsChosenObjectsAndCountersTheSpell) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    Card* card = card_registry.get(kRepulse);
    auto mine = addUnit(P1, kInvalidId, 3, /*at_bf=*/0);
    auto enemy_spell = pushSpellOnChain(P2, kInvalidId, {});
    EXPECT_FALSE(card->hasLegalTargets(state, P1));
    state.chain.items.back().chosen_objects = {mine};      // only in chosen_objects
    EXPECT_TRUE(card->hasLegalTargets(state, P1));
    auto repulse = addToHand(P1, kRepulse);
    Seen seen;

    driveSpell(kRepulse, P1, repulse, {}, pickObj(mine, &seen), exec);

    EXPECT_TRUE(seen.wasOffered(mine)) << "\"Choose a friendly unit at a battlefield\"";
    EXPECT_TRUE(state.chain.items.empty());
    EXPECT_EQ(state.getObject(enemy_spell).zone, ZoneType::Trash);
    EXPECT_TRUE(inTrash(P2, enemy_spell));
}

TEST_F(RulesCards2Test, RulesCards2_CR426_Repulse_CountersAnAbility) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    Card* card = card_registry.get(kRepulse);
    auto mine = addUnit(P1, kInvalidId, 3, /*at_bf=*/0);
    auto enemy_source = addUnit(P2, kInvalidId, 3, /*at_bf=*/0);
    ChainItem ability;
    ability.id = state.chain.allocateId();
    ability.source = enemy_source;
    ability.controller = P2;
    ability.is_ability = true;
    ability.is_activated_ability = true;
    ability.targets = {mine};
    ability.chosen_objects = {mine};
    state.chain.items.push_back(ability);
    ASSERT_TRUE(card->hasLegalTargets(state, P1)) << "\"an enemy spell OR ABILITY\"";
    auto repulse = addToHand(P1, kRepulse);

    driveSpell(kRepulse, P1, repulse, {}, pickObj(mine), exec);

    EXPECT_TRUE(state.chain.items.empty()) << "the ability is removed from the chain";
    EXPECT_TRUE(onBoard(enemy_source)) << "its source stays where it is";
    EXPECT_EQ(trashSize(P2), 0);
}

TEST_F(RulesCards2Test, RulesCards2_CR426_Repulse_UncounterableAbilityStays) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto mine = addUnit(P1, kInvalidId, 3, /*at_bf=*/0);
    auto enemy_source = addUnit(P2, kInvalidId, 3, /*at_bf=*/0);
    state.player(P2).spells_cant_be_countered = true;       // "Your spells and abilities ..."
    ChainItem ability;
    ability.id = state.chain.allocateId();
    ability.source = enemy_source;
    ability.controller = P2;
    ability.is_ability = true;
    ability.chosen_objects = {mine};
    state.chain.items.push_back(ability);
    auto repulse = addToHand(P1, kRepulse);

    driveSpell(kRepulse, P1, repulse, {}, pickObj(mine), exec);

    EXPECT_EQ(state.chain.items.size(), 1u);
}

TEST_F(RulesCards2Test, RulesCards2_CR426_NotSoFast_CountersAnAbilityChoosingFriendlyGear) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    Card* card = card_registry.get(kNotSoFast);
    ASSERT_NE(card, nullptr);
    auto my_gear = addGear(P1, kInvalidId);
    auto enemy_source = addUnit(P2, kInvalidId, 3, /*at_bf=*/0);
    ChainItem ability;
    ability.id = state.chain.allocateId();
    ability.source = enemy_source;
    ability.controller = P2;
    ability.is_ability = true;
    ability.chosen_objects = {my_gear};                     // only in chosen_objects
    state.chain.items.push_back(ability);
    ASSERT_TRUE(card->hasLegalTargets(state, P1))
        << "\"an enemy spell or ABILITY that chooses a friendly unit or GEAR\"";
    auto nsf = addToHand(P1, kNotSoFast);

    invokeOnResolve(nsf, kNotSoFast, P1, {}, exec);

    EXPECT_TRUE(state.chain.items.empty());
    EXPECT_TRUE(onBoard(enemy_source));
}

TEST_F(RulesCards2Test, RulesCards2_NotSoFast_NotWhenOnlyEnemyObjectsAreChosen) {
    Card* card = card_registry.get(kNotSoFast);
    auto theirs = addUnit(P2, kInvalidId, 3, /*at_bf=*/0);
    pushSpellOnChain(P2, kInvalidId, {});
    state.chain.items.back().chosen_objects = {theirs};

    EXPECT_FALSE(card->hasLegalTargets(state, P1));
}

}  // namespace
}  // namespace test_rules_compliance_cards2
}  // namespace riftbound::test
