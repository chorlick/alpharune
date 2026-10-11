/// @file test_review_fix_c3.cpp
/// Regression tests for the card findings of the 2026-10 engine review that
/// fall in stream c3: spells 0364..0933 (plans/cards/batch-03.md, and the
/// entries for those cards in batch-08 / batch-09 and plans 03, 07, 08).
///
/// Every test plays the card through a real GameEngine (cost, chain,
/// finalization, resolution) and asserts on the game state.

#include "tests/cards/card_test_fixture.h"
#include "cards/card_helpers.h"
#include "engine/game_engine.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <functional>
#include <vector>

namespace riftbound::test {
namespace test_review_fix_c3 {  // file-unique: test sources are unity-built
namespace {

// Units used as scenery.
constexpr CardDefId kPoutyPoro = 13;        // 2 Energy, 2 [M], Poro
constexpr CardDefId kMegaMech = 88;         // 7 Energy, 8 [M], Mech, no text
constexpr CardDefId kMountainDrake = 142;   // 9 Energy, no text
constexpr CardDefId kMysticPoro = 171;      // 2 Energy, [Deflect]
constexpr CardDefId kSkulker = 175;         // 3 Energy, 3 [M], no text
constexpr CardDefId kDragonsoulSage = 655;  // 2 Energy, no Bird/Cat/Dog/Poro tag
constexpr CardDefId kBaronNashor = 709;     // "can't be chosen by enemy spells and abilities"

// The cards under test.
constexpr CardDefId kRocketBarrage = 400;
constexpr CardDefId kBellowsBreath = 402;
constexpr CardDefId kStrikeDown = 429;
constexpr CardDefId kHereToHelp = 433;
constexpr CardDefId kTemptation = 450;
constexpr CardDefId kHardBargain = 457;
constexpr CardDefId kSwitcheroo = 466;
constexpr CardDefId kBondsOfStrength = 472;
constexpr CardDefId kGuards = 475;
constexpr CardDefId kDeathgrip = 484;
constexpr CardDefId kDragUnder = 485;
constexpr CardDefId kRallyTheTroops = 487;
constexpr CardDefId kDangerZone = 501;
constexpr CardDefId kRelentlessPursuit = 503;
constexpr CardDefId kVoidRush = 505;
constexpr CardDefId kDefiantDance = 511;
constexpr CardDefId kArise = 513;
constexpr CardDefId kArcaneShift = 515;
constexpr CardDefId kOnTheHunt = 518;
constexpr CardDefId kRiposte = 520;
constexpr CardDefId kSmite = 568;
constexpr CardDefId kLotusTrap = 575;
constexpr CardDefId kRightOfConquest = 577;
constexpr CardDefId kSquareUp = 579;
constexpr CardDefId kDancingGrenade = 582;
constexpr CardDefId kDoubleTrouble = 594;
constexpr CardDefId kSkywardStrike = 600;
constexpr CardDefId kBackOff = 604;
constexpr CardDefId kFlurryOfFeathers = 606;
constexpr CardDefId kTricksyTentacles = 616;
constexpr CardDefId kEclipse = 625;
constexpr CardDefId kTurnToDust = 632;
constexpr CardDefId kCrescentStrike = 634;
constexpr CardDefId kDeadlyFlourish = 635;
constexpr CardDefId kSmokeAndMirrors = 645;
constexpr CardDefId kCallToBattle = 663;
constexpr CardDefId kDisposalOrder = 665;
constexpr CardDefId kStareDown = 669;
constexpr CardDefId kAbandon = 693;
constexpr CardDefId kExistentialDread = 696;
constexpr CardDefId kBoneSkewer = 701;
constexpr CardDefId kConscription = 702;
constexpr CardDefId kHeedlessResurrection = 704;
constexpr CardDefId kShadowsCall = 727;
constexpr CardDefId kUndyingLoyalty = 730;
constexpr CardDefId kSacrifice = 735;
constexpr CardDefId kCurtainCall = 743;
constexpr CardDefId kThrillOfTheHunt = 745;
constexpr CardDefId kDeathFromBelow = 747;
constexpr CardDefId kLiltingLullaby = 750;
constexpr CardDefId kMoonfall = 755;
constexpr CardDefId kVoidAssault = 758;
constexpr CardDefId kKeepersVerdict = 760;
constexpr CardDefId kResonatingStrike = 821;
constexpr CardDefId kCrumblingSands = 826;
constexpr CardDefId kMesmerize = 839;
constexpr CardDefId kUpFromTheDeep = 887;
constexpr CardDefId kTwilightStep = 892;
constexpr CardDefId kDecreeOfDiscord = 894;
constexpr CardDefId kShurikenFlip = 927;
constexpr CardDefId kDeathMark = 931;
constexpr CardDefId kSiphoningStrike = 933;

/// Answers MakeChoice prompts in order from a script and passes priority.
/// O(id) picks the option that chooses object `id`; V(v) the option whose
/// chosen_value is `v` (yes/no: 1/0, a mode, a destination). A step whose
/// option is not on offer counts as `missed` and the first option is taken.
class StepAgent : public AgentInterface {
public:
    struct Step { bool object; int value; };
    std::vector<Step> steps;
    std::vector<std::vector<Intent>> prompts;
    int missed = 0;
    /// Run once, the first time this player is offered priority: the test's
    /// stand-in for "the opponent responds".
    std::function<void()> on_priority;

    StepAgent() = default;
    StepAgent(std::initializer_list<Step> s) : steps(s) {}

    Intent selectAction(const GameState&, const std::vector<Intent>& legal) override {
        if (legal.empty()) return Intent{};
        const bool is_choice = std::all_of(legal.begin(), legal.end(),
            [](const Intent& i) { return i.type == IntentType::MakeChoice; });
        if (is_choice) {
            prompts.push_back(legal);
            if (next_ < steps.size()) {
                const Step s = steps[next_++];
                for (const auto& i : legal) {
                    if (s.object) {
                        if (!i.chosen_objects.empty() &&
                            i.chosen_objects.front() == static_cast<GameObjectId>(s.value))
                            return i;
                    } else if (i.chosen_objects.empty() && i.chosen_value.has_value() &&
                               *i.chosen_value == s.value) {
                        return i;
                    }
                }
                ++missed;
            }
            return legal.front();
        }
        for (const auto& i : legal)
            if (i.type == IntentType::PassPriority || i.type == IntentType::PassFocus) {
                if (on_priority) {
                    auto f = std::move(on_priority);
                    on_priority = nullptr;
                    f();
                }
                return i;
            }
        return legal.front();
    }
    bool offered(GameObjectId id) const {
        for (const auto& p : prompts)
            for (const auto& i : p)
                if (!i.chosen_objects.empty() && i.chosen_objects.front() == id) return true;
        return false;
    }

private:
    size_t next_ = 0;
};
StepAgent::Step O(GameObjectId id) { return {true, static_cast<int>(id)}; }
StepAgent::Step V(int v) { return {false, v}; }

class C3Test : public CardTestFixture {
protected:
    GameObjectId addGear(PlayerId owner, bool equipment) {
        auto id = state.createObject();
        auto& g = state.getObject(id);
        g.owner = g.controller = owner;
        g.card_type = CardType::Gear;
        g.name = equipment ? "TestEquipment" : "TestGear";
        if (equipment) g.tags = {"Equipment"};
        g.zone = ZoneType::Base;
        g.location = BaseLocation{owner};
        return id;
    }
    void attach(GameObjectId gear, GameObjectId unit) {
        auto& g = state.getObject(gear);
        auto& u = state.getObject(unit);
        g.attached_to = unit;
        g.location = u.location;
        g.zone = u.zone;
        u.attachments.push_back(gear);
    }
    void control(PlayerId p, int bf) { state.battlefields[bf].controller = p; }
    GameObjectId addToTrash(PlayerId owner, CardDefId def) {
        auto id = addToHand(owner, def);
        auto& h = state.player(owner).hand;
        h.erase(std::remove(h.begin(), h.end(), id), h.end());
        state.getObject(id).zone = ZoneType::Trash;
        state.player(owner).trash.push_back(id);
        return id;
    }
    /// Take a resolved spell back out of its owner's trash into their hand.
    void backToHand(GameObjectId card) {
        auto& obj = state.getObject(card);
        auto& ps = state.player(obj.owner);
        ps.trash.erase(std::remove(ps.trash.begin(), ps.trash.end(), card), ps.trash.end());
        obj.zone = ZoneType::Hand;
        obj.controller = obj.owner;
        ps.hand.push_back(card);
    }
    bool inBanishment(PlayerId p, GameObjectId id) {
        auto& b = state.player(p).banishment;
        return std::find(b.begin(), b.end(), id) != b.end();
    }
    std::optional<BattlefieldId> bfOf(GameObjectId id) { return state.getObject(id).battlefieldId(); }
    bool onBoard(GameObjectId id) { return state.getObject(id).location.has_value(); }

