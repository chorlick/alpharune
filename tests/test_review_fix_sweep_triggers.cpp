/// @file test_review_fix_sweep_triggers.cpp
/// Regression tests for the stage 3 sweep, part 3 (trigger_manager,
/// chain_manager, effect_executor): the engine pieces the card streams could
/// not add, and the cards that waited on them (plans/STATUS.md carry-over).
///
/// Every test drives the real GameEngine (TriggerManager + ChainManager +
/// finalization) through the fixture's engine entry points.

#include "cards/card_test_fixture.h"
#include "cards/card_helpers.h"
#include "engine/chain_manager.h"
#include "engine/effect_executor.h"
#include "engine/game_engine.h"

#include <gtest/gtest.h>

#include <algorithm>
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
    static Intent keepOnTop(const std::vector<Intent>& legal) {      // a Predict prompt
        for (const auto& i : legal) if (i.chosen_objects.empty() && !i.chosen_value) return i;
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
    withEngine([&](GameEngine& e) { e.testHook_scoreHold(P1, 0); e.testHook_runChain(); });
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
    withEngine([&](GameEngine& e) { e.testHook_scoreHold(P1, 1); e.testHook_runChain(); });
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
    withEngine([&](GameEngine& e) { e.testHook_scoreConquer(P1, 0); e.testHook_runChain(); },
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

// A unit's own "When I die" goes through the same gate as every other
// trigger: LeBlanc, Everywhere at Once — "your [Temporary] effects at my
// battlefield don't trigger."
TEST_F(SweepTriggersTest, OwnDeathTrigger_IsSuppressedForATemporaryUnitAtLeBlancsBattlefield) {
    addUnit(P1, kLeBlanc, 4, 0);
    auto crab = addUnit(P1, kScuttleCrab, 1, 0);
    obj(crab).keywords.set(Keyword::Temporary);
    const int xp = state.player(P1).xp;
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) { exec.killObject(crab); });
    EXPECT_EQ(state.player(P1).xp, xp) << "the [Deathknell] of a [Temporary] unit there does not trigger";
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
    EXPECT_NE(state.player(P1).zilean_double_token_turn, state.turn.turn_number)
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
    EnginePathAgent recycle;                        // first option: recycle it
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
    EnginePathAgent agent(EnginePathAgent::choosing(gear));   // yes, then the gear
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

// Death from Below 747 played again while it resolves: the card is on the
// chain for the new play, not in the trash, and is filed once at the end.
TEST_F(SweepTriggersTest, ReplayedSpell_StaysOnTheChainUntilTheNewPlayResolves) {
    auto a = addUnit(P2, kInvalidId, 1, 0);
    auto b = addUnit(P2, kInvalidId, 1, 0);
    auto spell = addToHand(P1, kDeathFromBelow);
    state.player(P1).rune_pool.universal_power += 1;      // the [A] for the replay
    EnginePathAgent p1(EnginePathAgent::choosing(a));     // a, then yes, then what is left
    TrashWatcher p2;
    p2.spell = spell;
    playThroughEngine(spell, &p1, std::nullopt, &p2);
    EXPECT_FALSE(obj(a).location.has_value());
    EXPECT_FALSE(obj(b).location.has_value()) << "the replay killed the second unit";
    EXPECT_GT(p2.seen_on_chain, 0);
    EXPECT_EQ(p2.seen_in_trash_while_on_chain, 0);
    const auto& trash = state.player(P1).trash;
    EXPECT_EQ(std::count(trash.begin(), trash.end(), spell), 1);
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
    Intent play;
    play.type = IntentType::PlayCard;
    play.player = P1;
    play.card = spell;
    play.targets = {enemy};
    EnginePathAgent agent(EnginePathAgent::choosing(enemy));
    withEngine([&](GameEngine& e) { e.testHook_executeIntent(play); }, &agent);
    EXPECT_EQ(obj(enemy).temp_might_bonus, -5) << "no [A] was needed for its [Deflect]";
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

}  // namespace
}  // namespace test_review_fix_sweep_triggers
}  // namespace riftbound::test
