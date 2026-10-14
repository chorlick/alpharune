/// @file test_review_fix_sweep_triggers.cpp
/// Regression tests for the stage 3 sweep, part 3 (trigger_manager,
/// chain_manager, effect_executor): the engine pieces the card streams could
/// not add, and the cards that waited on them (plans/STATUS.md carry-over).
///
/// Every test drives the real GameEngine (TriggerManager + ChainManager +
/// finalization) through the fixture's engine entry points.

#include "ml/feature_extractor.h"
#include "cards/card_test_fixture.h"
#include "cards/card_helpers.h"
#include "engine/chain_manager.h"
#include "engine/effect_executor.h"
#include "engine/game_engine.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cctype>
#include <regex>
#include <functional>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <memory>
#include <string>
#include <vector>

namespace riftbound::test {
namespace test_review_fix_sweep_triggers {  // file-unique: test sources are unity-built
namespace {

constexpr CardDefId kBackAlleyBar    = 272;  // "When a unit moves from here, give it +1 [M] this turn."
constexpr CardDefId kMinotaur        = 337;  // "Units can't move to base."
constexpr CardDefId kSoraka          = 550;  // "... instead heal it, exhaust it, and recall it."
constexpr CardDefId kBlueSentinel    = 649;  // "Your hold effects for holding here trigger an additional time."
constexpr CardDefId kRedBrambleback  = 591;  // "Your conquer effects for conquering here trigger an additional time."
constexpr CardDefId kBattleMistress  = 517;  // legend: "When one or more enemy units die, ready me."
constexpr CardDefId kLoyalPoro       = 718;  // "[Deathknell] If I didn't die alone, draw 1."
constexpr CardDefId kLonelyPoro      = 777;  // "[Deathknell] If I died alone, draw 1."
constexpr CardDefId kLeBlanc         = 652;  // "your [Temporary] effects at my battlefield don't trigger."
constexpr CardDefId kScuttleCrab     = 615;  // "[Deathknell] ... Gain 1 XP."
constexpr CardDefId kDredgeUp        = 836;  // filler card for decks ("Draw 1.")
constexpr CardDefId kReluctantLeader = 908;  // "When you play another unit, give me +2 [M] this turn."
constexpr CardDefId kLillia          = 620;  // "When you play a token unit, give me +1 [M] this turn."
constexpr CardDefId kZilean          = 648;  // "... you may play that token and an additional copy of it instead."
constexpr CardDefId kSolariShrine    = 72;   // "When you kill a stunned enemy unit, you may exhaust this to draw 1."
constexpr CardDefId kUnlicensedArmory = 23;  // "The next time it would die this turn, you may pay [C] to ... recall it instead."
constexpr CardDefId kGemcraftSeer    = 100;  // "[Vision] ... Other friendly units have [Vision]."
constexpr CardDefId kForecaster      = 388;  // "Your Mechs have [Vision]." (a Mech itself)
constexpr CardDefId kEmperor         = 512;  // legend: "Your Sand Soldiers have [Weaponmaster]."
constexpr CardDefId kRenektonBrute   = 879;  // "When my Might becomes 10 or more, empower me."
constexpr CardDefId kFaeDragon       = 423;  // "When you spend a buff, play a Gold gear token exhausted."
constexpr CardDefId kDeathFromBelow  = 747;  // "... You may play this from your trash for [A]."
constexpr CardDefId kVexMocking      = 617;  // "When you stun an enemy unit at a battlefield, you may move me there."
constexpr CardDefId kDariusTrifarian = 27;   // "When you play your second card in a turn, give me +2 [M] this turn and ready me."
constexpr CardDefId kYiHoned         = 307;  // "I enter ready."
constexpr CardDefId kBlightedBattleaxe = 581; // "At the end of your turn, if I didn't conquer this turn, unattach this and deal 4 to me."
constexpr CardDefId kCalledShot      = 443;  // "Look at the top 2 cards of your Main Deck. Draw one and recycle the other."
constexpr CardDefId kDecreeOfInsight = 848;  // "Ignore [Deflect] while paying this spell's cost."
constexpr CardDefId kShepherdsHeirloom = 720; // "When you play this, gain 1 XP."
constexpr CardDefId kSvellsongur     = 382;  // "copy that unit's text to this Equipment's effect text"
constexpr CardDefId kCaptainFarron   = 15;   // "Other friendly units here have [Assault]."
constexpr CardDefId kChemtechEnforcer = 3;   // printed [Assault 2]
constexpr CardDefId kTheRuination    = 742;  // "Kill all units."
constexpr CardDefId kKarmaChanneler  = 235;  // "When you recycle one or more cards to your Main Deck, buff a friendly unit."
constexpr CardDefId kStackedDeck     = 183;  // "Look at the top 3 ... Put 1 into your hand and recycle the rest."
constexpr CardDefId kFateWeaver      = 626;  // looks at the top 4, may reveal a spell and draw it
constexpr CardDefId kShadowCloneCard = 955;  // the Shadow Clone token card

/// Passes priority; reports, each time it is asked while `spell` has a play on
/// the chain, whether the card is also filed in its owner's trash.
class TrashWatcher : public AgentInterface {
public:
    GameObjectId spell = kInvalidId;
    PlayerId owner = PlayerId::Player1;
    int seen_on_chain = 0;
    int seen_in_trash_while_on_chain = 0;
    Intent selectAction(const GameState& st, const std::vector<Intent>& legal) override {
        bool on_chain = false;
        for (const auto& ci : st.chain.items) if (ci.is_spell && ci.source == spell) on_chain = true;
        if (on_chain) {
            ++seen_on_chain;
            const auto& trash = st.player(owner).trash;
            if (std::find(trash.begin(), trash.end(), spell) != trash.end())
                ++seen_in_trash_while_on_chain;
        }
        for (const auto& i : legal)
            if (i.type == IntentType::PassPriority || i.type == IntentType::PassFocus) return i;
        return legal.empty() ? Intent{} : legal.front();
    }
};

class SweepTriggersTest : public CardTestFixture {
protected:
    GameObject& obj(GameObjectId id) { return state.getObject(id); }

    /// A gear on the board attached to `unit`.
    GameObjectId addAttachedGear(PlayerId owner, GameObjectId unit, CardDefId def = kInvalidId) {
        auto id = state.createObject();
        auto& g = state.getObject(id);
        g.owner = g.controller = owner;
        g.card_type = CardType::Gear;
        g.card_def_id = def;
        g.name = def != kInvalidId ? card_db.get(def).name : "TestGear";
        g.tags = {"Equipment"};
        g.location = state.getObject(unit).location;
        g.zone = state.getObject(unit).zone;
        g.attached_to = unit;
        state.getObject(unit).attachments.push_back(id);
        return id;
    }

    /// Make `def` the card of battlefield `bf`.
    GameObjectId setBattlefieldCard(int bf, CardDefId def,
                                    std::optional<PlayerId> controller = std::nullopt) {
        auto id = state.createObject();
        auto& o = state.getObject(id);
        o.card_def_id = def;
        o.card_type = CardType::Battlefield;
        o.name = card_db.get(def).name;
        o.zone = ZoneType::BattlefieldZone;
        state.battlefields[static_cast<size_t>(bf)].card_object_id = id;
        state.battlefields[static_cast<size_t>(bf)].controller = controller;
        return id;
    }

    GameObjectId addLegend(PlayerId owner, CardDefId def) {
        auto id = state.createObject();
        auto& o = state.getObject(id);
        o.card_def_id = def;
        o.owner = o.controller = owner;
        o.card_type = CardType::Legend;
        o.name = card_db.get(def).name;
        o.zone = ZoneType::LegendZone;
        state.player(owner).legend_zone = id;
        return id;
    }

    /// An unattached gear on `owner`'s base.
    GameObjectId addGear(PlayerId owner, CardDefId def = kInvalidId, bool equipment = false) {
        auto id = state.createObject();
        auto& g = state.getObject(id);
        g.owner = g.controller = owner;
        g.card_type = CardType::Gear;
        g.card_def_id = def;
        g.name = def != kInvalidId ? card_db.get(def).name : "TestGear";
        if (def != kInvalidId) g.tags = card_db.get(def).tags;
        if (equipment) g.tags = {"Equipment"};
        g.location = LocationId{BaseLocation{owner}};
        g.zone = ZoneType::Base;
        withEquipmentCard(id);   // CR 821.1.c.4: see the fixture
        return id;
    }
    LocationId base(PlayerId p) { return LocationId{BaseLocation{p}}; }
    LocationId battlefield(int bf) {
        return LocationId{BattlefieldLocation{state.battlefields[static_cast<size_t>(bf)].id}};
    }
    std::vector<GameObjectId> tokenUnits(PlayerId p) {
        std::vector<GameObjectId> out;
        for (const auto& [id, o] : state.objects)
            if (o.isUnit() && o.isToken() && o.controller == p && o.location.has_value())
                out.push_back(id);
        return out;
    }
    /// This file's own picker, so its tests do not depend on what the shared
    /// fixture's pickers do with a prompt they were not told about: choose
    /// object `id` when it is offered, say yes (1) to a yes/no, else the
    /// first option.
    static EnginePathAgent::Picker objectOrYes(GameObjectId id) {
        return [id](const std::vector<Intent>& legal) {
            for (const auto& i : legal)
                if (!i.chosen_objects.empty() && i.chosen_objects.front() == id) return i;
            for (const auto& i : legal)
                if (i.chosen_value.has_value() && *i.chosen_value == 1) return i;
            return legal.front();
        };
    }
    static Intent recycleIt(const std::vector<Intent>& legal) {      // a Predict prompt
        for (const auto& i : legal) if (!i.chosen_objects.empty()) return i;
        return legal.front();
    }
    static Intent keepOnTop(const std::vector<Intent>& legal) {      // a Predict prompt
        // Corrected (sweep-costs, 9.GG1): "Keep it" is now named by value 0.
        for (const auto& i : legal) if (i.chosen_objects.empty()) return i;
        return legal.front();
    }

    bool atBase(GameObjectId id) {
        const auto& o = state.getObject(id);
        return o.location.has_value() && o.isAtBase();
    }
    int handSize(PlayerId p) { return static_cast<int>(state.player(p).hand.size()); }
    void stockDeck(PlayerId p, int n) { for (int i = 0; i < n; ++i) addToDeck(p, kDredgeUp); }
};

// ═══════════════════════════════════════════════════════════════════════════
// 1. Recall is not a move (CR 457-460); attached gear stays (CR 719.3.a)
// ═══════════════════════════════════════════════════════════════════════════

// "heal it, exhaust it, and recall it instead" (Tactical Retreat / Highlander)
// applied to an effect kill: the unit goes to its base with its gear, and
// "When a unit moves from here" does not trigger.
TEST_F(SweepTriggersTest, Recall_DeathReplacementOnAnEffectKillKeepsGearAndIsNotAMove) {
    setBattlefieldCard(0, kBackAlleyBar);
    auto unit = addUnit(P1, kInvalidId, 3, 0);
    auto gear = addAttachedGear(P1, unit);
    obj(unit).death_replacement_recall_pending = true;

    resolveThroughEngine([&](EffectExecutor& exec, GameState&) { exec.killObject(unit); });

    EXPECT_TRUE(atBase(unit));
    ASSERT_TRUE(obj(gear).attached_to.has_value()) << "a recalled unit keeps its gear (CR 719.3.a)";
    EXPECT_EQ(*obj(gear).attached_to, unit);
    EXPECT_EQ(obj(unit).attachments, std::vector<GameObjectId>{gear});
    EXPECT_TRUE(obj(gear).isAtBase());
    EXPECT_EQ(obj(unit).temp_might_bonus, 0) << "a recall is not a move (CR 458.1)";
    EXPECT_EQ(obj(unit).moves_this_turn, 0);
    EXPECT_TRUE(obj(unit).is_exhausted);
}

// The same replacement on a lethal-damage death (GameEngine::killUnit).
TEST_F(SweepTriggersTest, Recall_DeathReplacementOnLethalDamageKeepsGearAndIsNotAMove) {
    setBattlefieldCard(0, kBackAlleyBar);
    auto unit = addUnit(P1, kInvalidId, 3, 0);
    auto gear = addAttachedGear(P1, unit);
    obj(unit).death_replacement_recall_pending = true;
    obj(unit).damage_marked = 5;

    withEngine([&](GameEngine& e) { e.testHook_cleanup(); e.testHook_runChain(); });

    EXPECT_TRUE(atBase(unit));
    ASSERT_TRUE(obj(gear).attached_to.has_value()) << "a recalled unit keeps its gear (CR 719.3.a)";
    EXPECT_EQ(*obj(gear).attached_to, unit);
    EXPECT_EQ(obj(unit).temp_might_bonus, 0) << "a recall is not a move (CR 458.1)";
    EXPECT_EQ(obj(unit).damage_marked, 0);
}

// Soraka, Wanderer 550: "instead heal it, exhaust it, and recall it. (Send it
// to base. This isn't a move.)"
TEST_F(SweepTriggersTest, Soraka_RecallsTheUnitWithoutMovingIt) {
    setBattlefieldCard(0, kBackAlleyBar);
    addUnit(P1, kSoraka, 4, 0);
    auto unit = addUnit(P1, kInvalidId, 1, 0);
    auto gear = addAttachedGear(P1, unit);

    resolveThroughEngine([&](EffectExecutor& exec, GameState&) { exec.killObject(unit); });

    ASSERT_TRUE(atBase(unit)) << "Soraka replaces the death with a recall";
    EXPECT_EQ(obj(unit).moves_this_turn, 0) << "a recall is not a move (CR 458)";
    EXPECT_EQ(obj(unit).temp_might_bonus, 0) << "Back-Alley Bar triggers on moves only";
    ASSERT_TRUE(obj(gear).attached_to.has_value());
    EXPECT_EQ(*obj(gear).attached_to, unit);
}

// Vilemaw's Lair 290: "Units can't move from here to base." — an effect's
// move is a move.
TEST_F(SweepTriggersTest, MoveToBase_AnEffectCannotMoveAUnitOutOfVilemawsLair) {
    state.battlefields[0].blocks_move_to_base = true;
    auto unit = addUnit(P1, kInvalidId, 3, 0);
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) { exec.moveToBase(unit); });
    EXPECT_FALSE(atBase(unit));
    EXPECT_EQ(obj(unit).moves_this_turn, 0);
}

// Minotaur Reckoner 337: "Units can't move to base."
TEST_F(SweepTriggersTest, MinotaurReckoner_StopsEffectMovesToBase) {
    addUnit(P2, kMinotaur, 0, -1);
    auto unit = addUnit(P1, kInvalidId, 3, 0);
    withEngine([&](GameEngine& e) {
        e.testHook_cleanup();                       // the Reckoner's continuous effect
        e.testHook_executor().moveToBase(unit);
    });
    EXPECT_FALSE(atBase(unit));
}

// ═══════════════════════════════════════════════════════════════════════════
// 2. "hold / conquer effects for ... here trigger an additional time"
// ═══════════════════════════════════════════════════════════════════════════

// Blue Sentinel 649: its own "When I hold" is a hold effect for holding here,
// so it triggers twice.
TEST_F(SweepTriggersTest, BlueSentinel_HoldEffectsHereTriggerAnAdditionalTime) {
    auto sentinel = addUnit(P1, kBlueSentinel, 0, 0);
    state.battlefields[0].controller = P1;
    withEngine([&](GameEngine& e) { e.testHook_scoreHold(P1, BattlefieldId::fromRaw(0)); e.testHook_runChain(); });
    int scheduled = 0;
    for (const auto& da : state.delayed_abilities)
        if (da.source == sentinel && da.trigger == TriggerType::AtStartOfMain) ++scheduled;
    EXPECT_EQ(scheduled, 2);
}

// A hold somewhere else is not "holding here".
TEST_F(SweepTriggersTest, BlueSentinel_DoesNotDoubleAHoldAtAnotherBattlefield) {
    addUnit(P1, kBlueSentinel, 0, 0);
    auto other = addUnit(P1, kBlueSentinel, 0, 1);
    state.battlefields[1].controller = P1;
    // The Sentinel at battlefield 0 adds nothing to a hold at battlefield 1.
    withEngine([&](GameEngine& e) { e.testHook_scoreHold(P1, BattlefieldId::fromRaw(1)); e.testHook_runChain(); });
    int scheduled = 0;
    for (const auto& da : state.delayed_abilities)
        if (da.source == other) ++scheduled;
    EXPECT_EQ(scheduled, 2) << "one extra time for the one Sentinel here, not two";
}

// Red Brambleback 591: "When I conquer, [Buff] a friendly unit" triggers twice.
TEST_F(SweepTriggersTest, RedBrambleback_ConquerEffectsHereTriggerAnAdditionalTime) {
    auto bramble = addUnit(P1, kRedBrambleback, 0, 0);
    auto other = addUnit(P1, kInvalidId, 2, -1);
    // The first trigger buffs the Brambleback, the second the other unit.
    auto asked = std::make_shared<int>(0);
    EnginePathAgent agent([=](const std::vector<Intent>& legal) {
        const GameObjectId want = ((*asked)++ % 2 == 0) ? bramble : other;
        for (const auto& i : legal)
            if (!i.chosen_objects.empty() && i.chosen_objects.front() == want) return i;
        return legal.front();
    });
    withEngine([&](GameEngine& e) { e.testHook_scoreConquer(P1, BattlefieldId::fromRaw(0)); e.testHook_runChain(); },
               &agent);
    EXPECT_EQ(obj(bramble).buff_count, 1);
    EXPECT_EQ(obj(other).buff_count, 1) << "the conquer effect triggered a second time";
}

// ═══════════════════════════════════════════════════════════════════════════
// 5. Simultaneous deaths
// ═══════════════════════════════════════════════════════════════════════════

// Battle Mistress 517: "When one or more enemy units die, ready me." — once
// for a batch of deaths.
TEST_F(SweepTriggersTest, BattleMistress_TriggersOnceForABatchOfDeaths) {
    auto legend = addLegend(P1, kBattleMistress);
    auto a = addUnit(P2, kInvalidId, 1, 0);
    auto b = addUnit(P2, kInvalidId, 1, 0);
    obj(a).damage_marked = 3;
    obj(b).damage_marked = 3;
    int queued = 0;
    withEngine([&](GameEngine& e) {
        e.testHook_processLethalDamage();
        for (const auto& ci : e.mutableState().chain.items)
            if (ci.source == legend) ++queued;
        e.testHook_runChain();
    });
    EXPECT_EQ(queued, 1);
}

// Loyal Poro 718: two Poros that die together each had the other for company.
TEST_F(SweepTriggersTest, LoyalPoro_UnitsThatDieTogetherWereNotAlone) {
    stockDeck(P1, 4);
    auto a = addUnit(P1, kLoyalPoro, 1, 0);
    auto b = addUnit(P1, kLoyalPoro, 1, 0);
    obj(a).damage_marked = 3;
    obj(b).damage_marked = 3;
    const int before = handSize(P1);
    withEngine([&](GameEngine& e) { e.testHook_processLethalDamage(); e.testHook_runChain(); });
    EXPECT_EQ(handSize(P1) - before, 2) << "each died with another friendly unit here";
}

// Lonely Poro 777: neither of two Poros that die together died alone.
TEST_F(SweepTriggersTest, LonelyPoro_UnitsThatDieTogetherDidNotDieAlone) {
    stockDeck(P1, 4);
    auto a = addUnit(P1, kLonelyPoro, 1, 0);
    auto b = addUnit(P1, kLonelyPoro, 1, 0);
    obj(a).damage_marked = 3;
    obj(b).damage_marked = 3;
    const int before = handSize(P1);
    withEngine([&](GameEngine& e) { e.testHook_processLethalDamage(); e.testHook_runChain(); });
    EXPECT_EQ(handSize(P1) - before, 0);
}

TEST_F(SweepTriggersTest, LonelyPoro_DrawsWhenItDiesAlone) {
    stockDeck(P1, 4);
    auto a = addUnit(P1, kLonelyPoro, 1, 0);
    addUnit(P1, kInvalidId, 3, 1);                 // a friendly unit somewhere else
    obj(a).damage_marked = 3;
    const int before = handSize(P1);
    withEngine([&](GameEngine& e) { e.testHook_processLethalDamage(); e.testHook_runChain(); });
    EXPECT_EQ(handSize(P1) - before, 1);
}

// LeBlanc, Everywhere at Once — "your [Temporary] effects at my battlefield
// don't trigger" — stops the [Temporary] ability and nothing else: a
// [Temporary] unit's own "When I die" triggers there as anywhere.
// (D55, testing phase. Old expectation: the [Deathknell] does not trigger —
// that pinned the defect; the card's sentence names [Temporary] effects.)
TEST_F(SweepTriggersTest, OwnDeathTrigger_OfATemporaryUnitAtLeBlancsBattlefieldStillTriggers) {
    addUnit(P1, kLeBlanc, 4, 0);
    auto crab = addUnit(P1, kScuttleCrab, 1, 0);
    obj(crab).keywords.set(Keyword::Temporary);
    const int xp = state.player(P1).xp;
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) { exec.killObject(crab); });
    EXPECT_EQ(state.player(P1).xp, xp + 1) << "its [Deathknell] is not a [Temporary] effect";
}