    /// Play `card` from hand with the targets locked into the play intent
    /// (spells that do not defer their targets to pickTarget). The cost is
    /// covered with floating Energy / [A].
    void playAt(GameObjectId card, std::vector<GameObjectId> targets,
                AgentInterface* p1 = nullptr, AgentInterface* p2 = nullptr) {
        const PlayerId player = state.getObject(card).controller;
        const auto& def = card_db.get(state.getObject(card).card_def_id);
        state.player(player).rune_pool.energy += def.energy_cost;
        state.player(player).rune_pool.universal_power += def.power_cost;
        Intent play;
        play.type = IntentType::PlayCard;
        play.player = player;
        play.card = card;
        play.targets = std::move(targets);
        withEngine([&](GameEngine& e) { e.testHook_executeIntent(play); settleLikeTheEngine(e); }, p1, p2, player);
    }
    /// Every way the engine offers `card` to its controller right now, with
    /// the cost covered.
    std::vector<Intent> offers(GameObjectId card) {
        const PlayerId player = state.getObject(card).controller;
        const auto& def = card_db.get(state.getObject(card).card_def_id);
        const auto pool = state.player(player).rune_pool;
        state.player(player).rune_pool.energy += def.energy_cost;
        state.player(player).rune_pool.universal_power += def.power_cost;
        std::vector<Intent> out;
        withEngine([&](GameEngine& e) {
            for (const auto& i : e.generateLegalActions())
                if (i.card == card && (i.type == IntentType::PlayCard ||
                                       i.type == IntentType::PlayActionCard ||
                                       i.type == IntentType::PlayReaction))
                    out.push_back(i);
        }, nullptr, nullptr, player);
        state.player(player).rune_pool = pool;
        return out;
    }
    /// A fake spell of `controller` on the chain (resolving it does nothing).
    GameObjectId enemySpellOnChain(PlayerId controller) {
        auto id = pushSpellOnChain(controller, kDragUnder);   // 5 Energy; no target, no effect
        state.getObject(id).name = "FakeSpell";
        return id;
    }
    bool hasLegalTargets(CardDefId def, PlayerId p) {
        return card_registry.get(def)->hasLegalTargets(state, p);
    }
};

// ═══ batch-03 ═══════════════════════════════════════════════════════════════

TEST_F(C3Test, RocketBarrage_ChoosesTheModeThenATargetThatFitsIt) {
    // "Choose one — Deal 4 to a unit in a base. / Kill a gear." Any on-board
    // object used to be a legal play target (a unit at a battlefield, a rune),
    // and one that fit neither mode made the spell do nothing.
    auto spell = addToHand(P1, kRocketBarrage);
    auto at_bf = addUnit(P2, kInvalidId, 5, 0);
    auto rune = addRune(P2, Domain::Fury);
    auto gear = addGear(P2, false);

    StepAgent agent{V(1), O(gear)};
    playThroughEngine(spell, &agent);
    EXPECT_EQ(agent.missed, 0);
    EXPECT_FALSE(onBoard(gear)) << "the gear is killed";
    EXPECT_FALSE(agent.offered(at_bf)) << "a unit at a battlefield fits neither mode";
    EXPECT_FALSE(agent.offered(rune));
    ASSERT_GE(agent.prompts.size(), 1u);
    EXPECT_EQ(agent.prompts[0].size(), 1u) << "no unit in a base: only 'Kill a gear' is a mode";
}

TEST_F(C3Test, RocketBarrage_IsNotPlayableWithNothingEitherModeCanAffect) {
    auto spell = addToHand(P1, kRocketBarrage);
    addUnit(P2, kInvalidId, 5, 0);   // at a battlefield only
    EXPECT_TRUE(offers(spell).empty());
    addUnit(P2, kInvalidId, 5);      // one in a base
    EXPECT_EQ(offers(spell).size(), 1u);
}

TEST_F(C3Test, BellowsBreath_TargetSetsAreUnitsAtOneLocation) {
    // "Deal 1 to up to three units at the same location."
    auto spell = addToHand(P1, kBellowsBreath);
    auto a = addUnit(P2, kInvalidId, 3, 0);
    auto b = addUnit(P2, kInvalidId, 3, 0);
    auto c = addUnit(P2, kInvalidId, 3, 1);
    bool pair_ab = false;
    for (const auto& play : offers(spell)) {
        if (play.targets.empty()) continue;   // "up to three": none may be chosen (CR 355.13)
        const auto loc = state.getObject(play.targets[0]).location;
        for (auto t : play.targets)
            EXPECT_EQ(state.getObject(t).location, loc) << "targets at different locations";
        if (play.targets.size() == 2 &&
            std::count(play.targets.begin(), play.targets.end(), a) &&
            std::count(play.targets.begin(), play.targets.end(), b)) pair_ab = true;
    }
    EXPECT_TRUE(pair_ab);

    playAt(spell, {a, b});
    EXPECT_EQ(state.getObject(a).damage_marked, 1);
    EXPECT_EQ(state.getObject(b).damage_marked, 1);
    EXPECT_EQ(state.getObject(c).damage_marked, 0);
}

TEST_F(C3Test, Deathgrip_IsOfferedWithTwoFriendlyUnitsAndPumpsTheSecond) {
    // "Kill a friendly unit. If you do, give +[M] equal to its Might to
    // another friendly unit this turn. Draw 1." It was never offered: the
    // generator paired a friendly target with an enemy one.
    auto spell = addToHand(P1, kDeathgrip);
    auto kill = addUnit(P1, kInvalidId, 4);
    auto pump = addUnit(P1, kInvalidId, 2);
    addToDeck(P1, kInvalidId);
    const auto plays = offers(spell);
    EXPECT_EQ(plays.size(), 2u) << "{kill, pump} either way round";

    playAt(spell, {kill, pump});
    EXPECT_FALSE(onBoard(kill));
    EXPECT_EQ(state.getObject(pump).current_might, 6);
    EXPECT_EQ(handSize(P1), 1) << "Draw 1";
}

TEST_F(C3Test, Deathgrip_IsPlayableOnASingleFriendlyUnit) {
    auto spell = addToHand(P1, kDeathgrip);
    auto only = addUnit(P1, kInvalidId, 4);
    addToDeck(P1, kInvalidId);
    const auto plays = offers(spell);
    ASSERT_EQ(plays.size(), 1u);
    EXPECT_EQ(plays[0].targets, std::vector<GameObjectId>{only});

    playAt(spell, {only});
    EXPECT_FALSE(onBoard(only));
    EXPECT_EQ(handSize(P1), 1);
}

TEST_F(C3Test, SmokeAndMirrors_IsOfferedForUnitsAtDifferentLocationsOnly) {
    auto spell = addToHand(P1, kSmokeAndMirrors);
    auto at_base = addUnit(P1, kInvalidId, 2);
    auto also_base = addUnit(P1, kInvalidId, 2);
    addToDeck(P1, kInvalidId);
    EXPECT_TRUE(offers(spell).empty()) << "both units share a location";
    auto at_bf = addUnit(P1, kInvalidId, 2, 0);
    state.getObject(at_bf).keywords.set(Keyword::Temporary);
    for (const auto& play : offers(spell)) {
        ASSERT_EQ(play.targets.size(), 2u);
        EXPECT_NE(state.getObject(play.targets[0]).location,
                  state.getObject(play.targets[1]).location);
    }
    EXPECT_EQ(offers(spell).size(), 4u);

    playAt(spell, {at_base, at_bf});
    EXPECT_TRUE(state.getObject(at_bf).isAtBase());
    ASSERT_TRUE(bfOf(at_base).has_value());
    EXPECT_EQ(*bfOf(at_base), 0u);
    EXPECT_TRUE(state.getObject(also_base).isAtBase());
    EXPECT_EQ(handSize(P1), 1) << "Draw 1";
}

TEST_F(C3Test, DangerZone_GivesEveryMechAndOnlyMechsPlusOne) {
    // "Give your Mechs +1 [M] this turn." It took one friendly-unit target.
    auto spell = addToHand(P1, kDangerZone);
    auto mech1 = addUnit(P1, kMegaMech);
    auto mech2 = addUnit(P1, kMegaMech, 0, 0);
    auto other = addUnit(P1, kSkulker);
    auto theirs = addUnit(P2, kMegaMech);
    StepAgent agent;
    playThroughEngine(spell, &agent);
    EXPECT_TRUE(agent.prompts.empty()) << "nothing is chosen";
    EXPECT_EQ(state.getObject(mech1).current_might, 9);
    EXPECT_EQ(state.getObject(mech2).current_might, 9);
    EXPECT_EQ(state.getObject(other).current_might, 3);
    EXPECT_EQ(state.getObject(theirs).current_might, 8);
}

TEST_F(C3Test, DangerZone_IsPlayableWithNoUnits) {
    auto spell = addToHand(P1, kDangerZone);
    EXPECT_EQ(offers(spell).size(), 1u);
}

TEST_F(C3Test, OnTheHunt_ReadiesAllYourUnits) {
    auto spell = addToHand(P1, kOnTheHunt);
    std::vector<GameObjectId> mine = {addUnit(P1, kInvalidId, 2), addUnit(P1, kInvalidId, 2, 0),
                                      addUnit(P1, kInvalidId, 2, 1)};
    auto theirs = addUnit(P2, kInvalidId, 2);
    for (auto id : mine) state.getObject(id).is_exhausted = true;
    state.getObject(theirs).is_exhausted = true;
    playThroughEngine(spell);
    for (auto id : mine) EXPECT_FALSE(state.getObject(id).is_exhausted);
    EXPECT_TRUE(state.getObject(theirs).is_exhausted);
}

TEST_F(C3Test, RightOfConquest_DrawsOnePlusOnePerControlledBattlefield) {
    for (int i = 0; i < 6; ++i) addToDeck(P1, kInvalidId);
    auto none = addToHand(P1, kRightOfConquest);
    playThroughEngine(none);
    EXPECT_EQ(handSize(P1), 1) << "no battlefield controlled: Draw 1";
    control(P1, 0);
    control(P1, 1);
    auto two = addToHand(P1, kRightOfConquest);
    playThroughEngine(two);
    EXPECT_EQ(handSize(P1), 4) << "1 + 2 more";
}

TEST_F(C3Test, CrescentStrike_HitsTheOtherEnemyUnitsThereAndNeedsABattlefield) {
    auto spell = addToHand(P1, kCrescentStrike);
    auto main = addUnit(P2, kInvalidId, 6, 0);
    auto beside = addUnit(P2, kInvalidId, 6, 0);
    auto elsewhere = addUnit(P2, kInvalidId, 6, 1);
    auto in_base = addUnit(P2, kInvalidId, 6);
    auto mine = addUnit(P1, kInvalidId, 6, 0);
    for (const auto& play : offers(spell))
        EXPECT_NE(play.targets, std::vector<GameObjectId>{in_base}) << "a unit in base is not 'there'";
    playAt(spell, {main});
    EXPECT_EQ(state.getObject(main).damage_marked, 4);
    EXPECT_EQ(state.getObject(beside).damage_marked, 1);
    EXPECT_EQ(state.getObject(elsewhere).damage_marked, 0);
    EXPECT_EQ(state.getObject(mine).damage_marked, 0);
}

TEST_F(C3Test, TricksyTentacles_MovesSeveralUnitsWithinTheMightBudgetToOneLocation) {
    // "Move any number of enemy units with the same controller and a total
    // Might of 8 or less to a single location." It moved one unit, to base.
    auto spell = addToHand(P1, kTricksyTentacles);
    auto a = addUnit(P2, kInvalidId, 3, 0);
    auto b = addUnit(P2, kInvalidId, 3);
    auto big = addUnit(P2, kInvalidId, 6, 0);
    // a, then b (2 Might left: nothing else fits, so no third prompt), then
    // battlefield #1 (destination 2).
    StepAgent agent{O(a), O(b), V(2)};
    playThroughEngine(spell, &agent);
    EXPECT_EQ(agent.missed, 0);
    ASSERT_TRUE(bfOf(a).has_value());
    ASSERT_TRUE(bfOf(b).has_value());
    EXPECT_EQ(*bfOf(a), 1u);
    EXPECT_EQ(*bfOf(b), 1u);
    EXPECT_EQ(*bfOf(big), 0u) << "6 Might did not fit after the first pick";
    ASSERT_GE(agent.prompts.size(), 2u);
    for (const auto& i : agent.prompts[1])
        EXPECT_TRUE(i.chosen_objects.empty() || i.chosen_objects.front() != big);
}

// Corrected (sweep-costs, 8.9): the Repeat is now offered — its cost is the
// card's declaration (Card::repeatCosts) — so there is one prompt, and
// declining it keeps the hand.
// Was: SquareUp_DoesNotForceADiscard.
TEST_F(C3Test, SquareUp_TheRepeatOfferCanBeDeclined) {
    // "[Repeat] — Discard 1 ... Give a unit [Assault 4] this turn." The
    // discard is the Repeat's cost; the base effect used to demand it.
    auto spell = addToHand(P1, kSquareUp);
    auto keep = addToHand(P1, kSkulker);
    auto unit = addUnit(P1, kInvalidId, 2, 0);
    StepAgent agent{V(0)};   // decline the Repeat
    playAt(spell, {unit}, &agent);
    EXPECT_EQ(agent.missed, 0);
    EXPECT_EQ(agent.prompts.size(), 1u) << "only the Repeat offer";
    EXPECT_TRUE(inHand(P1, keep));
    EXPECT_TRUE(state.getObject(unit).hasKeyword(Keyword::Assault));
    EXPECT_EQ(state.getObject(unit).assault_value, 4);
}

TEST_F(C3Test, SquareUp_RepeatIsPaidWithADiscard) {
    auto spell = addToHand(P1, kSquareUp);
    auto fodder = addToHand(P1, kSkulker);
    auto unit = addUnit(P1, kInvalidId, 2, 0);
    StepAgent agent{V(1)};   // pay the Repeat
    playAt(spell, {unit}, &agent);
    EXPECT_EQ(agent.missed, 0);
    EXPECT_FALSE(inHand(P1, fodder)) << "the discard is the Repeat's cost";
    EXPECT_EQ(state.getObject(unit).assault_value, 8) << "the effect ran twice";

    // With nothing to discard the Repeat is not offered.
    auto again = addToHand(P1, kSquareUp);
    auto other = addUnit(P1, kInvalidId, 2, 0);
    StepAgent none;
    playAt(again, {other}, &none);
    EXPECT_TRUE(none.prompts.empty());
    EXPECT_EQ(state.getObject(other).assault_value, 4);
}

TEST_F(C3Test, CurtainCall_TargetFitsTheModeAndALaterCastStartsFresh) {
    // The "already chosen" mask lived on the card object for ever, and any
    // unit was a legal play target whatever the mode.
    auto spell = addToHand(P1, kCurtainCall);
    auto at_bf = addUnit(P2, kInvalidId, 6, 0);
    auto at_base = addUnit(P2, kInvalidId, 6);
    StepAgent first{V(2), O(at_base)};   // "Deal 3 to a unit at a base"
    playThroughEngine(spell, &first);
    EXPECT_EQ(first.missed, 0);
    EXPECT_EQ(state.getObject(at_base).damage_marked, 3);
    EXPECT_FALSE(first.offered(at_bf)) << "a unit at a battlefield is not a target of that mode";

    backToHand(spell);
    StepAgent second{V(2), O(at_base)};  // the same mode again, on a new play
    playThroughEngine(spell, &second);
    EXPECT_EQ(second.missed, 0);
    // 3 + 3 on a 6-Might unit is lethal: the cleanup after the play kills it
    // (playThroughEngine runs that cleanup, as a game does).
    EXPECT_TRUE(inTrash(P2, at_base)) << "the second 3 damage added to the first";
}

TEST_F(C3Test, Eclipse_Predicts) {
    auto spell = addToHand(P1, kEclipse);
    auto unit = addUnit(P2, kInvalidId, 6, 0);
    addToDeck(P1, kInvalidId);
    addToDeck(P1, kInvalidId);
    StepAgent agent;
    playAt(spell, {unit}, &agent);
    EXPECT_EQ(state.getObject(unit).current_might, 2);
    EXPECT_GE(agent.prompts.size(), 1u) << "[Predict] asks whether to recycle the top card";
}

TEST_F(C3Test, RallyTheTroops_BuffsEveryFriendlyUnitPlayedThisTurn) {
    auto spell = addToHand(P1, kRallyTheTroops);
    addToDeck(P1, kInvalidId);
    playThroughEngine(spell);
    EXPECT_EQ(handSize(P1), 1) << "Draw 1";

    auto first = addToHand(P1, kSkulker);
    auto second = addToHand(P1, kSkulker);
    playThroughEngine(first);
    playThroughEngine(second);
    EXPECT_EQ(state.getObject(first).buff_count, 1);
    EXPECT_EQ(state.getObject(second).buff_count, 1);

    // It is gone with the turn.
    ASSERT_EQ(state.delayed_abilities.size(), 1u);
    EXPECT_EQ(state.delayed_abilities[0].expires_on_turn, state.turn.turn_number);
}

TEST_F(C3Test, RallyTheTroops_DoesNotBuffAnOpponentsUnit) {
    auto spell = addToHand(P1, kRallyTheTroops);
    playThroughEngine(spell);
    auto theirs = addToHand(P2, kSkulker);
    playThroughEngine(theirs);
    // 9.B7: nothing showed the opponent's unit was played at all (a play that
    // never happened leaves buff_count at 0 too), nor that the rider was live.
    EXPECT_FALSE(inHand(P2, theirs));
    ASSERT_TRUE(onBoard(theirs)) << "the opponent's unit reached the board";
    EXPECT_EQ(state.getObject(theirs).controller, P2);
    EXPECT_EQ(state.getObject(theirs).buff_count, 0);
    auto mine = addToHand(P1, kSkulker);
    playThroughEngine(mine);
    EXPECT_EQ(state.getObject(mine).buff_count, 1) << "the rider is live: a friendly unit is buffed";
    EXPECT_EQ(state.getObject(theirs).buff_count, 0);
}

TEST_F(C3Test, RelentlessPursuit_MovesALoneUnitWhereItsControllerChooses) {
    // The destination was "the first battlefield where you have ANOTHER
    // unit": a lone unit in base did not move at all.
    auto spell = addToHand(P1, kRelentlessPursuit);
    auto unit = addUnit(P1, kInvalidId, 3);
    auto sword = addGear(P1, true);
    auto shield = addGear(P1, true);
    // Battlefield #1 (destination 2), attach: yes, the second Equipment.
    StepAgent agent{V(2), V(1), O(shield)};
    playAt(spell, {unit}, &agent);
    EXPECT_EQ(agent.missed, 0);
    ASSERT_TRUE(bfOf(unit).has_value());
    EXPECT_EQ(*bfOf(unit), 1u);
    EXPECT_EQ(state.getObject(shield).attached_to, std::optional<GameObjectId>(unit));
    EXPECT_FALSE(state.getObject(sword).attached_to.has_value());
}

TEST_F(C3Test, RelentlessPursuit_AttachingIsOptionalAndTheConquerRiderIsArmed) {
    auto spell = addToHand(P1, kRelentlessPursuit);
    auto unit = addUnit(P1, kInvalidId, 3);
    auto sword = addGear(P1, true);
    StepAgent agent{V(1), V(0)};   // battlefield #0, attach: no
    playAt(spell, {unit}, &agent);
    EXPECT_EQ(agent.missed, 0);
    EXPECT_FALSE(state.getObject(sword).attached_to.has_value());
    ASSERT_EQ(state.delayed_abilities.size(), 1u);
    EXPECT_EQ(state.delayed_abilities[0].trigger, TriggerType::WhenIConquer);
    EXPECT_EQ(state.delayed_abilities[0].target_filter, unit);
    EXPECT_EQ(state.delayed_abilities[0].expires_on_turn, state.turn.turn_number);

    // "When I conquer, you may move me to my base."
    StepAgent yes{V(1)};
    withEngine([&](GameEngine& e) {
        e.testHook_scoreConquer(P1, 0);
        e.testHook_runChain();
    }, &yes);
    EXPECT_TRUE(state.getObject(unit).isAtBase());
}

TEST_F(C3Test, Riposte_StillCountersWhenTheChosenUnitIsGone) {
    // "Counter that spell and give that unit +[M]...": the counter does not
    // depend on the unit. The spell used to do nothing at all.
    auto riposte = addToHand(P1, kRiposte);
    auto unit = addUnit(P1, kInvalidId, 3);
    auto their_spell = enemySpellOnChain(P2);
    // P2 "responds": the chosen unit leaves the board. The engine works on its
    // own copy of the state, so the response is made through the engine handle.
    StepAgent p2;
    GameEngine* engine = nullptr;
    p2.on_priority = [&]() {
        auto& u = engine->mutableState().getObject(unit);
        u.location = std::nullopt;
        u.zone = ZoneType::Trash;
    };
    state.player(P1).rune_pool.energy += 2;
    state.player(P1).rune_pool.universal_power += 2;
    Intent play;
    play.type = IntentType::PlayCard;
    play.player = P1;
    play.card = riposte;
    StepAgent p1{O(unit)};
    withEngine([&](GameEngine& e) {
        engine = &e;
        e.testHook_executeIntent(play); settleLikeTheEngine(e);
    }, &p1, &p2);
    EXPECT_TRUE(inTrash(P2, their_spell)) << "countered";
    EXPECT_TRUE(state.chain.items.empty());
}

TEST_F(C3Test, CountersAreOnlyPlayableAgainstTheChainTop) {
    // Peek-and-pop counters answer the top of the chain; a spell under an
    // ability cannot be reached, so it must not make them playable.
    addUnit(P1, kInvalidId, 3);   // Riposte also needs a friendly unit
    state.player(P2).spells_played_this_turn = 2;
    for (CardDefId counter : {kHardBargain, kRiposte, kAbandon, kLiltingLullaby, kCrumblingSands})
        EXPECT_FALSE(hasLegalTargets(counter, P1)) << "empty chain: " << counter;
    enemySpellOnChain(P2);
    for (CardDefId counter : {kHardBargain, kRiposte, kAbandon, kLiltingLullaby, kCrumblingSands})
        EXPECT_TRUE(hasLegalTargets(counter, P1)) << "a spell on top: " << counter;
    ChainItem ability;
    ability.id = state.chain.allocateId();
    ability.controller = P2;
    ability.is_ability = true;
    state.chain.items.push_back(ability);
    for (CardDefId counter : {kHardBargain, kRiposte, kAbandon, kLiltingLullaby, kCrumblingSands})
        EXPECT_FALSE(hasLegalTargets(counter, P1)) << "an ability on top: " << counter;
}

TEST_F(C3Test, Sacrifice_KillsTheMightyUnitAsItIsPlayed) {
    // "As an additional cost to play this, kill a friendly [Mighty] unit."
    // The kill happened at resolution, so a countered Sacrifice cost nothing.
    auto spell = addToHand(P1, kSacrifice);
    auto mighty = addUnit(P1, kInvalidId, 6);
    addToDeck(P1, kInvalidId);
    addToDeck(P1, kInvalidId);
    GameEngine* engine = nullptr;
    bool dead_before_anyone_could_respond = false;
    StepAgent p2;
    p2.on_priority = [&]() {
        dead_before_anyone_could_respond =
            !engine->mutableState().getObject(mighty).location.has_value();
    };
    state.player(P1).rune_pool.energy += 1;
    Intent play;
    play.type = IntentType::PlayCard;
    play.player = P1;
    play.card = spell;
    StepAgent p1{O(mighty)};
    withEngine([&](GameEngine& e) {
        engine = &e;
        e.testHook_executeIntent(play); settleLikeTheEngine(e);
    }, &p1, &p2);
    EXPECT_TRUE(dead_before_anyone_could_respond);
    EXPECT_FALSE(onBoard(mighty));
    EXPECT_EQ(handSize(P1), 2) << "Draw 2";
}

TEST_F(C3Test, DeadlyFlourish_TakesAnEnemyUnitHasNoTimingKeywordAndWatchesThisTurnOnly) {
    const auto& def = card_db.get(kDeadlyFlourish);
    EXPECT_FALSE(def.keywords.has(Keyword::Reaction)) << "no [Reaction] is printed";
    EXPECT_FALSE(def.keywords.has(Keyword::Action));

    auto spell = addToHand(P1, kDeadlyFlourish);
    auto mine = addUnit(P1, kInvalidId, 6, 0);
    auto theirs = addUnit(P2, kInvalidId, 6, 0);
    StepAgent agent{O(theirs)};
    playThroughEngine(spell, &agent);
    EXPECT_FALSE(agent.offered(mine)) << "'an enemy unit'";
    EXPECT_EQ(state.getObject(theirs).damage_marked, 3);
    ASSERT_EQ(state.delayed_abilities.size(), 1u);
    EXPECT_EQ(state.delayed_abilities[0].expires_on_turn, state.turn.turn_number)
        << "'when it dies THIS TURN'";
}

TEST_F(C3Test, SiphoningStrike_DeathWatchExpiresWithTheTurn) {
    auto spell = addToHand(P1, kSiphoningStrike);
    auto theirs = addUnit(P2, kInvalidId, 9, 0);
    StepAgent agent{O(theirs)};
    playThroughEngine(spell, &agent);
    EXPECT_EQ(state.getObject(theirs).damage_marked, 4);
    ASSERT_EQ(state.delayed_abilities.size(), 1u);
    EXPECT_EQ(state.delayed_abilities[0].expires_on_turn, state.turn.turn_number);
}

TEST_F(C3Test, SkywardStrike_MovesWhereChosenAndAtLevelSixAlsoStuns) {
    // "Move an enemy unit. [Level 6][>] [Stun] an enemy unit." — no "instead".
    auto low = addToHand(P1, kSkywardStrike);
    auto mover = addUnit(P2, kInvalidId, 3);       // in base: can be pulled out
    auto other = addUnit(P2, kInvalidId, 3, 0);
    StepAgent a1{O(mover), V(2)};                  // to battlefield #1
    playThroughEngine(low, &a1);
    EXPECT_EQ(a1.missed, 0);
    ASSERT_TRUE(bfOf(mover).has_value());
    EXPECT_EQ(*bfOf(mover), 1u);
    EXPECT_FALSE(state.getObject(other).is_stunned);

    setXp(P1, 6);
    auto high = addToHand(P1, kSkywardStrike);
    StepAgent a2{O(mover), O(other), V(0)};        // move one to base, stun the other
    playThroughEngine(high, &a2);
    EXPECT_EQ(a2.missed, 0);
    EXPECT_TRUE(state.getObject(mover).isAtBase()) << "the move still happens at Level 6";
    EXPECT_TRUE(state.getObject(other).is_stunned);
}

TEST_F(C3Test, KeepersVerdict_TheOwnerChoosesTopOrBottom) {
    auto spell = addToHand(P1, kKeepersVerdict);
    // A real card: a unit with no card behind it is a token, and a token's
    // owner is not asked (it ceases to exist, CR 183.1).
    auto victim = addUnit(P2, 175, 3, 0);   // Shipyard Skulker
    addToDeck(P2, kInvalidId);
    addToDeck(P2, kInvalidId);
    StepAgent p1{O(victim)};
    StepAgent p2{V(0)};   // the OWNER answers: top
    playThroughEngine(spell, &p1, std::nullopt, &p2);
    EXPECT_EQ(p1.prompts.size(), 1u) << "the caster only chooses the unit";
    ASSERT_EQ(p2.prompts.size(), 1u);
    ASSERT_EQ(deckSize(P2), 3);
    EXPECT_EQ(state.player(P2).main_deck.back(), victim) << "top of the deck, as its owner chose";
}

TEST_F(C3Test, ThrillOfTheHunt_TheUnitComesBackWithoutItsStatuses) {
    auto spell = addToHand(P1, kThrillOfTheHunt);
    auto unit = addUnit(P1, kSkulker, 0, 0);
    auto& u = state.getObject(unit);
    u.is_stunned = true;
    u.buff_count = 1;
    u.temp_might_bonus = 2;
    u.combat_designation = CombatDesignation::Attacker;
    StepAgent agent{O(unit), V(1)};
    playThroughEngine(spell, &agent);
    const auto& back = state.getObject(unit);
    ASSERT_TRUE(back.battlefieldId().has_value());
    EXPECT_EQ(*back.battlefieldId(), 1u);
    EXPECT_FALSE(back.is_stunned);
    EXPECT_EQ(back.buff_count, 0);
    EXPECT_EQ(back.temp_might_bonus, 0);
    EXPECT_EQ(back.combat_designation, CombatDesignation::None);
}

TEST_F(C3Test, VoidRush_ThePlayerChoosesWhichCardAndMayDecline) {
    // It always took the top card, banished it before checking it could be
    // paid for, and never asked.
    auto cheap = addToDeck(P1, kSkulker);          // second from the top
    auto dear = addToDeck(P1, kMountainDrake);     // top: 9 Energy
    auto spell = addToHand(P1, kVoidRush);
    state.player(P1).rune_pool.energy += 1;        // Skulker at 3 - 2
    StepAgent agent{V(1), O(cheap), V(0)};         // yes, the Skulker, to base
    playThroughEngine(spell, &agent);
    EXPECT_EQ(agent.missed, 0);
    EXPECT_FALSE(agent.offered(dear)) << "it cannot be paid for, so it is not offered";
    EXPECT_TRUE(onBoard(cheap));
    EXPECT_TRUE(inHand(P1, dear)) << "Draw any you didn't banish";
    EXPECT_FALSE(inBanishment(P1, dear));
    EXPECT_FALSE(inBanishment(P1, cheap));
}

TEST_F(C3Test, VoidRush_DecliningDrawsBoth) {
    auto a = addToDeck(P1, kSkulker);
    auto b = addToDeck(P1, kSkulker);
    auto spell = addToHand(P1, kVoidRush);
    state.player(P1).rune_pool.energy += 1;
    StepAgent agent{V(0)};
    playThroughEngine(spell, &agent);
    EXPECT_TRUE(inHand(P1, a));
    EXPECT_TRUE(inHand(P1, b));
    EXPECT_EQ(state.player(P1).rune_pool.energy, 1) << "nothing was paid";
}

TEST_F(C3Test, DragUnder_CostsTwoLessWhenAnEffectPlaysItFromOutsideTheHand) {
    // "I cost [2] less to play from anywhere other than your hand." Void Rush
    // plays it off the deck: 5 - 2 (Void Rush) - 2 (its own) = [1] + [O].
    auto drag = addToDeck(P1, kDragUnder);
    auto spell = addToHand(P1, kVoidRush);
    state.player(P1).rune_pool.energy += 1;
    state.player(P1).rune_pool.universal_power += 1;
    StepAgent agent{V(1), O(drag)};
    playThroughEngine(spell, &agent);
    EXPECT_EQ(agent.missed, 0) << "affordable only with its own [2] reduction";
    EXPECT_TRUE(inTrash(P1, drag)) << "played, resolved, in the trash";
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0);
}

TEST_F(C3Test, ModalSpellsChooseTheModeFirstSoTheTargetAlwaysFits) {
    // Mesmerize: "Return a friendly unit to its owner's hand. / Give an enemy
    // unit -2 [M] this turn." The engine used to ask for a unit before the
    // mode, from "any unit".
    auto spell = addToHand(P1, kMesmerize);
    auto mine = addUnit(P1, kInvalidId, 3);
    auto theirs = addUnit(P2, kInvalidId, 3);
    StepAgent agent{V(0), O(mine)};
    playThroughEngine(spell, &agent);
    EXPECT_EQ(agent.missed, 0);
    ASSERT_GE(agent.prompts.size(), 2u);
    EXPECT_TRUE(agent.prompts[0].front().chosen_value.has_value()) << "the mode comes first";
    EXPECT_FALSE(agent.offered(theirs)) << "an enemy unit is not a target of 'return a friendly unit'";
    EXPECT_FALSE(onBoard(mine));
}

TEST_F(C3Test, ResonatingStrike_OffersOnlyUnitsThatAreNotAtTheChosenBattlefield) {
    auto spell = addToHand(P1, kResonatingStrike);
    control(P1, 0);
    auto already_there = addUnit(P1, kInvalidId, 3, 0);
    auto in_base = addUnit(P1, kInvalidId, 3);
    StepAgent agent{V(0), O(in_base)};
    playThroughEngine(spell, &agent);
    EXPECT_EQ(agent.missed, 0);
    EXPECT_FALSE(agent.offered(already_there));
    ASSERT_TRUE(bfOf(in_base).has_value());
    EXPECT_EQ(*bfOf(in_base), 0u);
    EXPECT_EQ(state.getObject(in_base).current_might, 5);
}

TEST_F(C3Test, ExistentialDread_NeedsAnAttackerAndStunsForItsController) {
    auto spell = addToHand(P1, kExistentialDread);
    auto idle = addUnit(P2, kInvalidId, 3, 0);
    EXPECT_TRUE(offers(spell).empty()) << "no attacking enemy unit";
    auto attacker = addUnit(P2, kInvalidId, 3, 1);
    state.getObject(attacker).combat_designation = CombatDesignation::Attacker;
    PlayerId stunner = PlayerId::None;
    auto conn = events.on_unit_stunned.connect(
        [&](const UnitStunnedEvent& e) { stunner = e.stunner; });
    StepAgent agent{O(attacker)};
    playThroughEngine(spell, &agent);
    conn.disconnect();
    EXPECT_FALSE(agent.offered(idle));
    EXPECT_TRUE(state.getObject(attacker).is_stunned);
    EXPECT_EQ(stunner, P1) << "'when you stun' sees the caster";
}

TEST_F(C3Test, VoidAssault_EachUnitGetsItsOwnDestination) {
    auto spell = addToHand(P1, kVoidAssault);
    auto mine = addUnit(P1, kInvalidId, 3);          // base: may go to bf 0 or bf 1
    auto theirs = addUnit(P2, kInvalidId, 3, 0);     // bf 0: may go to base or bf 1
    // Options are the pairs (mine x theirs): mine {bf0, bf1} x theirs {base, bf1}.
    // Index 2 = mine -> bf1, theirs -> base.
    StepAgent agent{V(2)};
    playAt(spell, {mine, theirs}, &agent);
    EXPECT_EQ(agent.missed, 0);
    ASSERT_TRUE(bfOf(mine).has_value());
    EXPECT_EQ(*bfOf(mine), 1u);
    EXPECT_TRUE(state.getObject(theirs).isAtBase());
}

TEST_F(C3Test, Smite_BanishesTheUnitIfItDiesLaterThisTurn) {
    auto spell = addToHand(P1, kSmite);
    auto unit = addUnit(P2, kSkulker, 5, 0);
    playAt(spell, {unit});
    ASSERT_TRUE(onBoard(unit)) << "3 damage does not kill a 5 [M] unit";
    int deaths = 0;
    auto conn = events.on_unit_died.connect([&](const UnitDiedEvent&) { ++deaths; });
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) { exec.killObject(unit); });
    conn.disconnect();
    EXPECT_TRUE(inBanishment(P2, unit));
    EXPECT_FALSE(inTrash(P2, unit));
    EXPECT_EQ(deaths, 0) << "banished instead: it did not die";
}

