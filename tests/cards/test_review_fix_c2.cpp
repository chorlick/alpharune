/// @file test_review_fix_c2.cpp
/// Regression tests for the 2026-10 engine review, card stream c2
/// (legends 930+, runes, spells up to 363): plans/cards/batch-02.md, the c2
/// cards of plans/cards/batch-08.md and of plans/03 / 07 / 08.
///
/// Every test drives the real engine: the card is played from hand (cost,
/// chain, finalization, resolution), prompts are answered by a scripted
/// EnginePathAgent, and the assertions are on game state.

#include "tests/cards/card_test_fixture.h"
#include "cards/card_helpers.h"

#include <gtest/gtest.h>

#include <boost/signals2/connection.hpp>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound::test {
namespace test_review_fix_c2 {
namespace {

constexpr CardDefId kPlayfulPhantom = 49;   // vanilla unit, [5], 5 [M]
constexpr CardDefId kMegaMech = 88;         // vanilla unit, [7], 8 [M]
constexpr CardDefId kBaronNashor = 709;     // "can't be chosen by enemy spells and abilities"

constexpr CardDefId kBlindFury = 25;
constexpr CardDefId kFallingStar = 29;
constexpr CardDefId kShakedown = 33;
constexpr CardDefId kCharm = 43;
constexpr CardDefId kDefy = 45;
constexpr CardDefId kFindYourCenter = 47;
constexpr CardDefId kMeditation = 48;
constexpr CardDefId kRunePrison = 50;
constexpr CardDefId kStandUnited = 53;
constexpr CardDefId kReinforce = 62;
constexpr CardDefId kWindWall = 64;
constexpr CardDefId kPartyFavors = 71;
constexpr CardDefId kMysticReversal = 80;
constexpr CardDefId kSpriteCall = 94;
constexpr CardDefId kPortalRescue = 102;
constexpr CardDefId kSingularity = 105;
constexpr CardDefId kPromisingFuture = 115;
constexpr CardDefId kTimeWarp = 122;
constexpr CardDefId kCannonBarrage = 127;
constexpr CardDefId kConfront = 129;
constexpr CardDefId kSpoilsOfWar = 144;
constexpr CardDefId kUnyieldingSpirit = 145;
constexpr CardDefId kOvertOperation = 153;
constexpr CardDefId kFightOrFlight = 168;
constexpr CardDefId kRideTheWind = 173;
constexpr CardDefId kStackedDeck = 183;
constexpr CardDefId kWhirlwind = 187;
constexpr CardDefId kTheHarrowing = 198;
constexpr CardDefId kBackToBack = 206;
constexpr CardDefId kCullTheWeak = 209;
constexpr CardDefId kFacebreaker = 220;
constexpr CardDefId kImperialDecree = 221;
constexpr CardDefId kSalvage = 224;
constexpr CardDefId kGrandStrategem = 233;
constexpr CardDefId kKingsEdict = 237;
constexpr CardDefId kDivineJudgment = 244;
constexpr CardDefId kIcathianRain = 247;
constexpr CardDefId kStormbringer = 248;
constexpr CardDefId kSuperMegaDeathRocket = 250;
constexpr CardDefId kNoxianGuillotine = 251;
constexpr CardDefId kFoxFire = 253;
constexpr CardDefId kDragonsRage = 255;
constexpr CardDefId kLastBreath = 257;
constexpr CardDefId kZenithBlade = 258;
constexpr CardDefId kGuerillaWarfare = 260;
constexpr CardDefId kBulletTime = 263;
constexpr CardDefId kShowstopper = 265;
constexpr CardDefId kGentlemensDuel = 306;
constexpr CardDefId kFlash = 309;
constexpr CardDefId kRecruitTheVanguard = 313;
constexpr CardDefId kDecisiveStrike = 322;
constexpr CardDefId kAgainstTheOdds = 323;
constexpr CardDefId kBushwhack = 327;
constexpr CardDefId kDetonate = 328;
constexpr CardDefId kAngleShot = 334;
constexpr CardDefId kPiercingLight = 346;
constexpr CardDefId kDesertsCall = 354;
constexpr CardDefId kThwonk = 363;
constexpr CardDefId kCuratorOfTheSands = 932;
constexpr CardDefId kDefenderOfTomorrow = 936;

// One scripted answer to a MakeChoice prompt.
struct Pick {
    enum Kind { Object, Value, Pair } kind = Object;
    GameObjectId a = kInvalidId, b = kInvalidId;
    int v = 0;
};
Pick O(GameObjectId id) { return Pick{Pick::Object, id, kInvalidId, 0}; }
Pick V(int v) { return Pick{Pick::Value, kInvalidId, kInvalidId, v}; }
Pick P(GameObjectId a, GameObjectId b) { return Pick{Pick::Pair, a, b, 0}; }

/// Answers the n-th prompt with the n-th Pick; a Pick nothing matches is a
/// test failure. Once the script runs out: first option.
EnginePathAgent::Picker script(std::vector<Pick> picks) {
    auto n = std::make_shared<size_t>(0);
    return [picks = std::move(picks), n](const std::vector<Intent>& legal) {
        if (*n >= picks.size()) return legal.front();
        const Pick p = picks[(*n)++];
        for (const auto& i : legal) {
            if (p.kind == Pick::Object && i.chosen_objects.size() == 1 &&
                i.chosen_objects.front() == p.a) return i;
            if (p.kind == Pick::Value && i.chosen_objects.empty() &&
                i.chosen_value.has_value() && *i.chosen_value == p.v) return i;
            if (p.kind == Pick::Pair && i.chosen_objects.size() == 2 &&
                ((i.chosen_objects[0] == p.a && i.chosen_objects[1] == p.b) ||
                 (i.chosen_objects[0] == p.b && i.chosen_objects[1] == p.a))) return i;
        }
        ADD_FAILURE() << "prompt " << *n << ": the scripted answer was not offered";
        return legal.front();
    };
}

class ReviewFixC2Test : public CardTestFixture {
protected:
    void fund(GameObjectId card) {
        const auto& obj = state.getObject(card);
        const auto& def = card_db.get(obj.card_def_id);
        state.player(obj.controller).rune_pool.energy += def.energy_cost;
        state.player(obj.controller).rune_pool.universal_power += def.power_cost;
    }

    /// Play `card` from hand with targets locked in as it is played.
    void playWithTargets(GameObjectId card, std::vector<GameObjectId> targets,
                         AgentInterface* p1_agent = nullptr,
                         AgentInterface* p2_agent = nullptr) {
        const PlayerId player = state.getObject(card).controller;
        ASSERT_TRUE(inHand(player, card));
        fund(card);
        Intent play;
        play.type = IntentType::PlayCard;
        play.player = player;
        play.card = card;
        play.targets = std::move(targets);
        withEngine([&](GameEngine& e) { e.testHook_executeIntent(play); },
                   p1_agent, p2_agent, player);
    }

    /// The ways the engine offers to play `card` right now (cost covered).
    std::vector<Intent> playsOf(GameObjectId card) {
        const PlayerId player = state.getObject(card).controller;
        const auto saved = state.player(player).rune_pool;
        fund(card);
        std::vector<Intent> out;
        withEngine([&](GameEngine& e) {
            for (const auto& i : e.generateLegalActions())
                if (i.card == card &&
                    (i.type == IntentType::PlayCard || i.type == IntentType::PlayActionCard ||
                     i.type == IntentType::PlayReaction))
                    out.push_back(i);
        }, nullptr, nullptr, player);
        state.player(player).rune_pool = saved;
        return out;
    }

    static bool hasTargets(const std::vector<Intent>& plays, std::vector<GameObjectId> t) {
        for (const auto& i : plays) if (i.targets == t) return true;
        return false;
    }

    GameObjectId addGear(PlayerId owner, bool exhausted = false) {
        auto id = state.createObject();
        auto& g = state.getObject(id);
        g.owner = g.controller = owner;
        g.card_type = CardType::Gear;
        g.name = "TestGear";
        g.zone = ZoneType::Base;
        g.location = BaseLocation{owner};
        g.is_exhausted = exhausted;
        return id;
    }

    GameObjectId addToTrash(PlayerId owner, CardDefId def_id) {
        auto id = addToHand(owner, def_id);
        auto& h = state.player(owner).hand;
        h.erase(std::remove(h.begin(), h.end(), id), h.end());
        state.getObject(id).zone = ZoneType::Trash;
        state.player(owner).trash.push_back(id);
        return id;
    }

    GameObjectId setLegend(PlayerId p, CardDefId def_id) {
        auto id = state.createObject();
        auto& lo = state.getObject(id);
        lo.owner = lo.controller = p;
        lo.card_type = CardType::Legend;
        lo.card_def_id = def_id;
        lo.name = card_db.get(def_id).name;
        lo.zone = ZoneType::LegendZone;
        state.player(p).legend_zone = id;
        return id;
    }