// ═══════════════════════════════════════════════════════════════════════════
// 6. A token is played (CR 350.2)
// ═══════════════════════════════════════════════════════════════════════════

// Reluctant Leader 908 ("When you play another unit") and Lillia 620 ("When
// you play a token unit") see a unit token being played.
TEST_F(SweepTriggersTest, TokenUnit_IsAUnitBeingPlayed) {
    auto leader = addUnit(P1, kReluctantLeader, 3, -1);
    auto lillia = addUnit(P1, kLillia, 3, -1);
    auto theirs = addUnit(P2, kReluctantLeader, 3, -1);
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) {
        exec.createToken(P1, CardType::Unit, "Recruit", 1, {}, KeywordSet{}, base(P1));
    });
    EXPECT_EQ(obj(leader).temp_might_bonus, 2);
    EXPECT_EQ(obj(lillia).temp_might_bonus, 1);
    EXPECT_EQ(obj(theirs).temp_might_bonus, 0) << "the other player did not play a unit";
}

// Vex, Apathetic 712: "When an opponent plays a unit ..., [Stun] it." — a
// token unit too, and it is the trigger's subject.
TEST_F(SweepTriggersTest, TokenUnit_IsAUnitTheOpponentPlayed) {
    addUnit(P2, 712, 4, 0);
    GameObjectId token = kInvalidId;
    withEngine([&](GameEngine& e) {
        token = e.testHook_executor().createToken(P1, CardType::Unit, "Recruit", 1, {},
                                                  KeywordSet{}, battlefield(0));
        e.testHook_runChain();
    });
    ASSERT_NE(token, kInvalidId);
    EXPECT_TRUE(obj(token).is_stunned);
}

// A gear token is not a unit.
TEST_F(SweepTriggersTest, TokenGear_DoesNotTriggerPlayAUnit) {
    auto leader = addUnit(P1, kReluctantLeader, 3, -1);
    auto lillia = addUnit(P1, kLillia, 3, -1);
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) {
        exec.createToken(P1, CardType::Gear, "Gold", 0, {}, KeywordSet{}, base(P1));
    });
    EXPECT_EQ(obj(leader).temp_might_bonus, 0);
    EXPECT_EQ(obj(lillia).temp_might_bonus, 0);
}

// Zilean, Time Mage 648: the additional copy is a "may", and when the
// instruction let the controller choose where the token goes the copy gets
// the same choice.
TEST_F(SweepTriggersTest, Zilean_AdditionalCopyIsAskedAndPlacedByChoice) {
    addUnit(P1, kZilean, 5, 0);
    state.battlefields[0].controller = P1;
    EnginePathAgent agent(EnginePathAgent::script({1 /*yes*/, 1 /*second location*/}));
    withEngine([&](GameEngine& e) {
        e.testHook_cleanup();                       // Zilean's continuous effect
        e.testHook_executor().createToken(P1, CardType::Unit, "Recruit", 1, {}, KeywordSet{},
                                          base(P1), false, {base(P1), battlefield(0)});
    }, &agent);
    const auto tokens = tokenUnits(P1);
    ASSERT_EQ(tokens.size(), 2u);
    int in_base = 0, at_bf = 0;
    for (auto t : tokens) (obj(t).isAtBase() ? in_base : at_bf)++;
    EXPECT_EQ(in_base, 1);
    EXPECT_EQ(at_bf, 1) << "the copy went where its controller chose";
    EXPECT_GE(agent.prompts.size(), 2u);
}

TEST_F(SweepTriggersTest, Zilean_AdditionalCopyCanBeDeclined) {
    addUnit(P1, kZilean, 5, 0);
    state.battlefields[0].controller = P1;
    EnginePathAgent agent(EnginePathAgent::script({0 /*no*/}));
    withEngine([&](GameEngine& e) {
        e.testHook_cleanup();
        e.testHook_executor().createToken(P1, CardType::Unit, "Recruit", 1, {}, KeywordSet{},
                                          base(P1));
    }, &agent);
    EXPECT_EQ(tokenUnits(P1).size(), 1u);
    EXPECT_EQ(state.player(P1).additional_token_copies_used, 0)
        << "declining does not use up the once-each-turn effect";
}

// ═══════════════════════════════════════════════════════════════════════════
// 7. Who killed it; replacements that belong to the unit
// ═══════════════════════════════════════════════════════════════════════════

// Solari Shrine 72: "When YOU kill a stunned enemy unit".
TEST_F(SweepTriggersTest, SolariShrine_TriggersOnlyWhenItsControllerKilledTheUnit) {
    stockDeck(P1, 4);
    auto shrine = addGear(P1, kSolariShrine);
    auto mine = addUnit(P2, kInvalidId, 2, 0);
    obj(mine).is_stunned = true;
    obj(mine).damage_marked = 5;
    obj(mine).damage_marked_by[0] = 5;             // P1's damage is lethal
    EnginePathAgent yes(EnginePathAgent::script({1}));
    const int before = handSize(P1);
    withEngine([&](GameEngine& e) { e.testHook_processLethalDamage(); e.testHook_runChain(); },
               &yes);
    EXPECT_EQ(handSize(P1) - before, 1);
    EXPECT_TRUE(obj(shrine).is_exhausted);
    EXPECT_EQ(obj(mine).last_known.killed_by, P1);
}

TEST_F(SweepTriggersTest, SolariShrine_DoesNotTriggerWhenTheOpponentKilledTheirOwnUnit) {
    stockDeck(P1, 4);
    auto shrine = addGear(P1, kSolariShrine);
    auto theirs = addUnit(P2, kInvalidId, 2, 0);
    obj(theirs).is_stunned = true;
    obj(theirs).damage_marked = 5;
    obj(theirs).damage_marked_by[1] = 5;           // P2's own damage
    EnginePathAgent yes(EnginePathAgent::script({1}));
    const int before = handSize(P1);
    withEngine([&](GameEngine& e) { e.testHook_processLethalDamage(); e.testHook_runChain(); },
               &yes);
    EXPECT_EQ(handSize(P1) - before, 0);
    EXPECT_FALSE(obj(shrine).is_exhausted);
    EXPECT_EQ(obj(theirs).last_known.killed_by, P2);
}

// Unlicensed Armory 23: the replacement is on the chosen unit, so it applies
// with no Armory in play.
TEST_F(SweepTriggersTest, UnlicensedArmory_ReplacementAppliesWithTheGearGone) {
    auto unit = addUnit(P1, kInvalidId, 2, 0);
    obj(unit).pending_death_replacements.push_back(
        {kUnlicensedArmory, P1, kInvalidId, state.turn.turn_number});
    state.player(P1).rune_pool.universal_power = 1;
    EnginePathAgent yes(EnginePathAgent::script({1}));
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) { exec.killObject(unit); }, &yes);
    EXPECT_TRUE(atBase(unit));
    EXPECT_TRUE(obj(unit).is_exhausted);
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 0) << "[C] was paid";
    EXPECT_TRUE(obj(unit).pending_death_replacements.empty()) << "the next time only";
}

TEST_F(SweepTriggersTest, UnlicensedArmory_ReplacementEndsWithItsTurn) {
    auto unit = addUnit(P1, kInvalidId, 2, 0);
    obj(unit).pending_death_replacements.push_back(
        {kUnlicensedArmory, P1, kInvalidId, state.turn.turn_number - 1});
    state.player(P1).rune_pool.universal_power = 1;
    EnginePathAgent yes(EnginePathAgent::script({1}));
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) { exec.killObject(unit); }, &yes);
    EXPECT_FALSE(obj(unit).location.has_value()) << "it died";
}

// ═══════════════════════════════════════════════════════════════════════════
// 8. Keyword abilities: [Vision] (CR 817), [Weaponmaster] (CR 821)
// ═══════════════════════════════════════════════════════════════════════════

// The controller decides whether the card looked at is recycled.
TEST_F(SweepTriggersTest, Vision_TheControllerChoosesWhetherToRecycle) {
    addUnit(P1, kGemcraftSeer, 3, -1);              // "Other friendly units have [Vision]."
    stockDeck(P1, 3);
    const GameObjectId top = state.player(P1).main_deck.back();
    auto card = addToHand(P1, kLoyalPoro);
    EnginePathAgent keep(&SweepTriggersTest::keepOnTop);
    playThroughEngine(card, &keep);
    EXPECT_EQ(keep.prompts.size(), 1u) << "one instance of [Vision], one Predict";
    EXPECT_EQ(state.player(P1).main_deck.back(), top) << "kept on top";

    auto card2 = addToHand(P1, kLoyalPoro);
    EnginePathAgent recycle(&SweepTriggersTest::recycleIt);
    playThroughEngine(card2, &recycle);
    EXPECT_EQ(state.player(P1).main_deck.front(), top) << "recycled to the bottom";
}

// CR 817.2: each instance of [Vision] triggers separately.
TEST_F(SweepTriggersTest, Vision_EachInstanceTriggersSeparately) {
    addUnit(P1, kGemcraftSeer, 3, -1);
    addUnit(P1, kGemcraftSeer, 3, -1);
    stockDeck(P1, 3);
    auto card = addToHand(P1, kLoyalPoro);
    EnginePathAgent keep(&SweepTriggersTest::keepOnTop);
    playThroughEngine(card, &keep);
    EXPECT_EQ(keep.prompts.size(), 2u);
}

// Forecaster 388 has [Vision] once: from its own "Your Mechs have [Vision]".
TEST_F(SweepTriggersTest, Forecaster_HasOneInstanceOfVision) {
    EXPECT_FALSE(card_db.get(kForecaster).keywords.has(Keyword::Vision));
    stockDeck(P1, 3);
    auto card = addToHand(P1, kForecaster);
    EnginePathAgent keep(&SweepTriggersTest::keepOnTop);
    playThroughEngine(card, &keep);
    EXPECT_EQ(keep.prompts.size(), 1u);
}

// Emperor of the Sands 512: "Your Sand Soldiers have [Weaponmaster]." — a
// token has no card class, so the keyword's own ability does the work.
TEST_F(SweepTriggersTest, Weaponmaster_ASandSoldierTokenMayEquipAsItIsPlayed) {
    addLegend(P1, kEmperor);
    auto gear = addGear(P1, kInvalidId, /*equipment=*/true);
    EnginePathAgent agent(objectOrYes(gear));   // yes, then the gear
    withEngine([&](GameEngine& e) {
        e.testHook_executor().createToken(P1, CardType::Unit, "Sand Soldier", 2, {"Shurima"},
                                          KeywordSet{}, base(P1));
        e.testHook_runChain();
    }, &agent);
    const auto tokens = tokenUnits(P1);
    ASSERT_EQ(tokens.size(), 1u);
    ASSERT_TRUE(obj(gear).attached_to.has_value());
    EXPECT_EQ(*obj(gear).attached_to, tokens.front());
}

TEST_F(SweepTriggersTest, Weaponmaster_IsAMay) {
    addLegend(P1, kEmperor);
    auto gear = addGear(P1, kInvalidId, /*equipment=*/true);
    EnginePathAgent no(EnginePathAgent::script({0}));
    withEngine([&](GameEngine& e) {
        e.testHook_executor().createToken(P1, CardType::Unit, "Sand Soldier", 2, {"Shurima"},
                                          KeywordSet{}, base(P1));
        e.testHook_runChain();
    }, &no);
    EXPECT_FALSE(obj(gear).attached_to.has_value());
    EXPECT_EQ(no.prompts.size(), 1u);
}

// ═══════════════════════════════════════════════════════════════════════════
// 9. New trigger sources
// ═══════════════════════════════════════════════════════════════════════════

// Renekton, Brute 879: "When my Might becomes 10 or more" — whatever raised it.
TEST_F(SweepTriggersTest, Renekton_IsEmpoweredWhenAnotherEffectTakesHimToTen) {
    auto renekton = addUnit(P1, kRenektonBrute, 9, 0);
    withEngine([&](GameEngine& e) {
        e.testHook_cleanup();
        e.testHook_runChain();
        EXPECT_FALSE(e.mutableState().getObject(renekton).isEmpowered());
        e.testHook_executor().giveTemporaryMight(renekton, 1);   // not his own ability
        e.testHook_cleanup();
        e.testHook_runChain();
    });
    EXPECT_TRUE(obj(renekton).isEmpowered());
}

// Fae Dragon 423: "When you spend a buff, play a Gold gear token exhausted."
TEST_F(SweepTriggersTest, FaeDragon_PlaysGoldWhenItsControllerSpendsABuff) {
    addUnit(P1, kFaeDragon, 7, -1);
    auto buffed = addUnit(P1, kInvalidId, 2, -1);
    obj(buffed).buff_count = 1;
    auto theirs = addUnit(P2, kInvalidId, 2, -1);
    obj(theirs).buff_count = 1;
    auto gold = [&]() {
        int n = 0;
        for (const auto& [id, o] : state.objects)
            if (o.isGear() && o.isToken() && o.controller == P1 && o.location.has_value()) ++n;
        return n;
    };
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) {
        EXPECT_TRUE(exec.spendBuff(buffed, P1));
    });
    EXPECT_EQ(obj(buffed).buff_count, 0);
    EXPECT_EQ(gold(), 1);
    // The other player spending a buff is not "you".
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) {
        EXPECT_TRUE(exec.spendBuff(theirs, P2));
        EXPECT_FALSE(exec.spendBuff(theirs, P2)) << "nothing left to spend";
    });
    EXPECT_EQ(gold(), 1);
}

// Vex, Mocking 617 through a real stun; stunning your own unit is not
// stunning an enemy.
TEST_F(SweepTriggersTest, WhenYouStun_FiresForAnEnemyUnitWithItAsTheSubject) {
    auto vex = addUnit(P1, kVexMocking, 5, 0);
    auto enemy = addUnit(P2, kInvalidId, 3, 1);
    auto own = addUnit(P1, kInvalidId, 3, 1);
    EnginePathAgent yes(EnginePathAgent::script({1}));
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) { exec.stunUnitBy(own, vex); },
                         &yes);
    ASSERT_TRUE(obj(vex).battlefieldId().has_value());
    EXPECT_EQ(*obj(vex).battlefieldId(), state.battlefields[0].id);
    EXPECT_TRUE(yes.prompts.empty());

    resolveThroughEngine([&](EffectExecutor& exec, GameState&) { exec.stunUnitBy(enemy, vex); },
                         &yes);
    ASSERT_TRUE(obj(vex).battlefieldId().has_value());
    EXPECT_EQ(*obj(vex).battlefieldId(), state.battlefields[1].id);
}

// Darius, Trifarian 27: he can be "your second card" himself.
TEST_F(SweepTriggersTest, Darius_SeesHimselfAsTheSecondCard) {
    state.player(P1).cards_played_this_turn = 1;
    auto darius = addToHand(P1, kDariusTrifarian);
    playThroughEngine(darius);
    EXPECT_EQ(obj(darius).temp_might_bonus, 2);
    EXPECT_FALSE(obj(darius).is_exhausted) << "ready me";
}

TEST_F(SweepTriggersTest, Darius_IsNotTriggeredAsTheFirstCard) {
    auto darius = addToHand(P1, kDariusTrifarian);
    playThroughEngine(darius);
    EXPECT_EQ(obj(darius).temp_might_bonus, 0);
    EXPECT_TRUE(obj(darius).is_exhausted);
}

// Scuttle Crab 615: "You can look at their facedown cards this turn" covers a
// card hidden later in the turn.
TEST_F(SweepTriggersTest, ScuttleCrab_ShowsCardsHiddenLaterThatTurn) {
    auto crab = addUnit(P1, kScuttleCrab, 1, 0);
    auto hidden = addToHand(P2, kLoyalPoro);
    int shown = 0;
    auto conn = events.on_card_revealed.connect([&](const CardRevealedEvent& e) {
        // (The [Deathknell] also shows P2's hand, which this card is in.)
        if (e.card == hidden && !e.revealed_to_all && e.revealed_to == P1 &&
            e.source_zone == ZoneType::FacedownZone) ++shown;
    });
    withEngine([&](GameEngine& e) {
        e.testHook_executor().killObject(crab);
        e.testHook_runChain();
        events.emit(CardHiddenEvent{hidden, P2});
    });
    conn.disconnect();
    EXPECT_EQ(shown, 1);
    // Without the permission nothing is shown.
    state.player(P1).may_look_at_opp_facedown_turn = -1;
    withEngine([&](GameEngine&) { events.emit(CardHiddenEvent{hidden, P2}); });
    EXPECT_EQ(shown, 1);
}

// ═══════════════════════════════════════════════════════════════════════════
// 10. Delayed abilities
// ═══════════════════════════════════════════════════════════════════════════

// An ability nobody dated ends with the turn; kUntilFired does not.
TEST_F(SweepTriggersTest, DelayedAbility_DefaultExpiresAtEndOfTurn) {
    auto src = addUnit(P1, kLoyalPoro, 1, -1);
    DelayedAbility dated;
    dated.source = src; dated.card_def_id = kLoyalPoro; dated.controller = P1;
    dated.trigger = TriggerType::WhenIConquer;
    DelayedAbility standing = dated;
    standing.expires_on_turn = DelayedAbility::kUntilFired;
    DelayedAbility past = dated;
    past.expires_on_turn = state.turn.turn_number - 1;
    DelayedAbility future = dated;
    future.expires_on_turn = state.turn.turn_number + 2;
    state.delayed_abilities = {dated, standing, past, future};
    withEngine([&](GameEngine& e) { e.testHook_expirationStep(); });
    ASSERT_EQ(state.delayed_abilities.size(), 2u);
    EXPECT_EQ(state.delayed_abilities[0].expires_on_turn, DelayedAbility::kUntilFired);
    EXPECT_EQ(state.delayed_abilities[1].expires_on_turn, future.expires_on_turn);
}