TEST_F(C3Test, DancingGrenade_TheUnitsControllerMayPlayItAgainForOnePower) {
    auto spell = addToHand(P1, kDancingGrenade);
    auto mine = addUnit(P1, kInvalidId, 9);
    auto theirs = addUnit(P2, kInvalidId, 9);
    state.player(P2).rune_pool.universal_power = 1;
    StepAgent p1{O(theirs)};
    StepAgent p2{V(1), O(mine)};   // yes, pay [A]; then their target
    playThroughEngine(spell, &p1, std::nullopt, &p2);
    EXPECT_EQ(p2.missed, 0);
    EXPECT_EQ(state.getObject(theirs).damage_marked, 2);
    EXPECT_EQ(state.getObject(mine).damage_marked, 3) << "2 + 1 Bonus Damage for the earlier hit";
    EXPECT_EQ(state.player(P2).rune_pool.universal_power, 0);
    const auto& trash = state.player(P1).trash;
    EXPECT_EQ(std::count(trash.begin(), trash.end(), spell), 1) << "in its owner's trash, once";
}

TEST_F(C3Test, DancingGrenade_TheHitCountDoesNotCarryIntoALaterTurn) {
    auto spell = addToHand(P1, kDancingGrenade);
    auto theirs = addUnit(P2, kInvalidId, 9);
    state.turn.turn_number = 5;
    auto& counters = state.getObject(spell).card_counters;
    counters["__dancing_grenade_hits"] = 3;
    counters["__dancing_grenade_turn"] = 4;
    StepAgent p1{O(theirs)};
    playThroughEngine(spell, &p1);
    EXPECT_EQ(state.getObject(theirs).damage_marked, 2);
}