    /// A spell object on the chain that is not a registered card (a damage
    /// source for executor-level causes).
    GameObjectId looseSpell(PlayerId controller) {
        auto id = state.createObject();
        auto& s = state.getObject(id);
        s.owner = s.controller = controller;
        s.card_type = CardType::Spell;
        s.zone = ZoneType::Chain;
        s.name = "TestSpell";
        return id;
    }

    bool onBoard(GameObjectId id) { return state.getObject(id).location.has_value(); }
    bool atBf(GameObjectId id, BattlefieldId bf) {
        auto here = state.getObject(id).battlefieldId();
        return here.has_value() && *here == bf;
    }
    int count(const std::vector<GameObjectId>& v, GameObjectId id) {
        return static_cast<int>(std::count(v.begin(), v.end(), id));
    }
};

// ─── Multi-target spells (08 item 8.6) ──────────────────────────────────────

TEST_F(ReviewFixC2Test, Singularity_TwoEnemyUnitsOrOneWithNoFriendlyUnit) {
    // "Deal 6 to each of up to two units." Used to need one friendly and one
    // enemy unit.
    auto e1 = addUnit(P2, kInvalidId, 7, 0);
    auto e2 = addUnit(P2, kInvalidId, 7);
    auto spell = addToHand(P1, kSingularity);

    auto plays = playsOf(spell);
    EXPECT_TRUE(hasTargets(plays, {e1}));
    EXPECT_TRUE(hasTargets(plays, {e2}));
    EXPECT_TRUE(hasTargets(plays, {e1, e2}) || hasTargets(plays, {e2, e1}));

    playWithTargets(spell, {e1, e2});
    EXPECT_EQ(state.getObject(e1).damage_marked, 6);
    EXPECT_EQ(state.getObject(e2).damage_marked, 6);
}

TEST_F(ReviewFixC2Test, PiercingLight_FirstAtABattlefield_SecondAnywhere_OrNone) {
    // "Deal 2 to a unit at a battlefield, then deal 2 to up to one other unit."
    auto at_bf = addUnit(P2, kInvalidId, 5, 0);
    auto in_base = addUnit(P2, kInvalidId, 5);
    auto spell = addToHand(P1, kPiercingLight);

    auto plays = playsOf(spell);
    ASSERT_EQ(plays.size(), 2u);
    EXPECT_TRUE(hasTargets(plays, {at_bf})) << "the second unit is optional";
    EXPECT_TRUE(hasTargets(plays, {at_bf, in_base})) << "the second unit may be in a base";
    for (const auto& i : plays) EXPECT_EQ(i.targets.front(), at_bf) << "the first is at a battlefield";

    playWithTargets(spell, {at_bf, in_base});
    EXPECT_EQ(state.getObject(at_bf).damage_marked, 2);
    EXPECT_EQ(state.getObject(in_base).damage_marked, 2);
}

TEST_F(ReviewFixC2Test, PiercingLight_NotOfferedWithoutAUnitAtABattlefield) {
    addUnit(P2, kInvalidId, 5);   // base only
    auto spell = addToHand(P1, kPiercingLight);
    EXPECT_TRUE(playsOf(spell).empty());
}

TEST_F(ReviewFixC2Test, BackToBack_BothChosenFriendlyUnitsGetTheBonus) {
    // "Give two friendly units each +2 [M] this turn." Gave one unit +2.
    auto a = addUnit(P1, kInvalidId, 3);
    auto b = addUnit(P1, kInvalidId, 3, 0);
    auto c = addUnit(P1, kInvalidId, 3, 1);
    auto spell = addToHand(P1, kBackToBack);

    EnginePathAgent agent(script({O(a), O(c)}));
    playThroughEngine(spell, &agent);
    EXPECT_EQ(state.getObject(a).temp_might_bonus, 2);
    EXPECT_EQ(state.getObject(c).temp_might_bonus, 2);
    EXPECT_EQ(state.getObject(b).temp_might_bonus, 0);
}

TEST_F(ReviewFixC2Test, BackToBack_NeedsTwoFriendlyUnits) {
    addUnit(P1, kInvalidId, 3);
    auto spell = addToHand(P1, kBackToBack);
    EXPECT_TRUE(playsOf(spell).empty());
}

TEST_F(ReviewFixC2Test, GrandStrategemAndDecisiveStrike_BuffEveryFriendlyUnit) {
    // "Give friendly units +5 [M] this turn." / "+2 [M]". Each buffed one unit.
    auto a = addUnit(P1, kInvalidId, 3);
    auto b = addUnit(P1, kInvalidId, 3, 0);
    auto c = addUnit(P1, kInvalidId, 3, 1);
    auto enemy = addUnit(P2, kInvalidId, 3, 0);

    auto strategem = addToHand(P1, kGrandStrategem);
    EXPECT_EQ(playsOf(strategem).size(), 1u) << "nothing is targeted";
    playThroughEngine(strategem);
    for (auto u : {a, b, c}) EXPECT_EQ(state.getObject(u).temp_might_bonus, 5);
    EXPECT_EQ(state.getObject(enemy).temp_might_bonus, 0);

    auto strike = addToHand(P1, kDecisiveStrike);
    playThroughEngine(strike);
    for (auto u : {a, b, c}) EXPECT_EQ(state.getObject(u).temp_might_bonus, 7);
    EXPECT_EQ(state.getObject(enemy).temp_might_bonus, 0);
}

TEST_F(ReviewFixC2Test, AgainstTheOdds_TwoMightPerEnemyUnitThere) {
    // "+2 [M] this turn for each enemy unit there." Was a flat +2.
    auto outnumbered = addUnit(P1, kInvalidId, 3, 0);
    auto alone = addUnit(P1, kInvalidId, 3, 1);
    addUnit(P2, kInvalidId, 2, 0);
    addUnit(P2, kInvalidId, 2, 0);
    addUnit(P2, kInvalidId, 2, 0);
    addUnit(P2, kInvalidId, 2);   // in base: not "there"

    playWithTargets(addToHand(P1, kAgainstTheOdds), {outnumbered});
    EXPECT_EQ(state.getObject(outnumbered).temp_might_bonus, 6);

    playWithTargets(addToHand(P1, kAgainstTheOdds), {alone});
    EXPECT_EQ(state.getObject(alone).temp_might_bonus, 0) << "no enemy unit there";
}

TEST_F(ReviewFixC2Test, Detonate_TheGearsControllerDrawsTwo) {
    // "Kill a gear. Its controller draws 2." The draw was missing.
    auto gear = addGear(P2);
    for (int i = 0; i < 3; ++i) addToDeck(P2, kPlayfulPhantom);
    const int p1_hand_before = handSize(P1);

    playWithTargets(addToHand(P1, kDetonate), {gear});
    EXPECT_FALSE(onBoard(gear));
    EXPECT_EQ(handSize(P2), 2);
    EXPECT_EQ(handSize(P1), p1_hand_before - 0) << "the caster draws nothing";
}

TEST_F(ReviewFixC2Test, CannonBarrage_OnlyEnemyUnitsInCombat) {
    // "Deal 2 to all enemy units in combat." Hit every enemy unit on the board.
    auto fighting = addUnit(P2, kInvalidId, 5, 0);
    auto elsewhere = addUnit(P2, kInvalidId, 5, 1);
    auto in_base = addUnit(P2, kInvalidId, 5);
    auto mine = addUnit(P1, kInvalidId, 5, 0);
    state.getObject(fighting).combat_designation = CombatDesignation::Defender;
    state.getObject(mine).combat_designation = CombatDesignation::Attacker;

    playThroughEngine(addToHand(P1, kCannonBarrage));
    EXPECT_EQ(state.getObject(fighting).damage_marked, 2);
    EXPECT_EQ(state.getObject(elsewhere).damage_marked, 0);
    EXPECT_EQ(state.getObject(in_base).damage_marked, 0);
    EXPECT_EQ(state.getObject(mine).damage_marked, 0);
}

TEST_F(ReviewFixC2Test, IcathianRain_SixSeparatelyChosenUnits) {
    // "Deal 2 to a unit." x6. All six hit the one play-time target.
    auto e1 = addUnit(P2, kInvalidId, 2, 0);
    auto e2 = addUnit(P2, kInvalidId, 2, 0);
    auto e3 = addUnit(P2, kInvalidId, 2);
    auto tough = addUnit(P2, kInvalidId, 9, 1);

    EnginePathAgent agent(script({O(e1), O(e2), O(e3), O(tough), O(tough), O(tough)}));
    playThroughEngine(addToHand(P1, kIcathianRain), &agent);
    EXPECT_EQ(agent.prompts.size(), 6u);
    EXPECT_FALSE(onBoard(e1));
    EXPECT_FALSE(onBoard(e2));
    EXPECT_FALSE(onBoard(e3));
    EXPECT_EQ(state.getObject(tough).damage_marked, 6);
}

// ─── Fox-Fire ───────────────────────────────────────────────────────────────

TEST_F(ReviewFixC2Test, FoxFire_TheControllerChoosesWhichUnitsDie) {
    // "Kill any number of units at a battlefield with total Might 4 or less."
    // The greedy pick killed the caster's own 1-Might unit and spared the
    // enemy 4-Might one.
    auto my_recruit = addUnit(P1, kInvalidId, 1, 0);
    auto enemy = addUnit(P2, kInvalidId, 4, 0);

    EnginePathAgent agent(script({O(enemy)}));
    playThroughEngine(addToHand(P1, kFoxFire), &agent);
    EXPECT_FALSE(onBoard(enemy));
    EXPECT_TRUE(onBoard(my_recruit));
}

TEST_F(ReviewFixC2Test, FoxFire_AnyNumberUpToTheTotal_AndMayStopEarly) {
    auto a = addUnit(P2, kInvalidId, 2, 0);
    auto b = addUnit(P2, kInvalidId, 2, 0);
    auto c = addUnit(P2, kInvalidId, 1, 0);
    auto other_bf = addUnit(P2, kInvalidId, 1, 1);

    EnginePathAgent agent(script({O(a), O(b)}));
    playThroughEngine(addToHand(P1, kFoxFire), &agent);
    EXPECT_FALSE(onBoard(a));
    EXPECT_FALSE(onBoard(b));
    EXPECT_TRUE(onBoard(c)) << "the total was used up";
    EXPECT_FALSE(agent.wasOffered(other_bf) && agent.prompts.size() > 1 &&
                 std::any_of(agent.prompts[1].begin(), agent.prompts[1].end(),
                             [&](const Intent& i) {
                                 return !i.chosen_objects.empty() &&
                                        i.chosen_objects.front() == other_bf;
                             }))
        << "later units come from the first unit's battlefield";

    // Stop after one.
    auto d = addUnit(P2, kInvalidId, 1, 1);
    EnginePathAgent stop(script({O(other_bf), V(0)}));
    playThroughEngine(addToHand(P1, kFoxFire), &stop);
    EXPECT_FALSE(onBoard(other_bf));
    EXPECT_TRUE(onBoard(d));
}

// ─── Delayed "takes damage" abilities ───────────────────────────────────────

TEST_F(ReviewFixC2Test, ImperialDecree_KillsUnitsDamagedAfterItResolves) {
    // "When any unit takes damage this turn, kill it." Killed the units that
    // were ALREADY damaged and nothing afterwards.
    auto hurt_before = addUnit(P2, kInvalidId, 5, 0);
    auto hurt_after = addUnit(P2, kInvalidId, 5, 0);
    auto untouched = addUnit(P1, kInvalidId, 5, 0);
    state.getObject(hurt_before).damage_marked = 1;
    auto source = looseSpell(P1);

    playThroughEngine(addToHand(P1, kImperialDecree));
    EXPECT_TRUE(onBoard(hurt_before)) << "damage from before the spell does not count";
    EXPECT_TRUE(onBoard(hurt_after));

    resolveThroughEngine([&](EffectExecutor& ex, GameState&) {
        ex.dealDamage(hurt_after, 1, source);
    });
    EXPECT_FALSE(onBoard(hurt_after)) << "1 damage to a 5-Might unit kills it";
    EXPECT_TRUE(onBoard(hurt_before));
    EXPECT_TRUE(onBoard(untouched));

    // It keeps watching: a second unit damaged later the same turn dies too.
    resolveThroughEngine([&](EffectExecutor& ex, GameState&) {
        ex.dealDamage(untouched, 1, source);
    });
    EXPECT_FALSE(onBoard(untouched));
}

TEST_F(ReviewFixC2Test, ImperialDecree_KillsAUnitThatSurvivesCombatDamage) {
    // The normal use: cast it, then fight. The defender takes 3 of its 5.
    auto attacker = addUnit(P1, kInvalidId, 3, 0);
    auto defender = addUnit(P2, kInvalidId, 5, 0);
    auto& bf = state.battlefields[0];
    bf.controller = P2;
    bf.is_contested = true;
    bf.contested_by = P1;
    bf.combat_staged = true;

    playThroughEngine(addToHand(P1, kImperialDecree));
    withEngine([&](GameEngine& e) {
        e.testHook_runCombat(0);
        e.testHook_runChain();
    });
    EXPECT_FALSE(onBoard(defender)) << "3 combat damage to a 5-Might unit, then the Decree";
    EXPECT_FALSE(onBoard(attacker));
}

TEST_F(ReviewFixC2Test, NoxianGuillotine_WithoutLegion_KillsTheUnitTheNextTimeItIsDamaged) {
    // The non-Legion branch wrote a counter nothing read.
    auto marked = addUnit(P2, kInvalidId, 6, 0);
    auto other = addUnit(P2, kInvalidId, 6, 0);
    auto source = looseSpell(P1);

    playWithTargets(addToHand(P1, kNoxianGuillotine), {marked});
    EXPECT_TRUE(onBoard(marked)) << "no Legion: not killed now";

    resolveThroughEngine([&](EffectExecutor& ex, GameState&) {
        ex.dealDamage(other, 1, source);
    });
    EXPECT_TRUE(onBoard(other)) << "only the chosen unit";
    EXPECT_TRUE(onBoard(marked));

    resolveThroughEngine([&](EffectExecutor& ex, GameState&) {
        ex.dealDamage(marked, 1, source);
    });
    EXPECT_FALSE(onBoard(marked));
    EXPECT_TRUE(state.delayed_abilities.empty()) << "one shot";
}

TEST_F(ReviewFixC2Test, NoxianGuillotine_WithLegion_KillsNow) {
    auto marked = addUnit(P2, kInvalidId, 6, 0);
    state.player(P1).cards_played_this_turn = 1;   // another card this turn
    playWithTargets(addToHand(P1, kNoxianGuillotine), {marked});
    EXPECT_FALSE(onBoard(marked));
}

// ─── Whole sentences that were missing ──────────────────────────────────────

TEST_F(ReviewFixC2Test, LastBreath_TheReadiedUnitDealsItsMightToAnEnemyUnitAtABattlefield) {
    // Only the "ready" half existed.
    auto mine = addUnit(P1, kInvalidId, 5);
    state.getObject(mine).is_exhausted = true;
    auto enemy = addUnit(P2, kInvalidId, 8, 0);
    auto enemy_in_base = addUnit(P2, kInvalidId, 8);

    EnginePathAgent agent(script({O(mine), O(enemy)}));
    playThroughEngine(addToHand(P1, kLastBreath), &agent);
    EXPECT_FALSE(state.getObject(mine).is_exhausted);
    EXPECT_EQ(state.getObject(enemy).damage_marked, 5);
    EXPECT_FALSE(agent.wasOffered(enemy_in_base)) << "an enemy unit AT A BATTLEFIELD";
}

TEST_F(ReviewFixC2Test, ConfrontAndBushwhack_UnitsPlayedLaterThisTurnEnterReady) {
    // "Units you play this turn enter ready. Draw 1." — only the draw existed.
    addToDeck(P1, kPlayfulPhantom);
    playThroughEngine(addToHand(P1, kConfront));
    EXPECT_EQ(handSize(P1), 1) << "draw 1";
    auto unit = state.player(P1).hand.front();
    playThroughEngine(unit);
    ASSERT_TRUE(onBoard(unit));
    EXPECT_FALSE(state.getObject(unit).is_exhausted);

    state.player(P1).resetTurnTracking();   // next turn
    auto later = addToHand(P1, kPlayfulPhantom);
    playThroughEngine(later);
    EXPECT_TRUE(state.getObject(later).is_exhausted) << "the effect lasts one turn";
}

TEST_F(ReviewFixC2Test, Bushwhack_DoesNotReadyUnitsAlreadyOnTheBoard) {
    // "Friendly units enter ready this turn. Play a Gold gear token exhausted."
    // Readied the whole board instead.
    auto veteran = addUnit(P1, kInvalidId, 3);
    state.getObject(veteran).is_exhausted = true;

    playThroughEngine(addToHand(P1, kBushwhack));
    EXPECT_TRUE(state.getObject(veteran).is_exhausted);
    int gold = 0;
    for (auto& [id, obj] : state.objects)
        if (obj.isGear() && obj.name == "Gold") { ++gold; EXPECT_TRUE(obj.is_exhausted); }
    EXPECT_EQ(gold, 1);

    auto unit = addToHand(P1, kPlayfulPhantom);
    playThroughEngine(unit);
    EXPECT_FALSE(state.getObject(unit).is_exhausted);
    // A unit token played this turn enters ready too.
    GameObjectId token = kInvalidId;
    resolveThroughEngine([&](EffectExecutor& ex, GameState&) {
        token = ex.createToken(P1, CardType::Unit, "Recruit", 1, {"Recruit"}, KeywordSet{},
                               LocationId{BaseLocation{P1}}, false);
    });
    EXPECT_FALSE(state.getObject(token).is_exhausted);
}

TEST_F(ReviewFixC2Test, StandUnited_BuffsAreWorthOneMoreThisTurn) {
    // "Buff a friendly unit. Buffs give an additional +1 [M] to friendly
    //  units this turn." The second sentence was missing.
    auto target = addUnit(P1, kInvalidId, 3);
    auto later = addUnit(P1, kInvalidId, 3);
    auto enemy = addUnit(P2, kInvalidId, 3);
    state.getObject(enemy).buff_count = 1;

    playWithTargets(addToHand(P1, kStandUnited), {target});
    withEngine([&](GameEngine& e) { e.testHook_cleanup(); });
    EXPECT_EQ(state.getObject(target).buff_count, 1);
    EXPECT_EQ(state.getObject(target).current_might, 5) << "3 + buff + 1";

    resolveThroughEngine([&](EffectExecutor& ex, GameState&) { ex.buffUnit(later); });
    withEngine([&](GameEngine& e) { e.testHook_cleanup(); });
    EXPECT_EQ(state.getObject(later).current_might, 5) << "a buff placed later this turn too";
    EXPECT_EQ(state.getObject(enemy).current_might, 4) << "friendly units only";

    state.player(P1).resetTurnTracking();
    withEngine([&](GameEngine& e) { e.testHook_cleanup(); });
    EXPECT_EQ(state.getObject(target).current_might, 4) << "this turn only";
}

TEST_F(ReviewFixC2Test, SpoilsOfWar_DiscountOnlyWhenAnEnemyUnitDiedThisTurn) {
    // Any unit card in the opponent's trash, from any turn, gave the discount.
    const Card* spoils = card_registry.get(kSpoilsOfWar);
    addToTrash(P2, kPlayfulPhantom);   // died on an earlier turn (or was discarded)
    EXPECT_EQ(spoils->selfCostReduction(state, P1), 0);

    auto mine = addUnit(P1, kInvalidId, 2);
    resolveThroughEngine([&](EffectExecutor& ex, GameState&) { ex.killObject(mine); });
    EXPECT_EQ(spoils->selfCostReduction(state, P1), 0) << "a friendly death does not count";

    auto enemy_token = addUnit(P2, kInvalidId, 2);
    state.getObject(enemy_token).is_token = true;
    resolveThroughEngine([&](EffectExecutor& ex, GameState&) { ex.killObject(enemy_token); });
    EXPECT_EQ(spoils->selfCostReduction(state, P1), 2) << "an enemy token that died counts";

    state.player(P2).resetTurnTracking();
    EXPECT_EQ(spoils->selfCostReduction(state, P1), 0) << "this turn only";
}

TEST_F(ReviewFixC2Test, GuerillaWarfare_ChoosesTheCardsAndHidingIsFreeThisTurn) {
    // "Return up to two cards with [Hidden] from your trash to your hand. You
    //  can hide cards ignoring costs this turn."
    auto h1 = addToTrash(P1, kFoxFire);
    auto h2 = addToTrash(P1, kStandUnited);
    auto h3 = addToTrash(P1, kSpriteCall);
    auto plain = addToTrash(P1, kConfront);
    // Control that survives a cleanup: a unit of P1's at the battlefield.
    addUnit(P1, kInvalidId, 2, 0);
    state.battlefields[0].controller = P1;

    EnginePathAgent agent(script({O(h1), O(h3)}));
    playThroughEngine(addToHand(P1, kGuerillaWarfare), &agent);
    EXPECT_TRUE(inHand(P1, h1));
    EXPECT_TRUE(inHand(P1, h3));
    EXPECT_TRUE(inTrash(P1, h2)) << "the controller chose the other two";
    EXPECT_FALSE(inTrash(P1, h1));
    EXPECT_FALSE(agent.wasOffered(plain));

    // No rune and no Power: a card can still be hidden this turn.
    state.player(P1).rune_pool = RunePool{};
    bool hide_offered = false;
    withEngine([&](GameEngine& e) {
        for (const auto& i : e.generateLegalActions())
            if (i.type == IntentType::HideCard && i.card == h1) hide_offered = true;
    });
    EXPECT_TRUE(hide_offered);
    Intent hide;
    hide.type = IntentType::HideCard;
    hide.player = P1;
    hide.card = h1;
    hide.chosen_battlefield = 0;
    withEngine([&](GameEngine& e) { e.testHook_executeIntent(hide); });
    EXPECT_TRUE(state.getObject(h1).is_hidden);

    state.player(P1).resetTurnTracking();
    hide_offered = false;
    withEngine([&](GameEngine& e) {
        for (const auto& i : e.generateLegalActions())
            if (i.type == IntentType::HideCard) hide_offered = true;
    });
    EXPECT_FALSE(hide_offered) << "next turn the Hide cost is back";
}

TEST_F(ReviewFixC2Test, GuerillaWarfare_UpToTwo_MayStopAfterOne) {
    auto h1 = addToTrash(P1, kFoxFire);
    auto h2 = addToTrash(P1, kStandUnited);
    EnginePathAgent agent(script({O(h2), V(0)}));
    playThroughEngine(addToHand(P1, kGuerillaWarfare), &agent);
    EXPECT_TRUE(inHand(P1, h2));
    EXPECT_TRUE(inTrash(P1, h1));
}

TEST_F(ReviewFixC2Test, SuperMegaDeathRocket_ReturnsFromTheTrashWhenYouConquer) {
    // "When you conquer, you may discard 1 to return this from your trash to
    //  your hand." Was not implemented.
    auto rocket = addToTrash(P1, kSuperMegaDeathRocket);
    auto fodder = addToHand(P1, kPlayfulPhantom);
    addUnit(P1, kInvalidId, 3, 0);

    EnginePathAgent decline(script({V(0)}));
    withEngine([&](GameEngine& e) {
        e.testHook_scoreConquer(P1, 0);
        e.testHook_runChain();
    }, &decline);
    ASSERT_EQ(decline.prompts.size(), 1u) << "the may was offered";
    EXPECT_TRUE(inTrash(P1, rocket));
    EXPECT_TRUE(inHand(P1, fodder));

    EnginePathAgent accept(script({V(1), O(fodder)}));
    withEngine([&](GameEngine& e) {
        e.testHook_scoreConquer(P1, 1);
        e.testHook_runChain();
    }, &accept);
    EXPECT_TRUE(inHand(P1, rocket));
    EXPECT_FALSE(inTrash(P1, rocket));
    EXPECT_TRUE(inTrash(P1, fodder)) << "discarded as the cost";
}

TEST_F(ReviewFixC2Test, SuperMegaDeathRocket_NotOnAHoldOrAnOpponentsConquer) {
    auto rocket = addToTrash(P1, kSuperMegaDeathRocket);
    addToHand(P1, kPlayfulPhantom);
    EnginePathAgent agent;
    withEngine([&](GameEngine& e) {
        e.testHook_scoreHold(P1, 0);
        e.testHook_runChain();
        e.testHook_scoreConquer(P2, 1);
        e.testHook_runChain();
    }, &agent);
    EXPECT_TRUE(agent.prompts.empty());
    EXPECT_TRUE(inTrash(P1, rocket));
}

// ─── Moves with a chosen destination ────────────────────────────────────────

TEST_F(ReviewFixC2Test, RideTheWind_MovesABaseUnitToAChosenBattlefieldAndReadiesIt) {
    // "Move a friendly unit and ready it." Always moved to base.
    auto unit = addUnit(P1, kInvalidId, 3);
    state.getObject(unit).is_exhausted = true;

    EnginePathAgent agent(script({O(unit), V(2)}));   // destination: battlefield 1
    playThroughEngine(addToHand(P1, kRideTheWind), &agent);
    EXPECT_TRUE(atBf(unit, 1));
    EXPECT_FALSE(state.getObject(unit).is_exhausted);
}

TEST_F(ReviewFixC2Test, Charm_MovesAnEnemyUnitWhereTheCasterChooses) {
    // "Move an enemy unit." Always its base — a no-op for a unit already there.
    auto enemy = addUnit(P2, kInvalidId, 3);   // in its base

    EnginePathAgent agent(script({O(enemy), V(1)}));   // destination: battlefield 0
    playThroughEngine(addToHand(P1, kCharm), &agent);
    EXPECT_TRUE(atBf(enemy, 0));

    EnginePathAgent home(script({O(enemy), V(0)}));    // back to its base
    playThroughEngine(addToHand(P1, kCharm), &home);
    EXPECT_TRUE(state.getObject(enemy).isAtBase());
}

TEST_F(ReviewFixC2Test, Showstopper_OnlyABaseUnit_ToTheChosenBattlefield) {
    // "Buff a friendly unit in your base, then move it to a battlefield."
    auto in_base = addUnit(P1, kInvalidId, 3);
    auto at_bf = addUnit(P1, kInvalidId, 3, 0);
    state.battlefields[0].controller = P1;   // the old code always went here
    auto spell = addToHand(P1, kShowstopper);

    auto plays = playsOf(spell);
    ASSERT_EQ(plays.size(), 1u);
    EXPECT_TRUE(hasTargets(plays, {in_base})) << "a unit at a battlefield is not a target";
    (void)at_bf;

    EnginePathAgent agent(script({V(2)}));   // battlefield 1
    playWithTargets(spell, {in_base}, &agent);
    EXPECT_EQ(state.getObject(in_base).buff_count, 1) << "buffed once";
    EXPECT_TRUE(atBf(in_base, 1));
}

TEST_F(ReviewFixC2Test, Stormbringer_TheBattlefieldIsChosen_AndTheUnitMovesEvenWithNoEnemies) {
    // The battlefield was the first one holding an enemy unit; with none the
    // unit did not move.
    auto mine = addUnit(P1, kInvalidId, 4);
    auto e0 = addUnit(P2, kInvalidId, 6, 0);
    auto e1a = addUnit(P2, kInvalidId, 6, 1);
    auto e1b = addUnit(P2, kInvalidId, 6, 1);

    EnginePathAgent agent(script({O(mine), V(2)}));   // battlefield 1
    playThroughEngine(addToHand(P1, kStormbringer), &agent);
    EXPECT_EQ(state.getObject(e0).damage_marked, 0);
    EXPECT_EQ(state.getObject(e1a).damage_marked, 4);
    EXPECT_EQ(state.getObject(e1b).damage_marked, 4);
    EXPECT_TRUE(atBf(mine, 1));
}

TEST_F(ReviewFixC2Test, Stormbringer_MovesToAnEmptyBattlefield) {
    auto mine = addUnit(P1, kInvalidId, 4);
    EnginePathAgent agent(script({O(mine), V(1)}));
    playThroughEngine(addToHand(P1, kStormbringer), &agent);
    EXPECT_TRUE(atBf(mine, 0));
}

TEST_F(ReviewFixC2Test, DragonsRage_DestinationAndOpponentAreChosen) {
    // "Move an enemy unit. Then do this: Choose another enemy unit at its
    //  destination. They deal damage equal to their Mights to each other."
    auto mover = addUnit(P2, kInvalidId, 3, 0);
    auto small = addUnit(P2, kInvalidId, 2, 1);
    auto big = addUnit(P2, kInvalidId, 5, 1);
    auto mine_there = addUnit(P1, kInvalidId, 4, 1);

    EnginePathAgent agent(script({O(mover), V(2), O(big)}));
    playThroughEngine(addToHand(P1, kDragonsRage), &agent);
    EXPECT_FALSE(onBoard(mover)) << "took 5, had 3 Might";
    EXPECT_EQ(state.getObject(big).damage_marked, 3);
    EXPECT_EQ(state.getObject(small).damage_marked, 0);
    EXPECT_FALSE(agent.wasOffered(mine_there)) << "another ENEMY unit";
}

TEST_F(ReviewFixC2Test, DragonsRage_CanSendTheUnitToItsBaseToFightAUnitThere) {
    auto mover = addUnit(P2, kInvalidId, 3, 0);
    auto in_base = addUnit(P2, kInvalidId, 9);

    EnginePathAgent agent(script({O(mover), V(0), O(in_base)}));
    playThroughEngine(addToHand(P1, kDragonsRage), &agent);
    EXPECT_EQ(state.getObject(in_base).damage_marked, 3);
    EXPECT_FALSE(onBoard(mover));
}

TEST_F(ReviewFixC2Test, ZenithBlade_TheFriendlyUnitToMoveIsChosen) {
    // 07 item 7.24: the first friendly unit in map order was moved.
    auto first = addUnit(P1, kInvalidId, 3);
    auto second = addUnit(P1, kInvalidId, 3, 1);
    auto enemy = addUnit(P2, kInvalidId, 3, 0);

    EnginePathAgent agent(script({O(enemy), V(1), O(second)}));
    playThroughEngine(addToHand(P1, kZenithBlade), &agent);
    EXPECT_TRUE(state.getObject(enemy).is_stunned);
    EXPECT_TRUE(atBf(second, 0));
    EXPECT_TRUE(state.getObject(first).isAtBase());
}

TEST_F(ReviewFixC2Test, Flash_UpToTwo_OneUnitCanStayBehind) {
    // "Move up to 2 friendly units to base." With two available both moved.
    auto leaves = addUnit(P1, kInvalidId, 3, 0);
    auto stays = addUnit(P1, kInvalidId, 3, 1);

    EnginePathAgent agent(script({O(leaves), V(0)}));
    playThroughEngine(addToHand(P1, kFlash), &agent);
    EXPECT_TRUE(state.getObject(leaves).isAtBase());
    EXPECT_TRUE(atBf(stays, 1));
}

TEST_F(ReviewFixC2Test, Flash_StillMovesTwo) {
    auto a = addUnit(P1, kInvalidId, 3, 0);
    auto b = addUnit(P1, kInvalidId, 3, 1);
    EnginePathAgent agent(script({O(a), O(b)}));
    playThroughEngine(addToHand(P1, kFlash), &agent);
    EXPECT_TRUE(state.getObject(a).isAtBase());
    EXPECT_TRUE(state.getObject(b).isAtBase());
}

TEST_F(ReviewFixC2Test, FightOrFlight_ABaseUnitIsNotATarget) {
    // "Move a unit from a battlefield to its base."
    auto in_base = addUnit(P2, kInvalidId, 3);
    auto spell = addToHand(P1, kFightOrFlight);
    EXPECT_TRUE(playsOf(spell).empty());
    auto at_bf = addUnit(P2, kInvalidId, 3, 0);
    auto plays = playsOf(spell);
    ASSERT_EQ(plays.size(), 1u);
    EXPECT_TRUE(hasTargets(plays, {at_bf}));
    (void)in_base;
}

// ─── Stuns ──────────────────────────────────────────────────────────────────

TEST_F(ReviewFixC2Test, Thwonk_OnlyAnAttackingUnit_AndTheCasterIsTheStunner) {
    // "Stun an attacking unit." Any unit was a legal target, and the stun was
    // attributed to the victim's controller ("When you stun" never fired).
    auto attacker = addUnit(P2, kInvalidId, 3, 0);
    auto bystander = addUnit(P2, kInvalidId, 3, 1);
    auto spell = addToHand(P1, kThwonk);
    EXPECT_TRUE(playsOf(spell).empty()) << "no attacking unit";

    state.getObject(attacker).combat_designation = CombatDesignation::Attacker;
    PlayerId stunner = PlayerId::None;
    boost::signals2::scoped_connection c = events.on_unit_stunned.connect(
        [&](const UnitStunnedEvent& e) { stunner = e.stunner; });
    EnginePathAgent agent;
    playThroughEngine(spell, &agent);
    EXPECT_TRUE(state.getObject(attacker).is_stunned);
    EXPECT_FALSE(state.getObject(bystander).is_stunned);
    EXPECT_FALSE(agent.wasOffered(bystander));
    EXPECT_EQ(stunner, P1);
}

TEST_F(ReviewFixC2Test, Thwonk_APaidRepeatStunsASecondAttacker) {
    // The Repeat re-ran on the already-stunned unit and did nothing.
    auto a = addUnit(P2, kInvalidId, 3, 0);
    auto b = addUnit(P2, kInvalidId, 3, 0);
    state.getObject(a).combat_designation = CombatDesignation::Attacker;
    state.getObject(b).combat_designation = CombatDesignation::Attacker;
    state.player(P1).rune_pool.energy += 2;   // the [Repeat] [2]

    // Pay the Repeat when asked; for a unit prompt take one not picked before.
    GameObjectId last_pick = kInvalidId;
    EnginePathAgent agent([&](const std::vector<Intent>& legal) {
        for (const auto& i : legal)
            if (!i.chosen_objects.empty() && i.chosen_objects.front() != last_pick) {
                last_pick = i.chosen_objects.front();
                return i;
            }
        for (const auto& i : legal)
            if (i.chosen_value.has_value() && *i.chosen_value == 1) return i;
        return legal.front();
    });
    playThroughEngine(addToHand(P1, kThwonk), &agent);
    EXPECT_TRUE(state.getObject(a).is_stunned);
    EXPECT_TRUE(state.getObject(b).is_stunned);
}

TEST_F(ReviewFixC2Test, RunePrisonAndFacebreaker_TheCasterIsTheStunner) {
    auto enemy = addUnit(P2, kInvalidId, 3, 0);
    auto mine = addUnit(P1, kInvalidId, 3, 0);
    std::vector<std::pair<GameObjectId, PlayerId>> stuns;
    boost::signals2::scoped_connection c = events.on_unit_stunned.connect(
        [&](const UnitStunnedEvent& e) { stuns.emplace_back(e.stunned_unit, e.stunner); });

    playWithTargets(addToHand(P1, kRunePrison), {enemy});
    ASSERT_EQ(stuns.size(), 1u);
    EXPECT_EQ(stuns[0].second, P1);

    state.getObject(enemy).is_stunned = false;
    stuns.clear();
    EnginePathAgent agent(script({O(mine), O(enemy)}));
    playThroughEngine(addToHand(P1, kFacebreaker), &agent);
    bool enemy_stunned_by_caster = false;
    for (const auto& [unit, by] : stuns)
        if (unit == enemy && by == P1) enemy_stunned_by_caster = true;
    EXPECT_TRUE(enemy_stunned_by_caster);
}

// ─── Optional and opponent choices ──────────────────────────────────────────

TEST_F(ReviewFixC2Test, Salvage_CanBePlayedWithoutKillingTheOnlyGear) {
    // "You may kill up to one gear. Draw 1." With gear on the board a target
    // was mandatory.
    auto my_gear = addGear(P1);
    addToDeck(P1, kPlayfulPhantom);
    auto spell = addToHand(P1, kSalvage);

    auto plays = playsOf(spell);
    EXPECT_TRUE(hasTargets(plays, {}));
    EXPECT_TRUE(hasTargets(plays, {my_gear}));

    playWithTargets(spell, {});
    EXPECT_TRUE(onBoard(my_gear));
    EXPECT_EQ(handSize(P1), 1);
}

TEST_F(ReviewFixC2Test, OvertOperation_PerUnitChoice_AndNoDoubleDebuff) {
    // "For each friendly unit, you may spend its buff to ready it. Then buff
    //  all friendly units." One yes/no covered every unit, and spending a buff
    //  also took 1 temporary Might.
    auto tired = addUnit(P1, kInvalidId, 3);
    auto fresh = addUnit(P1, kInvalidId, 3);
    auto plain = addUnit(P1, kInvalidId, 3);
    for (auto u : {tired, fresh}) {
        state.getObject(u).buff_count = 1;
        state.getObject(u).recomputeMight();
    }
    state.getObject(tired).is_exhausted = true;
    state.getObject(fresh).is_exhausted = true;

    EnginePathAgent agent(script({V(1), V(0)}));   // tired: spend; fresh: keep
    playThroughEngine(addToHand(P1, kOvertOperation), &agent);
    EXPECT_EQ(agent.prompts.size(), 2u) << "one question per buffed unit";
    EXPECT_FALSE(state.getObject(tired).is_exhausted);
    EXPECT_TRUE(state.getObject(fresh).is_exhausted) << "kept its buff, stays exhausted";
    for (auto u : {tired, fresh, plain}) {
        EXPECT_EQ(state.getObject(u).buff_count, 1);
        EXPECT_EQ(state.getObject(u).temp_might_bonus, 0);
        EXPECT_EQ(state.getObject(u).current_might, 4);
    }
}

TEST_F(ReviewFixC2Test, Reinforce_BanishingAUnitIsOptional) {
    // "You may banish a unit from among them..." The pick was forced, and an
    // unaffordable unit ended up stuck in banishment.
    auto unit = addToDeck(P1, kMegaMech);   // [7]: costs [2] after the reduction
    addToDeck(P1, kConfront);

    EnginePathAgent agent(script({V(0)}));   // Banish none
    playThroughEngine(addToHand(P1, kReinforce), &agent);
    EXPECT_TRUE(inDeck(P1, unit));
    EXPECT_EQ(count(state.player(P1).banishment, unit), 0);
    EXPECT_EQ(deckSize(P1), 2) << "both recycled";
}

TEST_F(ReviewFixC2Test, Reinforce_PlaysTheChosenUnitForItsReducedCost) {
    auto unit = addToDeck(P1, kMegaMech);
    addToDeck(P1, kConfront);
    state.player(P1).rune_pool.energy += 2;   // [7] - [5]

    EnginePathAgent agent(script({O(unit)}));
    playThroughEngine(addToHand(P1, kReinforce), &agent);
    EXPECT_TRUE(onBoard(unit));
    EXPECT_EQ(count(state.player(P1).banishment, unit), 0) << "on the board, not also banished";
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0) << "the reduced cost was charged";
}

TEST_F(ReviewFixC2Test, KingsEdict_TheOpponentChoosesTheirUnit) {
    // The engine killed the opponent's lowest-Might unit.
    auto holder = addUnit(P2, kInvalidId, 2, 0);
    auto spare = addUnit(P2, kInvalidId, 4);
    auto mine = addUnit(P1, kInvalidId, 1);

    EnginePathAgent p2(script({O(spare)}));
    playThroughEngine(addToHand(P1, kKingsEdict), nullptr, std::nullopt, &p2);
    EXPECT_FALSE(onBoard(spare));
    EXPECT_TRUE(onBoard(holder));
    EXPECT_FALSE(p2.wasOffered(mine)) << "a unit the caster doesn't control";
}

TEST_F(ReviewFixC2Test, CullTheWeak_EachPlayerChoosesTheirOwnUnit) {
    // 07 item 7.18: the opponent's first unit in map order died.
    auto mine_a = addUnit(P1, kInvalidId, 1);
    auto mine_b = addUnit(P1, kInvalidId, 5);
    auto theirs_a = addUnit(P2, kInvalidId, 6);
    auto theirs_b = addUnit(P2, kInvalidId, 1);

    EnginePathAgent p1(script({O(mine_a)}));
    EnginePathAgent p2(script({O(theirs_b)}));
    playThroughEngine(addToHand(P1, kCullTheWeak), &p1, std::nullopt, &p2);
    EXPECT_FALSE(onBoard(mine_a));
    EXPECT_FALSE(onBoard(theirs_b));
    EXPECT_TRUE(onBoard(mine_b));
    EXPECT_TRUE(onBoard(theirs_a));
    EXPECT_FALSE(p2.wasOffered(mine_b)) << "their own units only";
}

TEST_F(ReviewFixC2Test, Shakedown_TheUnitsControllerMayHaveTheCasterDrawTwoInstead) {
    // "Deal 6 to it unless its controller has you draw 2." Always dealt 6.
    auto champion = addUnit(P2, kInvalidId, 5, 0);
    for (int i = 0; i < 4; ++i) addToDeck(P1, kPlayfulPhantom);

    EnginePathAgent pays(script({V(1)}));
    playWithTargets(addToHand(P1, kShakedown), {champion}, nullptr, &pays);
    EXPECT_EQ(state.getObject(champion).damage_marked, 0);
    EXPECT_EQ(handSize(P1), 2);

    EnginePathAgent refuses(script({V(0)}));
    playWithTargets(addToHand(P1, kShakedown), {champion}, nullptr, &refuses);
    EXPECT_EQ(state.getObject(champion).damage_marked, 6);
    EXPECT_EQ(handSize(P1), 2);
}

TEST_F(ReviewFixC2Test, Meditation_TheUnitToExhaustIsChosen) {
    // "As an additional cost to play this, you may exhaust a friendly unit. If
    //  you do, draw 2. Otherwise, draw 1." The first ready unit was exhausted.
    auto attacker = addUnit(P1, kInvalidId, 5, 0);
    auto idle = addUnit(P1, kInvalidId, 1);
    for (int i = 0; i < 4; ++i) addToDeck(P1, kPlayfulPhantom);

    EnginePathAgent agent(script({V(1), O(idle)}));
    playThroughEngine(addToHand(P1, kMeditation), &agent);
    EXPECT_TRUE(state.getObject(idle).is_exhausted);
    EXPECT_FALSE(state.getObject(attacker).is_exhausted);
    EXPECT_EQ(handSize(P1), 2);

    EnginePathAgent decline(script({V(0)}));
    playThroughEngine(addToHand(P1, kMeditation), &decline);
    EXPECT_FALSE(state.getObject(attacker).is_exhausted);
    EXPECT_EQ(handSize(P1), 3) << "otherwise, draw 1";
}

TEST_F(ReviewFixC2Test, PartyFavors_TheTwoAnswersAreDistinctValues) {
    // 07 item 7.33: both options differed only in ability_index, which the
    // action vocabulary does not encode — Runes could never be chosen.
    for (PlayerId p : {P1, P2}) {
        auto rune = addRune(p, Domain::Fury);
        auto& r = state.getObject(rune);
        r.location = std::nullopt;
        r.zone = ZoneType::RuneDeck;
        state.player(p).rune_deck.push_back(rune);
    }
    EnginePathAgent p2(script({V(1)}));   // Runes
    playThroughEngine(addToHand(P1, kPartyFavors), nullptr, std::nullopt, &p2);
    ASSERT_EQ(p2.prompts.size(), 1u);
    ASSERT_EQ(p2.prompts[0].size(), 2u);
    EXPECT_NE(p2.prompts[0][0].chosen_value, p2.prompts[0][1].chosen_value);
    EXPECT_TRUE(state.player(P1).rune_deck.empty()) << "each channelled 1 rune";
    EXPECT_TRUE(state.player(P2).rune_deck.empty());
    EXPECT_EQ(handSize(P1), 0) << "nobody drew";
}

TEST_F(ReviewFixC2Test, DivineJudgment_EachPlayerChoosesWhatToKeep) {
    // "Each player chooses 2 units, 2 gear, 2 runes, and 2 cards in their
    //  hands. Recycle the rest." Nobody chose; a recycled token entered the
    //  deck and a recycled unit kept its gear.
    auto a = addUnit(P1, kPlayfulPhantom, 5);
    auto b = addUnit(P1, kPlayfulPhantom, 5);
    auto token = addUnit(P1, kInvalidId, 1);
    state.getObject(token).is_token = true;
    auto gear = addGear(P1);
    state.getObject(gear).attached_to = a;
    state.getObject(a).attachments.push_back(gear);

    auto x = addUnit(P2, kPlayfulPhantom, 5);
    auto y = addUnit(P2, kPlayfulPhantom, 5);
    auto z_token = addUnit(P2, kInvalidId, 1);
    state.getObject(z_token).is_token = true;
    auto h1 = addToHand(P2, kConfront);
    auto h2 = addToHand(P2, kConfront);
    auto h3 = addToHand(P2, kConfront);

    EnginePathAgent p1(script({P(b, token)}));               // P1 gives up `a`
    EnginePathAgent p2(script({P(x, y), P(h1, h3)}));        // P2 gives up the token and h2
    playThroughEngine(addToHand(P1, kDivineJudgment), &p1, std::nullopt, &p2);

    EXPECT_TRUE(onBoard(b));
    EXPECT_TRUE(onBoard(token));
    EXPECT_FALSE(onBoard(a));
    EXPECT_TRUE(inDeck(P1, a));
    EXPECT_TRUE(onBoard(gear)) << "one gear: kept without asking";
    EXPECT_FALSE(state.getObject(gear).attached_to.has_value()) << "its unit left";

    EXPECT_TRUE(onBoard(x));
    EXPECT_TRUE(onBoard(y));
    EXPECT_FALSE(onBoard(z_token));
    EXPECT_FALSE(inDeck(P2, z_token)) << "a token ceases to exist (CR 183.1)";
    EXPECT_TRUE(inHand(P2, h1));
    EXPECT_TRUE(inHand(P2, h3));
    EXPECT_TRUE(inDeck(P2, h2));
}

// ─── Chain interactions ─────────────────────────────────────────────────────

TEST_F(ReviewFixC2Test, Defy_DoesNotCounterASpellThatCostsMoreThanOnePower) {
    // "...no more than [4] and no more than [A]." Only Energy was checked.
    int resolved = 0;
    GameObjectId watched = kInvalidId;
    boost::signals2::scoped_connection c = events.on_spell_resolved.connect(
        [&](const SpellResolvedEvent& e) { if (e.spell == watched) ++resolved; });

    watched = pushSpellOnChain(P2, kWindWall);   // [3] and two Power
    playThroughEngine(addToHand(P1, kDefy));
    EXPECT_EQ(resolved, 1) << "not countered: it resolved";

    resolved = 0;
    watched = pushSpellOnChain(P2, kRunePrison);   // [2] and one Power
    playThroughEngine(addToHand(P1, kDefy));
    EXPECT_EQ(resolved, 0) << "countered";
    EXPECT_TRUE(inTrash(P2, watched));
}

TEST_F(ReviewFixC2Test, MysticReversal_TheNewControllerMakesNewChoices) {
    // "Gain control of a spell. You may make new choices for it." The stolen
    // spell kept aiming at the thief's unit.
    auto mine = addUnit(P1, kInvalidId, 3, 0);
    auto theirs = addUnit(P2, kInvalidId, 3, 0);
    pushSpellOnChain(P2, kRunePrison, {mine});

    EnginePathAgent agent(script({O(theirs)}));
    playThroughEngine(addToHand(P1, kMysticReversal), &agent);
    EXPECT_TRUE(state.getObject(theirs).is_stunned);
    EXPECT_FALSE(state.getObject(mine).is_stunned);
}

TEST_F(ReviewFixC2Test, TimeWarp_IsBanishedAndNotAlsoFiledInTheTrash) {
    auto spell = addToHand(P1, kTimeWarp);
    playThroughEngine(spell);
    EXPECT_EQ(count(state.player(P1).banishment, spell), 1);
    EXPECT_EQ(count(state.player(P1).trash, spell), 0);
    EXPECT_EQ(state.getObject(spell).zone, ZoneType::Banishment);
    EXPECT_EQ(state.player(P1).additional_turns.size(), 1u);
}

// ─── Plays from other zones ─────────────────────────────────────────────────

TEST_F(ReviewFixC2Test, BlindFury_PlaysARevealedUnitAndLeavesNoCopyInBanishment) {
    auto top = addToDeck(P2, kPlayfulPhantom);
    playThroughEngine(addToHand(P1, kBlindFury));
    EXPECT_TRUE(onBoard(top));
    EXPECT_EQ(state.getObject(top).controller, P1);
    EXPECT_EQ(count(state.player(P2).banishment, top), 0);
    EXPECT_FALSE(inDeck(P2, top));
}

TEST_F(ReviewFixC2Test, BlindFury_PlaysARevealedSpell) {
    // A revealed spell was banished and never played.
    addToDeck(P1, kPlayfulPhantom);
    auto top = addToDeck(P2, kConfront);   // "Units you play this turn enter ready. Draw 1."
    playThroughEngine(addToHand(P1, kBlindFury));
    EXPECT_EQ(handSize(P1), 1) << "the caster played it: the caster drew";
    EXPECT_TRUE(state.player(P1).units_enter_ready_this_turn);
    EXPECT_EQ(count(state.player(P2).banishment, top), 0);
    EXPECT_FALSE(inDeck(P2, top));
}

TEST_F(ReviewFixC2Test, PortalRescueAndTheHarrowing_PlayFromTheirZoneOnce) {
    auto unit = addUnit(P1, kPlayfulPhantom, 5, 0);
    playWithTargets(addToHand(P1, kPortalRescue), {unit});
    EXPECT_TRUE(state.getObject(unit).isAtBase());
    EXPECT_EQ(count(state.player(P1).banishment, unit), 0);

    auto dead = addToTrash(P1, kPlayfulPhantom);
    EnginePathAgent agent(script({O(dead)}));
    playThroughEngine(addToHand(P1, kTheHarrowing), &agent);
    EXPECT_TRUE(onBoard(dead));
    EXPECT_EQ(count(state.player(P1).trash, dead), 0);
}

TEST_F(ReviewFixC2Test, PromisingFuture_BanishesThroughTheExecutor) {
    // 07 item 7.31: a direct zone write skipped the banish count and event.
    auto unit = addToDeck(P1, kPlayfulPhantom);
    int banished_events = 0;
    boost::signals2::scoped_connection c = events.on_object_state_changed.connect(
        [&](const ObjectStateChangedEvent& e) {
            if (e.what_changed == "banished_owned" && e.object == unit) ++banished_events;
        });
    playThroughEngine(addToHand(P1, kPromisingFuture));
    EXPECT_EQ(banished_events, 1);
    EXPECT_EQ(state.player(P1).cards_banished_this_turn, 1);
    EXPECT_TRUE(onBoard(unit)) << "then played, ignoring its Energy cost";
    EXPECT_EQ(count(state.player(P1).banishment, unit), 0);
}

TEST_F(ReviewFixC2Test, StackedDeck_AnnouncesTheRecycleOnce) {
    // 02 item 2.15: "When you recycle" abilities never saw Stacked Deck.
    for (int i = 0; i < 3; ++i) addToDeck(P1, kPlayfulPhantom);
    int recycled = 0;
    boost::signals2::scoped_connection c = events.on_object_state_changed.connect(
        [&](const ObjectStateChangedEvent& e) {
            if (e.what_changed == "recycled_main") ++recycled;
        });
    playThroughEngine(addToHand(P1, kStackedDeck));
    EXPECT_EQ(recycled, 1);
    EXPECT_EQ(handSize(P1), 1);
    EXPECT_EQ(deckSize(P1), 2);
}

// ─── Costs ──────────────────────────────────────────────────────────────────

TEST_F(ReviewFixC2Test, BulletTime_XCountsReadyRunesAndPaysThroughTheEngine) {
    // 07 item 7.16 / 03 item 3.24: X was capped at already-exhausted runes and
    // the hand-rolled recycle skipped the Power bookkeeping.
    auto e1 = addUnit(P2, kInvalidId, 9, 0);
    auto e2 = addUnit(P2, kInvalidId, 9, 0);
    for (int i = 0; i < 3; ++i) addRune(P1, Domain::Body);   // all ready

    // "all enemy units at a battlefield": the battlefield is a mode, no unit is chosen.
    EnginePathAgent agent(script({V(0), V(3)}));
    playThroughEngine(addToHand(P1, kBulletTime), &agent);
    EXPECT_EQ(state.getObject(e1).damage_marked, 3);
    EXPECT_EQ(state.getObject(e2).damage_marked, 3);
    EXPECT_EQ(state.player(P1).rune_deck.size(), 3u);
    EXPECT_EQ(state.player(P1).power_spent_this_turn, 3);
}

TEST_F(ReviewFixC2Test, FindYourCenter_UsesTheVictoryScoreInEffect) {
    // "within 3 points of the Victory Score": with a battlefield that raises
    // it by 1, an opponent at 5 is 4 away.
    const Card* card = card_registry.get(kFindYourCenter);
    setScore(P2, 5);
    EXPECT_EQ(card->selfCostReduction(state, P1), 2);
    state.turn.victory_score_bonus = 1;
    EXPECT_EQ(card->selfCostReduction(state, P1), 0);
    setScore(P2, 6);
    EXPECT_EQ(card->selfCostReduction(state, P1), 2);
}

TEST_F(ReviewFixC2Test, UnyieldingSpirit_PreventsAnOpponentsSpellDamageToTheirOwnUnits) {
    // "Prevent ALL spell and ability damage this turn."
    auto theirs = addUnit(P2, kInvalidId, 5, 0);
    auto their_spell = looseSpell(P2);
    playThroughEngine(addToHand(P1, kUnyieldingSpirit));
    resolveThroughEngine([&](EffectExecutor& ex, GameState&) {
        ex.dealDamage(theirs, 3, their_spell);
    });
    EXPECT_EQ(state.getObject(theirs).damage_marked, 0);
}

// ─── Tokens ─────────────────────────────────────────────────────────────────

TEST_F(ReviewFixC2Test, RecruitTheVanguard_TokensCanGoToAControlledBattlefield) {
    // "(They can be played to your base or to battlefields you control.)"
    state.battlefields[0].controller = P1;
    EnginePathAgent agent(script({V(1), V(1), V(0), V(1)}));
    playThroughEngine(addToHand(P1, kRecruitTheVanguard), &agent);
    int at_bf = 0, in_base = 0, elsewhere = 0;
    for (auto& [id, obj] : state.objects) {
        if (obj.name != "Recruit" || !obj.location.has_value()) continue;
        if (obj.isAtBase()) ++in_base;
        else if (obj.battlefieldId() == BattlefieldId{0}) ++at_bf;
        else ++elsewhere;
    }
    EXPECT_EQ(at_bf, 3);
    EXPECT_EQ(in_base, 1);
    EXPECT_EQ(elsewhere, 0);
}

TEST_F(ReviewFixC2Test, RecruitTheVanguard_NoPromptWhenOnlyTheBaseIsAvailable) {
    EnginePathAgent agent;
    playThroughEngine(addToHand(P1, kRecruitTheVanguard), &agent);
    EXPECT_TRUE(agent.prompts.empty());
    int in_base = 0;
    for (auto& [id, obj] : state.objects)
        if (obj.name == "Recruit" && obj.isAtBase()) ++in_base;
    EXPECT_EQ(in_base, 4);
}

TEST_F(ReviewFixC2Test, SpriteCallAndDesertsCall_TokenToAControlledBattlefield) {
    state.battlefields[1].controller = P1;
    EnginePathAgent sprite(script({V(2)}));
    playThroughEngine(addToHand(P1, kSpriteCall), &sprite);
    EnginePathAgent soldier(script({V(2)}));
    playThroughEngine(addToHand(P1, kDesertsCall), &soldier);
    int found = 0;
    for (auto& [id, obj] : state.objects) {
        if (obj.name != "Sprite" && obj.name != "Sand Soldier") continue;
        ++found;
        EXPECT_EQ(obj.battlefieldId(), BattlefieldId{1}) << obj.name;
    }
    EXPECT_EQ(found, 2);
}

// ─── "Can't be chosen by enemy spells and abilities" (batch-08) ─────────────

TEST_F(ReviewFixC2Test, HandBuiltUnitChoicesSkipAnUnchoosableEnemyUnit) {
    auto baron = addUnit(P2, kBaronNashor, 12, 0);
    state.getObject(baron).untargetable_by_enemy = true;
    auto plain = addUnit(P2, kInvalidId, 3, 0);
    auto mine = addUnit(P1, kInvalidId, 3, 0);
    for (int i = 0; i < 3; ++i) addToDeck(P1, kPlayfulPhantom);   // Angle Shot draws

    EnginePathAgent star;
    playThroughEngine(addToHand(P1, kFallingStar), &star);
    EXPECT_FALSE(star.wasOffered(baron)) << "Falling Star";
    EXPECT_TRUE(star.wasOffered(plain));

    EnginePathAgent shot;
    playThroughEngine(addToHand(P1, kAngleShot), &shot);
    EXPECT_FALSE(shot.wasOffered(baron)) << "Angle Shot";

    // Whirlwind: each player chooses; Baron's own controller may pick it, the
    // caster may not.
    EnginePathAgent p1, p2;
    playThroughEngine(addToHand(P1, kWhirlwind), &p1, std::nullopt, &p2);
    EXPECT_FALSE(p1.wasOffered(baron)) << "Whirlwind, the enemy player's pick";
    EXPECT_TRUE(p2.wasOffered(baron)) << "Whirlwind, its controller's pick";
    (void)mine;
}

TEST_F(ReviewFixC2Test, Whirlwind_ChoosingAnOpponentsDeflectUnitCostsPower) {
    auto deflector = addUnit(P2, kInvalidId, 3, 0);
    state.getObject(deflector).keywords.set(Keyword::Deflect);
    state.getObject(deflector).deflect_value = 1;

    // No Power to pay with: the unit is not offered to the caster.
    EnginePathAgent broke, p2a;
    playThroughEngine(addToHand(P1, kWhirlwind), &broke, std::nullopt, &p2a);
    EXPECT_FALSE(broke.wasOffered(deflector));
    EXPECT_TRUE(onBoard(deflector));

    // With one [A] floating it is offered, and choosing it spends the Power.
    state.player(P1).rune_pool.universal_power += 1;
    EnginePathAgent rich(script({O(deflector)}));
    EnginePathAgent p2b;   // declines (first option)
    playThroughEngine(addToHand(P1, kWhirlwind), &rich, std::nullopt, &p2b);
    EXPECT_FALSE(onBoard(deflector));
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 0);
}