// ═══════════════════════════════════════════════════════════════════════════
// 11. Executor primitives
// ═══════════════════════════════════════════════════════════════════════════

// recycleCards takes a permanent off the board like any other departure.
TEST_F(SweepTriggersTest, RecycleCards_APermanentLeavesTheBoardProperly) {
    auto unit = addUnit(P1, kLoyalPoro, 1, 0);
    auto gear = addAttachedGear(P1, unit);
    auto token = addUnit(P1, kInvalidId, 1, 0);
    obj(token).super_type = SuperType::Token;
    ASSERT_TRUE(obj(token).isToken());
    std::vector<GameObjectId> left;
    auto conn = events.on_left_board.connect([&](const LeftBoardEvent& e) { left.push_back(e.object); });
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) {
        exec.recycleCards(P1, {unit, token});
    });
    conn.disconnect();
    EXPECT_TRUE(inDeck(P1, unit));
    EXPECT_FALSE(obj(unit).location.has_value());
    EXPECT_FALSE(obj(gear).attached_to.has_value()) << "CR 719.5";
    EXPECT_TRUE(obj(gear).location.has_value()) << "the gear stays on the board";
    EXPECT_TRUE(obj(unit).attachments.empty());
    EXPECT_FALSE(inDeck(P1, token)) << "a token ceases to exist (CR 183.1)";
    EXPECT_FALSE(obj(token).location.has_value());
    EXPECT_NE(std::find(left.begin(), left.end(), unit), left.end());
    EXPECT_NE(std::find(left.begin(), left.end(), token), left.end());
}

// An effect-played unit that says "I enter ready" enters ready.
TEST_F(SweepTriggersTest, PlayIgnoringCost_AsksTheUnitWhetherItEntersReady) {
    auto yi = addToHand(P1, kYiHoned);
    auto plain = addToHand(P1, kLoyalPoro);
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) {
        exec.playIgnoringCost(P1, yi);
        exec.playIgnoringCost(P1, plain);
    });
    ASSERT_TRUE(obj(yi).location.has_value());
    EXPECT_FALSE(obj(yi).is_exhausted);
    ASSERT_TRUE(obj(plain).location.has_value());
    EXPECT_TRUE(obj(plain).is_exhausted) << "CR 143.4";
}

// [Deflect] given "this turn" is a field of its own and ends with the turn.
TEST_F(SweepTriggersTest, TemporaryDeflect_EndsAtTheExpirationStep) {
    auto unit = addUnit(P1, kInvalidId, 2, 0);
    withEngine([&](GameEngine& e) {
        e.testHook_executor().giveTemporaryKeyword(unit, Keyword::Deflect, 1);
        e.testHook_executor().giveTemporaryKeyword(unit, Keyword::Deflect, 1);
        EXPECT_EQ(e.mutableState().getObject(unit).temp_deflect_value, 2);
        EXPECT_EQ(e.testHook_executor().deflectCostFor(P2, unit), 2);
        e.testHook_expirationStep();
        EXPECT_EQ(e.mutableState().getObject(unit).temp_deflect_value, 0);
        EXPECT_EQ(e.testHook_executor().deflectCostFor(P2, unit), 0);
    });
}

// Every attach is the executor's: both ends on the board, text Inactive.
TEST_F(SweepTriggersTest, AttachGear_RefusesAUnitThatIsNotOnTheBoard) {
    auto gear = addGear(P1, kInvalidId, true);
    auto in_hand = addToHand(P1, kLoyalPoro);
    auto on_board = addUnit(P1, kInvalidId, 2, 0);
    auto other = addUnit(P1, kInvalidId, 2, 1);
    withEngine([&](GameEngine& e) {
        auto& exec = e.testHook_executor();
        EXPECT_FALSE(exec.attachGear(gear, in_hand));
        EXPECT_FALSE(e.mutableState().getObject(gear).attached_to.has_value());
        EXPECT_TRUE(exec.attachGear(gear, on_board));
        EXPECT_TRUE(e.mutableState().getObject(gear).is_rules_text_inactive);
        // Re-attached elsewhere: it comes off the first unit (CR 718.5.d).
        EXPECT_TRUE(exec.attachGear(gear, other));
    });
    EXPECT_TRUE(obj(in_hand).attachments.empty());
    EXPECT_TRUE(obj(on_board).attachments.empty());
    EXPECT_EQ(obj(other).attachments, std::vector<GameObjectId>{gear});
    EXPECT_EQ(obj(gear).location, obj(other).location);
}

// Blighted Battleaxe 581: its end-of-turn ability is effect text, so it
// applies while attached (when the gear's printed text is Inactive).
TEST_F(SweepTriggersTest, BlightedBattleaxe_EndOfTurnAbilityFiresWhileAttached) {
    auto unit = addUnit(P1, kInvalidId, 6, 0);
    auto axe = addGear(P1, kBlightedBattleaxe);
    withEngine([&](GameEngine& e) {
        ASSERT_TRUE(e.testHook_executor().attachGear(axe, unit));
        e.testHook_endingStep();
        e.testHook_runChain();
    });
    EXPECT_FALSE(obj(axe).attached_to.has_value());
    EXPECT_EQ(obj(unit).damage_marked, 4);
}

// Called Shot 443: "Draw one" is a draw.
TEST_F(SweepTriggersTest, CalledShot_DrawingOneCountsAsADraw) {
    stockDeck(P1, 3);
    auto spell = addToHand(P1, kCalledShot);
    const int draws = state.player(P1).draws_this_turn;
    int drawn_events = 0;
    auto conn = events.on_cards_drawn.connect([&](const CardsDrawnEvent& e) {
        if (e.player == P1) drawn_events += e.count;
    });
    playThroughEngine(spell);
    conn.disconnect();
    EXPECT_EQ(state.player(P1).draws_this_turn - draws, 1);
    EXPECT_EQ(drawn_events, 1);
    EXPECT_EQ(handSize(P1), 1);
    EXPECT_EQ(deckSize(P1), 2);
}

// "... reveal a <card> from among them and draw it": a draw, for Ornn,
// Blacksmith 381, Ivern, Nurturer 613 and Fate Weaver 626 alike.
TEST_F(SweepTriggersTest, RevealAndDrawIt_CountsAsADraw) {
    struct Case { CardDefId unit; TriggerType trigger; CardDefId wanted; };
    const Case cases[] = {
        {381, TriggerType::WhenIHold, kShepherdsHeirloom},     // a gear
        {613, TriggerType::WhenIHold, kLoyalPoro},             // a unit
        {626, TriggerType::WhenYouPlayMe, kDeathFromBelow},    // a spell costing [4] or more
    };
    for (const auto& c : cases) {
        SCOPED_TRACE(card_db.get(c.unit).name);
        auto source = addUnit(P1, c.unit, 3, 0);
        stockDeck(P1, 2);
        auto wanted = addToDeck(P1, c.wanted);              // on top
        const int draws = state.player(P1).draws_this_turn;
        int drawn_events = 0;
        auto conn = events.on_cards_drawn.connect([&](const CardsDrawnEvent& e) {
            if (e.player == P1) drawn_events += e.count;
        });
        EnginePathAgent agent(objectOrYes(wanted));   // yes, and that card
        fireTriggerThroughEngine(source, c.trigger, &agent);
        conn.disconnect();
        EXPECT_TRUE(inHand(P1, wanted));
        EXPECT_EQ(state.player(P1).draws_this_turn - draws, 1);
        EXPECT_EQ(drawn_events, 1);
    }
}

// "Gain N XP" always counts toward "if you've gained XP this turn".
TEST_F(SweepTriggersTest, GainXp_IsCountedForTheTurn) {
    auto heirloom = addToHand(P1, kShepherdsHeirloom);
    playThroughEngine(heirloom);
    EXPECT_EQ(state.player(P1).xp, 1);
    EXPECT_EQ(state.player(P1).xp_gained_this_turn, 1);
}

// ═══════════════════════════════════════════════════════════════════════════
// 12. Chain
// ═══════════════════════════════════════════════════════════════════════════

// Dancing Grenade 582: "Its controller may play this spell again for [A]."
// has no "do this" — it is not a reflexive trigger (CR 388.1) but a play made
// as part of the spell's resolution (CR 420.3), while the card is still on the
// chain (it goes to the trash only once its effect has been executed, CR
// 359.3.d). So the card is on the chain for the new play, never in a trash
// meanwhile, and is filed once, in its owner's trash, when that play resolves.
TEST_F(SweepTriggersTest, ReplayedSpell_StaysOnTheChainUntilTheNewPlayResolves) {
    constexpr CardDefId kDancingGrenade = 582;
    EXPECT_FALSE(card_registry.get(kDancingGrenade)->hasReflexiveTrigger());
    auto theirs = addUnit(P2, kInvalidId, 6, 0);
    auto mine = addUnit(P1, kInvalidId, 6, 0);
    auto spell = addToHand(P1, kDancingGrenade);
    state.player(P2).rune_pool.universal_power = 1;        // their [A] for the replay
    // P1: the first target. P2: yes to the replay, then its target; passes
    // otherwise, and notes where the card is whenever it has a play on the chain.
    EnginePathAgent p1([&](const std::vector<Intent>& legal) {
        for (const auto& i : legal)
            if (!i.chosen_objects.empty() && i.chosen_objects.front() == theirs) return i;
        ADD_FAILURE() << "P1 was asked something other than its target";
        return legal.front();
    });
    struct Replayer : AgentInterface {
        GameObjectId spell = kInvalidId, target = kInvalidId;
        int seen_on_chain = 0, seen_in_a_trash = 0, answered = 0;
        Intent selectAction(const GameState& st, const std::vector<Intent>& legal) override {
            bool on_chain = false;
            for (const auto& ci : st.chain.items) if (ci.is_spell && ci.source == spell) on_chain = true;
            if (on_chain) {
                ++seen_on_chain;
                for (const auto& ps : st.players)
                    if (std::find(ps.trash.begin(), ps.trash.end(), spell) != ps.trash.end())
                        ++seen_in_a_trash;
            }
            const bool choice = !legal.empty() && legal.front().type == IntentType::MakeChoice;
            if (choice) {
                ++answered;
                for (const auto& i : legal)
                    if (!i.chosen_objects.empty() && i.chosen_objects.front() == target) return i;
                for (const auto& i : legal) if (i.chosen_value && *i.chosen_value == 1) return i;
                ADD_FAILURE() << "P2 was asked something the test did not expect";
                return legal.front();
            }
            for (const auto& i : legal)
                if (i.type == IntentType::PassPriority || i.type == IntentType::PassFocus) return i;
            return legal.empty() ? Intent{} : legal.front();
        }
    } p2;
    p2.spell = spell;
    p2.target = mine;
    playThroughEngine(spell, &p1, std::nullopt, &p2);
    EXPECT_EQ(obj(theirs).damage_marked, 2);
    EXPECT_EQ(obj(mine).damage_marked, 3) << "the replay: 2, +1 Bonus Damage for the earlier hit";
    EXPECT_EQ(state.player(P2).rune_pool.universal_power, 0);
    EXPECT_EQ(p2.answered, 2) << "the may, then the replay's target";
    EXPECT_GT(p2.seen_on_chain, 0);
    EXPECT_EQ(p2.seen_in_a_trash, 0) << "on the chain for the new play, not filed in between";
    const auto& trash = state.player(P1).trash;
    EXPECT_EQ(std::count(trash.begin(), trash.end(), spell), 1) << "filed once, in its owner's trash";
    EXPECT_TRUE(state.player(P2).trash.empty());
    EXPECT_EQ(obj(spell).zone, ZoneType::Trash);
}

// Decree of Insight 848: "Ignore [Deflect] while paying this spell's cost." —
// also when its target is locked in as it is played.
TEST_F(SweepTriggersTest, IgnoresDeflect_IsHonouredForATargetChosenAsTheSpellIsPlayed) {
    auto enemy = addUnit(P2, kInvalidId, 6, 0);
    obj(enemy).domains = {Domain::Body};
    obj(enemy).keywords.set(Keyword::Deflect);
    obj(enemy).deflect_value = 1;
    auto spell = addToHand(P1, kDecreeOfInsight);
    state.player(P1).rune_pool.energy += card_db.get(kDecreeOfInsight).energy_cost;
    state.player(P1).rune_pool.universal_power = 1;      // could pay for [Deflect]; must not
    Intent play;
    play.type = IntentType::PlayCard;
    play.player = P1;
    play.card = spell;
    play.targets = {enemy};
    EnginePathAgent agent(objectOrYes(enemy));
    withEngine([&](GameEngine& e) { e.testHook_executeIntent(play); }, &agent);
    EXPECT_EQ(obj(enemy).temp_might_bonus, -5) << "no [A] was needed for its [Deflect]";
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 1) << "[Deflect] was not charged";
}

// ═══════════════════════════════════════════════════════════════════════════
// 13. Log lines say who may read them
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(SweepTriggersTest, LogEvent_ADrawIsVisibleOnlyToThePlayerWhoDrew) {
    stockDeck(P1, 2);
    const std::string name = obj(state.player(P1).main_deck.back()).name;
    int private_draw_lines = 0, leaked = 0, public_lines = 0;
    auto conn = events.on_log.connect([&](const LogEvent& e) {
        const bool names_card = e.message.find("DREW: " + name) != std::string::npos;
        if (names_card) {
            if (e.visibleTo(P1) && !e.visibleTo(P2) && !e.isPublic()) ++private_draw_lines;
            else ++leaked;
        } else if (e.isPublic()) {
            ++public_lines;
        }
    });
    withEngine([&](GameEngine& e) { e.testHook_executor().drawCards(P1, 1); });
    conn.disconnect();
    EXPECT_EQ(private_draw_lines, 1);
    EXPECT_EQ(leaked, 0);
    EXPECT_GT(public_lines, 0) << "that a card was drawn is public";
}

TEST(SweepTriggersLogAudience, SaysExactlyWhichPlayersMayRead) {
    const PlayerId p1 = PlayerId::Player1, p2 = PlayerId::Player2;
    EXPECT_TRUE(LogAudience::both().includes(p1) && LogAudience::both().includes(p2));
    EXPECT_TRUE(LogAudience::both().isBoth());
    EXPECT_TRUE(LogAudience::only(p2).includes(p2));
    EXPECT_FALSE(LogAudience::only(p2).includes(p1));
    EXPECT_FALSE(LogAudience::only(p2).isBoth());
    EXPECT_FALSE(LogAudience::only(p2).isNeither());
    EXPECT_TRUE(LogAudience::neither().isNeither());
    EXPECT_FALSE(LogAudience::neither().includes(p1) || LogAudience::neither().includes(p2));
    EXPECT_FALSE(LogAudience::both().includes(PlayerId::None));
    // A plain log line is for both players.
    EventBus bus;
    LogAudience seen = LogAudience::neither();
    auto conn = bus.on_log.connect([&](const LogEvent& e) { seen = e.audience; });
    bus.logTrace("x");
    EXPECT_TRUE(seen.isBoth());
    bus.logTracePrivate(p1, "y");
    EXPECT_TRUE(seen.includes(p1) && !seen.includes(p2));
    bus.logTraceFor(LogAudience::neither(), "z");
    EXPECT_TRUE(seen.isNeither());
}


// ═══════════════════════════════════════════════════════════════════════════
// 14. Svellsongur 382: "copy that unit's text to this Equipment's effect text"
// ═══════════════════════════════════════════════════════════════════════════

// The bearer's triggered ability triggers a second time.
TEST_F(SweepTriggersTest, Svellsongur_TheBearersTriggeredAbilityTriggersTwice) {
    auto leader = addUnit(P1, kReluctantLeader, 3, -1);   // "When you play another unit, +2 [M]"
    auto sword = addGear(P1, kSvellsongur);
    auto plain = addUnit(P1, kReluctantLeader, 3, -1);    // no Equipment: once
    withEngine([&](GameEngine& e) {
        ASSERT_TRUE(e.testHook_executor().attachGear(sword, leader));
        e.testHook_executor().createToken(P1, CardType::Unit, "Recruit", 1, {}, KeywordSet{},
                                          base(P1));
        e.testHook_runChain();
    });
    EXPECT_EQ(obj(leader).temp_might_bonus, 4);
    EXPECT_EQ(obj(plain).temp_might_bonus, 2);
}

// ... its [Deathknell] too, judged as the unit last existed.
TEST_F(SweepTriggersTest, Svellsongur_TheBearersDeathknellTriggersTwice) {
    stockDeck(P1, 4);
    auto poro = addUnit(P1, kLoyalPoro, 1, 0);
    addUnit(P1, kInvalidId, 3, 0);                         // it does not die alone
    auto sword = addGear(P1, kSvellsongur);
    const int before = handSize(P1);
    withEngine([&](GameEngine& e) {
        ASSERT_TRUE(e.testHook_executor().attachGear(sword, poro));
        e.testHook_executor().killObject(poro);
        e.testHook_runChain();
    });
    EXPECT_EQ(handSize(P1) - before, 2);
    EXPECT_FALSE(obj(sword).attached_to.has_value());
}

// The bearer's passive ability applies a second time — for that instance
// only, and to another copy of the same card as well.
TEST_F(SweepTriggersTest, Svellsongur_TheBearersPassiveAbilityAppliesTwice) {
    auto farron = addUnit(P1, kCaptainFarron, 3, 0);
    auto other_farron = addUnit(P1, kCaptainFarron, 3, 0);
    auto ally = addUnit(P1, kInvalidId, 2, 0);
    auto sword = addGear(P1, kSvellsongur);
    auto assaults = [&](GameObjectId id) {
        int n = 0;
        for (const auto& ae : obj(id).aura_effects) if (ae.keyword == Keyword::Assault) ++n;
        return n;
    };
    withEngine([&](GameEngine& e) {
        ASSERT_TRUE(e.testHook_executor().attachGear(sword, farron));
        e.testHook_cleanup();
    });
    EXPECT_EQ(assaults(ally), 3) << "the bearer's twice, the other Farron's once";
    EXPECT_EQ(assaults(other_farron), 2) << "from the bearer, twice";
    EXPECT_EQ(assaults(farron), 1) << "from the other Farron, once";
    EXPECT_EQ(obj(other_farron).card_def_id, kCaptainFarron);

    // "for as long as this is attached to it"
    withEngine([&](GameEngine& e) {
        e.testHook_executor().unattachGear(sword);
        e.testHook_cleanup();
    });
    EXPECT_EQ(assaults(ally), 2);
}

// A printed [Assault N] is text: it counts again.
TEST_F(SweepTriggersTest, Svellsongur_ThePrintedAssaultCountsAgain) {
    auto enforcer = addUnit(P1, kChemtechEnforcer, 0, 0);
    auto sword = addGear(P1, kSvellsongur);
    withEngine([&](GameEngine& e) {
        ASSERT_TRUE(e.testHook_executor().attachGear(sword, enforcer));
        e.testHook_cleanup();
    });
    int extra = 0;
    for (const auto& ae : obj(enforcer).aura_effects)
        if (ae.keyword == Keyword::Assault) extra += ae.keyword_value;
    EXPECT_EQ(extra, card_db.get(kChemtechEnforcer).assault_value);
}

// A second copy of an activated ability allows nothing the first does not:
// the bearer's ability is offered once.
TEST_F(SweepTriggersTest, Svellsongur_AnActivatedAbilityIsOfferedOnce) {
    auto renekton = addUnit(P1, kRenektonBrute, 4, 0);    // "[1]: Give me +1 [M] this turn."
    auto sword = addGear(P1, kSvellsongur);
    state.player(P1).rune_pool.energy = 3;
    int offered = 0;
    withEngine([&](GameEngine& e) {
        ASSERT_TRUE(e.testHook_executor().attachGear(sword, renekton));
        e.testHook_cleanup();
        for (const auto& i : e.generateLegalActions())
            if (i.type == IntentType::ActivateAbility && i.ability_source == renekton) ++offered;
    });
    EXPECT_EQ(offered, 1);
}