TEST_F(C3Test, CallToBattle_BothPlayersChoose) {
    auto spell = addToHand(P1, kCallToBattle);
    control(P1, 0);
    control(P1, 1);
    auto mine = addUnit(P1, kInvalidId, 3);
    auto their_big = addUnit(P2, kInvalidId, 7);
    auto their_small = addUnit(P2, kInvalidId, 1);
    StepAgent p1{O(mine), V(1)};        // battlefield #1
    StepAgent p2{O(their_small)};       // they send the small one
    playThroughEngine(spell, &p1, std::nullopt, &p2);
    EXPECT_EQ(p1.missed, 0);
    EXPECT_EQ(p2.missed, 0);
    ASSERT_TRUE(bfOf(mine).has_value());
    EXPECT_EQ(*bfOf(mine), 1u);
    ASSERT_TRUE(bfOf(their_small).has_value());
    EXPECT_EQ(*bfOf(their_small), 1u);
    EXPECT_TRUE(state.getObject(their_big).isAtBase());
}

TEST_F(C3Test, Guards_ALaterCastOfTheSameCardMakesANewToken) {
    auto spell = addToHand(P1, kGuards);
    auto soldiers = [&]() {
        int n = 0;
        for (const auto& [id, obj] : state.objects)
            if (obj.name == "Sand Soldier" && obj.location.has_value()) ++n;
        return n;
    };
    playThroughEngine(spell);
    EXPECT_EQ(soldiers(), 1);
    backToHand(spell);
    playThroughEngine(spell);
    EXPECT_EQ(soldiers(), 2);
}