TEST_F(ReviewFixC2Test, GentlemensDuel_GivesTheBonusOnce) {
    // batch-08 [plausible]: +3 twice when the second pick was made at
    // resolution. Fixed in the helper (07 item 7.32); pinned here.
    auto mine = addUnit(P1, kInvalidId, 2, 0);
    auto enemy = addUnit(P2, kInvalidId, 1, 0);
    EnginePathAgent agent(script({O(mine), O(enemy)}));
    playThroughEngine(addToHand(P1, kGentlemensDuel), &agent);
    EXPECT_EQ(state.getObject(mine).temp_might_bonus, 3);
    EXPECT_FALSE(onBoard(enemy));
}

// ─── Legends ────────────────────────────────────────────────────────────────

TEST_F(ReviewFixC2Test, DefenderOfTomorrow_TheGearToReadyIsChosen) {
    // "[1], [E]: Ready a gear." The first exhausted gear in map order was readied.
    auto legend = setLegend(P1, kDefenderOfTomorrow);
    auto g1 = addGear(P1, /*exhausted=*/true);
    auto g2 = addGear(P1, /*exhausted=*/true);
    auto g3 = addGear(P1, /*exhausted=*/true);
    state.player(P1).rune_pool.energy = 1;

    Intent act;
    act.type = IntentType::ActivateAbility;
    act.player = P1;
    act.ability_source = legend;
    act.ability_index = 0;
    EnginePathAgent agent(script({O(g2)}));
    withEngine([&](GameEngine& e) { e.testHook_executeIntent(act); }, &agent);
    EXPECT_FALSE(state.getObject(g2).is_exhausted);
    EXPECT_TRUE(state.getObject(g1).is_exhausted);
    EXPECT_TRUE(state.getObject(g3).is_exhausted);
    EXPECT_TRUE(state.getObject(legend).is_exhausted);

    // Empowered: two gear, both chosen.
    state.getObject(legend).is_exhausted = false;
    state.getObject(legend).empowered = 1;
    state.player(P1).rune_pool.energy = 1;
    EnginePathAgent two(script({O(g3), O(g1)}));
    withEngine([&](GameEngine& e) { e.testHook_executeIntent(act); }, &two);
    EXPECT_FALSE(state.getObject(g3).is_exhausted);
    EXPECT_FALSE(state.getObject(g1).is_exhausted);
}