// Blue Sentinel's "trigger an additional time" is in the copied text too.
TEST_F(SweepTriggersTest, Svellsongur_CountsAnAdditionalTimeEffectAgain) {
    auto sentinel = addUnit(P1, kBlueSentinel, 0, 0);
    auto sword = addGear(P1, kSvellsongur);
    state.battlefields[0].controller = P1;
    withEngine([&](GameEngine& e) {
        ASSERT_TRUE(e.testHook_executor().attachGear(sword, sentinel));
        e.testHook_scoreHold(P1, BattlefieldId::fromRaw(0));
        e.testHook_runChain();
    });
    int scheduled = 0;
    for (const auto& da : state.delayed_abilities) if (da.source == sentinel) ++scheduled;
    // "When I hold" twice (the card's and the copy's), each two additional times.
    EXPECT_EQ(scheduled, 6);
}


// ═══════════════════════════════════════════════════════════════════════════
// 15. Follow-up (plans/09 section D)
// ═══════════════════════════════════════════════════════════════════════════

// 9.D2: units one effect kills together are one batch. The Ruination 742
// ("Kill all units.") against Loyal Poro, Lonely Poro and Battle Mistress.
TEST_F(SweepTriggersTest, KillAllEffect_UnitsItKillsDiedTogether) {
    stockDeck(P1, 6);
    addUnit(P1, kLoyalPoro, 1, 0);
    addUnit(P1, kLoyalPoro, 1, 0);          // each had the other: 2 draws
    addUnit(P1, kLonelyPoro, 1, 0);         // not alone: no draw
    auto legend = addLegend(P2, kBattleMistress);
    auto spell = addToHand(P2, kTheRuination);
    int legend_abilities = 0;
    TrashWatcher p1;                         // passes; counts nothing useful here
    struct Counter : AgentInterface {
        GameObjectId legend = kInvalidId;
        int* seen = nullptr;
        Intent selectAction(const GameState& st, const std::vector<Intent>& legal) override {
            int n = 0;
            for (const auto& ci : st.chain.items) if (ci.source == legend) ++n;
            *seen = std::max(*seen, n);
            for (const auto& i : legal)
                if (i.type == IntentType::PassPriority || i.type == IntentType::PassFocus) return i;
            return legal.empty() ? Intent{} : legal.front();
        }
    } p2;
    p2.legend = legend;
    p2.seen = &legend_abilities;
    const int before = handSize(P1);
    playThroughEngine(spell, &p1, std::nullopt, &p2);
    EXPECT_EQ(handSize(P1) - before, 2) << "two Loyal Poros that died together; the Lonely Poro did not die alone";
    EXPECT_EQ(legend_abilities, 1) << "\"one or more enemy units die\": once";
}

// 9.D4: the mulligan's recycle is announced, once for the cards set aside.
TEST_F(SweepTriggersTest, Mulligan_AnnouncesTheRecycleOnce) {
    stockDeck(P1, 6);
    auto a = addToHand(P1, kLoyalPoro);
    auto b = addToHand(P1, kLoyalPoro);
    auto karma = addUnit(P1, kKarmaChanneler, 2, -1);
    int announced = 0;
    auto conn = events.on_object_state_changed.connect([&](const ObjectStateChangedEvent& e) {
        if (e.what_changed == "recycled_main") ++announced;
    });
    EnginePathAgent agent(objectOrYes(karma));
    withEngine([&](GameEngine& e) {
        e.testHook_executeIntent(Intent::mulligan(P1, {a, b}));
        e.testHook_runChain();
    }, &agent);
    conn.disconnect();
    EXPECT_EQ(announced, 1);
    EXPECT_TRUE(inDeck(P1, a) && inDeck(P1, b));
    EXPECT_EQ(obj(karma).buff_count, 1) << "\"When you recycle one or more cards to your Main Deck\"";
}

// 9.D4 / 9.D10: a card played in response goes through the callback the engine
// installs (ChainManager::setPlayCard). A play that adds no chain item was
// "not offered again in that priority window"; since the testing phase
// (finding D6) it is an engine error: an offered option must do what it says.
TEST_F(SweepTriggersTest, ReactionPlay_GoesThroughThePlayCallbackAndAFailedPlayIsAnError) {
    auto source = addUnit(P1, kInvalidId, 2, -1);
    auto reaction = addToHand(P2, kDecreeOfInsight);
    ChainManager cm(state, events, card_db);
    std::vector<Intent> played;
    cm.setPlayCard([&](const Intent& i) { played.push_back(i); });   // adds no chain item
    cm.addAbility(source, P1, kInvalidId);
    int p2_asked = 0, p2_offered_play = 0;
    Intent play;
    play.type = IntentType::PlayReaction;
    play.player = P2;
    play.card = reaction;
    EXPECT_THROW(cm.processFEPR(
        [&](PlayerId p, const std::vector<Intent>& legal) -> Intent {
            if (p == P2) {
                ++p2_asked;
                for (const auto& i : legal)
                    if (i.type == IntentType::PlayReaction) { ++p2_offered_play; return i; }
            }
            return Intent::passPriority(p);
        },
        [](const ChainItem&) {},
        [](const ChainItem&) {},
        [&](PlayerId p) {
            std::vector<Intent> out{Intent::passPriority(p)};
            if (p == P2) out.push_back(play);
            return out;
        }), std::logic_error);
    ASSERT_EQ(played.size(), 1u) << "the play went to the engine's play path, once";
    EXPECT_EQ(played[0].card, reaction);
    EXPECT_EQ(p2_offered_play, 1);
    EXPECT_EQ(p2_asked, 1) << "it did not happen: an error, not a second question";
    EXPECT_TRUE(inHand(P2, reaction)) << "the bare fallback path did not run";
}

// 9.D4: the same through the real engine — a [Reaction] spell played in
// response is played like any other card: Feral Strength 357's [Repeat] [2]
// is offered and paid as it is played (only the engine's play path does that).
TEST_F(SweepTriggersTest, ReactionPlay_ThroughTheEngineIsAFullPlay) {
    constexpr CardDefId kFeralStrength = 357;   // "[Reaction] [Repeat] [2] Give a unit +2 [M] this turn."
    auto target = addUnit(P2, kInvalidId, 3, 0);
    auto first = addToHand(P1, kDredgeUp);
    stockDeck(P1, 2);
    auto reaction = addToHand(P2, kFeralStrength);
    state.player(P2).rune_pool.energy = card_db.get(kFeralStrength).energy_cost + 2;
    struct Responder : AgentInterface {
        GameObjectId card = kInvalidId, target = kInvalidId;
        int repeat_prompts = 0;
        Intent selectAction(const GameState&, const std::vector<Intent>& legal) override {
            for (const auto& i : legal)
                if (i.type == IntentType::PlayReaction && i.card == card &&
                    (i.targets.empty() || i.targets.front() == target)) return i;
            for (const auto& i : legal)
                if (i.type == IntentType::MakeChoice && !i.chosen_objects.empty() &&
                    i.chosen_objects.front() == target) return i;
            for (const auto& i : legal)
                if (i.type == IntentType::MakeChoice && i.chosen_value && *i.chosen_value == 1) {
                    ++repeat_prompts;
                    return i;
                }
            for (const auto& i : legal)
                if (i.type == IntentType::PassPriority || i.type == IntentType::PassFocus) return i;
            return legal.empty() ? Intent{} : legal.front();
        }
    } p2;
    p2.card = reaction;
    p2.target = target;
    playThroughEngine(first, nullptr, std::nullopt, &p2);
    EXPECT_TRUE(inTrash(P2, reaction));
    EXPECT_EQ(state.player(P2).cards_played_this_turn, 1);
    EXPECT_GE(p2.repeat_prompts, 1) << "[Repeat] was offered as the card was played";
    EXPECT_EQ(state.player(P2).rune_pool.energy, 0) << "its cost and the Repeat cost were paid";
    EXPECT_EQ(obj(target).temp_might_bonus, 4) << "the effect was repeated";
}

// 9.D7: Stacked Deck 183 says "Put 1 into your hand" — not a draw.
TEST_F(SweepTriggersTest, StackedDeck_PuttingACardIntoHandIsNotADraw) {
    EXPECT_NE(card_db.get(kStackedDeck).ability_text.find("Put 1 into your hand"),
              std::string::npos);
    stockDeck(P1, 4);
    auto spell = addToHand(P1, kStackedDeck);
    const int draws = state.player(P1).draws_this_turn;
    int drawn_events = 0;
    auto conn = events.on_cards_drawn.connect([&](const CardsDrawnEvent& e) {
        if (e.player == P1) drawn_events += e.count;
    });
    playThroughEngine(spell);
    conn.disconnect();
    EXPECT_EQ(handSize(P1), 1) << "one card was put into hand";
    EXPECT_EQ(state.player(P1).draws_this_turn, draws);
    EXPECT_EQ(drawn_events, 0);
}

// 9.D9: a token is its registered token card when its play is announced, and
// so is an additional copy.
TEST_F(SweepTriggersTest, CreateToken_TheTokenCardIsSetBeforeThePlayIsAnnounced) {
    addUnit(P1, kZilean, 5, 0);
    std::vector<CardDefId> seen;
    EnginePathAgent yes(EnginePathAgent::script({1}));
    withEngine([&](GameEngine& e) {
        e.testHook_cleanup();
        auto c = events.on_token_created.connect([&](const TokenCreatedEvent& ev) {
            seen.push_back(e.mutableState().getObject(ev.object).card_def_id);
        });
        e.testHook_executor().createToken(P1, CardType::Unit, "Shadow Clone", 0, {},
                                          KeywordSet{}, base(P1), false, {}, kShadowCloneCard);
        c.disconnect();
    }, &yes);
    ASSERT_EQ(seen.size(), 2u) << "the token and Zilean's additional copy";
    EXPECT_EQ(seen[0], kShadowCloneCard);
    EXPECT_EQ(seen[1], kShadowCloneCard);
}

// 9.D11: a prompt line that lists cards from a deck by name is for the player
// being asked only; one that lists units on the board is for both.
TEST_F(SweepTriggersTest, PromptLines_NamingHiddenCardsArePrivate) {
    auto weaver = addUnit(P1, kFateWeaver, 3, 0);
    stockDeck(P1, 2);
    auto spell = addToDeck(P1, kDeathFromBelow);
    const std::string name = obj(spell).name;
    int private_lines = 0, leaked = 0;
    auto conn = events.on_log.connect([&](const LogEvent& e) {
        if (e.message.find("TGT_PROMPT") == std::string::npos) return;
        if (e.message.find(name) == std::string::npos) return;
        if (e.visibleTo(P1) && !e.visibleTo(P2)) ++private_lines; else ++leaked;
    });
    EnginePathAgent agent(objectOrYes(spell));
    fireTriggerThroughEngine(weaver, TriggerType::WhenYouPlayMe, &agent);
    conn.disconnect();
    EXPECT_GE(private_lines, 1);
    EXPECT_EQ(leaked, 0);

    // Units on the board: public.
    auto vex = addUnit(P1, kVexMocking, 5, 0);
    (void)vex;
    auto bramble = addUnit(P1, kRedBrambleback, 0, 0);
    int public_lines = 0, hidden_lines = 0;
    auto conn2 = events.on_log.connect([&](const LogEvent& e) {
        if (e.message.find("TGT_PROMPT") == std::string::npos) return;
        (e.isPublic() ? public_lines : hidden_lines)++;
    });
    withEngine([&](GameEngine& e) { e.testHook_scoreConquer(P1, BattlefieldId::fromRaw(0)); e.testHook_runChain(); });
    conn2.disconnect();
    (void)bramble;
    EXPECT_GE(public_lines, 1);
    EXPECT_EQ(hidden_lines, 0);
}


// ═══════════════════════════════════════════════════════════════════════════
// 16. Second follow-up (plans/09 section H)
// ═══════════════════════════════════════════════════════════════════════════

constexpr CardDefId kTKillBuffKill = 1899;   // test spell, see below

// A spell that opens one death batch over the enemy units at battlefield 0,
// kills the first, buffs the second and then kills it too — all in one block,
// with the batch still open (what 9.H1 guards against).
class TKillBuffKill : public SpellCard {
public:
    const CardDef& def() const override {
        static const CardDef d = [] {
            CardDef c;
            c.id = kTKillBuffKill;
            c.name = "Test Kill Buff Kill";
            c.card_type = CardType::Spell;
            return c;
        }();
        return d;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        std::vector<GameObjectId> units;
        for (const auto& [id, o] : ctx.state.objects)
            if (o.isUnit() && o.controller != ctx.controller && o.location.has_value())
                units.push_back(id);
        std::sort(units.begin(), units.end());
        if (units.size() < 2) return;
        DeathBatch together(ctx.executor, units);
        ctx.executor.killObject(units[0]);
        ctx.executor.buffUnit(units[1]);             // the survivor changes
        ctx.executor.killObject(units[1]);           // ... and dies later, batch still open
    }
};

constexpr CardDefId kTRecordsPlaySource = 1898;
constexpr CardDefId kTDelayedSpawner = 1897;

// A free spell that notes where it was being played from each time its cost
// is worked out (PlayerState::current_play_source).
class TRecordsPlaySource : public SpellCard {
public:
    static inline std::vector<Intent::PlaySource> seen;
    const CardDef& def() const override {
        static const CardDef d = [] {
            CardDef c;
            c.id = kTRecordsPlaySource;
            c.name = "Test Records Play Source";
            c.card_type = CardType::Spell;
            return c;
        }();
        return d;
    }
    int selfCostReduction(const GameState& state, PlayerId player) const override {
        seen.push_back(state.player(player).current_play_source);
        return 0;
    }
};

// The card behind the delayed abilities of the 9.M18 test; each resolution is
// counted on the source object.
class TDelayedSpawner : public UnitCard {
public:
    const CardDef& def() const override {
        static const CardDef d = [] {
            CardDef c;
            c.id = kTDelayedSpawner;
            c.name = "Test Delayed Spawner";
            c.card_type = CardType::Unit;
            c.might = 1;
            return c;
        }();
        return d;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        ctx.state.getObject(ctx.source).card_counters["resolved"]++;
    }
};

class SweepTriggersCardTest : public SweepTriggersTest {
protected:
    void SetUp() override {
        SweepTriggersTest::SetUp();
        card_registry.registerCard(kTKillBuffKill, std::make_unique<TKillBuffKill>());
        card_registry.registerCard(kTRecordsPlaySource, std::make_unique<TRecordsPlaySource>());
        card_registry.registerCard(kTDelayedSpawner, std::make_unique<TDelayedSpawner>());
        card_db.buildFromClasses(card_registry);
        TRecordsPlaySource::seen.clear();
    }
};

// 9.H1: a unit listed in a batch that survives, is changed and dies later
// gets last-known information from when it died, not from the batch.
TEST_F(SweepTriggersCardTest, DeathBatch_ASurvivorThatChangedIsSnapshottedAfresh) {
    auto first = addUnit(P2, kInvalidId, 2, 0);
    auto second = addUnit(P2, kInvalidId, 2, 0);
    ASSERT_LT(first, second);
    auto spell = addToHand(P1, kTKillBuffKill);
    playThroughEngine(spell);
    ASSERT_FALSE(obj(first).location.has_value());
    ASSERT_FALSE(obj(second).location.has_value());
    // The first died with the second beside it.
    EXPECT_EQ(obj(first).last_known.other_friendly_units_here,
              std::vector<GameObjectId>{second});
    // The second died later: buffed, and alone.
    EXPECT_EQ(obj(second).last_known.buff_count, 1);
    EXPECT_TRUE(obj(second).last_known.other_friendly_units_here.empty());
}

// 9.H2 (NOT A DEFECT, CR 420.4.a.1 / 420.4.b): when the first card played is
// countered, Astral Heron 831 does not trigger for it, and the card played
// after it is the second card, not "your first card".
TEST_F(SweepTriggersTest, AstralHeron_ACounteredFirstCardStillCountsAsPlayed) {
    constexpr CardDefId kAstralHeron = 831;
    auto heron = addUnit(P1, kAstralHeron, 0, 0);
    auto second = addToHand(P1, kDredgeUp);
    // The first card was played and countered: it was never announced as
    // played, but it was the first card of the turn.
    state.player(P1).cards_played_this_turn = 2;
    obj(second).card_counters["__play_ordinal"] = 2;
    obj(second).card_counters["__play_ordinal_turn"] = state.turn.turn_number + 1;
    fireTriggerThroughEngine(heron, TriggerType::WhenYouPlayACard, nullptr, second);
    EXPECT_TRUE(state.player(P1).cost_modifiers.empty());
}

// 9.H3: "play a Gold gear token exhausted" — exhausted when its play is
// announced, for Bloodharbor Ripper 746 and Card Sharp 403.
TEST_F(SweepTriggersTest, GoldToken_PlayedExhaustedIsExhaustedWhenAnnounced) {
    constexpr CardDefId kBloodharborRipper = 746, kCardSharp = 403;
    auto legend = addLegend(P1, kBloodharborRipper);
    auto unit = addUnit(P1, kInvalidId, 2, 0);
    auto sharp = addToHand(P1, kCardSharp);
    state.player(P1).rune_pool.energy = 1 + card_db.get(kCardSharp).energy_cost;
    state.player(P1).rune_pool.universal_power = card_db.get(kCardSharp).power_cost;
    int announced = 0, ready_when_announced = 0;
    EnginePathAgent yes1(EnginePathAgent::script({1})), yes2(EnginePathAgent::script({1}));
    withEngine([&](GameEngine& e) {
        auto conn = events.on_token_created.connect([&](const TokenCreatedEvent& ev) {
            if (ev.card_type != CardType::Gear) return;
            ++announced;
            if (!e.mutableState().getObject(ev.object).is_exhausted) ++ready_when_announced;
        });
        Intent act;
        act.type = IntentType::ActivateAbility;
        act.player = P1;
        act.ability_source = legend;
        act.targets = {unit};
        e.testHook_executeIntent(act);
        EXPECT_EQ(announced, 1) << "Bloodharbor Ripper";
        Intent play;
        play.type = IntentType::PlayCard;
        play.player = P1;
        play.card = sharp;
        play.play_location = LocationId{BaseLocation{P1}};
        e.testHook_executeIntent(play);
        conn.disconnect();
    }, &yes1, &yes2);
    EXPECT_EQ(announced, 4) << "the Ripper's, then Card Sharp: mine, theirs, and mine again";
    EXPECT_EQ(ready_when_announced, 0);
}

// ... unless "your tokens enter ready" replaces how it enters (CR 184.1).
TEST_F(SweepTriggersTest, GoldToken_EntersExhaustedYieldsToTokensEnterReady) {
    GameObjectId plain = kInvalidId, replaced = kInvalidId;
    withEngine([&](GameEngine& e) {
        auto& exec = e.testHook_executor();
        plain = exec.createToken(P1, CardType::Gear, "Gold", 0, {}, KeywordSet{}, base(P1),
                                 false, {}, kGoldGearCardDefId, /*enters_exhausted=*/true);
        e.mutableState().player(P1).tokens_enter_ready = true;
        replaced = exec.createToken(P1, CardType::Gear, "Gold", 0, {}, KeywordSet{}, base(P1),
                                    false, {}, kGoldGearCardDefId, /*enters_exhausted=*/true);
    });
    EXPECT_TRUE(obj(plain).is_exhausted);
    EXPECT_FALSE(obj(replaced).is_exhausted);
}