TEST_F(C3Test, Guards_PlayedFromFacedownTheTokenGoesToThatBattlefield) {
    // CR 811.1.d.5. Hidden at battlefield #1 on an earlier turn, played now.
    auto spell = addToHand(P1, kGuards);
    control(P1, 1);
    state.turn.turn_number = 3;
    {
        auto& h = state.player(P1).hand;
        h.erase(std::remove(h.begin(), h.end(), spell), h.end());
        auto& card = state.getObject(spell);
        card.zone = ZoneType::BattlefieldZone;
        card.is_hidden = true;
        card.hidden_at = 1;
        card.hidden_on_turn = 2;
        state.battlefields[1].facedown.push_back(spell);
    }
    Intent play;
    play.type = IntentType::PlayReaction;
    play.player = P1;
    play.card = spell;
    withEngine([&](GameEngine& e) { e.testHook_executeIntent(play); settleLikeTheEngine(e); });
    GameObjectId token = kInvalidId;
    for (const auto& [id, obj] : state.objects)
        if (obj.name == "Sand Soldier" && obj.location.has_value()) token = id;
    ASSERT_NE(token, kInvalidId);
    ASSERT_TRUE(bfOf(token).has_value());
    EXPECT_EQ(*bfOf(token), 1u);
}

TEST_F(C3Test, LotusTrap_PlayedFromFacedownKeepsTheTargetItWasPlayedWith) {
    // CR 811.1.d.2: the hidden play carries a target at that battlefield; the
    // card used to ask again, over every unit on the board.
    auto spell = addToHand(P1, kLotusTrap);
    control(P1, 1);
    auto here = addUnit(P2, kInvalidId, 3, 1);
    auto elsewhere = addUnit(P2, kInvalidId, 3, 0);
    state.turn.turn_number = 3;
    {
        auto& h = state.player(P1).hand;
        h.erase(std::remove(h.begin(), h.end(), spell), h.end());
        auto& card = state.getObject(spell);
        card.zone = ZoneType::BattlefieldZone;
        card.is_hidden = true;
        card.hidden_at = 1;
        card.hidden_on_turn = 2;
        state.battlefields[1].facedown.push_back(spell);
    }
    Intent play;
    play.type = IntentType::PlayReaction;
    play.player = P1;
    play.card = spell;
    play.targets = {here};
    StepAgent agent{O(elsewhere)};
    withEngine([&](GameEngine& e) { e.testHook_executeIntent(play); settleLikeTheEngine(e); }, &agent);
    EXPECT_TRUE(agent.prompts.empty()) << "the target is not asked again";
    EXPECT_TRUE(state.getObject(here).damage_doubled_this_turn);
    EXPECT_FALSE(state.getObject(elsewhere).damage_doubled_this_turn);
}