TEST_F(ReviewFixC2Test, CuratorOfTheSands_TheRunesToReadyAreChosen) {
    // "...you may exhaust me to ready up to 2 runes." The first two exhausted
    // runes in map order were readied.
    auto legend = setLegend(P1, kCuratorOfTheSands);
    auto r1 = addRune(P1, Domain::Calm, /*exhausted=*/true);
    auto r2 = addRune(P1, Domain::Mind, /*exhausted=*/true);
    auto r3 = addRune(P1, Domain::Fury, /*exhausted=*/true);
    auto src = addUnit(P1, kInvalidId, 3);

    EnginePathAgent agent(script({V(1), O(r3), O(r1)}));
    resolveThroughEngine([&](EffectExecutor&, GameState& st) {
        st.getObject(src).card_counters["__ability_energy_cost"] = 7;
        events.emit(ObjectStateChangedEvent{src, "ability_activated"});
    }, &agent);
    EXPECT_TRUE(state.getObject(legend).is_exhausted);
    EXPECT_FALSE(state.getObject(r3).is_exhausted);
    EXPECT_FALSE(state.getObject(r1).is_exhausted);
    EXPECT_TRUE(state.getObject(r2).is_exhausted);
}

}  // namespace
}  // namespace test_review_fix_c2
}  // namespace riftbound::test