// 9.H4: an ability used in response that adds nothing to the chain (a rune's
// Add) is not a pass — twelve of them, then a real response, in one window.
TEST_F(SweepTriggersTest, PriorityWindow_CostOnlyActivationsAreNotPasses) {
    auto source = addUnit(P1, kInvalidId, 2, -1);
    auto rune = addUnit(P1, kInvalidId, 0, -1);
    auto reaction = addToHand(P1, kDecreeOfInsight);
    ChainManager cm(state, events, card_db);
    int activations = 0, plays = 0;
    cm.setActivateAbility([&](const Intent&) { ++activations; return false; });   // cost-only
    cm.setPlayCard([&](const Intent&) { ++plays; cm.addAbility(source, P1, kInvalidId); });
    cm.addAbility(source, P1, kInvalidId);
    Intent tap;
    tap.type = IntentType::ActivateReactionAbility;
    tap.player = P1;
    tap.ability_source = rune;
    Intent play;
    play.type = IntentType::PlayReaction;
    play.player = P1;
    play.card = reaction;
    constexpr int kTaps = 12;
    cm.processFEPR(
        [&](PlayerId p, const std::vector<Intent>& legal) -> Intent {
            if (p == P1 && plays == 0) {
                const IntentType want = activations < kTaps ? IntentType::ActivateReactionAbility
                                                            : IntentType::PlayReaction;
                for (const auto& i : legal) if (i.type == want) return i;
            }
            return Intent::passPriority(p);
        },
        [](const ChainItem&) {},
        [](const ChainItem&) {},
        [&](PlayerId p) {
            std::vector<Intent> out{Intent::passPriority(p)};
            if (p == P1) { out.push_back(tap); out.push_back(play); }
            return out;
        });
    EXPECT_EQ(activations, kTaps);
    EXPECT_EQ(plays, 1) << "the window was still open for the real response";
    EXPECT_TRUE(state.chain.items.empty());
}


// ═══════════════════════════════════════════════════════════════════════════
// 17. The move travels with its trigger (plans/09 section I)
// ═══════════════════════════════════════════════════════════════════════════

// 9.I2 / 9.I3: Lillia, Fae Fawn 644 moves twice before either "When I move"
// resolves — a Sprite where each move started, not two at the second origin.
TEST_F(SweepTriggersTest, MoveTrigger_TwoMovesGiveEachTriggerItsOwnOrigin) {
    constexpr CardDefId kLilliaFaeFawn = 644;
    auto lillia = addUnit(P1, kLilliaFaeFawn, 3, -1);
    withEngine([&](GameEngine& e) {
        auto& exec = e.testHook_executor();
        exec.moveToBattlefield(lillia, state.battlefields[0].id);   // base -> 0
        exec.moveToBattlefield(lillia, state.battlefields[1].id);   // 0 -> 1
        int with_move = 0;
        for (const auto& ci : e.mutableState().chain.items)
            if (ci.source == lillia && ci.triggering_move.has_value()) ++with_move;
        EXPECT_EQ(with_move, 2);
        EXPECT_FALSE(e.mutableState().chain.move_being_announced.has_value());
        e.testHook_runChain();
    });
    int in_base = 0, at_bf0 = 0, elsewhere = 0;
    for (auto t : tokenUnits(P1)) {
        if (obj(t).name != "Sprite") continue;
        if (obj(t).isAtBase()) ++in_base;
        else if (obj(t).battlefieldId() == std::optional<BattlefieldId>{state.battlefields[0].id}) ++at_bf0;
        else ++elsewhere;
    }
    EXPECT_EQ(in_base, 1) << "the first move started in base";
    EXPECT_EQ(at_bf0, 1) << "the second move started at battlefield 0";
    EXPECT_EQ(elsewhere, 0);
}

// 9.I1: Fae Porter 446 — "the same battlefield" is where the move ended, read
// from the trigger, with the Porter already gone from it when it is finalized.
TEST_F(SweepTriggersTest, MoveTrigger_FaePorterUsesTheDestinationOfItsMove) {
    constexpr CardDefId kFaePorter = 446;
    auto porter = addUnit(P1, kFaePorter, 2, -1);
    auto ally = addUnit(P1, kInvalidId, 2, -1);
    state.player(P1).rune_pool.power[static_cast<int>(Domain::Chaos)] = 1;
    EnginePathAgent agent(objectOrYes(ally));
    withEngine([&](GameEngine& e) {
        auto& exec = e.testHook_executor();
        exec.moveToBattlefield(porter, state.battlefields[1].id);
        // It leaves the battlefield before its trigger is finalized.
        auto& p = e.mutableState().getObject(porter);
        p.location = LocationId{BaseLocation{P1}};
        p.zone = ZoneType::Base;
        e.testHook_runChain();
    }, &agent);
    EXPECT_EQ(obj(ally).battlefieldId(), std::optional<BattlefieldId>{state.battlefields[1].id});
    EXPECT_EQ(state.player(P1).rune_pool.power[static_cast<int>(Domain::Chaos)], 0);
}

// Akali, Deadly Weapon 808: "a battlefield I moved to or from" are the ends
// of that move, also after she has moved on.
TEST_F(SweepTriggersTest, MoveTrigger_AkaliChoosesAtTheEndsOfThatMove) {
    constexpr CardDefId kAkali = 808;
    auto akali = addUnit(P1, kAkali, 3, 0);
    auto left_behind = addUnit(P2, kInvalidId, 9, 0);
    EnginePathAgent agent(objectOrYes(left_behind));
    withEngine([&](GameEngine& e) {
        auto& exec = e.testHook_executor();
        exec.moveToBase(akali);                    // 0 -> base: only battlefield 0 qualifies
        e.testHook_runChain();
    }, &agent);
    EXPECT_EQ(obj(left_behind).damage_marked, 1);
    EXPECT_TRUE(agent.wasOffered(left_behind));
}

// A move trigger cannot resolve without its move (9.I3: no silent default).
TEST_F(SweepTriggersTest, MoveTrigger_WithoutAMoveIsAnError) {
    constexpr CardDefId kLilliaFaeFawn = 644;
    auto lillia = addUnit(P1, kLilliaFaeFawn, 3, 0);
    EXPECT_THROW(fireTriggerThroughEngine(lillia, TriggerType::WhenIMove), std::logic_error);
}


// ═══════════════════════════════════════════════════════════════════════════
// 18. Third follow-up (plans/09 9.J5, 9.K2)
// ═══════════════════════════════════════════════════════════════════════════

// 9.J5: only something on the board can be killed (CR 429.2.a) — a unit that
// was banished as it died is not killed a second time.
TEST_F(SweepTriggersTest, KillObject_AUnitBanishedOnDeathIsNotKilledAgain) {
    auto unit = addUnit(P1, kLoyalPoro, 1, 0);
    obj(unit).banish_if_would_die_this_turn = true;      // "If it would die this turn, banish it instead."
    int left = 0, died = 0;
    auto c1 = events.on_left_board.connect([&](const LeftBoardEvent& e) { if (e.object == unit) ++left; });
    auto c2 = events.on_unit_died.connect([&](const UnitDiedEvent& e) { if (e.object == unit) ++died; });
    resolveThroughEngine([&](EffectExecutor& exec, GameState& live) {
        exec.killObject(unit);
        EXPECT_EQ(live.getObject(unit).zone, ZoneType::Banishment);
        EXPECT_FALSE(live.getObject(unit).location.has_value());
        // Had it kept a location by mistake, only the zone would tell: the
        // guard is "not on the board", whatever zone that is.
        exec.killObject(unit);
    });
    c1.disconnect(); c2.disconnect();
    EXPECT_EQ(left, 1);
    EXPECT_EQ(died, 0) << "banished instead of dying";
    const auto& banished = state.player(P1).banishment;
    EXPECT_EQ(std::count(banished.begin(), banished.end(), unit), 1);
    EXPECT_EQ(obj(unit).zone, ZoneType::Banishment);
}

// 9.K2: a survivor of an open batch that was only GIVEN a keyword since, and
// then dies in it, is taken afresh: its last-known keywords include the grant.
TEST_F(SweepTriggersTest, DeathBatch_ASurvivorGivenAKeywordIsSnapshottedAfresh) {
    auto first = addUnit(P2, kInvalidId, 2, 0);
    auto second = addUnit(P2, kInvalidId, 2, 0);
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) {
        DeathBatch together(exec, {first, second});
        exec.killObject(first);
        exec.giveTemporaryKeyword(second, Keyword::Ganking, 0);
        exec.killObject(second);
    });
    ASSERT_FALSE(obj(second).location.has_value());
    EXPECT_TRUE(obj(second).last_known.keywords.has(Keyword::Ganking));
    EXPECT_TRUE(obj(second).last_known.other_friendly_units_here.empty());
    EXPECT_EQ(obj(first).last_known.other_friendly_units_here, std::vector<GameObjectId>{second});
}


// ═══════════════════════════════════════════════════════════════════════════
// 19. Carry-over audit (plans/09 section M, 9.N7)
// ═══════════════════════════════════════════════════════════════════════════

/// A chain item to stand in `chain.resuming` for "an effect of `controller`
/// is resolving".
static ChainItem resolvingFor(GameState& live, PlayerId controller, bool spell = false) {
    ChainItem ri;
    ri.id = live.chain.allocateId();
    ri.controller = controller;
    ri.is_spell = spell;
    ri.is_ability = !spell;
    return ri;
}

// 9.M13: "They can't move it this turn" (Vex, Apathetic 712) stops its
// controller's move effects; an enemy's move effect and a recall still work.
TEST_F(SweepTriggersTest, CantMoveThisTurn_StopsItsControllersMoveEffectsOnly) {
    addUnit(P2, 712, 4, 0);
    GameObjectId token = kInvalidId;
    withEngine([&](GameEngine& e) {
        auto& exec = e.testHook_executor();
        auto& live = e.mutableState();
        token = exec.createToken(P1, CardType::Unit, "Recruit", 1, {}, KeywordSet{}, battlefield(1));
        e.testHook_runChain();                       // Vex: stun it, it can't be moved this turn
        ASSERT_TRUE(live.getObject(token).cant_move_this_turn);

        live.chain.resuming = resolvingFor(live, P1);
        exec.moveToBase(token);
        EXPECT_FALSE(live.getObject(token).isAtBase()) << "its controller's effect";
        exec.moveToBattlefield(token, state.battlefields[0].id);
        EXPECT_EQ(live.getObject(token).battlefieldId(),
                  std::optional<BattlefieldId>{state.battlefields[1].id});
        EXPECT_EQ(live.getObject(token).moves_this_turn, 0);

        live.chain.resuming = resolvingFor(live, P2);
        exec.moveToBattlefield(token, state.battlefields[0].id);
        EXPECT_EQ(live.getObject(token).battlefieldId(),
                  std::optional<BattlefieldId>{state.battlefields[0].id}) << "an enemy's effect moves it";

        live.chain.resuming = resolvingFor(live, P1);
        exec.recallToBase(token);
        EXPECT_TRUE(live.getObject(token).isAtBase()) << "a recall is not a move (CR 458.3)";
        live.chain.resuming.reset();
        live.chain.items.clear();
    });
}

// 9.M15: Ashe, Focused 731 — "When they hold, return it" is her controller's
// ability waiting on the opponent's hold; the holder's Blue Sentinel does not
// repeat it.
TEST_F(SweepTriggersTest, DelayedAbility_WatchedPlayerIsNotItsController) {
    constexpr CardDefId kAsheFocused = 731;
    auto taken = addToHand(P2, kLoyalPoro);
    auto ashe = addToHand(P1, kAsheFocused);
    EnginePathAgent agent(objectOrYes(taken));
    playThroughEngine(ashe, &agent);
    ASSERT_EQ(obj(taken).zone, ZoneType::Banishment);
    ASSERT_EQ(state.delayed_abilities.size(), 1u);
    EXPECT_EQ(state.delayed_abilities[0].controller, P1);
    EXPECT_EQ(state.delayed_abilities[0].watched_player, P2);
    EXPECT_EQ(state.delayed_abilities[0].expires_on_turn, DelayedAbility::kUntilFired);

    // P1's own hold does not fire it.
    state.battlefields[0].controller = P1;
    withEngine([&](GameEngine& e) { e.testHook_scoreHold(P1, BattlefieldId::fromRaw(0)); e.testHook_runChain(); });
    EXPECT_EQ(obj(taken).zone, ZoneType::Banishment);

    // P2 holds a battlefield with a Blue Sentinel there.
    addUnit(P2, kBlueSentinel, 0, 1);
    state.battlefields[1].controller = P2;
    int ashe_items = 0;
    PlayerId item_controller = PlayerId::None;
    withEngine([&](GameEngine& e) {
        e.testHook_scoreHold(P2, BattlefieldId::fromRaw(1));
        for (const auto& ci : e.mutableState().chain.items)
            if (ci.source == ashe) { ++ashe_items; item_controller = ci.controller; }
        e.testHook_runChain();
    }, nullptr, nullptr, P2);
    EXPECT_EQ(ashe_items, 1) << "not one of the holder's hold effects: not repeated";
    EXPECT_EQ(item_controller, P1) << "Ashe's controller controls it (CR 391-392)";
    EXPECT_TRUE(inHand(P2, taken));
    EXPECT_TRUE(state.delayed_abilities.empty() ||
                state.delayed_abilities[0].source != ashe);
}

// 9.M16: "Once each turn" is printed on each Zilean, Time Mage 648.
TEST_F(SweepTriggersTest, Zilean_EachCopyIsOnceEachTurnOnItsOwn) {
    addUnit(P1, kZilean, 5, 0);
    addUnit(P1, kZilean, 5, 1);
    EnginePathAgent yes(EnginePathAgent::script({1, 1, 1}));
    withEngine([&](GameEngine& e) {
        e.testHook_cleanup();
        EXPECT_EQ(e.mutableState().player(P1).additional_token_copy_sources, 2);
        auto& exec = e.testHook_executor();
        // 9.Q2 (CR 370.2, 372): both Replacement Effects apply to the one
        // play, each once — the token and two additional copies.
        exec.createToken(P1, CardType::Unit, "Recruit", 1, {}, KeywordSet{}, base(P1));
        EXPECT_EQ(exec.lastTokensCreated().size(), 3u);
        EXPECT_EQ(e.mutableState().player(P1).additional_token_copies_used, 2);
        // Both are used up for the turn.
        for (int i = 0; i < 2; ++i)
            exec.createToken(P1, CardType::Unit, "Recruit", 1, {}, KeywordSet{}, base(P1));
    }, &yes);
    EXPECT_EQ(tokenUnits(P1).size(), 5u);
    EXPECT_EQ(yes.prompts.size(), 2u) << "one \"may\" per Zilean, none once both are used";
}

// 9.M17: both Gold helpers make the same token — no tag (CR 187.5), the Gold
// card, and "exhausted" only where the instruction says so.
TEST_F(SweepTriggersTest, GoldHelpers_MakeTheSameToken) {
    GameObjectId ready = kInvalidId, exhausted = kInvalidId;
    withEngine([&](GameEngine& e) {
        CardContext ctx{e.mutableState(), events, e.testHook_executor(), P1, kInvalidId};
        const auto before = e.mutableState().objects.size();
        createGoldToken(ctx);
        ASSERT_EQ(e.mutableState().objects.size(), before + 1);
        ready = e.testHook_executor().lastTokensCreated().front();
        exhausted = createGoldExhausted(ctx);
    });
    EXPECT_TRUE(obj(ready).tags.empty());
    EXPECT_EQ(obj(ready).tags, obj(exhausted).tags);
    EXPECT_EQ(obj(ready).card_def_id, kGoldGearCardDefId);
    EXPECT_EQ(obj(exhausted).card_def_id, kGoldGearCardDefId);
    EXPECT_FALSE(obj(ready).is_exhausted);
    EXPECT_TRUE(obj(exhausted).is_exhausted);
}

// 9.M18: checkDelayedAbilities decides what fires before it queues anything.
// A delayed ability added while the items are being queued did not see the
// event; the ones that did each fire exactly once.
TEST_F(SweepTriggersCardTest, DelayedAbilities_OneAddedWhileQueueingDoesNotFireForThatEvent) {
    auto a = addUnit(P1, kTDelayedSpawner, 1, -1);
    auto b = addUnit(P1, kTDelayedSpawner, 1, -1);
    auto c = addUnit(P1, kTDelayedSpawner, 1, -1);
    auto make = [&](GameObjectId src) {
        DelayedAbility da;
        da.source = src;
        da.card_def_id = kTDelayedSpawner;
        da.controller = P1;
        da.trigger = TriggerType::WhenYouStun;
        return da;
    };
    state.delayed_abilities = {make(a), make(b)};
    auto enemy = addUnit(P2, kInvalidId, 2, 0);
    auto stunner = addUnit(P1, kInvalidId, 2, 0);
    withEngine([&](GameEngine& e) {
        auto& live = e.mutableState();
        bool added = false;
        // The trace line is emitted for each ability as it is queued: add a
        // third (and force the vector to move) in the middle of that.
        auto conn = events.on_log.connect([&](const LogEvent& ev) {
            if (added || ev.message.find("DELAYED_ABILITY: firing") == std::string::npos) return;
            added = true;
            live.delayed_abilities.reserve(live.delayed_abilities.capacity() + 64);
            live.delayed_abilities.push_back(make(c));
        });
        e.testHook_executor().stunUnitBy(enemy, stunner);
        conn.disconnect();
        ASSERT_TRUE(added);
        int items = 0;
        for (const auto& ci : live.chain.items)
            if (ci.source == a || ci.source == b || ci.source == c) ++items;
        EXPECT_EQ(items, 2);
        e.testHook_runChain();
    });
    EXPECT_EQ(obj(a).card_counters["resolved"], 1);
    EXPECT_EQ(obj(b).card_counters["resolved"], 1);
    EXPECT_EQ(obj(c).card_counters["resolved"], 0) << "added after the event";
    ASSERT_EQ(state.delayed_abilities.size(), 1u) << "the two that fired are gone, the new one waits";
    EXPECT_EQ(state.delayed_abilities[0].source, c);
}

// 9.M19: a card an effect plays off a deck is staged as played from the deck.
TEST_F(SweepTriggersCardTest, EffectPlay_StagesTheZoneTheCardIsReallyPlayedFrom) {
    auto from_deck = addToDeck(P1, kTRecordsPlaySource);
    auto from_hand = addToHand(P1, kTRecordsPlaySource);
    withEngine([&](GameEngine& e) {
        auto& exec = e.testHook_executor();
        TRecordsPlaySource::seen.clear();
        ASSERT_TRUE(exec.canPlayPayingCost(P1, from_deck));
        ASSERT_FALSE(TRecordsPlaySource::seen.empty());
        for (auto src : TRecordsPlaySource::seen) EXPECT_EQ(src, Intent::PlaySource::MainDeck);
        TRecordsPlaySource::seen.clear();
        ASSERT_TRUE(exec.canPlayPayingCost(P1, from_hand));
        for (auto src : TRecordsPlaySource::seen) EXPECT_EQ(src, Intent::PlaySource::Hand);
        EXPECT_EQ(e.mutableState().player(P1).current_play_source, Intent::PlaySource::Hand)
            << "restored afterwards";
    });
}