TEST_F(C3Test, DeathFromBelow_TheReplayIsOfferedAsItResolvesAndLeavesNoStandingGrant) {
    auto spell = addToHand(P1, kDeathFromBelow);
    auto small = addUnit(P2, kInvalidId, 2, 0);
    auto second = addUnit(P2, kInvalidId, 6, 1);
    state.player(P1).rune_pool.universal_power += 1;   // the [A] for the replay
    StepAgent agent{O(small), V(1), O(second)};
    playThroughEngine(spell, &agent);
    EXPECT_EQ(agent.missed, 0);
    EXPECT_FALSE(onBoard(small));
    EXPECT_FALSE(onBoard(second)) << "played again for [A]";
    EXPECT_TRUE(state.player(P1).trash_replay_grants.empty());
    const auto& trash = state.player(P1).trash;
    EXPECT_EQ(std::count(trash.begin(), trash.end(), spell), 1);
}

TEST_F(C3Test, TokensWithNoPrintedLocationCanGoToAControlledBattlefield) {
    // Up from the Deep: "Play two 1 [M] Tentacle unit tokens".
    auto spell = addToHand(P1, kUpFromTheDeep);
    control(P1, 1);
    StepAgent agent{V(1), V(0)};   // first to the battlefield, second to base
    playThroughEngine(spell, &agent);
    EXPECT_EQ(agent.missed, 0);
    int at_bf = 0, at_base = 0;
    for (const auto& [id, obj] : state.objects) {
        if (obj.name != "Tentacle" || !obj.location.has_value()) continue;
        if (obj.isAtBase()) ++at_base; else ++at_bf;
    }
    EXPECT_EQ(at_bf, 1);
    EXPECT_EQ(at_base, 1);
}

TEST_F(C3Test, FlurryOfFeathers_PlaysFourBirdsOnceWhateverTheLocationPrompts) {
    auto spell = addToHand(P1, kFlurryOfFeathers);
    control(P1, 0);
    StepAgent agent{V(1), V(1), V(0), V(1), V(0)};   // mode: birds; then four locations
    playThroughEngine(spell, &agent);
    int birds = 0;
    for (const auto& [id, obj] : state.objects)
        if (obj.name == "Bird" && obj.location.has_value()) ++birds;
    EXPECT_EQ(birds, 4);
}

TEST_F(C3Test, DoubleTrouble_RevealingAUnitIsOptionalChosenAndARealDraw) {
    auto bottom = addToDeck(P1, kInvalidId);
    auto u1 = addToDeck(P1, kSkulker);
    auto u2 = addToDeck(P1, kPoutyPoro);
    auto top = addToDeck(P1, kDragUnder);
    int draws = 0;
    auto conn = events.on_cards_drawn.connect(
        [&](const CardsDrawnEvent& e) { if (e.player == P1) draws += e.count; });
    auto spell = addToHand(P1, kDoubleTrouble);
    StepAgent agent{V(1), O(u1)};   // yes; the Skulker, not the first unit found
    playThroughEngine(spell, &agent);
    conn.disconnect();
    EXPECT_EQ(agent.missed, 0);
    EXPECT_TRUE(inHand(P1, u1));
    EXPECT_EQ(draws, 1) << "'draw it' is a draw";
    EXPECT_TRUE(inDeck(P1, u2));
    EXPECT_TRUE(inDeck(P1, top));
    EXPECT_EQ(state.player(P1).main_deck.back(), bottom) << "the rest went under it";

    auto again = addToHand(P1, kDoubleTrouble);
    const int hand_before = handSize(P1);
    StepAgent no{V(0)};
    playThroughEngine(again, &no);
    EXPECT_EQ(handSize(P1), hand_before - 1) << "declined: nothing drawn";
}

// ═══ batch-03 Systemic (S5 / S7) ════════════════════════════════════════════

TEST_F(C3Test, DisposalOrder_TheCasterChoosesWhichCards) {
    auto spell = addToHand(P1, kDisposalOrder);
    auto old_card = addToTrash(P2, kSkulker);
    auto recent1 = addToTrash(P2, kPoutyPoro);
    auto recent2 = addToTrash(P2, kMegaMech);
    StepAgent agent{V(0), O(old_card), V(0)};   // mode: recycle; the oldest card; Done
    playThroughEngine(spell, &agent);
    EXPECT_EQ(agent.missed, 0);
    EXPECT_TRUE(inDeck(P2, old_card));
    EXPECT_TRUE(inTrash(P2, recent1));
    EXPECT_TRUE(inTrash(P2, recent2));
}

TEST_F(C3Test, StareDown_TheBattlefieldIsChosenAndTheXpIsGainedOnce) {
    auto spell = addToHand(P1, kStareDown);
    auto mine = addUnit(P1, kInvalidId, 5);
    auto many1 = addUnit(P2, kInvalidId, 2, 0);
    auto many2 = addUnit(P2, kInvalidId, 2, 0);
    auto single = addUnit(P2, kInvalidId, 2, 1);
    StepAgent agent{O(mine), V(1)};   // the battlefield with fewer weaker units
    playThroughEngine(spell, &agent);
    EXPECT_EQ(agent.missed, 0);
    EXPECT_TRUE(state.getObject(single).isAtBase());
    EXPECT_FALSE(state.getObject(many1).isAtBase());
    EXPECT_FALSE(state.getObject(many2).isAtBase());
    EXPECT_EQ(state.player(P1).xp, 1);
}

TEST_F(C3Test, Moonfall_TheBattlefieldAndTheEnemyUnitAreChosen) {
    auto spell = addToHand(P1, kMoonfall);
    addUnit(P1, kInvalidId, 3, 0);
    addUnit(P1, kInvalidId, 3, 1);
    auto first = addUnit(P2, kInvalidId, 5);
    auto second = addUnit(P2, kInvalidId, 5);
    StepAgent agent{V(1), V(1), O(second)};   // battlefield #1; yes; the second enemy unit
    playThroughEngine(spell, &agent);
    EXPECT_EQ(agent.missed, 0);
    ASSERT_TRUE(bfOf(second).has_value());
    EXPECT_EQ(*bfOf(second), 1u);
    EXPECT_EQ(state.getObject(second).current_might, 3);
    EXPECT_TRUE(state.getObject(first).isAtBase());
    EXPECT_EQ(state.getObject(first).current_might, 5);
}

TEST_F(C3Test, TurnToDust_TheGearKeepsTemporaryPastTheEndOfTheTurn) {
    // giveTemporaryKeyword marks the grant "this turn", so the Expiration Step
    // stripped it and the gear was never killed.
    auto spell = addToHand(P1, kTurnToDust);
    auto gear = addGear(P2, false);
    playAt(spell, {gear});
    EXPECT_TRUE(state.getObject(gear).keywords.has(Keyword::Temporary));
    EXPECT_FALSE(state.getObject(gear).temp_keywords.has(Keyword::Temporary))
        << "not a this-turn grant";
    // Its controller's next Beginning Phase kills it.
    state.turn.turn_player = P2;
    withEngine([&](GameEngine& e) { e.testHook_beginningStep(); });
    EXPECT_FALSE(onBoard(gear));
}

// ═══ plans 07 / batch-08 / batch-09 ═════════════════════════════════════════

TEST_F(C3Test, ShadowsCall_OnlyUnitsWithoutTemporaryAndTheGrantLasts) {
    auto spell = addToHand(P1, kShadowsCall);
    auto plain = addUnit(P1, kInvalidId, 3);
    auto already = addUnit(P1, kInvalidId, 3);
    state.getObject(already).keywords.set(Keyword::Temporary);
    addToDeck(P1, kInvalidId);
    addToDeck(P1, kInvalidId);
    StepAgent agent{O(plain)};
    playThroughEngine(spell, &agent);
    EXPECT_FALSE(agent.offered(already)) << "'a friendly unit without [Temporary]'";
    EXPECT_TRUE(state.getObject(plain).keywords.has(Keyword::Temporary));
    EXPECT_FALSE(state.getObject(plain).temp_keywords.has(Keyword::Temporary));
    EXPECT_EQ(handSize(P1), 2);
}