// ... and Drag Under 485 ("costs [2] less if played from anywhere other than
// your hand") still gets its reduction off the deck.
TEST_F(SweepTriggersTest, DragUnder_PlayedOffTheDeckCostsTwoLess) {
    constexpr CardDefId kDragUnder = 485;
    auto in_deck = addToDeck(P1, kDragUnder);
    auto in_hand = addToHand(P1, kDragUnder);
    state.player(P1).rune_pool.energy = card_db.get(kDragUnder).energy_cost - 2;
    state.player(P1).rune_pool.universal_power = card_db.get(kDragUnder).power_cost;
    withEngine([&](GameEngine& e) {
        EXPECT_TRUE(e.testHook_executor().canPlayPayingCost(P1, in_deck));
        EXPECT_FALSE(e.testHook_executor().canPlayPayingCost(P1, in_hand));
    });
}

// 9.M20: a unit an effect plays enters ready with Magma Wurm 11 out ("Other
// friendly units enter ready.") and after Confront 129 ("Units you play this
// turn enter ready.").
TEST_F(SweepTriggersTest, PlayIgnoringCost_HonoursEntersReadyEffects) {
    constexpr CardDefId kMagmaWurm = 11, kConfront = 129;
    auto plain = addToHand(P1, kLoyalPoro);
    withEngine([&](GameEngine& e) { e.testHook_executor().playIgnoringCost(P1, plain); });
    ASSERT_TRUE(obj(plain).is_exhausted) << "baseline: CR 143.4";

    addUnit(P1, kMagmaWurm, 6, -1);
    auto with_wurm = addToHand(P1, kLoyalPoro);
    withEngine([&](GameEngine& e) {
        e.testHook_cleanup();                           // the Wurm's continuous effect
        e.testHook_executor().playIgnoringCost(P1, with_wurm);
    });
    EXPECT_FALSE(obj(with_wurm).is_exhausted);

    // The other player's units are not "friendly" to the Wurm.
    auto theirs = addToHand(P2, kLoyalPoro);
    withEngine([&](GameEngine& e) {
        e.testHook_cleanup();
        e.testHook_executor().playIgnoringCost(P2, theirs);
    });
    EXPECT_TRUE(obj(theirs).is_exhausted);

    stockDeck(P2, 2);
    playThroughEngine(addToHand(P2, kConfront));
    auto after_confront = addToHand(P2, kLoyalPoro);
    withEngine([&](GameEngine& e) { e.testHook_executor().playIgnoringCost(P2, after_confront); },
               nullptr, nullptr, P2);
    EXPECT_FALSE(obj(after_confront).is_exhausted);
}

// 9.M21: "When a combat that I was in ends" (Mournful Witness 815) — a unit
// recalled out of the combat before it ends was in it; a unit elsewhere was not.
TEST_F(SweepTriggersTest, CombatParticipants_AUnitRecalledBeforeTheEndWasInTheCombat) {
    constexpr CardDefId kMournfulWitness = 815;
    auto recalled = addUnit(P1, kMournfulWitness, 2, 0);
    auto elsewhere = addUnit(P1, kMournfulWitness, 2, 1);
    addUnit(P2, kInvalidId, 2, 0);
    const BattlefieldId bf = state.battlefields[0].id;
    withEngine([&](GameEngine& e) {
        auto& live = e.mutableState();
        live.battlefields[0].combat_in_progress = true;
        events.emit(CombatStartedEvent{bf, P1, P2});
        e.testHook_executor().recallToBase(recalled);     // the tie recall
        live.battlefields[0].combat_in_progress = false;
        events.emit(CombatEndedEvent{bf, std::nullopt});
        e.testHook_runChain();
    });
    EXPECT_TRUE(obj(recalled).isEmpowered());
    EXPECT_FALSE(obj(elsewhere).isEmpowered());
    EXPECT_EQ(obj(recalled).card_counters.count("__combat_participant"), 0u) << "cleared with the combat";
}

// 9.M21: "When you kill a unit with a spell" (Immortal Phoenix 37) — a unit
// that dies in the cleanup of lethal damage a spell dealt was killed with it.
TEST_F(SweepTriggersTest, KillWithASpell_CountsLethalSpellDamageFoundInTheCleanup) {
    constexpr CardDefId kImmortalPhoenix = 37;
    auto phoenix = addToHand(P1, kImmortalPhoenix);
    {
        auto& h = state.player(P1).hand;
        h.erase(std::remove(h.begin(), h.end(), phoenix), h.end());
        obj(phoenix).zone = ZoneType::Trash;
        state.player(P1).trash.push_back(phoenix);
    }
    auto spell = addToHand(P1, kDredgeUp);              // a P1 spell object as the source
    auto unit_source = addUnit(P1, kInvalidId, 3, 0);
    auto by_spell = addUnit(P2, kInvalidId, 2, 0);
    auto by_unit = addUnit(P2, kInvalidId, 2, 1);
    auto queued_after = [&](GameObjectId victim, GameObjectId source) {
        int queued = 0;
        withEngine([&](GameEngine& e) {
            e.testHook_executor().dealDamage(victim, 5, source);   // nothing is resolving
            e.testHook_processLethalDamage();
            for (const auto& ci : e.mutableState().chain.items) if (ci.source == phoenix) ++queued;
            e.mutableState().chain.items.clear();
        });
        return queued;
    };
    EXPECT_EQ(queued_after(by_unit, unit_source), 0) << "a unit's damage is not a spell";
    EXPECT_EQ(queued_after(by_spell, spell), 1);
    EXPECT_TRUE(obj(by_spell).last_known.killed_with_spell);
    EXPECT_EQ(obj(by_spell).last_known.killed_by, P1);
}

// 9.M23: when more than one replacement applies to a death, the unit's
// controller chooses which is applied (CR 372).
TEST_F(SweepTriggersTest, DeathReplacements_TheControllerChoosesWhenSeveralApply) {
    constexpr CardDefId kZhonyas = 77, kGuardianAngel = 374;
    auto setup = [&](GameObjectId& unit, GameObjectId& zhonya, GameObjectId& angel) {
        unit = addUnit(P1, kInvalidId, 2, 0);
        zhonya = addGear(P1, kZhonyas);
        angel = addAttachedGear(P1, unit, kGuardianAngel);
    };
    GameObjectId unit, zhonya, angel;

    // Chooses the second listed (Guardian Angel has the higher id).
    setup(unit, zhonya, angel);
    EnginePathAgent second(EnginePathAgent::script({1}));
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) { exec.killObject(unit); }, &second);
    ASSERT_EQ(second.prompts.size(), 1u);
    EXPECT_EQ(second.prompts[0].size(), 2u);
    EXPECT_TRUE(atBase(unit));
    EXPECT_TRUE(obj(zhonya).location.has_value()) << "Zhonya's Hourglass was not used";
    EXPECT_FALSE(obj(angel).location.has_value()) << "Guardian Angel was";

    // Chooses the first: Zhonya's Hourglass, on the lethal-damage path.
    obj(zhonya).location.reset();                  // the unused one from above leaves play
    obj(zhonya).zone = ZoneType::Trash;
    setup(unit, zhonya, angel);
    obj(unit).damage_marked = 5;
    EnginePathAgent first(EnginePathAgent::script({0}));
    withEngine([&](GameEngine& e) { e.testHook_processLethalDamage(); e.testHook_runChain(); },
               &first);
    ASSERT_EQ(first.prompts.size(), 1u);
    EXPECT_EQ(first.prompts[0].size(), 2u);
    EXPECT_TRUE(atBase(unit));
    EXPECT_FALSE(obj(zhonya).location.has_value());
    EXPECT_TRUE(obj(angel).location.has_value());
    EXPECT_EQ(obj(angel).attached_to, std::optional<GameObjectId>{unit}) << "recalled with its gear";
}

// ... nobody is asked when only one applies, and with nobody to ask the order
// is the listed one: the unit's own, then cards in play by object id.
TEST_F(SweepTriggersTest, DeathReplacements_OrderIsStableWithoutAChoice) {
    constexpr CardDefId kZhonyas = 77, kGuardianAngel = 374;
    auto lone = addUnit(P1, kInvalidId, 2, 0);
    auto only = addGear(P1, kZhonyas);
    EnginePathAgent agent;
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) { exec.killObject(lone); }, &agent);
    EXPECT_TRUE(agent.prompts.empty());
    EXPECT_TRUE(atBase(lone));
    EXPECT_FALSE(obj(only).location.has_value());

    // Guardian Angel created first (lower id) and Zhonya's second, and the
    // unit also carries "heal, exhaust and recall it instead": the default
    // (first option) is the unit's own, leaving both gear in play.
    auto unit = addUnit(P1, kInvalidId, 2, 0);
    auto angel = addAttachedGear(P1, unit, kGuardianAngel);
    auto zhonya = addGear(P1, kZhonyas);
    obj(unit).death_replacement_recall_pending = true;
    EnginePathAgent dflt(EnginePathAgent::script({0}));   // the first listed
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) { exec.killObject(unit); }, &dflt);
    ASSERT_EQ(dflt.prompts.size(), 1u);
    ASSERT_EQ(dflt.prompts[0].size(), 3u);
    EXPECT_NE(dflt.prompts[0][1].choice_label.find("Guardian Angel"), std::string::npos);
    EXPECT_NE(dflt.prompts[0][2].choice_label.find("Zhonya"), std::string::npos);
    EXPECT_TRUE(atBase(unit));
    EXPECT_TRUE(obj(angel).location.has_value());
    EXPECT_TRUE(obj(zhonya).location.has_value());
    EXPECT_FALSE(obj(unit).death_replacement_recall_pending);
}

// 9.N7: what the two paths emit. A lethally damaged unit beside Soraka 550 is
// recalled by the cleanup ("recalled", no death); killed later at its base,
// away from her, it dies (UnitDiedEvent).
TEST_F(SweepTriggersTest, Soraka_LethalDamageRecallThenALaterKillEmitWhatTheyShould) {
    addUnit(P1, kSoraka, 4, 0);
    auto unit = addUnit(P1, kInvalidId, 1, 0);
    obj(unit).damage_marked = 3;
    int recalled = 0, died = 0, moved = 0;
    auto c1 = events.on_object_state_changed.connect([&](const ObjectStateChangedEvent& e) {
        if (e.object == unit && e.what_changed == "recalled") ++recalled;
    });
    auto c2 = events.on_unit_died.connect([&](const UnitDiedEvent& e) { if (e.object == unit) ++died; });
    auto c3 = events.on_unit_moved.connect([&](const UnitMovedEvent& e) { if (e.object == unit) ++moved; });
    withEngine([&](GameEngine& e) { e.testHook_processLethalDamage(); e.testHook_runChain(); });
    EXPECT_TRUE(atBase(unit));
    EXPECT_EQ(recalled, 1);
    EXPECT_EQ(died, 0);
    EXPECT_EQ(moved, 0);
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) { exec.killObject(unit); });
    c1.disconnect(); c2.disconnect(); c3.disconnect();
    EXPECT_FALSE(obj(unit).location.has_value());
    EXPECT_EQ(died, 1);
    EXPECT_EQ(recalled, 1);
}


// ═══════════════════════════════════════════════════════════════════════════
// 20. Fifth follow-up (plans/09 section Q)
// ═══════════════════════════════════════════════════════════════════════════

// 9.Q2: each Zilean's "may" is its own; one that is declined has not been
// applied this turn (CR 371.2.b) and is offered again on the next play.
TEST_F(SweepTriggersTest, Zilean_ADeclinedCopyIsStillAvailableLaterThatTurn) {
    addUnit(P1, kZilean, 5, 0);
    addUnit(P1, kZilean, 5, 1);
    EnginePathAgent agent(EnginePathAgent::script({0, 1, 1}));
    withEngine([&](GameEngine& e) {
        e.testHook_cleanup();
        auto& exec = e.testHook_executor();
        exec.createToken(P1, CardType::Unit, "Recruit", 1, {}, KeywordSet{}, base(P1));
        EXPECT_EQ(exec.lastTokensCreated().size(), 2u) << "declined one, took the other";
        EXPECT_EQ(e.mutableState().player(P1).additional_token_copies_used, 1);
        exec.createToken(P1, CardType::Unit, "Recruit", 1, {}, KeywordSet{}, base(P1));
        EXPECT_EQ(exec.lastTokensCreated().size(), 2u) << "the declined one, now";
    }, &agent);
    EXPECT_EQ(agent.prompts.size(), 3u);
    EXPECT_EQ(tokenUnits(P1).size(), 4u);
}

// 9.Q1: every card with a replacement effect says when it applies — the
// default applies to nothing — and the engine never lists one that does not.
TEST_F(SweepTriggersTest, ReplacementEffects_EveryCardDefinesWhenItApplies) {
    // Found from the registry, not listed by hand (9.V1): a card that says it
    // has a replacement effect must answer Yes or No — never the base
    // class's NotDefined — and a card that has none leaves it NotDefined.
    auto probe = addUnit(P2, kInvalidId, 2, 1);
    std::vector<CardDefId> found;
    int without = 0;
    for (uint32_t n = 1; n <= static_cast<uint32_t>(ml::kCardVocabSize); ++n) {
        const CardDefId id = CardDefId::fromRaw(n);
        const Card* c = card_registry.get(id);
        if (!c) continue;
        const auto answer = c->replacementAppliesTo(state, kInvalidId, P2, probe);
        if (c->hasReplacementEffect()) {
            found.push_back(id);
            EXPECT_NE(answer, Card::Applies::NotDefined)
                << card_db.get(id).name << " has a replacement effect but does not say when it applies";
        } else {
            ++without;
            EXPECT_EQ(answer, Card::Applies::NotDefined) << card_db.get(id).name;
        }
    }
    ASSERT_GE(found.size(), 5u);
    EXPECT_GT(without, 900);

    // None applies to a death it has nothing to do with: an enemy unit dying
    // somewhere else, nothing attached, no combat.
    auto stranger = addUnit(P2, kInvalidId, 2, 1);
    for (CardDefId id : found) {
        SCOPED_TRACE(card_db.get(id).name);
        const auto& def = card_db.get(id);
        GameObjectId self = def.card_type == CardType::Gear ? addGear(P1, id)
                          : def.card_type == CardType::Legend ? addLegend(P1, id)
                          : def.card_type == CardType::Battlefield ? setBattlefieldCard(0, id)
                          : addUnit(P1, id, 4, 0);
        EXPECT_FALSE(card_registry.get(id)->replacementApplies(state, self, P2, stranger));
    }
    // ... and the one that does is the only one used, with nobody asked.
    auto mine = addUnit(P1, kInvalidId, 2, 1);
    EnginePathAgent agent;
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) { exec.killObject(mine); }, &agent);
    EXPECT_TRUE(atBase(mine)) << "Zhonya's Hourglass: any friendly unit";
    std::string asked;
    for (const auto& pr : agent.prompts) for (const auto& i : pr) asked += i.choice_label + " | ";
    EXPECT_TRUE(agent.prompts.empty()) << "only one replacement applied; asked: " << asked;
}

// 9.Q3: spell damage that was removed — here by a card zeroing the damage
// itself — does not make a later, non-spell death a kill "with a spell".
TEST_F(SweepTriggersTest, KillWithASpell_HealedSpellDamageIsForgotten) {
    constexpr CardDefId kImmortalPhoenix = 37;
    auto phoenix = addToHand(P1, kImmortalPhoenix);
    {
        auto& h = state.player(P1).hand;
        h.erase(std::remove(h.begin(), h.end(), phoenix), h.end());
        obj(phoenix).zone = ZoneType::Trash;
        state.player(P1).trash.push_back(phoenix);
    }
    auto spell = addToHand(P1, kDredgeUp);
    auto unit_source = addUnit(P1, kInvalidId, 3, 0);
    auto victim = addUnit(P2, kInvalidId, 4, 0);
    int queued = 0;
    withEngine([&](GameEngine& e) {
        auto& exec = e.testHook_executor();
        auto& live = e.mutableState();
        exec.dealDamage(victim, 1, spell);                 // not lethal
        live.getObject(victim).damage_marked = 0;          // as Thrill of the Hunt / Arcane Shift do
        exec.dealDamage(victim, 5, unit_source);           // lethal, not a spell
        e.testHook_processLethalDamage();
        for (const auto& ci : live.chain.items) if (ci.source == phoenix) ++queued;
        live.chain.items.clear();
    });
    EXPECT_FALSE(obj(victim).location.has_value());
    EXPECT_EQ(queued, 0);
    EXPECT_FALSE(obj(victim).last_known.killed_with_spell);
}

// 9.Q4: the real combat — a tie recalls the attackers, and the recalled
// Mournful Witness 815 still gets "When a combat that I was in ends".
TEST_F(SweepTriggersTest, CombatParticipants_ARecalledAttackerOfARealTieWasInTheCombat) {
    constexpr CardDefId kMournfulWitness = 815;
    state.battlefields[0].controller = P2;
    state.battlefields[0].is_contested = true;
    state.battlefields[0].contested_by = P1;
    state.battlefields[0].combat_staged = true;
    auto attacker = addUnit(P1, kMournfulWitness, 3, 0);
    auto defender = addUnit(P2, kInvalidId, 3, 0);
    auto bystander = addUnit(P1, kMournfulWitness, 3, 1);
    obj(attacker).is_stunned = true;                       // nobody deals damage: a tie
    obj(defender).is_stunned = true;
    std::vector<std::optional<PlayerId>> results;
    auto conn = events.on_combat_ended.connect(
        [&](const CombatEndedEvent& e) { results.push_back(e.winner); });
    int moved = 0;
    auto c2 = events.on_unit_moved.connect([&](const UnitMovedEvent& e) { if (e.object == attacker) ++moved; });
    withEngine([&](GameEngine& e) { e.testHook_runCombat(BattlefieldId::fromRaw(0)); e.testHook_runChain(); });
    conn.disconnect(); c2.disconnect();
    ASSERT_EQ(results.size(), 1u);
    EXPECT_FALSE(results[0].has_value()) << "a tie";
    ASSERT_TRUE(atBase(attacker)) << "the attacker was recalled";
    EXPECT_EQ(moved, 0) << "a recall is not a move";
    EXPECT_TRUE(obj(attacker).isEmpowered()) << "it was in that combat";
    EXPECT_FALSE(obj(bystander).isEmpowered());
    EXPECT_TRUE(obj(defender).location.has_value());
}


// ═══════════════════════════════════════════════════════════════════════════
// 21. Sixth follow-up (plans/09 9.U3, 9.V1)
// ═══════════════════════════════════════════════════════════════════════════

// 9.U3, as it stands since the testing phase (finding D6): a play that was
// offered at chain priority and puts nothing on the chain is an engine error.
// It used to be dropped from the window's list and the decision asked again
// (and an outside edit of the state brought it back) — an option that was
// offered and silently did nothing. Not one such play occurred in 2,268 games
// across every pairing of the constructed lists, so the tolerance hid nothing
// but future defects.
TEST_F(SweepTriggersTest, PriorityWindow_APlayThatPutsNothingOnTheChainIsAnError) {
    auto source = addUnit(P1, kInvalidId, 2, -1);
    auto reaction = addToHand(P1, kDecreeOfInsight);
    ChainManager cm(state, events, card_db);
    int plays = 0;
    cm.setPlayCard([&](const Intent&) { ++plays; });               // the play never happens
    cm.addAbility(source, P1, kInvalidId);
    Intent play;
    play.type = IntentType::PlayReaction;
    play.player = P1;
    play.card = reaction;
    int asked = 0;
    try {
        cm.processFEPR(
            [&](PlayerId, const std::vector<Intent>&) -> Intent { ++asked; return play; },
            [](const ChainItem&) {},
            [](const ChainItem&) {},
            [&](PlayerId p) { return std::vector<Intent>{Intent::passPriority(p), play}; });
        ADD_FAILURE() << "a play that did not happen was accepted";
    } catch (const std::logic_error& ex) {
        EXPECT_NE(std::string(ex.what()).find("put nothing on the chain"), std::string::npos)
            << ex.what();
    }
    EXPECT_EQ(plays, 1);
    EXPECT_EQ(asked, 1) << "not asked again";
}