TEST_F(C3Test, HereToHelp_MayBeDeclinedAndNeverPlaysToBase) {
    auto spell = addToHand(P1, kHereToHelp);
    auto unit = addToHand(P1, kSkulker);   // 3 Energy: free after the reduction
    {
        StepAgent agent;
        playThroughEngine(spell, &agent);
        EXPECT_TRUE(agent.prompts.empty()) << "no battlefield controlled: nothing to offer";
        EXPECT_TRUE(inHand(P1, unit)) << "not played to base";
    }
    // Control that survives a cleanup: a unit of P1's at each battlefield.
    addUnit(P1, kInvalidId, 2, 0);
    addUnit(P1, kInvalidId, 2, 1);
    control(P1, 0);
    control(P1, 1);
    backToHand(spell);
    {
        StepAgent agent{V(0)};
        playThroughEngine(spell, &agent);
        EXPECT_TRUE(inHand(P1, unit)) << "'you may': declined";
    }
    backToHand(spell);
    {
        StepAgent agent{V(1), O(unit), V(1)};   // yes; the unit; the second battlefield
        playThroughEngine(spell, &agent);
        EXPECT_EQ(agent.missed, 0);
        ASSERT_TRUE(bfOf(unit).has_value());
        EXPECT_EQ(*bfOf(unit), 1u);
    }
}

TEST_F(C3Test, StrikeDown_TheControllerChoosesWhichEquipmentIsDetached) {
    auto spell = addToHand(P1, kStrikeDown);
    auto unit = addUnit(P1, kInvalidId, 4);
    auto first = addGear(P1, true);
    auto second = addGear(P1, true);
    attach(first, unit);
    attach(second, unit);
    auto enemy = addUnit(P2, kInvalidId, 9);
    StepAgent agent{O(unit), O(enemy), O(second)};
    playThroughEngine(spell, &agent);
    EXPECT_EQ(agent.missed, 0);
    EXPECT_EQ(state.getObject(enemy).damage_marked, 4) << "dealt once";
    EXPECT_FALSE(state.getObject(second).attached_to.has_value());
    EXPECT_EQ(state.getObject(first).attached_to, std::optional<GameObjectId>(unit));
}

TEST_F(C3Test, Temptation_TheDestinationIsAChoiceNotASecondTarget) {
    auto spell = addToHand(P1, kTemptation);
    auto moved = addUnit(P2, kInvalidId, 3, 0);
    addUnit(P2, kInvalidId, 3);                       // makes their base a destination
    auto anchor = addUnit(P2, kMysticPoro, 0, 1);     // [Deflect]: must cost nothing here
    state.getObject(anchor).deflect_value = 1;
    StepAgent agent{O(moved), V(2)};                  // to battlefield #1
    playThroughEngine(spell, &agent);
    EXPECT_EQ(agent.missed, 0);
    ASSERT_GE(agent.prompts.size(), 2u);
    EXPECT_TRUE(agent.prompts[1].front().chosen_objects.empty())
        << "the destination is offered as a location, not as a unit to choose";
    ASSERT_TRUE(bfOf(moved).has_value());
    EXPECT_EQ(*bfOf(moved), 1u);
}

TEST_F(C3Test, HardBargain_FloatingEnergyCanPayTheTwo) {
    auto spell = addToHand(P1, kHardBargain);
    auto their_spell = enemySpellOnChain(P2);
    state.player(P2).rune_pool.energy = 2;   // no ready runes at all
    StepAgent p2{O(their_spell)};            // "pay"
    playThroughEngine(spell, nullptr, std::nullopt, &p2);
    ASSERT_EQ(p2.prompts.size(), 1u) << "they can pay, so they are asked";
    EXPECT_EQ(p2.missed, 0);
    EXPECT_EQ(state.player(P2).rune_pool.energy, 0);
}

TEST_F(C3Test, Switcheroo_DoesNotOfferAUnitThatCannotBeChosenByEnemySpells) {
    auto spell = addToHand(P1, kSwitcheroo);
    auto mine = addUnit(P1, kInvalidId, 2, 0);
    auto baron = addUnit(P2, kBaronNashor, 0, 0);
    state.getObject(baron).untargetable_by_enemy = true;
    auto plain = addUnit(P2, kInvalidId, 6, 0);
    StepAgent agent{O(mine), O(plain)};
    playThroughEngine(spell, &agent);
    EXPECT_FALSE(agent.offered(baron));
    EXPECT_EQ(state.getObject(mine).current_might, 6);
}

TEST_F(C3Test, DefiantDance_TheSecondUnitIsStillAffectedWhenTheFirstIsGone) {
    // CR 359.3.e.5: each target by itself.
    auto spell = addToHand(P1, kDefiantDance);
    auto first = addUnit(P1, kInvalidId, 3);
    auto second = addUnit(P2, kInvalidId, 5);
    GameEngine* engine = nullptr;
    StepAgent p2;
    p2.on_priority = [&]() {
        auto& u = engine->mutableState().getObject(first);
        u.location = std::nullopt;
        u.zone = ZoneType::Trash;
    };
    state.player(P1).rune_pool.energy += 1;
    state.player(P1).rune_pool.universal_power += 1;
    Intent play;
    play.type = IntentType::PlayCard;
    play.player = P1;
    play.card = spell;
    StepAgent p1{O(first), O(second)};
    withEngine([&](GameEngine& e) {
        engine = &e;
        e.testHook_executeIntent(play); settleLikeTheEngine(e);
    }, &p1, &p2);
    EXPECT_EQ(state.getObject(second).current_might, 3) << "-2 [M] still applies";
}

TEST_F(C3Test, ArcaneShift_ReplaysTheUnitOnceAndBanishesItself) {
    auto spell = addToHand(P1, kArcaneShift);
    auto unit = addUnit(P1, kSkulker);
    auto enemy = addUnit(P2, kInvalidId, 9, 0);
    int unit_plays = 0;
    auto conn = events.on_entered_board.connect(
        [&](const EnteredBoardEvent& e) { if (e.object == unit) ++unit_plays; });
    control(P1, 1);
    StepAgent agent{O(enemy), V(1)};   // the enemy unit; the unit comes back at the battlefield
    playAt(spell, {unit}, &agent);
    conn.disconnect();
    EXPECT_EQ(agent.missed, 0);
    EXPECT_LE(unit_plays, 1) << "banished and replayed once, not once per pass";
    EXPECT_EQ(state.player(P1).cards_banished_this_turn, 2) << "the unit, then the spell";
    ASSERT_TRUE(bfOf(unit).has_value());
    EXPECT_EQ(*bfOf(unit), 1u);
    EXPECT_EQ(state.getObject(enemy).damage_marked, 3);
    EXPECT_TRUE(inBanishment(P1, spell));
    EXPECT_FALSE(inTrash(P1, spell));
}

TEST_F(C3Test, BackOff_StunsForItsControllerAndDrawsOnlyWhenPlayedFromHand) {
    addToDeck(P1, kInvalidId);
    addToDeck(P1, kInvalidId);
    auto spell = addToHand(P1, kBackOff);
    auto unit = addUnit(P2, kInvalidId, 3, 1);
    PlayerId stunner = PlayerId::None;
    auto conn = events.on_unit_stunned.connect(
        [&](const UnitStunnedEvent& e) { stunner = e.stunner; });
    StepAgent agent{O(unit)};
    playThroughEngine(spell, &agent);
    conn.disconnect();
    EXPECT_EQ(stunner, P1);
    EXPECT_EQ(handSize(P1), 1) << "played from hand: draw 1";

    // Played from facedown: no draw.
    auto hidden = addToHand(P1, kBackOff);
    auto other = addUnit(P2, kInvalidId, 3, 1);
    control(P1, 1);
    state.turn.turn_number = 3;
    {
        auto& h = state.player(P1).hand;
        h.erase(std::remove(h.begin(), h.end(), hidden), h.end());
        auto& card = state.getObject(hidden);
        card.zone = ZoneType::BattlefieldZone;
        card.is_hidden = true;
        card.hidden_at = 1;
        card.hidden_on_turn = 2;
        state.battlefields[1].facedown.push_back(hidden);
    }
    Intent play;
    play.type = IntentType::PlayReaction;
    play.player = P1;
    play.card = hidden;
    play.targets = {other};
    withEngine([&](GameEngine& e) { e.testHook_executeIntent(play); settleLikeTheEngine(e); });
    EXPECT_TRUE(state.getObject(other).is_stunned);
    EXPECT_EQ(handSize(P1), 1) << "'if you played this from your hand' — it was not";
}

TEST_F(C3Test, Abandon_RecyclingThePredictedCardIsAnnounced) {
    auto spell = addToHand(P1, kAbandon);
    enemySpellOnChain(P2);
    addToDeck(P1, kInvalidId);
    auto top = addToDeck(P1, kSkulker);
    int recycled = 0;
    auto conn = events.on_object_state_changed.connect([&](const ObjectStateChangedEvent& e) {
        if (e.what_changed == "recycled_main") ++recycled;
    });
    StepAgent agent{O(top)};   // recycle it
    playThroughEngine(spell, &agent);
    conn.disconnect();
    EXPECT_EQ(state.player(P1).main_deck.front(), top);
    EXPECT_EQ(recycled, 1);
}

TEST_F(C3Test, BoneSkewer_ChoosingAUnitIsOptional) {
    auto spell = addToHand(P1, kBoneSkewer);
    auto their_unit = addToHand(P2, kSkulker);
    StepAgent agent{V(0), V(0)};   // battlefield #0; do not choose a unit
    playThroughEngine(spell, &agent);
    EXPECT_TRUE(inHand(P2, their_unit));

    backToHand(spell);
    StepAgent yes{V(1), V(1), O(their_unit)};
    playThroughEngine(spell, &yes);
    EXPECT_EQ(yes.missed, 0);
    ASSERT_TRUE(bfOf(their_unit).has_value());
    EXPECT_EQ(*bfOf(their_unit), 1u);
    EXPECT_TRUE(state.getObject(their_unit).is_stunned);
}