// 9.V1: the combat-damage half of 9.Q3, through a real combat. A unit takes
// spell damage, the damage is removed, and it then dies of combat damage: not
// "killed with a spell" (Immortal Phoenix 37 stays in the trash, unasked).
TEST_F(SweepTriggersTest, KillWithASpell_HealedSpellDamageThenCombatDamageIsNotASpellKill) {
    constexpr CardDefId kImmortalPhoenix = 37;
    auto phoenix = addToHand(P1, kImmortalPhoenix);
    {
        auto& h = state.player(P1).hand;
        h.erase(std::remove(h.begin(), h.end(), phoenix), h.end());
        obj(phoenix).zone = ZoneType::Trash;
        state.player(P1).trash.push_back(phoenix);
    }
    auto spell = addToHand(P1, kDredgeUp);
    state.battlefields[0].controller = P2;
    state.battlefields[0].is_contested = true;
    state.battlefields[0].contested_by = P1;
    state.battlefields[0].combat_staged = true;
    addUnit(P1, kInvalidId, 5, 0);
    auto defender = addUnit(P2, kInvalidId, 2, 0);
    int phoenix_triggers = 0;
    auto conn = events.on_log.connect([&](const LogEvent& e) {
        if (e.message.find("TRIGGER: Immortal Phoenix") != std::string::npos) ++phoenix_triggers;
    });
    withEngine([&](GameEngine& e) {
        e.testHook_executor().dealDamage(defender, 1, spell);      // P1's spell, not lethal
        e.mutableState().getObject(defender).damage_marked = 0;    // the damage is removed
        e.testHook_runCombat(BattlefieldId::fromRaw(0));
        e.testHook_runChain();
    });
    conn.disconnect();
    ASSERT_FALSE(obj(defender).location.has_value()) << "it died in the combat";
    EXPECT_EQ(obj(defender).last_known.killed_by, P1);
    EXPECT_FALSE(obj(defender).last_known.killed_with_spell);
    EXPECT_EQ(phoenix_triggers, 0);
}

// Control for the test above: with the spell damage still marked, the same
// combat death IS a kill with a spell (the marker is what the test removes).
TEST_F(SweepTriggersTest, KillWithASpell_SpellDamageStillMarkedCountsInCombat) {
    auto spell = addToHand(P1, kDredgeUp);
    state.battlefields[0].controller = P2;
    state.battlefields[0].is_contested = true;
    state.battlefields[0].contested_by = P1;
    state.battlefields[0].combat_staged = true;
    addUnit(P1, kInvalidId, 5, 0);
    auto defender = addUnit(P2, kInvalidId, 3, 0);
    withEngine([&](GameEngine& e) {
        e.testHook_executor().dealDamage(defender, 1, spell);
        e.testHook_runCombat(BattlefieldId::fromRaw(0));
        e.testHook_runChain();
    });
    ASSERT_FALSE(obj(defender).location.has_value());
    EXPECT_TRUE(obj(defender).last_known.killed_with_spell);
}


// ═══════════════════════════════════════════════════════════════════════════
// 22. Reflexive triggers are chain items (plans/09 9.BB3; CR 387-389)
// ═══════════════════════════════════════════════════════════════════════════

/// The other player's agent: passes, and the first time it is given priority
/// while a reflexive trigger of `source` is on the chain it calls `look`
/// with the state — what a player who may respond sees between the two parts.
class ReflexiveWatcher : public AgentInterface {
public:
    GameObjectId source = kInvalidId;
    std::function<void(const GameState&, const ChainItem&)> look;
    int seen = 0;
    Intent selectAction(const GameState& st, const std::vector<Intent>& legal) override {
        for (const auto& ci : st.chain.items)
            if (ci.source == source && ci.reflexive_index >= 0) {
                if (seen++ == 0 && look) look(st, ci);
                break;
            }
        for (const auto& i : legal)
            if (i.type == IntentType::PassPriority || i.type == IntentType::PassFocus) return i;
        return legal.empty() ? Intent{} : legal.front();
    }
};

/// Answers each prompt, in order, with the object named for it; fails the
/// test on a prompt it was not told about or that does not offer the object.
static EnginePathAgent::Picker inOrder(std::vector<GameObjectId> answers,
                                       std::shared_ptr<int> asked = std::make_shared<int>(0)) {
    return [answers = std::move(answers), asked](const std::vector<Intent>& legal) {
        const size_t n = static_cast<size_t>((*asked)++);
        if (n >= answers.size()) {
            ADD_FAILURE() << "prompt " << n + 1 << " was not expected";
            return legal.front();
        }
        for (const auto& i : legal)
            if (!i.chosen_objects.empty() && i.chosen_objects.front() == answers[n]) return i;
        ADD_FAILURE() << "prompt " << n + 1 << " did not offer object " << answers[n];
        return legal.front();
    };
}

constexpr CardDefId kHwei = 642;            // "... Then, do the following based on the discarded card's type"
constexpr CardDefId kTwistedFate = 200;     // "... Do one of the following based on its domain"
constexpr CardDefId kDeceiver = 756;        // "... Then do this: It becomes a copy of another unit there."

// Hwei 642: after the discard, "Unit — Give me +3 [M] this turn" is on the
// chain as its own item, and the opponent has priority before it resolves.
TEST_F(SweepTriggersTest, Reflexive_HweiTheOpponentHasPriorityAfterTheDiscard) {
    auto hwei = addUnit(P1, kHwei, 5, 0);
    stockDeck(P1, 1);
    auto discard = addToHand(P1, kLoyalPoro);                 // a unit card
    const GameObjectId drawn = state.player(P1).main_deck.back();
    (void)drawn;
    ReflexiveWatcher p2;
    p2.source = hwei;
    bool discarded_then = false;
    int might_then = -1, data_then = -1;
    p2.look = [&](const GameState& st, const ChainItem& ci) {
        const auto& trash = st.player(P1).trash;
        discarded_then = std::find(trash.begin(), trash.end(), discard) != trash.end();
        might_then = st.getObject(hwei).temp_might_bonus;
        data_then = ci.reflexive_data.empty() ? -1 : ci.reflexive_data[0];
    };
    auto asked = std::make_shared<int>(0);
    EnginePathAgent p1(inOrder({discard}, asked));
    fireTriggerThroughEngine(hwei, TriggerType::WhenIMove, &p1, kInvalidId, &p2);
    EXPECT_EQ(p2.seen > 0, true) << "the opponent got priority with the reflexive trigger on the chain";
    EXPECT_TRUE(discarded_then) << "the first part was done";
    EXPECT_EQ(might_then, 0) << "the second part had not happened yet";
    EXPECT_EQ(data_then, static_cast<int>(CardType::Unit));
    EXPECT_EQ(obj(hwei).temp_might_bonus, 3) << "and then it resolved";
    EXPECT_EQ(*asked, 1);
}

// Its choices are made as it is finalized (CR 388.4): "Gear — Ready up to 2
// runes" names its runes after the discard and before anyone has priority.
TEST_F(SweepTriggersTest, Reflexive_HweiChoosesItsRunesAtFinalization) {
    auto hwei = addUnit(P1, kHwei, 5, 0);
    stockDeck(P1, 1);
    auto gear = addToHand(P1, kShepherdsHeirloom);            // a gear card
    auto r1 = addRune(P1, Domain::Calm, /*exhausted=*/true);
    auto r2 = addRune(P1, Domain::Calm, /*exhausted=*/true);
    auto r3 = addRune(P1, Domain::Calm, /*exhausted=*/true);
    ReflexiveWatcher p2;
    p2.source = hwei;
    auto asked = std::make_shared<int>(0);
    int asked_when_p2_could_respond = -1;
    bool runes_still_exhausted = false;
    p2.look = [&](const GameState& st, const ChainItem&) {
        asked_when_p2_could_respond = *asked;
        runes_still_exhausted = st.getObject(r1).is_exhausted && st.getObject(r3).is_exhausted;
    };
    EnginePathAgent p1(inOrder({gear, r3, r1}, asked));
    fireTriggerThroughEngine(hwei, TriggerType::WhenIMove, &p1, kInvalidId, &p2);
    EXPECT_EQ(asked_when_p2_could_respond, 3) << "the discard, then both runes, already chosen";
    EXPECT_TRUE(runes_still_exhausted) << "chosen, not yet readied";
    EXPECT_FALSE(obj(r1).is_exhausted);
    EXPECT_FALSE(obj(r3).is_exhausted);
    EXPECT_TRUE(obj(r2).is_exhausted) << "the one that was not named";
    EXPECT_EQ(*asked, 3);
}

// A rune named at finalization that was readied, or left the board, before the
// trigger resolves is skipped, and no other rune takes its place: the list is
// rebuilt at resolution and selectSeveral drops what is no longer in it (which
// is why the card itself re-checks nothing).
TEST_F(SweepTriggersTest, Reflexive_HweiARuneReadiedOrGoneInResponseIsSkipped) {
    auto hwei = addUnit(P1, kHwei, 5, 0);
    stockDeck(P1, 1);
    auto gear = addToHand(P1, kShepherdsHeirloom);
    auto r1 = addRune(P1, Domain::Calm, /*exhausted=*/true);
    auto r2 = addRune(P1, Domain::Calm, /*exhausted=*/true);
    auto r3 = addRune(P1, Domain::Calm, /*exhausted=*/true);
    ReflexiveWatcher p2;
    p2.source = hwei;
    bool done = false;
    p2.look = [&](const GameState& st, const ChainItem&) {
        if (done) return;
        done = true;
        // The engine's own state (the fixture's `state` is written back after
        // the run), changed here as an effect played in response would.
        auto& live = const_cast<GameState&>(st);
        live.getObject(r1).is_exhausted = false;   // in response: readied by something else
        auto& gone = live.getObject(r2);           // in response: off the board
        gone.location = std::nullopt;
        gone.zone = ZoneType::Trash;
        live.player(P1).trash.push_back(r2);
    };
    auto asked = std::make_shared<int>(0);
    EnginePathAgent p1(inOrder({gear, r1, r2}, asked));
    // What the card says it readied: readyObject would itself refuse a rune
    // that is off the board, so the card's own count is what shows that the
    // stale picks never reached it.
    std::string said;
    auto conn = events.on_log.connect([&](const LogEvent& e) {
        const auto at = e.message.find("HWEI: discarded Gear");
        if (at != std::string::npos) said = e.message.substr(at);
    });
    fireTriggerThroughEngine(hwei, TriggerType::WhenIMove, &p1, kInvalidId, &p2);
    conn.disconnect();
    ASSERT_TRUE(done) << "the opponent had priority with the trigger on the chain";
    EXPECT_EQ(said, "HWEI: discarded Gear -> readied 0 rune(s)");
    EXPECT_EQ(*asked, 3) << "no new choice as it resolves";
    EXPECT_FALSE(obj(r1).is_exhausted);
    EXPECT_TRUE(obj(r2).is_exhausted) << "a rune that left the board is not readied";
    EXPECT_EQ(obj(r2).zone, ZoneType::Trash);
    EXPECT_TRUE(obj(r3).is_exhausted) << "no other rune takes the place of a named one";
}

// Twisted Fate, Gambler 200: the rune is revealed and recycled, then the
// payoff for its domain is a chain item — its target chosen at finalization,
// the opponent with priority before the stun.
TEST_F(SweepTriggersTest, Reflexive_TwistedFateThePayoffIsAChainItem) {
    auto tf = addUnit(P1, kTwistedFate, 4, 0);
    auto enemy = addUnit(P2, kInvalidId, 3, 1);
    auto rune = state.createObject();
    {
        auto& r = state.getObject(rune);
        r.owner = r.controller = P1;
        r.card_type = CardType::Rune;
        r.name = "Order Rune";
        r.domains = {Domain::Order};
        r.zone = ZoneType::RuneDeck;
        state.player(P1).rune_deck.push_back(rune);
    }
    auto bottom = state.createObject();
    {
        auto& r = state.getObject(bottom);
        r.owner = r.controller = P1;
        r.card_type = CardType::Rune;
        r.name = "Calm Rune";
        r.domains = {Domain::Calm};
        r.zone = ZoneType::RuneDeck;
        state.player(P1).rune_deck.insert(state.player(P1).rune_deck.begin(), bottom);
    }
    ReflexiveWatcher p2;
    p2.source = tf;
    auto asked = std::make_shared<int>(0);
    bool recycled_then = false, stunned_then = true;
    int asked_then = -1;
    p2.look = [&](const GameState& st, const ChainItem&) {
        recycled_then = st.player(P1).rune_deck.front() == rune;
        stunned_then = st.getObject(enemy).is_stunned;
        asked_then = *asked;
    };
    EnginePathAgent p1(inOrder({enemy}, asked));
    fireTriggerThroughEngine(tf, TriggerType::WhenIAttack, &p1, kInvalidId, &p2);
    EXPECT_GT(p2.seen, 0);
    EXPECT_TRUE(recycled_then) << "revealed and recycled before the payoff";
    EXPECT_FALSE(stunned_then);
    EXPECT_EQ(asked_then, 1) << "the unit to stun was chosen as the payoff was finalized";
    EXPECT_TRUE(obj(enemy).is_stunned);
}

// A rune of none of the three domains creates no reflexive trigger.
TEST_F(SweepTriggersTest, Reflexive_TwistedFateNoPayoffForAnotherDomain) {
    auto tf = addUnit(P1, kTwistedFate, 4, 0);
    auto rune = state.createObject();
    auto& r = state.getObject(rune);
    r.owner = r.controller = P1;
    r.card_type = CardType::Rune;
    r.name = "Calm Rune";
    r.domains = {Domain::Calm};
    r.zone = ZoneType::RuneDeck;
    state.player(P1).rune_deck.push_back(rune);
    ReflexiveWatcher p2;
    p2.source = tf;
    EnginePathAgent p1(inOrder({}));
    fireTriggerThroughEngine(tf, TriggerType::WhenIAttack, &p1, kInvalidId, &p2);
    EXPECT_EQ(p2.seen, 0);
    EXPECT_EQ(state.player(P1).rune_deck.size(), 1u) << "revealed and recycled all the same";
}

// Deceiver 756: the Reflection is in play — still a blank Reflection, without
// [Temporary] — while "Then do this: It becomes a copy of another unit there.
// Give it [Temporary]." waits on the chain with the opponent able to respond.
TEST_F(SweepTriggersTest, Reflexive_DeceiverTheCopyIsAChainItem) {
    auto legend = addLegend(P1, kDeceiver);
    state.battlefields[0].controller = P1;
    auto scored_bf = setBattlefieldCard(0, kBackAlleyBar, P1);
    auto model = addUnit(P1, kLoyalPoro, 1, 0);
    auto card = addToHand(P1, kDredgeUp);
    ReflexiveWatcher p2;
    p2.source = legend;
    std::string token_name_then;
    bool temporary_then = true;
    auto asked = std::make_shared<int>(0);
    int asked_then = -1;
    p2.look = [&](const GameState& st, const ChainItem& ci) {
        const auto token = GameObjectId::fromRaw(ci.reflexive_data.at(0));
        token_name_then = st.getObject(token).name;
        temporary_then = st.getObject(token).keywords.has(Keyword::Temporary);
        asked_then = *asked;
    };
    // Prompts: the "may" (yes), the card to discard, then — for the reflexive
    // trigger, as it is finalized — the unit to copy.
    EnginePathAgent p1([&, asked](const std::vector<Intent>& legal) {
        const int n = (*asked)++;
        if (n == 0) {
            for (const auto& i : legal) if (i.chosen_value && *i.chosen_value == 1) return i;
            ADD_FAILURE() << "prompt 1 was not the may";
        } else if (n == 1 || n == 2) {
            const GameObjectId want = n == 1 ? card : model;
            for (const auto& i : legal)
                if (!i.chosen_objects.empty() && i.chosen_objects.front() == want) return i;
            ADD_FAILURE() << "prompt " << n + 1 << " did not offer the expected object";
        } else {
            ADD_FAILURE() << "an unexpected prompt";
        }
        return legal.front();
    });
    fireTriggerThroughEngine(legend, TriggerType::WhenIConquerOrHold, &p1, scored_bf, &p2);
    EXPECT_GT(p2.seen, 0);
    EXPECT_EQ(token_name_then, "Reflection") << "not yet a copy";
    EXPECT_FALSE(temporary_then) << "[Temporary] is given by the reflexive trigger";
    EXPECT_EQ(asked_then, 3) << "what it copies was chosen as the reflexive trigger was finalized";
    const auto tokens = tokenUnits(P1);
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(obj(tokens[0]).card_def_id, kLoyalPoro);
    EXPECT_TRUE(obj(tokens[0]).keywords.has(Keyword::Temporary));
    EXPECT_FALSE(obj(tokens[0]).is_exhausted);
}

// With no engine attached there is nothing to put a reflexive trigger on.
TEST_F(SweepTriggersTest, Reflexive_CannotBeQueuedOutsideAGame) {
    auto hwei = addUnit(P1, kHwei, 5, 0);
    EffectExecutor exec(state, events, card_db);
    EXPECT_FALSE(exec.queueReflexiveTrigger(hwei, P1, 0, {1}));
    EXPECT_TRUE(state.chain.items.empty());
}


// ═══════════════════════════════════════════════════════════════════════════
// 23. The other reflexive-trigger cards (plans/09 9.DD4; CR 387-389)
// ═══════════════════════════════════════════════════════════════════════════

/// Fixture for the tests below: one play or trigger, P1 answering from a
/// declared list, P2 watching for the reflexive trigger on the chain.
class ReflexiveCardsTest : public SweepTriggersTest {
protected:
    ReflexiveWatcher p2;
    std::shared_ptr<int> unexpected = std::make_shared<int>(0);