TEST_F(C3Test, Conscription_WithoutTheXpOnlySmallUnitsAndTheRecallIsNotAMove) {
    auto spell = addToHand(P1, kConscription);
    auto small = addUnit(P2, kInvalidId, 3, 0);
    auto big = addUnit(P2, kInvalidId, 6, 0);
    int moves = 0;
    auto conn = events.on_unit_moved.connect([&](const UnitMovedEvent&) { ++moves; });
    StepAgent agent{O(small)};
    playThroughEngine(spell, &agent);
    conn.disconnect();
    EXPECT_FALSE(agent.offered(big)) << "more than 3 [M], and no XP was spent";
    EXPECT_EQ(state.getObject(small).controller, P1);
    EXPECT_TRUE(state.getObject(small).is_exhausted);
    EXPECT_EQ(state.getObject(small).location, std::optional<LocationId>(BaseLocation{P1}));
    EXPECT_EQ(moves, 0) << "a recall is not a move (CR 458)";
}

TEST_F(C3Test, Conscription_SpendingFiveXpIsPaidOnceAndWidensTheChoice) {
    auto spell = addToHand(P1, kConscription);
    auto big = addUnit(P2, kInvalidId, 6, 0);
    setXp(P1, 7);
    StepAgent agent{V(1), O(big)};
    playThroughEngine(spell, &agent);
    EXPECT_EQ(agent.missed, 0);
    EXPECT_EQ(state.player(P1).xp, 2);
    EXPECT_EQ(state.getObject(big).controller, P1);
}

TEST_F(C3Test, HeedlessResurrection_TheKillIsPaidAsItIsPlayedAndTheUnitCanComeBack) {
    auto spell = addToHand(P1, kHeedlessResurrection);
    auto unit = addUnit(P1, kSkulker);
    GameEngine* engine = nullptr;
    bool dead_before_anyone_could_respond = false;
    StepAgent p2;
    p2.on_priority = [&]() {
        dead_before_anyone_could_respond =
            !engine->mutableState().getObject(unit).location.has_value();
    };
    state.player(P1).rune_pool.energy += 2;
    state.player(P1).rune_pool.universal_power += 1;
    Intent play;
    play.type = IntentType::PlayCard;
    play.player = P1;
    play.card = spell;
    StepAgent p1{O(unit), O(unit), V(0)};   // kill it; play it back; to base
    withEngine([&](GameEngine& e) {
        engine = &e;
        e.testHook_executeIntent(play); settleLikeTheEngine(e);
    }, &p1, &p2);
    EXPECT_TRUE(dead_before_anyone_could_respond);
    EXPECT_EQ(p1.missed, 0);
    EXPECT_TRUE(onBoard(unit)) << "the killed unit is in the trash by then and may be the one played";
}

TEST_F(C3Test, UndyingLoyalty_OnlyABirdCatDogOrPoroEarnsTheDiscount) {
    // The discount used to apply whenever such a unit was in the trash,
    // whichever unit was then chosen.
    auto spell = addToHand(P1, kUndyingLoyalty);
    auto poro = addToTrash(P1, kPoutyPoro);
    auto sage = addToTrash(P1, kDragonsoulSage);
    // Discounted cost: [0] + one Power. Choosing the Sage costs the [2] back.
    state.player(P1).rune_pool.universal_power += 1;
    state.player(P1).rune_pool.energy += 2;
    Intent play;
    play.type = IntentType::PlayCard;
    play.player = P1;
    play.card = spell;
    StepAgent agent{O(sage), V(0)};
    withEngine([&](GameEngine& e) { e.testHook_executeIntent(play); settleLikeTheEngine(e); }, &agent);
    EXPECT_EQ(agent.missed, 0);
    EXPECT_TRUE(onBoard(sage));
    EXPECT_TRUE(inTrash(P1, poro));
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0) << "the [2] was paid after all";
}

TEST_F(C3Test, UndyingLoyalty_AUnitThatIsNotAPoroIsNotOfferedWithoutTheTwoEnergy) {
    auto spell = addToHand(P1, kUndyingLoyalty);
    auto poro = addToTrash(P1, kPoutyPoro);
    auto sage = addToTrash(P1, kDragonsoulSage);
    auto big = addToTrash(P1, kMegaMech);   // 7 Energy: never eligible
    state.player(P1).rune_pool.universal_power += 1;
    Intent play;
    play.type = IntentType::PlayCard;
    play.player = P1;
    play.card = spell;
    StepAgent agent{O(poro), V(0)};
    withEngine([&](GameEngine& e) { e.testHook_executeIntent(play); settleLikeTheEngine(e); }, &agent);
    EXPECT_FALSE(agent.offered(sage));
    EXPECT_FALSE(agent.offered(big));
    EXPECT_TRUE(onBoard(poro));
}

TEST_F(C3Test, TwilightStep_AsksForTheUnitOnceThenWhereItMoves) {
    auto spell = addToHand(P1, kTwilightStep);
    auto unit = addUnit(P2, kInvalidId, 3, 0);
    addUnit(P2, kInvalidId, 3, 0);
    StepAgent agent{O(unit), V(2)};
    playThroughEngine(spell, &agent);
    EXPECT_EQ(agent.missed, 0);
    EXPECT_EQ(agent.prompts.size(), 2u) << "one unit prompt (at play), one destination";
    ASSERT_TRUE(bfOf(unit).has_value());
    EXPECT_EQ(*bfOf(unit), 1u);
}

TEST_F(C3Test, DecreeOfDiscord_PaysDeflectForAChosenUnit) {
    auto spell = addToHand(P1, kDecreeOfDiscord);
    auto deflector = addUnit(P2, kInvalidId, 2, 0);
    auto& d = state.getObject(deflector);
    d.domains = {Domain::Order};
    d.keywords.set(Keyword::Deflect);
    d.deflect_value = 1;
    state.player(P1).rune_pool.universal_power += 1;   // for the Deflect
    StepAgent agent{O(deflector), V(0)};
    playThroughEngine(spell, &agent);
    EXPECT_FALSE(onBoard(deflector));
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 0) << "Deflect was paid";
    EXPECT_TRUE(state.player(P1).chose_enemy_unit_this_turn);
}

TEST_F(C3Test, DecreeOfDiscord_DoesNotOfferAUnitWhoseDeflectCannotBePaid) {
    auto spell = addToHand(P1, kDecreeOfDiscord);
    auto deflector = addUnit(P2, kInvalidId, 2, 0);
    auto& d = state.getObject(deflector);
    d.domains = {Domain::Order};
    d.keywords.set(Keyword::Deflect);
    d.deflect_value = 1;
    StepAgent agent{O(deflector)};
    playThroughEngine(spell, &agent);
    EXPECT_FALSE(agent.offered(deflector));
    EXPECT_TRUE(onBoard(deflector));
}

TEST_F(C3Test, ShurikenFlip_PaysDeflectForTheEnemyUnitItChooses) {
    auto spell = addToHand(P1, kShurikenFlip);
    auto mine = addUnit(P1, kInvalidId, 3);
    auto deflector = addUnit(P2, kInvalidId, 5, 0);
    state.getObject(deflector).keywords.set(Keyword::Deflect);
    state.getObject(deflector).deflect_value = 1;
    state.player(P1).rune_pool.universal_power += 1;   // for the Deflect
    StepAgent agent{O(deflector), O(mine), V(2)};
    playThroughEngine(spell, &agent);
    EXPECT_EQ(agent.missed, 0);
    EXPECT_EQ(state.getObject(deflector).damage_marked, 2);
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 0) << "Deflect was paid";
    ASSERT_TRUE(bfOf(mine).has_value());
    EXPECT_EQ(*bfOf(mine), 1u);
}

TEST_F(C3Test, DeathMark_BurnsOnceAndTheCloneCanGoToAControlledBattlefield) {
    for (int i = 0; i < 6; ++i) addToDeck(P1, kInvalidId);
    auto spell = addToHand(P1, kDeathMark);
    control(P1, 1);
    StepAgent agent{V(1)};
    playThroughEngine(spell, &agent);
    EXPECT_EQ(deckSize(P1), 3) << "[Burn 3], once";
    GameObjectId clone = kInvalidId;
    for (const auto& [id, obj] : state.objects)
        if (obj.name == "Shadow Clone" && obj.location.has_value()) clone = id;
    ASSERT_NE(clone, kInvalidId);
    ASSERT_TRUE(bfOf(clone).has_value());
    EXPECT_EQ(*bfOf(clone), 1u);
}

TEST_F(C3Test, Repeat_ATwoTargetSpellChoosesAgainForItsRepeat) {
    // Bonds of Strength, [Repeat] [2]: the repeated execution asks for its own
    // two units (the engine's Repeat loop, plans 02 item 2.4).
    auto spell = addToHand(P1, kBondsOfStrength);
    auto a = addUnit(P1, kInvalidId, 1);
    auto b = addUnit(P1, kInvalidId, 1);
    auto c = addUnit(P1, kInvalidId, 1);
    state.player(P1).rune_pool.energy += 2;   // the Repeat cost
    StepAgent agent{V(1), O(a), O(b), O(a), O(c)};   // pay the Repeat; a+b; then a+c
    playThroughEngine(spell, &agent);
    EXPECT_EQ(agent.missed, 0);
    EXPECT_EQ(state.getObject(a).current_might, 3);
    EXPECT_EQ(state.getObject(b).current_might, 2);
    EXPECT_EQ(state.getObject(c).current_might, 2);
}

TEST_F(C3Test, Arise_EachSandSoldierCanBePlayedToAControlledBattlefield) {
    auto spell = addToHand(P1, kArise);
    addGear(P1, true);
    addGear(P1, true);
    control(P1, 0);
    // Two tokens: battlefield, base; then ready none.
    StepAgent agent{V(1), V(0), V(0)};
    playThroughEngine(spell, &agent);
    int at_bf = 0, at_base = 0;
    for (const auto& [id, obj] : state.objects) {
        if (obj.name != "Sand Soldier" || !obj.location.has_value()) continue;
        if (obj.isAtBase()) ++at_base; else ++at_bf;
    }
    EXPECT_EQ(at_bf, 1);
    EXPECT_EQ(at_base, 1);
}

}  // namespace
}  // namespace test_review_fix_c3
}  // namespace riftbound::test