    /// P1's answers: an object from `objects` when one is offered (each used
    /// once), else "yes" to a yes/no, else an option whose value is in
    /// `values`. A prompt that matches none of those fails the test.
    EnginePathAgent::Picker answers(std::vector<GameObjectId> objects,
                                    std::vector<int> values = {}) {
        auto left = std::make_shared<std::vector<GameObjectId>>(std::move(objects));
        auto bad = unexpected;
        return [left, values, bad](const std::vector<Intent>& legal) {
            for (const auto& i : legal) {
                if (i.chosen_objects.empty()) continue;
                auto it = std::find(left->begin(), left->end(), i.chosen_objects.front());
                if (it != left->end()) { left->erase(it); return i; }
            }
            bool yes_no = legal.size() == 2;
            for (const auto& i : legal) if (!i.chosen_objects.empty()) yes_no = false;
            if (yes_no)
                for (const auto& i : legal)
                    if (i.chosen_value && *i.chosen_value == 1) return i;
            for (const auto& i : legal)
                if (i.chosen_value &&
                    std::find(values.begin(), values.end(), *i.chosen_value) != values.end())
                    return i;
            ++*bad;
            ADD_FAILURE() << "a prompt the test did not declare an answer for ("
                          << legal.size() << " options, first label: "
                          << legal.front().choice_label << ")";
            return legal.front();
        };
    }
    /// Play `spell` from P1's hand with `targets` locked in, cost funded.
    void cast(GameObjectId spell, std::vector<GameObjectId> targets,
              EnginePathAgent::Picker picker) {
        Intent play;
        play.type = IntentType::PlayCard;
        play.player = P1;
        play.card = spell;
        play.targets = std::move(targets);
        const auto& def = card_db.get(obj(spell).card_def_id);
        state.player(P1).rune_pool.energy += def.energy_cost;
        state.player(P1).rune_pool.universal_power += def.power_cost;
        EnginePathAgent p1(std::move(picker));
        p2.source = spell;
        withEngine([&](GameEngine& e) { e.testHook_executeIntent(play); }, &p1, &p2);
    }
    void trigger(GameObjectId source, TriggerType t, EnginePathAgent::Picker picker) {
        EnginePathAgent p1(std::move(picker));
        p2.source = source;
        fireTriggerThroughEngine(source, t, &p1, kInvalidId, &p2);
    }
    void TearDown() override {
        EXPECT_GT(p2.seen, 0) << "the opponent never had priority with the reflexive "
                                 "trigger on the chain";
        EXPECT_EQ(*unexpected, 0);
    }
    GameObjectId inTrashOf(PlayerId p, CardDefId def) {
        auto id = addToHand(p, def);
        auto& h = state.player(p).hand;
        h.erase(std::remove(h.begin(), h.end(), id), h.end());
        obj(id).zone = ZoneType::Trash;
        state.player(p).trash.push_back(id);
        return id;
    }
};

// Forgotten Relic 895: "When you burn a unit this way, do this: Give a
// friendly unit +[M] equal to the burned card's Might this turn."
TEST_F(ReflexiveCardsTest, ForgottenRelic) {
    auto relic = addGear(P1, 895);
    auto unit = addUnit(P1, kInvalidId, 1, 0);
    auto top = addToDeck(P1, kLoyalPoro);
    int might_then = -1; bool burned_then = false;
    p2.look = [&](const GameState& st, const ChainItem&) {
        might_then = st.getObject(unit).temp_might_bonus;
        const auto& tr = st.player(P1).trash;
        burned_then = std::find(tr.begin(), tr.end(), top) != tr.end();
    };
    trigger(relic, TriggerType::AtStartOfBeginning, answers({unit}));
    EXPECT_TRUE(burned_then);
    EXPECT_EQ(might_then, 0);
    EXPECT_EQ(obj(unit).temp_might_bonus, card_db.get(kLoyalPoro).might);
}

// Disintegrate 5: "If this kills it, do this: draw 1."
TEST_F(ReflexiveCardsTest, Disintegrate) {
    stockDeck(P1, 2);
    auto enemy = addUnit(P2, kInvalidId, 3, 0);
    auto spell = addToHand(P1, 5);
    int hand_then = -1; bool dead_then = false;
    p2.look = [&](const GameState& st, const ChainItem&) {
        hand_then = static_cast<int>(st.player(P1).hand.size());
        dead_then = !st.getObject(enemy).location.has_value();
    };
    cast(spell, {enemy}, answers({}));
    EXPECT_TRUE(dead_then);
    EXPECT_EQ(hand_then, 0);
    EXPECT_EQ(handSize(P1), 1);
}

// Dragon's Rage 255: "Move an enemy unit. Then do this: Choose another enemy
// unit at its destination. They deal damage equal to their Mights ..."
TEST_F(ReflexiveCardsTest, DragonsRage) {
    auto mover = addUnit(P2, kInvalidId, 2, 0);
    auto other = addUnit(P2, kInvalidId, 5, 1);
    auto spell = addToHand(P1, 255);
    bool moved_then = false; int damage_then = -1;
    p2.look = [&](const GameState& st, const ChainItem&) {
        moved_then = st.getObject(mover).battlefieldId() ==
                     std::optional<BattlefieldId>{state.battlefields[1].id};
        damage_then = st.getObject(other).damage_marked;
    };
    // The mover is locked in as it is played; then where it goes (battlefield
    // 1 = destination code 2), then — the reflexive trigger — the other unit.
    cast(spell, {mover}, answers({other}, {2}));
    EXPECT_TRUE(moved_then);
    EXPECT_EQ(damage_then, 0);
    EXPECT_EQ(obj(other).damage_marked, 2);
    EXPECT_FALSE(obj(mover).location.has_value()) << "5 damage to a 2 [M] unit";
}

// Guards! 475: "Play a 2 [M] Sand Soldier unit token. Then do this: You may
// pay [C] to ready it." — the may and the [C] at the trigger's finalization.
TEST_F(ReflexiveCardsTest, Guards) {
    auto spell = addToHand(P1, 475);
    state.player(P1).rune_pool.power[static_cast<int>(Domain::Order)] = 1;
    bool exhausted_then = false; int power_then = -1;
    p2.look = [&](const GameState& st, const ChainItem& ci) {
        exhausted_then = st.getObject(GameObjectId::fromRaw(ci.reflexive_data.at(0))).is_exhausted;
        power_then = st.player(P1).rune_pool.power[static_cast<int>(Domain::Order)];
    };
    cast(spell, {}, answers({}, {0}));
    EXPECT_TRUE(exhausted_then) << "not readied yet";
    EXPECT_EQ(power_then, 0) << "the [C] was paid as the trigger was finalized";
    const auto tokens = tokenUnits(P1);
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_FALSE(obj(tokens[0]).is_exhausted);
}

// Arise! 513: "... Then do this: Ready up to two of them."
TEST_F(ReflexiveCardsTest, Arise) {
    addGear(P1, kInvalidId, /*equipment=*/true);
    auto spell = addToHand(P1, 513);
    bool exhausted_then = false;
    GameObjectId token = kInvalidId;
    p2.look = [&](const GameState& st, const ChainItem& ci) {
        token = GameObjectId::fromRaw(ci.reflexive_data.at(0));
        exhausted_then = st.getObject(token).is_exhausted;
    };
    // The one token is the first (lowest-id) object offered; answered by a
    // picker that takes the only object option.
    EnginePathAgent::Picker only_object = [](const std::vector<Intent>& legal) {
        for (const auto& i : legal) if (!i.chosen_objects.empty()) return i;
        ADD_FAILURE() << "no token was offered";
        return legal.front();
    };
    cast(spell, {}, only_object);
    EXPECT_TRUE(exhausted_then);
    ASSERT_NE(token, kInvalidId);
    EXPECT_FALSE(obj(token).is_exhausted);
}

// Bone Skewer 701: "... They play that unit to that battlefield ... If they
// do, then do this: [Stun] it."
TEST_F(ReflexiveCardsTest, BoneSkewer) {
    auto theirs = addToHand(P2, kLoyalPoro);
    auto spell = addToHand(P1, 701);
    bool on_board_then = false, stunned_then = true;
    p2.look = [&](const GameState& st, const ChainItem&) {
        on_board_then = st.getObject(theirs).location.has_value();
        stunned_then = st.getObject(theirs).is_stunned;
    };
    // The battlefield (mode 0), the may (yes), the unit.
    cast(spell, {}, answers({theirs}, {0}));
    EXPECT_TRUE(on_board_then);
    EXPECT_FALSE(stunned_then);
    EXPECT_TRUE(obj(theirs).is_stunned);
}

// Death from Below 747 — the rule book's example (CR 388.4): the trigger is on
// the chain with the spell already in the trash, the may and the [A] settled
// at its finalization; then the card is played from the trash.
TEST_F(ReflexiveCardsTest, DeathFromBelow) {
    auto a = addUnit(P2, kInvalidId, 1, 0);
    auto b = addUnit(P2, kInvalidId, 1, 0);
    auto spell = addToHand(P1, kDeathFromBelow);
    state.player(P1).rune_pool.universal_power += 1;       // the [A]
    bool in_trash_then = false, b_alive_then = false; int power_then = -1;
    p2.look = [&](const GameState& st, const ChainItem&) {
        const auto& tr = st.player(P1).trash;
        in_trash_then = std::find(tr.begin(), tr.end(), spell) != tr.end();
        b_alive_then = st.getObject(b).location.has_value();
        power_then = st.player(P1).rune_pool.universal_power;
    };
    // In order: the unit to kill (a), the reflexive trigger's "may" (yes),
    // then the target of the play from the trash (b).
    auto n = std::make_shared<int>(0);
    cast(spell, {}, [=](const std::vector<Intent>& legal) {
        const int k = (*n)++;
        if (k == 1) {
            for (const auto& i : legal) if (i.chosen_value && *i.chosen_value == 1) return i;
        } else if (k == 0 || k == 2) {
            const GameObjectId want = k == 0 ? a : b;
            for (const auto& i : legal)
                if (!i.chosen_objects.empty() && i.chosen_objects.front() == want) return i;
        }
        ADD_FAILURE() << "prompt " << k + 1 << " was not the expected one";
        return legal.front();
    });
    EXPECT_TRUE(in_trash_then) << "the spell had resolved and was in the trash";
    EXPECT_TRUE(b_alive_then);
    EXPECT_EQ(power_then, 0) << "[A] paid as the trigger was finalized";
    EXPECT_FALSE(obj(a).location.has_value());
    EXPECT_FALSE(obj(b).location.has_value()) << "played again from the trash";
    const auto& trash = state.player(P1).trash;
    EXPECT_EQ(std::count(trash.begin(), trash.end(), spell), 1);
}

// Alpha Strike 751: "Then for each unit this kills, do this: Gain 1 XP."
TEST_F(ReflexiveCardsTest, AlphaStrike) {
    auto striker = addUnit(P1, kInvalidId, 4, -1);
    auto enemy = addUnit(P2, kInvalidId, 3, 0);
    auto spell = addToHand(P1, 751);
    int xp_then = -1; bool dead_then = false;
    p2.look = [&](const GameState& st, const ChainItem&) {
        xp_then = st.player(P1).xp;
        dead_then = !st.getObject(enemy).location.has_value();
    };
    // The split: that unit, then "no more targets" (value 0).
    cast(spell, {striker}, answers({enemy}, {0}));
    EXPECT_TRUE(dead_then);
    EXPECT_EQ(xp_then, 0);
    EXPECT_EQ(state.player(P1).xp, 1);
}

// Mirror Image 757: "... Then do this: It becomes a copy of that unit. Give it
// [Temporary]."
TEST_F(ReflexiveCardsTest, MirrorImage) {
    auto original = addUnit(P2, kLoyalPoro, 3, 0);
    auto spell = addToHand(P1, 757);
    std::string name_then; bool temporary_then = true;
    p2.look = [&](const GameState& st, const ChainItem& ci) {
        const auto& t = st.getObject(GameObjectId::fromRaw(ci.reflexive_data.at(0)));
        name_then = t.name;
        temporary_then = t.keywords.has(Keyword::Temporary);
    };
    cast(spell, {original}, answers({}));
    EXPECT_EQ(name_then, "Reflection");
    EXPECT_FALSE(temporary_then);
    const auto tokens = tokenUnits(P1);
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(obj(tokens[0]).card_def_id, kLoyalPoro);
    EXPECT_TRUE(obj(tokens[0]).keywords.has(Keyword::Temporary));
}

// Wild Claw 876: "... Then you may do this: Empower it."
TEST_F(ReflexiveCardsTest, WildClaw) {
    constexpr CardDefId kPunchingPoro = 794;
    auto poro = addToDeck(P1, kPunchingPoro);
    auto spell = addToHand(P1, 876);
    bool played_then = false, empowered_then = true;
    p2.look = [&](const GameState& st, const ChainItem&) {
        played_then = st.getObject(poro).location.has_value();
        empowered_then = st.getObject(poro).isEmpowered();
    };
    cast(spell, {}, answers({poro}));
    EXPECT_TRUE(played_then);
    EXPECT_FALSE(empowered_then);
    EXPECT_TRUE(obj(poro).isEmpowered());
}

// Rell, Magnetic 347: "... If you do, then do this: Attach it to me."
TEST_F(ReflexiveCardsTest, Rell) {
    auto rell = addUnit(P1, 347, 4, 0);
    auto gear = addToHand(P1, kShepherdsHeirloom);         // an Equipment, [2]
    bool on_board_then = false, attached_then = true;
    p2.look = [&](const GameState& st, const ChainItem&) {
        on_board_then = st.getObject(gear).location.has_value();
        attached_then = st.getObject(gear).attached_to.has_value();
    };
    trigger(rell, TriggerType::WhenIAttack, answers({gear}));
    EXPECT_TRUE(on_board_then);
    EXPECT_FALSE(attached_then);
    EXPECT_EQ(obj(gear).attached_to, std::optional<GameObjectId>{rell});
}

// Ivern, Nurturer 613: "Then if you revealed a Bird, Cat, Dog, or Poro, do
// this: [Buff] a friendly unit."
TEST_F(ReflexiveCardsTest, Ivern) {
    auto ivern = addUnit(P1, 613, 4, 0);
    auto poro = addToDeck(P1, kLoyalPoro);
    bool drawn_then = false; int buff_then = -1;
    p2.look = [&](const GameState& st, const ChainItem&) {
        const auto& h = st.player(P1).hand;
        drawn_then = std::find(h.begin(), h.end(), poro) != h.end();
        buff_then = st.getObject(ivern).buff_count;
    };
    trigger(ivern, TriggerType::WhenIHold, answers({poro, ivern}));
    EXPECT_TRUE(drawn_then);
    EXPECT_EQ(buff_then, 0);
    EXPECT_EQ(obj(ivern).buff_count, 1);
}

// Keeper of Masks 643: "... Then do this: They become copies of me."
TEST_F(ReflexiveCardsTest, KeeperOfMasks) {
    constexpr CardDefId kKeeper = 643;
    auto keeper = addUnit(P1, kKeeper, 2, 0);
    int blank_then = 0;
    p2.look = [&](const GameState& st, const ChainItem& ci) {
        for (auto v : ci.reflexive_data)
            if (st.getObject(GameObjectId::fromRaw(v)).card_def_id != kKeeper) ++blank_then;
    };
    trigger(keeper, TriggerType::WhenYouPlayMe, answers({}));
    EXPECT_EQ(blank_then, 2) << "two Reflections, not yet copies";
    const auto tokens = tokenUnits(P1);
    ASSERT_EQ(tokens.size(), 2u);
    for (auto t : tokens) EXPECT_EQ(obj(t).card_def_id, kKeeper);
}

// Kharox 901: "They [Burn 3]. Then you may do this: Choose a unit in their
// trash and play it, ignoring its cost."
TEST_F(ReflexiveCardsTest, Kharox) {
    auto kharox = addUnit(P1, 901, 5, 0);
    for (int i = 0; i < 2; ++i) addToDeck(P2, kDredgeUp);
    auto unit = addToDeck(P2, kLoyalPoro);                 // top: burned
    int deck_then = -1; bool in_trash_then = false;
    p2.look = [&](const GameState& st, const ChainItem&) {
        deck_then = static_cast<int>(st.player(P2).main_deck.size());
        const auto& tr = st.player(P2).trash;
        in_trash_then = std::find(tr.begin(), tr.end(), unit) != tr.end();
    };
    trigger(kharox, TriggerType::WhenIBecomeEmpowered, answers({unit}, {0}));
    EXPECT_EQ(deck_then, 0) << "burned 3";
    EXPECT_TRUE(in_trash_then) << "chosen, not yet played";
    EXPECT_TRUE(obj(unit).location.has_value());
    EXPECT_EQ(obj(unit).controller, P1);
}

// Every card whose text has one of the reflexive-trigger phrases (CR 388.1)
// either implements a reflexive trigger or says why its phrase is not one.
/// The rule the registry test applies to one card: "" when it is fine, else
/// what is wrong. CR 388.1: "Do this:", "Do the following", "Do one / X of the
/// following". ("for each of the following tags" — Friendship 608, Ivern 739,
/// Daisy! 754 — is a list, not one of the phrases.)
static std::string reflexiveDeclarationProblem(const Card& card) {
    const CardDef& def = card.def();
    std::string text = def.ability_text + " " + def.effect_text;
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    static const std::regex of_the_following(R"(\bdo (one|two|three|\d+|x) of the following)");
    const bool phrase = text.find("do this") != std::string::npos ||
                        text.find("do the following") != std::string::npos ||
                        std::regex_search(text, of_the_following);
    if (phrase && !card.hasReflexiveTrigger() && card.notReflexiveBecause() == nullptr)
        return "prints a reflexive-trigger phrase and neither implements onReflexiveTrigger "
               "nor says why it is not one";
    if (phrase && card.hasReflexiveTrigger() && card.notReflexiveBecause() != nullptr)
        return "both implements a reflexive trigger and says it has none";
    if (!phrase && card.hasReflexiveTrigger())
        return "claims a reflexive trigger its text does not have";
    return "";
}

TEST_F(SweepTriggersTest, Reflexive_EveryCardWithThePhraseDeclaresItself) {
    int with_phrase = 0;
    for (uint32_t n = 1; n <= static_cast<uint32_t>(ml::kCardVocabSize); ++n) {
        const CardDefId id = CardDefId::fromRaw(n);
        const Card* c = card_registry.get(id);
        if (!c) continue;
        EXPECT_EQ(reflexiveDeclarationProblem(*c), "") << c->def().name << " (" << id << ")";
        if (c->hasReflexiveTrigger()) ++with_phrase;
    }
    EXPECT_EQ(with_phrase, 17);
}

// The rule itself, on test cards: the "not reflexive because ..." marker is
// accepted, and each way of getting it wrong is caught.
TEST(SweepTriggersReflexiveRule, AcceptsTheMarkerAndCatchesEachMistake) {
    struct TCard : SpellCard {
        CardDef d;
        bool reflexive = false;
        const char* because = nullptr;
        TCard(const char* text, bool r, const char* b) : reflexive(r), because(b) {
            d.id = 1896;   // a test card: the spare range
            d.name = "Test Phrase Card";
            d.card_type = CardType::Spell;
            d.ability_text = text;
        }
        const CardDef& def() const override { return d; }
        bool hasReflexiveTrigger() const override { return reflexive; }
        const char* notReflexiveBecause() const override { return because; }
    };
    const char* kPhrase = "Deal 1 to a unit. Then do this: draw 1.";
    const char* kList = "Give it +1 [M] for each of the following tags among your units.";
    EXPECT_EQ(reflexiveDeclarationProblem(TCard(kPhrase, true, nullptr)), "");
    EXPECT_EQ(reflexiveDeclarationProblem(
                  TCard(kPhrase, false, "\"do this\" here is reminder text, not an ability")), "");
    EXPECT_NE(reflexiveDeclarationProblem(TCard(kPhrase, false, nullptr)), "")
        << "the phrase with neither declaration";
    EXPECT_NE(reflexiveDeclarationProblem(TCard(kPhrase, true, "but also not")), "");
    EXPECT_EQ(reflexiveDeclarationProblem(TCard(kList, false, nullptr)), "");
    EXPECT_NE(reflexiveDeclarationProblem(TCard(kList, true, nullptr)), "")
        << "a claim without the phrase";
    EXPECT_EQ(reflexiveDeclarationProblem(TCard("Do one of the following: draw 1; or burn 1.",
                                                true, nullptr)), "");
    EXPECT_NE(reflexiveDeclarationProblem(TCard("Do the following based on its type: draw 1.",
                                                false, nullptr)), "");
}

}  // namespace
}  // namespace test_review_fix_sweep_triggers
}  // namespace riftbound::test
