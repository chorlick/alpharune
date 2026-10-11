/// @file test_review_fix_verify_cards.cpp
/// Tests added by the verifier of the 2026-10 card review (stream
/// sweep-verify-cards). Three kinds, each through the real engine path:
///   - Pin_*      : pins behaviour a finding questioned and that was closed as
///                  NOT A DEFECT (the plan line names the test);
///   - *_Accepts* : the accept path of a "you may" that the stage 2 tests only
///                  declined (Card::confirmOptional lists "No" first, so a test
///                  that passes no agent declines);
///   - the rest   : a fix that was merged without a test of its own, or whose
///                  test would still pass with the defect back.

#include "card_test_fixture.h"
#include "cards/card_helpers.h"
#include "engine/effect_executor.h"
#include "engine/game_engine.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

namespace riftbound::test {
namespace test_review_fix_verify_cards {  // file-unique: test sources are unity-built
namespace {

constexpr CardDefId kFallingStar = 29;
constexpr CardDefId kVolibearFurious = 41;
constexpr CardDefId kPiratesHaven = 143;
constexpr CardDefId kSnapvine = 149;
constexpr CardDefId kSalvage = 224;
constexpr CardDefId kCandlelitSanctum = 286;
constexpr CardDefId kRelentlessStorm = 295;
constexpr CardDefId kAzirAscendant = 373;
constexpr CardDefId kArise = 513;
constexpr CardDefId kVoidBurrower = 553;
constexpr CardDefId kBloodRose = 671;
constexpr CardDefId kElderDragon = 680;
constexpr CardDefId kScryersBloom = 698;
constexpr CardDefId kAlphaStrike = 751;
constexpr CardDefId kRavenbloomPrefect = 889;
constexpr CardDefId kRecruit = 954;
constexpr CardDefId kSunDisc = 21;
constexpr CardDefId kWildclawShaman = 147;
constexpr CardDefId kMaddenedMarauder = 191;
constexpr CardDefId kIreliaFervent = 380;
constexpr CardDefId kJayceManOfProgress = 406;
constexpr CardDefId kBlastCone = 695;
constexpr CardDefId kDuskRoseLab = 765;
constexpr CardDefId kJayceBrilliantInventor = 855;
constexpr CardDefId kJaggedCutlass = 860;
constexpr CardDefId kMageseekerWarden = 70;
constexpr CardDefId kPromisingFuture = 115;
constexpr CardDefId kTeemoStrategist = 121;
constexpr CardDefId kIcathianRain = 247;
constexpr CardDefId kBulletTime = 263;
constexpr CardDefId kRekSaiSwarmQueen = 491;
constexpr CardDefId kArcaneShift = 515;
constexpr CardDefId kHeedlessResurrection = 704;
constexpr CardDefId kDeceiver = 756;
constexpr CardDefId kKeepersVerdict = 760;
constexpr CardDefId kForgottenRelic = 895;
constexpr CardDefId kSpiritsRefuge = 63;
constexpr CardDefId kConvergentMutation = 108;
constexpr CardDefId kUdyrWildman = 157;
constexpr CardDefId kBaitedHook = 242;
constexpr CardDefId kAltarOfMemories = 490;
constexpr CardDefId kSoulSword = 601;
constexpr CardDefId kAllay = 603;
constexpr CardDefId kToolsOfEmpire = 864;
constexpr CardDefId kVayne035 = 35;
constexpr CardDefId kAssemblyRig = 342;
constexpr CardDefId kStarSpring = 771;
constexpr CardDefId kAkshan = 431;
constexpr CardDefId kLongSword = 345;         // gear, Equipment
constexpr CardDefId kPurifier = 502;          // legend
constexpr CardDefId kForgeOfTheFluft = 522;
constexpr CardDefId kMeditation = 48;          // spell, [2]
constexpr CardDefId kBlitzcrank = 67;
constexpr CardDefId kMegaMech = 88;            // unit, [7], Mech, 8 [M]
constexpr CardDefId kKaiSaEvolutionary = 112;
constexpr CardDefId kAhri119 = 119;
constexpr CardDefId kMobilize = 134;
constexpr CardDefId kSealOfStrength163 = 163;
constexpr CardDefId kSett164 = 164;
constexpr CardDefId kShipyardSkulker = 175;    // unit, [3], 3 [M], no text
constexpr CardDefId kYasuo205 = 205;
constexpr CardDefId kSpectralMatron = 226;
constexpr CardDefId kKarma235 = 235;
constexpr CardDefId kDarius243 = 243;
constexpr CardDefId kDarkChild = 315;
constexpr CardDefId kGold = 326;
constexpr CardDefId kSerratedDirk = 332;       // gear, Equipment, [1]
constexpr CardDefId kRumbleHotheaded = 349;
constexpr CardDefId kNotSoFast = 368;
constexpr CardDefId kPlunderingPoro392 = 392;
constexpr CardDefId kBreakneckMech = 394;
constexpr CardDefId kEzrealDashing = 404;
constexpr CardDefId kDirewing = 416;
constexpr CardDefId kFaePorter = 446;
constexpr CardDefId kLoyalPup = 447;
constexpr CardDefId kOverzealousFan = 449;
constexpr CardDefId kFizzTrickster = 461;
constexpr CardDefId kGuards = 475;
constexpr CardDefId kGlascMixologist = 486;
constexpr CardDefId kShurelyasRequiem = 509;
constexpr CardDefId kForgottenMonument = 523;
constexpr CardDefId kVayne537 = 537;
constexpr CardDefId kAhri539 = 539;
constexpr CardDefId kSett543 = 543;
constexpr CardDefId kYasuo546 = 546;
constexpr CardDefId kDarius547 = 547;
constexpr CardDefId kKarma548 = 548;
constexpr CardDefId kVexMocking = 617;
constexpr CardDefId kHwei = 642;
constexpr CardDefId kRepulse = 668;
constexpr CardDefId kStalkingWolf = 728;
constexpr CardDefId kVoidAssault = 758;
constexpr CardDefId kVeteranPoro779 = 779;
constexpr CardDefId kNoxianDemolitionist = 867;
constexpr CardDefId kWildClaw = 876;
constexpr CardDefId kMatriarchOfWar = 940;
constexpr CardDefId kShadowClone = 955;

class VerifyCardsTest : public CardTestFixture {
protected:
    GameObject& o(GameObjectId id) { return state.getObject(id); }
    PlayerState& ps(PlayerId p) { return state.player(p); }
    bool onBoard(GameObjectId id) { return o(id).location.has_value(); }
    bool inBanishment(PlayerId p, GameObjectId id) {
        auto& b = ps(p).banishment;
        return std::find(b.begin(), b.end(), id) != b.end();
    }
    std::optional<BattlefieldId> bfOf(GameObjectId id) { return o(id).battlefieldId(); }
    int& power(PlayerId p, Domain d) { return ps(p).rune_pool.power[static_cast<int>(d)]; }

    GameObjectId addToTrash(PlayerId owner, CardDefId def_id) {
        auto id = addToHand(owner, def_id);
        auto& h = ps(owner).hand;
        h.erase(std::remove(h.begin(), h.end(), id), h.end());
        o(id).zone = ZoneType::Trash;
        if (def_id != kInvalidId) {
            o(id).base_might = card_db.get(def_id).might;
            o(id).current_might = o(id).base_might;
        }
        ps(owner).trash.push_back(id);
        return id;
    }
    /// Take an on-board object off the board into its owner's trash, as if it
    /// had died before the ability under test was finalized.
    void sendToTrash(GameObjectId id) {
        o(id).location = std::nullopt;
        o(id).zone = ZoneType::Trash;
        ps(o(id).owner).trash.push_back(id);
    }
    GameObjectId addGear(PlayerId owner, CardDefId def_id, int at_bf = -1) {
        auto id = state.createObject();
        auto& g = o(id);
        g.owner = g.controller = owner;
        g.card_def_id = def_id;
        g.card_type = CardType::Gear;
        if (def_id != kInvalidId) {
            const auto& def = card_db.get(def_id);
            g.name = def.name;
            g.super_type = def.super_type;
            g.keywords = def.keywords;
            g.domains = def.domains;
            g.tags = def.tags;
            g.might_bonus = def.might_bonus;
        } else {
            g.name = "TestGear";
        }
        if (at_bf >= 0) {
            g.zone = ZoneType::BattlefieldZone;
            g.location = BattlefieldLocation{static_cast<BattlefieldId>(at_bf)};
        } else {
            g.zone = ZoneType::Base;
            g.location = BaseLocation{owner};
        }
        return id;
    }
    GameObjectId addLegend(PlayerId owner, CardDefId def_id) {
        auto id = state.createObject();
        auto& l = o(id);
        const auto& def = card_db.get(def_id);
        l.owner = l.controller = owner;
        l.card_def_id = def_id;
        l.card_type = CardType::Legend;
        l.name = def.name;
        l.domains = def.domains;
        l.tags = def.tags;
        l.zone = ZoneType::LegendZone;
        ps(owner).legend_zone = id;
        return id;
    }
    void attach(GameObjectId gear, GameObjectId unit) {
        auto& g = o(gear);
        auto& u = o(unit);
        g.attached_to = unit;
        g.location = u.location;
        g.zone = u.zone;
        u.attachments.push_back(gear);
        u.attachment_might_bonus += g.might_bonus;
        u.recomputeMight();
    }
    void cleanup() {
        withEngine([](GameEngine& e) { e.testHook_cleanup(); });
    }
    void activate(GameObjectId source, AgentInterface* agent = nullptr, int index = 0) {
        Intent act;
        act.type = IntentType::ActivateAbility;
        act.player = o(source).controller;
        act.ability_source = source;
        act.ability_index = index;
        withEngine([&](GameEngine& e) { e.testHook_executeIntent(act); }, agent, nullptr,
                   act.player);
    }
    /// Play `card` with play-time `targets`, its printed cost covered.
    void playAt(GameObjectId card, std::vector<GameObjectId> targets,
                AgentInterface* p1 = nullptr, AgentInterface* p2 = nullptr,
                const std::function<void(GameEngine&)>& after = nullptr) {
        const PlayerId player = o(card).controller;
        const auto& def = card_db.get(o(card).card_def_id);
        ps(player).rune_pool.energy += def.energy_cost;
        ps(player).rune_pool.universal_power += def.power_cost;
        Intent play;
        play.type = IntentType::PlayCard;
        play.player = player;
        play.card = card;
        play.targets = std::move(targets);
        withEngine([&](GameEngine& e) {
            e.testHook_executeIntent(play);
            if (after) after(e);
        }, p1, p2, player);
    }
    /// Queue `source`'s trigger and run the chain. `probe` is handed the live
    /// state the first time the opponent gets priority: the ability has been
    /// finalized (choices made, costs paid) and has not resolved yet.
    void fireAndProbe(GameObjectId source, TriggerType trigger, AgentInterface* p1_agent,
                      const std::function<void(GameState&)>& probe,
                      GameObjectId subject = kInvalidId) {
        GameState* live = nullptr;
        bool probed = false;
        PickByPredicateAgent p2([&](int, const std::vector<Intent>& legal) {
            if (!probed && live) { probed = true; probe(*live); }
            for (const auto& i : legal)
                if (i.type == IntentType::PassPriority) return i;
            return legal.front();
        });
        withEngine([&](GameEngine& e) {
            live = &e.mutableState();
            e.testHook_executor().queueTriggeredAbility(source, P1, trigger, subject);
            e.testHook_runChain();
            live = nullptr;
        }, p1_agent, &p2);
        EXPECT_TRUE(probed) << "the opponent never got priority";
    }
    /// Execute `intent` for P1; `probe` is handed the live state the first
    /// time the opponent gets priority (the spell or ability is on the chain,
    /// finalized, not resolved).
    void executeAndProbe(const Intent& intent, AgentInterface* p1_agent,
                         const std::function<void(GameState&)>& probe) {
        GameState* live = nullptr;
        bool probed = false;
        PickByPredicateAgent p2([&](int, const std::vector<Intent>& legal) {
            if (!probed && live) { probed = true; probe(*live); }
            for (const auto& i : legal)
                if (i.type == IntentType::PassPriority) return i;
            return legal.front();
        });
        withEngine([&](GameEngine& e) {
            live = &e.mutableState();
            e.testHook_executeIntent(intent);
            e.testHook_runChain();
            live = nullptr;
        }, p1_agent, &p2);
        EXPECT_TRUE(probed) << "the opponent never got priority";
    }
    static void removeFromBoard(GameState& live, GameObjectId id) {
        auto& x = live.getObject(id);
        x.location = std::nullopt;
        x.zone = ZoneType::Trash;
        live.player(x.owner).trash.push_back(id);
    }
    /// A battlefield card of `def_id` in slot `bf`, controlled by `controller`.
    GameObjectId battlefieldCard(int bf, CardDefId def_id, PlayerId controller = P1) {
        auto id = state.createObject();
        auto& b = o(id);
        b.card_def_id = def_id;
        b.card_type = CardType::Battlefield;
        b.name = card_db.get(def_id).name;
        b.owner = b.controller = controller;
        b.zone = ZoneType::BattlefieldZone;
        state.battlefields[bf].card_object_id = id;
        state.battlefields[bf].controller = controller;
        return id;
    }
    GameObjectId unitInHand(PlayerId owner, CardDefId def_id) {
        auto id = addToHand(owner, def_id);
        o(id).base_might = card_db.get(def_id).might;
        o(id).current_might = o(id).base_might;
        return id;
    }
    int countNamed(PlayerId owner, const std::string& name) {
        int n = 0;
        for (const auto& [id, obj] : state.objects)
            if (obj.controller == owner && obj.location.has_value() && obj.name == name) ++n;
        return n;
    }
    GameObjectId firstNamed(PlayerId owner, const std::string& name) {
        GameObjectId best = kInvalidId;
        for (const auto& [id, obj] : state.objects)
            if (obj.controller == owner && obj.location.has_value() && obj.name == name &&
                (best == kInvalidId || id < best)) best = id;
        return best;
    }
    /// True if prompt `n` of `agent` offers the int-coded answer `value`.
    static bool promptOffersValue(const EnginePathAgent& agent, size_t n, int value) {
        if (n >= agent.prompts.size()) return false;
        for (const auto& i : agent.prompts[n])
            if (i.chosen_value.has_value() && *i.chosen_value == value) return true;
        return false;
    }
    std::vector<std::string> tagsOf(CardDefId id) { return card_db.get(id).tags; }
};

using Tags = std::vector<std::string>;

// ═══ Work item 5: Spectral Matron 226 / Rumble 349 use pickEffectPlayLocation ═

TEST_F(VerifyCardsTest, SpectralMatron_DoesNotOfferABattlefieldUnitsCannotBePlayedTo) {
    // CR 355.2.a through the shared helper: a controlled battlefield that
    // says units can't be played there (Rockfall Path) is not a choice.
    auto matron = addToHand(P1, kSpectralMatron);
    auto cheap = addToTrash(P1, kShipyardSkulker);
    state.battlefields[0].controller = P1;
    state.battlefields[0].blocks_unit_play = true;
    state.battlefields[1].controller = P1;
    EnginePathAgent agent(EnginePathAgent::script({1, static_cast<int>(cheap), 2}));
    playThroughEngine(matron, &agent);
    ASSERT_EQ(agent.prompts.size(), 3u) << "the may, the unit, where";
    EXPECT_TRUE(promptOffersValue(agent, 2, 0)) << "base";
    EXPECT_FALSE(promptOffersValue(agent, 2, 1)) << "units can't be played to battlefield #0";
    EXPECT_TRUE(promptOffersValue(agent, 2, 2));
    ASSERT_TRUE(onBoard(cheap));
    EXPECT_EQ(bfOf(cheap), std::optional<BattlefieldId>{1});
}

TEST_F(VerifyCardsTest, RumbleHotheaded_DoesNotOfferABattlefieldUnitsCannotBePlayedTo) {
    auto rumble = addUnit(P1, kRumbleHotheaded, 0, 0);
    auto fodder = addUnit(P1, kShipyardSkulker, 7);          // 7 [M]: the Mech is free
    auto mech = addToTrash(P1, kMegaMech);
    state.battlefields[0].controller = P1;
    state.battlefields[0].blocks_unit_play = true;
    state.battlefields[1].controller = P1;
    EnginePathAgent agent(EnginePathAgent::script(
        {1, static_cast<int>(fodder), static_cast<int>(mech), 2}));
    fireTriggerThroughEngine(rumble, TriggerType::WhenIConquer, &agent);
    ASSERT_FALSE(agent.prompts.empty());
    const size_t last = agent.prompts.size() - 1;
    EXPECT_FALSE(promptOffersValue(agent, last, 1)) << "units can't be played to battlefield #0";
    EXPECT_TRUE(promptOffersValue(agent, last, 2));
    ASSERT_TRUE(onBoard(mech));
    EXPECT_EQ(bfOf(mech), std::optional<BattlefieldId>{1});
}

// ═══ Work item 3: fixes merged without a test ═══════════════════════════════

// ── Wild Claw 876 (batch-09 "playPayingCost result ignored") ──

TEST_F(VerifyCardsTest, WildClaw_ThroughTheEngine_PlaysTheChosenUnitForFiveLessAndEmpowersIt) {
    addToDeck(P1, kMeditation);                       // sixth from the top: not looked at
    auto mech = addToDeck(P1, kMegaMech);             // [7]: [2] after the reduction
    for (int i = 0; i < 4; ++i) addToDeck(P1, kMeditation);
    auto spell = addToHand(P1, kWildClaw);
    ps(P1).rune_pool.energy = 2;
    EnginePathAgent agent(EnginePathAgent::choosing(mech));   // the unit, then "yes"
    playThroughEngine(spell, &agent);
    ASSERT_TRUE(onBoard(mech));
    EXPECT_TRUE(o(mech).isEmpowered()) << "Then you may do this: Empower it.";
    EXPECT_FALSE(inBanishment(P1, mech)) << "played out of banishment";
    EXPECT_EQ(ps(P1).rune_pool.energy, 0) << "only [5] of its [7] is taken off";
    EXPECT_EQ(deckSize(P1), 5) << "the other four were recycled";
}

TEST_F(VerifyCardsTest, WildClaw_APaymentThatFailsLeavesTheCardBanished) {
    // The card is chosen (and banished) at one prompt and paid for after the
    // next ("where to play it"). Nothing in the engine can change what the
    // controller can pay between the two — only the controller is asked, no
    // priority is given and no trigger resolves — so the failing payment is
    // forced here by emptying the pool while that second prompt is open.
    addToDeck(P1, kMeditation);
    auto mech = addToDeck(P1, kMegaMech);
    for (int i = 0; i < 4; ++i) addToDeck(P1, kMeditation);
    auto spell = addToHand(P1, kWildClaw);
    state.battlefields[0].controller = P1;            // so the location is asked
    ps(P1).rune_pool.energy = 2;

    GameEngine* live = nullptr;
    int prompt = 0;
    EnginePathAgent agent([&](const std::vector<Intent>& legal) {
        if (prompt++ == 0) {
            for (const auto& i : legal)
                if (!i.chosen_objects.empty() && i.chosen_objects.front() == mech) return i;
            ADD_FAILURE() << "Mega Mech was not offered";
        } else if (live) {
            live->mutableState().player(P1).rune_pool.energy = 0;
        }
        return legal.front();
    });
    const auto& def = card_db.get(kWildClaw);
    ps(P1).rune_pool.energy += def.energy_cost;
    ps(P1).rune_pool.universal_power += def.power_cost;
    Intent play;
    play.type = IntentType::PlayCard;
    play.player = P1;
    play.card = spell;
    withEngine([&](GameEngine& e) {
        live = &e;
        e.testHook_executeIntent(play);
        live = nullptr;
    }, &agent);

    EXPECT_EQ(agent.prompts.size(), 2u) << "no Empower prompt: nothing was played";
    EXPECT_FALSE(onBoard(mech));
    EXPECT_TRUE(inBanishment(P1, mech)) << "banished, and still listed there: not in no zone";
    EXPECT_EQ(o(mech).zone, ZoneType::Banishment);
    EXPECT_FALSE(inDeck(P1, mech));
}

// ── On-board guards: Vayne 537, Vex 617, Hwei 642 (07 item 7.26, batch-10 S2) ──

TEST_F(VerifyCardsTest, Vayne537_OffTheBoardIsNotOfferedTheReturnAndPaysNothing) {
    auto vayne = addUnit(P1, kVayne537, 0, 0);
    sendToTrash(vayne);                               // died before the trigger was finalized
    ps(P1).rune_pool.energy = 1;
    EnginePathAgent agent(EnginePathAgent::script({1}));
    fireTriggerThroughEngine(vayne, TriggerType::WhenIConquer, &agent);
    EXPECT_TRUE(agent.prompts.empty()) << "she is not on the board: nothing to return";
    EXPECT_EQ(ps(P1).rune_pool.energy, 1) << "the [1] is not taken for nothing";
    EXPECT_FALSE(inHand(P1, vayne));
    EXPECT_TRUE(inTrash(P1, vayne));
}

TEST_F(VerifyCardsTest, Vayne537_KilledInResponseStaysInTheTrash) {
    auto vayne = addUnit(P1, kVayne537, 0, 0);
    ps(P1).rune_pool.energy = 1;
    EnginePathAgent agent(EnginePathAgent::script({1}));
    fireAndProbe(vayne, TriggerType::WhenIConquer, &agent, [&](GameState& live) {
        EXPECT_EQ(live.player(P1).rune_pool.energy, 0) << "paid as the trigger was finalized";
        auto& v = live.getObject(vayne);              // ...and she is killed in response
        v.location = std::nullopt;
        v.zone = ZoneType::Trash;
        live.player(P1).trash.push_back(vayne);
    });
    EXPECT_FALSE(inHand(P1, vayne)) << "a card in the trash is not returned to hand";
    EXPECT_TRUE(inTrash(P1, vayne));
    EXPECT_EQ(o(vayne).zone, ZoneType::Trash);
}

TEST_F(VerifyCardsTest, Vayne537_AcceptingPaysOneAndReturnsHerToHand) {
    auto vayne = addUnit(P1, kVayne537, 0, 0);
    ps(P1).rune_pool.energy = 1;
    EnginePathAgent agent(EnginePathAgent::script({1}));
    fireTriggerThroughEngine(vayne, TriggerType::WhenIConquer, &agent);
    EXPECT_EQ(agent.prompts.size(), 1u);
    EXPECT_TRUE(inHand(P1, vayne));
    EXPECT_EQ(ps(P1).rune_pool.energy, 0);
}

TEST_F(VerifyCardsTest, VexMocking_OffTheBoardIsNotOfferedTheMove) {
    auto vex = addUnit(P1, kVexMocking, 0);
    auto enemy = addUnit(P2, kShipyardSkulker, 0, 1);
    o(vex).card_counters["__stunned_unit_id"] = static_cast<int>(enemy);
    sendToTrash(vex);
    EnginePathAgent agent(EnginePathAgent::script({1}));
    fireTriggerThroughEngine(vex, TriggerType::WhenYouStun, &agent);
    EXPECT_TRUE(agent.prompts.empty()) << "she left the board: the may is not offered";
    EXPECT_FALSE(onBoard(vex));
    EXPECT_TRUE(inTrash(P1, vex));
}

TEST_F(VerifyCardsTest, VexMocking_AcceptingMovesHerToTheStunnedUnitsBattlefield) {
    auto vex = addUnit(P1, kVexMocking, 0);
    auto enemy = addUnit(P2, kShipyardSkulker, 0, 1);
    EnginePathAgent agent(EnginePathAgent::script({1}));
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) {
        exec.stunUnitBy(enemy, vex);
    }, &agent);
    EXPECT_EQ(agent.prompts.size(), 1u);
    EXPECT_EQ(bfOf(vex), std::optional<BattlefieldId>{1});
}

TEST_F(VerifyCardsTest, Hwei_OffTheBoardStillDiscardsButGetsNoMight) {
    auto hwei = addUnit(P1, kHwei, 0, 0);
    addToDeck(P1, kMeditation);                       // the card drawn
    auto unit_card = addToHand(P1, kShipyardSkulker);
    sendToTrash(hwei);
    EnginePathAgent agent(EnginePathAgent::choosing(unit_card));
    fireTriggerThroughEngine(hwei, TriggerType::WhenIMove, &agent);
    EXPECT_TRUE(inTrash(P1, unit_card)) << "draw 1, then discard 1 still happen";
    EXPECT_EQ(o(hwei).temp_might_bonus, 0) << "\"Give me +3\": I am not on the board";
}

TEST_F(VerifyCardsTest, Hwei_OnTheBoardDiscardingAUnitGivesPlusThree) {
    auto hwei = addUnit(P1, kHwei, 0, 0);
    addToDeck(P1, kMeditation);
    auto unit_card = addToHand(P1, kShipyardSkulker);
    EnginePathAgent agent(EnginePathAgent::choosing(unit_card));
    fireTriggerThroughEngine(hwei, TriggerType::WhenIMove, &agent);
    EXPECT_TRUE(inTrash(P1, unit_card));
    EXPECT_EQ(o(hwei).temp_might_bonus, 3);
}

// ═══ Work item 2: pins for NOT A DEFECT closures ════════════════════════════

// 03 item 3.17: Seal of Strength 163 was never the wrong one (542 was).
TEST_F(VerifyCardsTest, Pin_3_17_SealOfStrength163_AddsBodyPower) {
    auto seal = addGear(P1, kSealOfStrength163);
    activate(seal);
    EXPECT_EQ(power(P1, Domain::Body), 1) << "[Add] [O]: Body";
    EXPECT_EQ(power(P1, Domain::Order), 0);
    EXPECT_TRUE(o(seal).is_exhausted);
}

// 03 item 3.18: Plundering Poro 392 already made its Gold exhausted (778 did not).
TEST_F(VerifyCardsTest, Pin_3_18_PlunderingPoro392_TheGoldEntersExhausted) {
    auto poro = addUnit(P1, kPlunderingPoro392, 0, 0);
    fireTriggerThroughEngine(poro, TriggerType::WhenIConquer);
    ASSERT_EQ(countNamed(P1, "Gold"), 1);
    EXPECT_TRUE(o(firstNamed(P1, "Gold")).is_exhausted) << "play a Gold gear token exhausted";
}

// 03 item 3.21 (and batch-06 548 / 547 / 543 / data): each printing carries
// the tags printed on it — the Spiritforged reprints print fewer region tags
// than the Origins cards (cards/raw/gallery_raw.json).
TEST_F(VerifyCardsTest, Pin_3_21_ReprintsCarryTheTagsOfTheirOwnPrinting) {
    EXPECT_EQ(tagsOf(kAhri119), (Tags{"Ahri", "Ionia"}));                 // OGN-119
    EXPECT_EQ(tagsOf(kAhri539), (Tags{"Ahri"}));                          // SFD-227
    EXPECT_EQ(tagsOf(kSett164), (Tags{"Sett", "Ionia"}));                 // OGN-164
    EXPECT_EQ(tagsOf(kSett543), (Tags{"Sett"}));                          // SFD-232
    EXPECT_EQ(tagsOf(kYasuo205), (Tags{"Yasuo", "Ionia"}));               // OGN-205
    EXPECT_EQ(tagsOf(kYasuo546), (Tags{"Yasuo"}));                        // SFD-235
    EXPECT_EQ(tagsOf(kVayne035), (Tags{"Vayne", "Demacia"}));             // OGN-035
    EXPECT_EQ(tagsOf(kVayne537), (Tags{"Vayne", "Demacia", "Sentinel"})); // SFD-223
    EXPECT_EQ(tagsOf(kDarius243), (Tags{"Trifarian", "Darius", "Noxus"})); // OGN-243
    EXPECT_EQ(tagsOf(kDarius547), (Tags{"Trifarian", "Darius"}));         // SFD-236
    EXPECT_EQ(tagsOf(kKarma235), (Tags{"Karma", "Ionia"}));               // OGN-235
    EXPECT_EQ(tagsOf(kKarma548), (Tags{"Karma"}));                        // SFD-237
    // Whatever its region tags, each is a legal Chosen Champion for its name.
    for (auto [id, name] : {std::pair<CardDefId, const char*>{kAhri539, "Ahri"},
                            {kSett543, "Sett"}, {kYasuo546, "Yasuo"},
                            {kVayne537, "Vayne"}, {kDarius547, "Darius"},
                            {kKarma548, "Karma"}}) {
        const auto t = tagsOf(id);
        EXPECT_NE(std::find(t.begin(), t.end(), name), t.end()) << name;
        EXPECT_EQ(card_db.get(id).super_type, SuperType::Champion) << name;
    }
}

// 03 item 3.21: Veteran Poro 779 — [Weaponmaster] is its only keyword.
TEST_F(VerifyCardsTest, Pin_3_21_VeteranPoro779_HasWeaponmasterAndNoEquip) {
    const auto& def = card_db.get(kVeteranPoro779);
    EXPECT_TRUE(def.keywords.has(Keyword::Weaponmaster));
    EXPECT_FALSE(def.keywords.has(Keyword::Equip))
        << "a Weaponmaster unit has no Equip ability of its own";
    bool offered = false;
    auto poro = addUnit(P1, kVeteranPoro779, 0);
    ps(P1).rune_pool.energy = 5;
    ps(P1).rune_pool.universal_power = 5;
    withEngine([&](GameEngine& e) {
        for (const auto& i : e.generateLegalActions())
            if (i.ability_source == poro) offered = true;
    });
    EXPECT_FALSE(offered) << "nothing to activate on it";
}

// batch-01 Systemic S5: Dark Child 315 "ready up to 2 runes" asks nothing.
TEST_F(VerifyCardsTest, Pin_Batch01_DarkChild315_ReadiesTwoRunesWithoutAsking) {
    auto annie = addLegend(P1, kDarkChild);
    addRune(P1, Domain::Fury, /*exhausted=*/true);
    addRune(P1, Domain::Chaos, /*exhausted=*/true);
    addRune(P1, Domain::Mind, /*exhausted=*/true);
    addRune(P2, Domain::Fury, /*exhausted=*/true);
    EnginePathAgent agent;
    fireTriggerThroughEngine(annie, TriggerType::AtEndOfTurn, &agent);
    EXPECT_TRUE(agent.prompts.empty()) << "which two is not a decision worth asking";
    EXPECT_EQ(readyRuneCount(P1), 2);
    EXPECT_EQ(exhaustedRuneCount(P1), 1);
    EXPECT_EQ(readyRuneCount(P2), 0) << "only its controller's runes";
}

// Carry-over (engine-auras-costs): Shurelya's Requiem 509 grants [Ganking]
// "here" — the sentence is the Equipment's effect_text, not its ability_text.
TEST_F(VerifyCardsTest, Pin_Shurelya509_UnitsWhereTheBearerIsHaveGanking) {
    const auto& def = card_db.get(kShurelyasRequiem);
    EXPECT_NE(def.effect_text.find("Your units here have [Ganking]"), std::string::npos);
    EXPECT_EQ(def.ability_text.find("Ganking"), std::string::npos);
    auto bearer = addUnit(P1, kShipyardSkulker, 0, 0);
    auto here = addUnit(P1, kShipyardSkulker, 0, 0);
    auto elsewhere = addUnit(P1, kShipyardSkulker, 0, 1);
    auto enemy_here = addUnit(P2, kShipyardSkulker, 0, 0);
    auto requiem = addGear(P1, kShurelyasRequiem);
    attach(requiem, bearer);
    cleanup();
    EXPECT_TRUE(o(bearer).hasKeyword(Keyword::Ganking));
    EXPECT_TRUE(o(here).hasKeyword(Keyword::Ganking));
    EXPECT_FALSE(o(elsewhere).hasKeyword(Keyword::Ganking));
    EXPECT_FALSE(o(enemy_here).hasKeyword(Keyword::Ganking));
}

// batch-03 758 Void Assault: "(If they both move to a battlefield you don't
// control, you're the attacker.)" needs no code in the card.
TEST_F(VerifyCardsTest, Pin_Batch03_VoidAssault758_TheCasterContestsAndIsTheAttacker) {
    auto spell = addToHand(P1, kVoidAssault);
    auto mine = addUnit(P1, kInvalidId, 3);          // base: may go to bf 0 or bf 1
    auto theirs = addUnit(P2, kInvalidId, 3, 0);     // bf 0: may go to base or bf 1
    // Options are the pairs (mine x theirs): index 3 = mine -> bf 1, theirs -> bf 1.
    EnginePathAgent agent(EnginePathAgent::script({3}));
    playAt(spell, {mine, theirs}, &agent, nullptr,
           [](GameEngine& e) { e.testHook_cleanup(); });
    ASSERT_EQ(bfOf(mine), std::optional<BattlefieldId>{1});
    ASSERT_EQ(bfOf(theirs), std::optional<BattlefieldId>{1});
    EXPECT_EQ(state.battlefields[1].contested_by, P1)
        << "CR 190.3.a: the turn player — the caster — applies Contested, and the "
           "player who applied it is the attacker";
}

// batch-03 368 Not So Fast / 668 Repulse: a triggered ability is never given
// targets; its event's subject is not something it chose.
TEST_F(VerifyCardsTest, Pin_Batch03_NotSoFastAndRepulse_ATriggersSubjectIsNotAChoice) {
    auto mine = addUnit(P1, kShipyardSkulker, 0, 0);
    auto their_poro = addUnit(P2, kPlunderingPoro392, 0, 0);
    Card* nsf = card_registry.get(kNotSoFast);
    Card* repulse = card_registry.get(kRepulse);
    ASSERT_NE(nsf, nullptr);
    ASSERT_NE(repulse, nullptr);
    withEngine([&](GameEngine& e) {
        // An enemy trigger whose event is about my unit, queued by TriggerManager.
        e.testHook_executor().queueTriggeredAbility(their_poro, P2, TriggerType::WhenIConquer, mine);
        auto& st = e.mutableState();
        ASSERT_FALSE(st.chain.items.empty());
        auto& top = st.chain.items.back();
        EXPECT_EQ(top.triggering_subject, mine);
        EXPECT_TRUE(top.targets.empty()) << "the engine never hands a trigger targets";
        EXPECT_FALSE(nsf->hasLegalTargets(st, P1)) << "it chose nothing";
        EXPECT_FALSE(repulse->hasLegalTargets(st, P1));
        // Control: once the ability really chooses my unit, both can answer it.
        top.chosen_objects = {mine};
        EXPECT_TRUE(nsf->hasLegalTargets(st, P1));
        EXPECT_TRUE(repulse->hasLegalTargets(st, P1));
        st.chain.items.clear();
    });
}

// batch-10 728 Stalking Wolf, the Ambush half: the unit to be killed is still
// at its battlefield while play locations are offered.
TEST_F(VerifyCardsTest, Pin_Batch10_StalkingWolf728_CanBePlayedToItsVictimsBattlefield) {
    auto wolf = addToHand(P1, kStalkingWolf);
    auto poro = addUnit(P1, kInvalidId, 1, 0);       // my only unit at battlefield #0
    o(poro).tags = {"Poro"};
    ps(P1).rune_pool.energy = 10;
    ps(P1).rune_pool.universal_power = 5;
    bool at_bf0 = false, at_bf1 = false;
    // [Ambush] is a play location at Reaction timing (CR 822.1.b): ask what
    // P1 may do while holding priority in a Closed State.
    state.turn.phase = TurnPhase::MainPhase;
    state.turn.turn_player = P1;
    state.turn.oc_state = OpenClosedState::Closed;
    state.turn.priority_holder = P1;
    withEngine([&](GameEngine& e) {
        for (const auto& i : e.generateLegalActions()) {
            if (i.card != wolf || !i.play_location) continue;
            if (*i.play_location == LocationId{BattlefieldLocation{0}}) at_bf0 = true;
            if (*i.play_location == LocationId{BattlefieldLocation{1}}) at_bf1 = true;
        }
    });
    state.turn.oc_state = OpenClosedState::Open;
    state.turn.priority_holder = std::nullopt;
    EXPECT_TRUE(at_bf0) << "[Ambush] to its battlefield";
    EXPECT_FALSE(at_bf1) << "no unit of mine there";
    EnginePathAgent agent(EnginePathAgent::choosing(poro));
    playThroughEngine(wolf, &agent, LocationId{BattlefieldLocation{0}}, nullptr, /*fund=*/false);
    EXPECT_EQ(bfOf(wolf), std::optional<BattlefieldId>{0})
        << "even if you don't have other units there";
    EXPECT_FALSE(onBoard(poro));
}

// batch-10 Systemic S3: two Ezreal, Dashing — "I don't deal combat damage"
// is applied once to each.
TEST_F(VerifyCardsTest, Pin_Batch10_EzrealDashing404_TwoCopiesEachSuppressedOnce) {
    auto a = addUnit(P1, kEzrealDashing, 0, 0);
    auto b = addUnit(P1, kEzrealDashing, 0, 1);
    auto other = addUnit(P1, kShipyardSkulker, 0, 0);
    cleanup();
    auto suppressions = [&](GameObjectId id) {
        int n = 0;
        for (const auto& ae : o(id).aura_effects) if (ae.suppress_combat_damage) ++n;
        return n;
    };
    EXPECT_EQ(suppressions(a), 1);
    EXPECT_EQ(suppressions(b), 1);
    EXPECT_EQ(suppressions(other), 0);
}

// batch-08 671 / 783 / 274: Matriarch of War 940 was the correct comparison —
// "Ready a unit." is any unit.
TEST_F(VerifyCardsTest, Pin_Batch08_MatriarchOfWar940_ReadyAUnitOffersEitherPlayers) {
    auto legend = addLegend(P1, kMatriarchOfWar);
    o(legend).empowered = 1;
    auto mine = addUnit(P1, kShipyardSkulker, 0);
    auto theirs = addUnit(P2, kShipyardSkulker, 0);
    o(mine).is_exhausted = true;
    o(theirs).is_exhausted = true;
    ps(P1).rune_pool.universal_power = 1;
    EnginePathAgent agent(EnginePathAgent::choosing(theirs));
    activate(legend, &agent);
    EXPECT_TRUE(agent.wasOffered(mine));
    EXPECT_TRUE(agent.wasOffered(theirs));
    EXPECT_FALSE(o(theirs).is_exhausted) << "the enemy unit was readied";
    EXPECT_TRUE(o(mine).is_exhausted);
    EXPECT_FALSE(o(legend).isEmpowered()) << "Disempower me";
}

// 07 item 7.35: Noxian Demolitionist 867 was named as the guarded example —
// a gear token with no card behind it costs [0] and is looked up nowhere.
TEST_F(VerifyCardsTest, Pin_7_35_NoxianDemolitionist867_AGearTokenWithoutACardCostsNothing) {
    auto demo = addUnit(P1, kNoxianDemolitionist, 1, 0);
    auto token = addGear(P2, kInvalidId);
    o(token).is_token = true;
    auto dear = addGear(P2, kShurelyasRequiem);      // [4]: more than 1 [M]
    EnginePathAgent agent(EnginePathAgent::choosing(token));
    fireTriggerThroughEngine(demo, TriggerType::WhenIConquer, &agent);
    EXPECT_TRUE(agent.wasOffered(token));
    EXPECT_FALSE(agent.wasOffered(dear)) << "Energy cost no more than my Might";
    EXPECT_FALSE(onBoard(token));
    EXPECT_TRUE(onBoard(dear));
}

// 02 item 2.20: Forgotten Monument 523 — before a player's third turn there
// is no score at all: no ScoreEvent, no point, no conquer trigger.
TEST_F(VerifyCardsTest, Pin_2_20_ForgottenMonument523_NoScoreMeansNoScoreEventAndNoConquerTrigger) {
    BattlefieldCard* monument = dynamic_cast<BattlefieldCard*>(card_registry.get(kForgottenMonument));
    ASSERT_NE(monument, nullptr);
    state.battlefields[0].min_turn_to_score = monument->minTurnToScore();
    addUnit(P1, kPlunderingPoro392, 0, 0);            // "When I conquer, play a Gold..."
    int scores = 0;
    auto conn = events.on_score.connect([&](const ScoreEvent&) { ++scores; });
    auto conquer = [&]() {
        withEngine([&](GameEngine& e) {
            e.testHook_scoreConquer(P1, 0);
            e.testHook_runChain();
        });
    };
    ps(P1).turns_taken = 2;
    conquer();
    EXPECT_EQ(scores, 0) << "Players can't score here until their third turn";
    EXPECT_EQ(ps(P1).score, 0);
    EXPECT_EQ(countNamed(P1, "Gold"), 0) << "no conquer happened, so nothing triggers";

    ps(P1).turns_taken = 3;
    conquer();
    conn.disconnect();
    EXPECT_EQ(scores, 1);
    EXPECT_EQ(countNamed(P1, "Gold"), 1);
}

// ═══ Work items 1 and 4: missing accept paths and tests that could not fail ═

// 03 item 3.7: Guards! 475 — the "may" re-enters the callback after the token
// exists; one token, readied, [Y] spent.
TEST_F(VerifyCardsTest, Guards_AcceptingPaysOrderPowerAndReadiesTheOneSandSoldier) {
    auto spell = addToHand(P1, kGuards);
    power(P1, Domain::Order) = 1;
    EnginePathAgent agent(EnginePathAgent::script({0, 1}));   // to base; yes, pay
    playThroughEngine(spell, &agent);
    EXPECT_EQ(agent.prompts.size(), 2u) << "where, then the may";
    ASSERT_EQ(countNamed(P1, "Sand Soldier"), 1) << "one token however often it re-enters";
    EXPECT_FALSE(o(firstNamed(P1, "Sand Soldier")).is_exhausted);
    EXPECT_EQ(power(P1, Domain::Order), 0);
}

TEST_F(VerifyCardsTest, Guards_WithoutOrderPowerTheMayIsNotOffered) {
    auto spell = addToHand(P1, kGuards);
    EnginePathAgent agent(EnginePathAgent::script({0, 1}));
    playThroughEngine(spell, &agent);
    EXPECT_EQ(agent.prompts.size(), 1u) << "where to play it; the may is not offered";
    ASSERT_EQ(countNamed(P1, "Sand Soldier"), 1);
    EXPECT_TRUE(o(firstNamed(P1, "Sand Soldier")).is_exhausted);
}

// 03 item 3.12 / 07 item 7.5: the cost is paid as the trigger is finalized —
// seen from the opponent's first priority, before anything resolves.
TEST_F(VerifyCardsTest, FaePorter_ThePowerIsGoneBeforeTheOpponentCanRespond) {
    auto porter = addUnit(P1, kFaePorter, 0, 0);
    auto ally = addUnit(P1, kShipyardSkulker, 0);
    power(P1, Domain::Chaos) = 1;
    EnginePathAgent agent(EnginePathAgent::choosing(ally));
    fireAndProbe(porter, TriggerType::WhenIMoveToFB, &agent, [&](GameState& live) {
        EXPECT_EQ(live.player(P1).rune_pool.power[static_cast<int>(Domain::Chaos)], 0);
        EXPECT_TRUE(live.getObject(ally).isAtBase()) << "not moved yet";
    });
    EXPECT_EQ(bfOf(ally), std::optional<BattlefieldId>{0});
}

TEST_F(VerifyCardsTest, RumbleHotheaded_TheUnitIsRecycledBeforeTheOpponentCanRespond) {
    auto rumble = addUnit(P1, kRumbleHotheaded, 0, 0);
    auto fodder = addUnit(P1, kShipyardSkulker, 7);
    auto mech = addToTrash(P1, kMegaMech);
    EnginePathAgent agent(EnginePathAgent::script(
        {1, static_cast<int>(fodder), static_cast<int>(mech)}));
    fireAndProbe(rumble, TriggerType::WhenIConquer, &agent, [&](GameState& live) {
        EXPECT_FALSE(live.getObject(fodder).location.has_value()) << "recycled as the cost";
        EXPECT_FALSE(live.getObject(mech).location.has_value()) << "not played yet";
    });
    EXPECT_TRUE(inDeck(P1, fodder));
    EXPECT_TRUE(onBoard(mech));
}

TEST_F(VerifyCardsTest, OverzealousFan_IsDeadBeforeTheOpponentCanRespond) {
    auto fan = addUnit(P1, kOverzealousFan, 0, 0);
    auto attacker = addUnit(P2, kShipyardSkulker, 0, 0);
    auto& b = state.battlefields[0];
    b.combat_in_progress = true;
    b.attacker = P2;
    b.defender = P1;
    o(fan).combat_designation = CombatDesignation::Defender;
    o(attacker).combat_designation = CombatDesignation::Attacker;
    o(fan).card_counters["__defend_attacker_id"] = static_cast<int>(attacker);
    EnginePathAgent agent(EnginePathAgent::choosing(attacker));
    fireAndProbe(fan, TriggerType::WhenIDefend, &agent, [&](GameState& live) {
        EXPECT_FALSE(live.getObject(fan).location.has_value()) << "\"kill me\" is the cost";
        EXPECT_FALSE(live.getObject(attacker).isAtBase()) << "not moved yet";
    });
    EXPECT_TRUE(o(attacker).isAtBase());
}

// 03 item 3.14: a source that left the board is not moved back.
TEST_F(VerifyCardsTest, Blitzcrank_HoldReturnsHimToHandOnlyFromTheBoard) {
    auto blitz = addUnit(P1, kBlitzcrank, 0, 0);
    fireTriggerThroughEngine(blitz, TriggerType::WhenIHold);
    EXPECT_TRUE(inHand(P1, blitz));

    auto dead = addUnit(P1, kBlitzcrank, 0, 0);
    sendToTrash(dead);
    fireTriggerThroughEngine(dead, TriggerType::WhenIHold);
    EXPECT_FALSE(inHand(P1, dead)) << "he was in the trash: not also put into hand";
    EXPECT_TRUE(inTrash(P1, dead));
    EXPECT_EQ(o(dead).zone, ZoneType::Trash);
}

TEST_F(VerifyCardsTest, EzrealDashing_ActivatedThenKilledIsNotMovedToBase) {
    auto ezreal = addUnit(P1, kEzrealDashing, 0, 0);
    power(P1, Domain::Mind) = 1;
    GameState* live = nullptr;
    bool killed = false;
    PickByPredicateAgent p2([&](int, const std::vector<Intent>& legal) {
        if (!killed && live) {                        // in response: he dies
            killed = true;
            auto& e = live->getObject(ezreal);
            e.location = std::nullopt;
            e.zone = ZoneType::Trash;
            live->player(P1).trash.push_back(ezreal);
        }
        for (const auto& i : legal)
            if (i.type == IntentType::PassPriority) return i;
        return legal.front();
    });
    Intent act;
    act.type = IntentType::ActivateAbility;
    act.player = P1;
    act.ability_source = ezreal;
    withEngine([&](GameEngine& e) {
        live = &e.mutableState();
        e.testHook_executeIntent(act);
        live = nullptr;
    }, nullptr, &p2);
    ASSERT_TRUE(killed) << "the opponent never got priority";
    EXPECT_EQ(power(P1, Domain::Mind), 0) << "the ability was activated and paid for";
    EXPECT_FALSE(onBoard(ezreal)) << "a card in the trash is not moved to base";
    EXPECT_EQ(o(ezreal).zone, ZoneType::Trash);
}

TEST_F(VerifyCardsTest, EzrealDashing_ActivatedMovesHimToBase) {
    auto ezreal = addUnit(P1, kEzrealDashing, 0, 0);
    power(P1, Domain::Mind) = 1;
    activate(ezreal);
    EXPECT_TRUE(o(ezreal).isAtBase());
}

// 03 item 3.16: through the engine — the played copy does not count itself,
// another copy does.
TEST_F(VerifyCardsTest, Direwing_ThroughTheEngine_EntersReadyOnlyNextToAnotherDragon) {
    auto first = addToHand(P1, kDirewing);
    playThroughEngine(first);
    ASSERT_TRUE(onBoard(first));
    EXPECT_TRUE(o(first).is_exhausted) << "alone: it is not \"another Dragon\" for itself";
    auto second = addToHand(P1, kDirewing);
    playThroughEngine(second);
    ASSERT_TRUE(onBoard(second));
    EXPECT_FALSE(o(second).is_exhausted) << "another Direwing is another Dragon";
}

TEST_F(VerifyCardsTest, BreakneckMech_ThroughTheEngine_EntersReadyOnlyNextToAnotherMech) {
    auto first = addToHand(P1, kBreakneckMech);
    playThroughEngine(first);
    ASSERT_TRUE(onBoard(first));
    EXPECT_TRUE(o(first).is_exhausted);
    auto second = addToHand(P1, kBreakneckMech);
    playThroughEngine(second);
    ASSERT_TRUE(onBoard(second));
    EXPECT_FALSE(o(second).is_exhausted);
}

// 07 item 7.15: Glasc Mixologist 486 — the accept path.
TEST_F(VerifyCardsTest, GlascMixologist_AcceptingPlaysTheChosenCheapUnitFromTheTrash) {
    auto glasc = addUnit(P1, kGlascMixologist, 0);
    auto cheap = addToTrash(P1, kShipyardSkulker);
    auto dear = addToTrash(P1, kMegaMech);            // [7]
    EnginePathAgent agent(EnginePathAgent::choosing(cheap));
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) { exec.killObject(glasc); },
                         &agent);
    ASSERT_FALSE(agent.prompts.empty());
    EXPECT_FALSE(agent.wasOffered(dear)) << "no more than [3]";
    EXPECT_TRUE(onBoard(cheap));
    EXPECT_FALSE(inTrash(P1, cheap));
}

TEST_F(VerifyCardsTest, GlascMixologist_DecliningLeavesTheUnitInTheTrash) {
    auto glasc = addUnit(P1, kGlascMixologist, 0);
    auto cheap = addToTrash(P1, kShipyardSkulker);
    EnginePathAgent agent(EnginePathAgent::script({0}));
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) { exec.killObject(glasc); },
                         &agent);
    EXPECT_EQ(agent.prompts.size(), 1u);
    EXPECT_TRUE(inTrash(P1, cheap));
}

// 07 item 7.24: the pick is neither the first nor the last of three, so an
// implementation that prompts and then takes its own favourite fails.
TEST_F(VerifyCardsTest, FizzTrickster_PlaysTheMiddleOfThreeSpells) {
    auto fizz = addToHand(P1, kFizzTrickster);
    auto oldest = addToTrash(P1, kMeditation);
    auto middle = addToTrash(P1, kMobilize);
    auto newest = addToTrash(P1, kMeditation);
    for (int i = 0; i < 4; ++i) addToDeck(P1, kShipyardSkulker);
    ps(P1).rune_pool.universal_power = 3;
    EnginePathAgent agent(EnginePathAgent::choosing(middle));
    playThroughEngine(fizz, &agent);
    EXPECT_TRUE(agent.wasOffered(oldest));
    EXPECT_TRUE(agent.wasOffered(newest));
    EXPECT_TRUE(inTrash(P1, oldest));
    EXPECT_TRUE(inTrash(P1, newest));
    EXPECT_FALSE(inTrash(P1, middle));
    EXPECT_TRUE(inDeck(P1, middle)) << "Then recycle it.";
}

TEST_F(VerifyCardsTest, KaiSaEvolutionary_PlaysTheMiddleOfThreeSpells) {
    auto kaisa = addUnit(P1, kKaiSaEvolutionary, 0, 0);
    setScore(P1, 5);
    auto oldest = addToTrash(P1, kMeditation);
    auto middle = addToTrash(P1, kMobilize);
    auto newest = addToTrash(P1, kMeditation);
    for (int i = 0; i < 4; ++i) addToDeck(P1, kShipyardSkulker);
    ps(P1).rune_pool.universal_power = 3;
    EnginePathAgent agent(EnginePathAgent::choosing(middle));
    fireTriggerThroughEngine(kaisa, TriggerType::WhenIConquer, &agent);
    EXPECT_TRUE(agent.wasOffered(oldest));
    EXPECT_TRUE(agent.wasOffered(newest));
    EXPECT_TRUE(inTrash(P1, oldest));
    EXPECT_TRUE(inTrash(P1, newest));
    EXPECT_FALSE(inTrash(P1, middle));
    EXPECT_TRUE(inDeck(P1, middle)) << "Then recycle it.";
}

// Work item 4: Loyal Pup 447 and Shadow Clone 955 — accept and decline.
TEST_F(VerifyCardsTest, LoyalPup_AcceptingMovesItToTheDefendedBattlefield) {
    auto pup = addUnit(P1, kLoyalPup, 0);
    o(pup).card_counters["__defend_bf"] = 1;
    EnginePathAgent yes(EnginePathAgent::script({1}));
    fireTriggerThroughEngine(pup, TriggerType::WhenYouDefendAtABattlefield, &yes);
    EXPECT_EQ(yes.prompts.size(), 1u);
    EXPECT_EQ(bfOf(pup), std::optional<BattlefieldId>{1});
}

TEST_F(VerifyCardsTest, LoyalPup_DecliningLeavesItInBase) {
    auto pup = addUnit(P1, kLoyalPup, 0);
    o(pup).card_counters["__defend_bf"] = 1;
    EnginePathAgent no(EnginePathAgent::script({0}));
    fireTriggerThroughEngine(pup, TriggerType::WhenYouDefendAtABattlefield, &no);
    EXPECT_EQ(no.prompts.size(), 1u);
    EXPECT_TRUE(o(pup).isAtBase());
}

TEST_F(VerifyCardsTest, ShadowClone_AcceptingBanishesTheChosenUnitAndGivesAssault) {
    auto clone = addUnit(P1, kShadowClone, 0, 0);
    auto corpse = addToTrash(P1, kShipyardSkulker);
    EnginePathAgent agent(EnginePathAgent::choosing(corpse));
    fireTriggerThroughEngine(clone, TriggerType::WhenIAttack, &agent);
    EXPECT_TRUE(inBanishment(P1, corpse));
    EXPECT_FALSE(inTrash(P1, corpse));
    EXPECT_EQ(ps(P1).cards_banished_this_turn, 1);
    EXPECT_TRUE(o(clone).hasKeyword(Keyword::Assault));
}

TEST_F(VerifyCardsTest, ShadowClone_DecliningBanishesNothingAndGivesNoAssault) {
    auto clone = addUnit(P1, kShadowClone, 0, 0);
    auto corpse = addToTrash(P1, kShipyardSkulker);
    EnginePathAgent agent(EnginePathAgent::script({0}));
    fireTriggerThroughEngine(clone, TriggerType::WhenIAttack, &agent);
    EXPECT_EQ(agent.prompts.size(), 1u);
    EXPECT_TRUE(inTrash(P1, corpse));
    EXPECT_FALSE(o(clone).hasKeyword(Keyword::Assault));
}

// ═══ Round 2: tests that could not fail, and accept paths never taken ═══════

// 03 item 3.14: Azir, Ascendant — he left the board in response: nobody moves.
TEST_F(VerifyCardsTest, AzirAscendant_KilledInResponseSwapsNobody) {
    auto azir = addUnit(P1, kAzirAscendant, 0);
    auto other = addUnit(P1, kShipyardSkulker, 0, 1);
    power(P1, Domain::Calm) = 1;
    Intent act;
    act.type = IntentType::ActivateAbility;
    act.player = P1;
    act.ability_source = azir;
    executeAndProbe(act, nullptr, [&](GameState& live) { removeFromBoard(live, azir); });
    EXPECT_EQ(power(P1, Domain::Calm), 0) << "activated and paid for";
    EXPECT_FALSE(onBoard(azir));
    EXPECT_EQ(o(azir).zone, ZoneType::Trash);
    EXPECT_EQ(bfOf(other), std::optional<BattlefieldId>{1}) << "it stays where it was";
}

TEST_F(VerifyCardsTest, AzirAscendant_SwapsPlacesWithTheChosenUnit) {
    auto azir = addUnit(P1, kAzirAscendant, 0);
    auto other = addUnit(P1, kShipyardSkulker, 0, 1);
    power(P1, Domain::Calm) = 1;
    Intent act;
    act.type = IntentType::ActivateAbility;
    act.player = P1;
    act.ability_source = azir;
    executeAndProbe(act, nullptr, [](GameState&) {});
    EXPECT_EQ(bfOf(azir), std::optional<BattlefieldId>{1});
    EXPECT_TRUE(o(other).isAtBase());
}

// 03 item 3.15: Pirate's Haven — the readied unit must still be on the board.
TEST_F(VerifyCardsTest, PiratesHaven_AUnitThatLeftTheBoardGetsNoMight) {
    auto haven = addGear(P1, kPiratesHaven);
    auto here = addUnit(P1, kShipyardSkulker, 0);
    fireTriggerThroughEngine(haven, TriggerType::WhenYouReadyAFriendlyUnit, nullptr, here);
    EXPECT_EQ(o(here).temp_might_bonus, 1);
    auto gone = addUnit(P1, kShipyardSkulker, 0);
    sendToTrash(gone);
    fireTriggerThroughEngine(haven, TriggerType::WhenYouReadyAFriendlyUnit, nullptr, gone);
    EXPECT_EQ(o(gone).temp_might_bonus, 0);
}

// 03 item 3.24: the shared splitDamageAmong, through the engine.
TEST_F(VerifyCardsTest, AlphaStrike_ThroughTheEngine_AllTheDamageWhereTheControllerPutsIt) {
    auto striker = addUnit(P1, kInvalidId, 4);
    auto e1 = addUnit(P2, kInvalidId, 3, 0);
    auto e2 = addUnit(P2, kInvalidId, 5, 1);
    auto in_base = addUnit(P2, kInvalidId, 2);
    auto spell = addToHand(P1, kAlphaStrike);
    EnginePathAgent agent(EnginePathAgent::choosing(e1));
    playAt(spell, {striker}, &agent);
    EXPECT_FALSE(agent.wasOffered(in_base)) << "enemy units at battlefields";
    EXPECT_FALSE(onBoard(e1)) << "all 4 on the 3 [M] unit kills it";
    EXPECT_EQ(o(e2).damage_marked, 0);
    EXPECT_EQ(ps(P1).xp, 1);
}

TEST_F(VerifyCardsTest, VolibearFurious_ThroughTheEngine_AllTheDamageWhereTheControllerPutsIt) {
    auto voli = addUnit(P1, kVolibearFurious, 0, 0);
    auto big = addUnit(P2, kInvalidId, 6, 0);
    auto small = addUnit(P2, kInvalidId, 2, 0);
    auto elsewhere = addUnit(P2, kInvalidId, 2, 1);
    EnginePathAgent agent(EnginePathAgent::choosing(big));
    fireTriggerThroughEngine(voli, TriggerType::WhenIAttack, &agent);
    EXPECT_FALSE(agent.wasOffered(elsewhere)) << "here";
    EXPECT_EQ(o(big).damage_marked, 5);
    EXPECT_EQ(o(small).damage_marked, 0);
}

// 07 item 7.5: Ravenbloom Prefect — "banish me" is paid as the trigger is
// finalized: there is a moment when she is banished and the gear is not yet.
TEST_F(VerifyCardsTest, RavenbloomPrefect_IsBanishedBeforeTheGearIs) {
    auto prefect = addUnit(P2, kRavenbloomPrefect, 0, 0);
    auto gear = addToHand(P1, kSealOfStrength163);
    GameEngine* live = nullptr;
    bool seen_cost_paid_first = false;
    PickByPredicateAgent me([&](int, const std::vector<Intent>& legal) {
        if (live) {
            const auto& st = live->mutableState();
            if (!st.getObject(prefect).location.has_value() &&
                st.getObject(gear).location.has_value()) seen_cost_paid_first = true;
        }
        for (const auto& i : legal)
            if (i.type == IntentType::PassPriority) return i;
        return legal.front();
    });
    EnginePathAgent them(EnginePathAgent::script({1}));
    const auto& def = card_db.get(kSealOfStrength163);
    ps(P1).rune_pool.energy += def.energy_cost;
    ps(P1).rune_pool.universal_power += def.power_cost;
    Intent play;
    play.type = IntentType::PlayCard;
    play.player = P1;
    play.card = gear;
    play.play_location = LocationId{BaseLocation{P1}};
    withEngine([&](GameEngine& e) {
        live = &e;
        e.testHook_executeIntent(play);
        live = nullptr;
    }, &me, &them);
    ASSERT_EQ(them.prompts.size(), 1u);
    EXPECT_TRUE(seen_cost_paid_first) << "the cost is paid before anyone can respond";
    EXPECT_TRUE(inBanishment(P2, prefect));
    EXPECT_TRUE(inBanishment(P1, gear));
}

// 07 item 7.8: Relentless Storm — the Mighty check is made when the unit is
// played; losing the unit in response does not cost the rune.
TEST_F(VerifyCardsTest, RelentlessStorm_StillChannelsWhenTheMightyUnitLeavesInResponse) {
    auto voli = addLegend(P1, kRelentlessStorm);
    auto rune = addRune(P1, Domain::Fury);
    o(rune).location = std::nullopt;
    o(rune).zone = ZoneType::RuneDeck;
    ps(P1).rune_deck.push_back(rune);
    auto mighty = unitInHand(P1, kMegaMech);
    ps(P1).rune_pool.energy += card_db.get(kMegaMech).energy_cost;
    Intent play;
    play.type = IntentType::PlayCard;
    play.player = P1;
    play.card = mighty;
    play.play_location = LocationId{BaseLocation{P1}};
    EnginePathAgent yes(EnginePathAgent::script({1}));
    executeAndProbe(play, &yes, [&](GameState& live) {
        EXPECT_TRUE(live.getObject(voli).is_exhausted) << "exhausted as the cost";
        removeFromBoard(live, mighty);
    });
    EXPECT_TRUE(ps(P1).rune_deck.empty()) << "the rune is channeled all the same";
    EXPECT_TRUE(onBoard(rune));
}

// 07 item 7.25: Scryer's Bloom — "Kill this" is a cost: paid before the
// opponent can respond, before any card is looked at.
TEST_F(VerifyCardsTest, ScryersBloom_IsKilledBeforeTheOpponentCanRespond) {
    auto bloom = addGear(P1, kScryersBloom);
    for (int i = 0; i < 3; ++i) addToDeck(P1, kInvalidId);
    ps(P1).rune_pool.energy = 1;
    Intent act;
    act.type = IntentType::ActivateAbility;
    act.player = P1;
    act.ability_source = bloom;
    executeAndProbe(act, nullptr, [&](GameState& live) {
        EXPECT_FALSE(live.getObject(bloom).location.has_value()) << "Kill this: the cost";
        EXPECT_TRUE(live.player(P1).hand.empty()) << "nothing drawn yet";
        EXPECT_EQ(live.player(P1).xp, 0);
    });
    EXPECT_EQ(handSize(P1), 1);
}

// 07 item 7.26: Vayne, Hunter 35 — as reprint 537.
TEST_F(VerifyCardsTest, Vayne035_OffTheBoardIsNotOfferedTheReturnAndPaysNothing) {
    auto vayne = addUnit(P1, kVayne035, 0, 0);
    sendToTrash(vayne);
    ps(P1).rune_pool.energy = 1;
    EnginePathAgent agent(EnginePathAgent::script({1}));
    fireTriggerThroughEngine(vayne, TriggerType::WhenIConquer, &agent);
    EXPECT_TRUE(agent.prompts.empty());
    EXPECT_EQ(ps(P1).rune_pool.energy, 1);
    EXPECT_FALSE(inHand(P1, vayne));
}

TEST_F(VerifyCardsTest, Vayne035_KilledInResponseStaysInTheTrash) {
    auto vayne = addUnit(P1, kVayne035, 0, 0);
    ps(P1).rune_pool.energy = 1;
    EnginePathAgent agent(EnginePathAgent::script({1}));
    fireAndProbe(vayne, TriggerType::WhenIConquer, &agent, [&](GameState& live) {
        EXPECT_EQ(live.player(P1).rune_pool.energy, 0);
        removeFromBoard(live, vayne);
    });
    EXPECT_FALSE(inHand(P1, vayne));
    EXPECT_EQ(o(vayne).zone, ZoneType::Trash);
}

// 07 item 7.27: Carnivorous Snapvine — a Snapvine that went back to hand
// neither damages nor kills. (The enemy has more Might than the Snapvine, so
// "damage 0" cannot be a killed-and-reset unit.)
TEST_F(VerifyCardsTest, Snapvine_ReturnedToHandInResponseFightsNobody) {
    auto enemy = addUnit(P2, kInvalidId, 9, 0);
    auto vine = addUnit(P1, kSnapvine);
    EnginePathAgent agent(EnginePathAgent::choosing(enemy));
    withEngine([&](GameEngine& e) {
        e.testHook_executor().queueTriggeredAbility(vine, P1, TriggerType::WhenYouPlayMe);
        e.testHook_executor().bounceToHand(vine);
        e.testHook_runChain();
    }, &agent);
    EXPECT_TRUE(onBoard(enemy));
    EXPECT_EQ(o(enemy).damage_marked, 0);
    EXPECT_TRUE(inHand(P1, vine));
    EXPECT_FALSE(inTrash(P1, vine));
}

// 07 item 7.28: Falling Star — both hits on one token: it dies once.
TEST_F(VerifyCardsTest, FallingStar_BothHitsOnOneTokenKillItOnce) {
    auto spell = addToHand(P1, kFallingStar);
    auto token = addUnit(P2, kInvalidId, 3, 0);
    o(token).is_token = true;
    o(token).super_type = SuperType::Token;
    int deaths = 0;
    auto conn = events.on_unit_died.connect([&](const UnitDiedEvent&) { ++deaths; });
    EnginePathAgent agent(EnginePathAgent::choosing(token));
    playThroughEngine(spell, &agent);
    conn.disconnect();
    EXPECT_EQ(agent.prompts.size(), 2u) << "two picks";
    EXPECT_FALSE(onBoard(token));
    EXPECT_EQ(deaths, 1) << "the second hit finds nothing on the board";
    EXPECT_FALSE(inTrash(P2, token)) << "a token ceases to exist";
}

// 07 item 7.31: Void Burrower — the banish goes through the executor.
TEST_F(VerifyCardsTest, VoidBurrower_ThroughTheEngine_TheBanishIsCountedAndAnnounced) {
    auto legend = addLegend(P1, kVoidBurrower);
    auto below = addToDeck(P1, kMeditation);
    auto top = addToDeck(P1, kShipyardSkulker);
    o(top).base_might = o(top).current_might = 3;
    ps(P1).rune_pool.energy = card_db.get(kShipyardSkulker).energy_cost;
    int announced = 0;
    auto conn = events.on_object_state_changed.connect([&](const ObjectStateChangedEvent& e) {
        if (e.what_changed == "banished_owned") ++announced;
    });
    EnginePathAgent agent(EnginePathAgent::choosing(top));
    fireTriggerThroughEngine(legend, TriggerType::WhenIConquer, &agent);
    conn.disconnect();
    EXPECT_TRUE(o(legend).is_exhausted);
    EXPECT_EQ(ps(P1).cards_banished_this_turn, 1);
    EXPECT_EQ(announced, 1);
    EXPECT_TRUE(onBoard(top)) << "banish one, then play it";
    EXPECT_FALSE(inBanishment(P1, top));
    EXPECT_TRUE(inDeck(P1, below)) << "recycle the rest";
}

// 07 item 7.36: Elder Dragon — every chosen unit is damaged before any dies.
TEST_F(VerifyCardsTest, ElderDragon_DamagesEveryChosenUnitBeforeAnyDies) {
    auto dragon = addUnit(P1, kElderDragon, 0, 0);
    auto in_base = addUnit(P2, kInvalidId, 1);
    auto at_bf = addUnit(P2, kInvalidId, 1, 0);
    GameEngine* live = nullptr;
    int deaths = 0;
    bool all_damaged_at_first_death = false;
    auto conn = events.on_unit_died.connect([&](const UnitDiedEvent&) {
        if (++deaths != 1 || !live) return;
        const auto& st = live->mutableState();
        all_damaged_at_first_death =
            (!st.getObject(in_base).location.has_value() || st.getObject(in_base).damage_marked >= 1) &&
            (!st.getObject(at_bf).location.has_value() || st.getObject(at_bf).damage_marked >= 1);
    });
    // One prompt per location: take the unit offered each time.
    EnginePathAgent agent([](const std::vector<Intent>& legal) {
        for (const auto& i : legal)
            if (!i.chosen_objects.empty()) return i;
        return legal.front();
    });
    withEngine([&](GameEngine& e) {
        live = &e;
        e.testHook_executor().queueTriggeredAbility(dragon, P1, TriggerType::WhenYouPlayMe);
        e.testHook_runChain();
        live = nullptr;
    }, &agent);
    conn.disconnect();
    EXPECT_EQ(deaths, 2);
    EXPECT_FALSE(onBoard(in_base));
    EXPECT_FALSE(onBoard(at_bf));
    EXPECT_TRUE(all_damaged_at_first_death) << "damage first, kills in a second pass";
}

// 07 item 7.6: Stalking Wolf — the engine consults the gate for a unit play.
TEST_F(VerifyCardsTest, StalkingWolf_TheEngineRefusesThePlayWithNothingToKill) {
    auto wolf = addToHand(P1, kStalkingWolf);
    addUnit(P1, kShipyardSkulker, 0);                 // a Pirate: not a Bird, Cat, Dog or Poro
    ps(P1).rune_pool.energy = 10;
    ps(P1).rune_pool.universal_power = 5;
    auto offered = [&]() {
        bool found = false;
        withEngine([&](GameEngine& e) {
            for (const auto& i : e.generateLegalActions())
                if (i.type == IntentType::PlayCard && i.card == wolf) found = true;
            EXPECT_EQ(e.testHook_canAfford(P1, wolf), found);
        });
        return found;
    };
    EXPECT_FALSE(offered()) << "the additional cost can't be paid";
    auto poro = addUnit(P1, kInvalidId, 1);
    o(poro).tags = {"Poro"};
    EXPECT_TRUE(offered());
}

// ── Accept paths the stage 2 tests never took ──

// 513 Arise!: "Ready up to two of them."
TEST_F(VerifyCardsTest, Arise_ReadyingTwoOfThreeSandSoldiers) {
    auto spell = addToHand(P1, kArise);
    for (int i = 0; i < 3; ++i) {
        auto g = addGear(P1, kSerratedDirk);
        ASSERT_TRUE(isEquipment(o(g)));
    }
    // Take a Sand Soldier whenever one is offered.
    EnginePathAgent agent([](const std::vector<Intent>& legal) {
        for (const auto& i : legal)
            if (!i.chosen_objects.empty()) return i;
        return legal.front();
    });
    playThroughEngine(spell, &agent);
    int ready = 0, exhausted = 0;
    for (const auto& [id, obj] : state.objects) {
        if (obj.name != "Sand Soldier" || !obj.location.has_value()) continue;
        if (obj.is_exhausted) ++exhausted; else ++ready;
    }
    EXPECT_EQ(ready, 2) << "up to two";
    EXPECT_EQ(exhausted, 1);
}

// 224 Salvage: "You may kill up to one gear. Draw 1."
TEST_F(VerifyCardsTest, Salvage_KillingTheChosenGearAndDrawing) {
    auto my_gear = addGear(P1, kSerratedDirk);
    auto other = addGear(P1, kSerratedDirk);
    addToDeck(P1, kMeditation);
    auto spell = addToHand(P1, kSalvage);
    playAt(spell, {my_gear});
    EXPECT_FALSE(onBoard(my_gear));
    EXPECT_TRUE(onBoard(other));
    EXPECT_EQ(handSize(P1), 1);
}

// 286 The Candlelit Sanctum: "You may recycle one or both of them."
TEST_F(VerifyCardsTest, CandlelitSanctum_RecyclingOneOfTheTwo) {
    auto sanctum = battlefieldCard(0, kCandlelitSanctum);
    auto third = addToDeck(P1, kRecruit);
    auto second = addToDeck(P1, kRecruit);
    auto top = addToDeck(P1, kRecruit);
    int recycled_events = 0;
    auto conn = events.on_object_state_changed.connect([&](const ObjectStateChangedEvent& e) {
        if (e.what_changed == "recycled_main") ++recycled_events;
    });
    // Recycle the first card asked about, keep the second.
    int asked = 0;
    EnginePathAgent agent([&](const std::vector<Intent>& legal) {
        const bool recycle = asked++ == 0;
        for (const auto& i : legal)
            if (i.chosen_objects.empty() != recycle) return i;
        return legal.front();
    });
    fireTriggerThroughEngine(sanctum, TriggerType::WhenYouConquerHere, &agent);
    conn.disconnect();
    ASSERT_EQ(deckSize(P1), 3);
    EXPECT_EQ(recycled_events, 1);
    const auto& deck = ps(P1).main_deck;
    const GameObjectId recycled = deck.front();
    EXPECT_TRUE(recycled == top || recycled == second) << "one of the two looked at";
    EXPECT_EQ(deck.back(), recycled == top ? second : top) << "the kept one is back on top";
    EXPECT_EQ(deck[1], third);
}

// 671 Blood Rose: "When you play a unit, you may pay [1] to gain 1 XP."
TEST_F(VerifyCardsTest, BloodRose_PayingOneForAnXpWhenAUnitIsPlayed) {
    addGear(P1, kBloodRose);
    auto unit = unitInHand(P1, kShipyardSkulker);
    ps(P1).rune_pool.energy = 1;
    EnginePathAgent yes(EnginePathAgent::script({1}));
    playThroughEngine(unit, &yes);
    EXPECT_EQ(yes.prompts.size(), 1u);
    EXPECT_EQ(ps(P1).xp, 1);
    EXPECT_EQ(ps(P1).rune_pool.energy, 0);

    auto second = unitInHand(P1, kShipyardSkulker);
    ps(P1).rune_pool.energy = 1;
    EnginePathAgent no(EnginePathAgent::script({0}));
    playThroughEngine(second, &no);
    EXPECT_EQ(no.prompts.size(), 1u);
    EXPECT_EQ(ps(P1).xp, 1) << "declined: no XP";
    EXPECT_EQ(ps(P1).rune_pool.energy, 1) << "and nothing paid";
}

// 07 item 7.34: Forge of the Fluft — the Equipment and the unit are chosen as
// the ability is activated (kFinalizeActivate), so they are on the chain item
// before anyone can respond.
TEST_F(VerifyCardsTest, ForgeOfTheFluft_TheEquipmentAndUnitAreChosenAtActivation) {
    battlefieldCard(0, kForgeOfTheFluft);
    addUnit(P1, kInvalidId, 1, 0);                    // holds the battlefield
    auto lg = addLegend(P1, kPurifier);               // any legend
    auto a = addUnit(P1, kInvalidId, 2);
    auto b = addUnit(P1, kInvalidId, 2);
    auto sword = addGear(P1, kLongSword);
    attach(sword, a);
    cleanup();
    ASSERT_EQ(o(lg).granted_abilities.size(), 1u);
    Intent act;
    act.type = IntentType::ActivateAbility;
    act.player = P1;
    act.ability_source = lg;
    act.granted_ability_def = kForgeOfTheFluft;
    EnginePathAgent agent(EnginePathAgent::script({static_cast<int>(sword), static_cast<int>(b)}));
    executeAndProbe(act, &agent, [&](GameState& live) {
        ASSERT_FALSE(live.chain.items.empty());
        const auto& chosen = live.chain.items.back().chosen_objects;
        EXPECT_NE(std::find(chosen.begin(), chosen.end(), sword), chosen.end());
        EXPECT_NE(std::find(chosen.begin(), chosen.end(), b), chosen.end());
        EXPECT_EQ(live.getObject(sword).attached_to, std::optional<GameObjectId>{a})
            << "not moved until it resolves";
    });
    EXPECT_EQ(o(sword).attached_to, std::optional<GameObjectId>{b});
}

// 08 S2 / 07 item 7.26: units whose own ability acts on themselves ("give me
// +N [M]", "buff me", "ready me", "move me") do nothing once they have left
// the board — whichever of their triggers is on the chain, and even when the
// controller says yes to everything.
TEST_F(VerifyCardsTest, S2_ASourceThatLeftTheBoardDoesNothingToItself) {
    const CardDefId ids[] = {27, 36, 56, 59, 103, 131, 167, 177, 197, 202, 304, 343,
                             380, 398, 434, 458, 590, 681};
    for (CardDefId def_id : ids) {
        SCOPED_TRACE("card " + std::to_string(def_id) + " " + card_db.get(def_id).name);
        Card* card = card_registry.get(def_id);
        ASSERT_NE(card, nullptr);
        for (TriggerType t : card->triggerTypes()) {
            if (t == TriggerType::None || t == TriggerType::Activated) continue;
            // A fresh board each time: the source in the trash, things around
            // for it to act on, and the means to pay for anything.
            auto source = addUnit(P1, def_id, 0, 0);
            auto ally = addUnit(P1, kShipyardSkulker, 0, 0);
            auto enemy = addUnit(P2, kShipyardSkulker, 9, 0);
            sendToTrash(source);
            ps(P1).rune_pool.energy = 5;
            ps(P1).rune_pool.universal_power = 5;
            ps(P1).xp = 5;
            const KeywordSet before = o(source).keywords;
            EnginePathAgent yes([](const std::vector<Intent>& legal) {
                for (const auto& i : legal)
                    if (i.chosen_value.has_value() && *i.chosen_value == 1) return i;
                return legal.front();
            });
            fireTriggerThroughEngine(source, t, &yes, ally);
            EXPECT_FALSE(onBoard(source)) << "not moved back onto the board";
            EXPECT_EQ(o(source).zone, ZoneType::Trash);
            EXPECT_EQ(o(source).temp_might_bonus, 0);
            EXPECT_EQ(o(source).buff_count, 0);
            EXPECT_TRUE(o(source).keywords == before);
            EXPECT_TRUE(o(source).attachments.empty());
            EXPECT_EQ(o(enemy).damage_marked, 0) << "its Might is not used from the trash";
            EXPECT_TRUE(onBoard(enemy));
            // Clear the board for the next round.
            for (auto id : {source, ally, enemy}) {
                o(id).location = std::nullopt;
                o(id).zone = ZoneType::Banishment;
            }
            ps(P1).trash.clear();
        }
    }
}

// 08 S2: Akshan, Mischievous — once he has left the board nothing is taken,
// and the enemy Equipment is not stripped from its bearer either.
TEST_F(VerifyCardsTest, Akshan_OffTheBoardLeavesTheEnemyEquipmentWhereItIs) {
    auto akshan = addUnit(P1, kAkshan, 0, 0);
    auto bearer = addUnit(P2, kShipyardSkulker, 0, 1);
    auto sword = addGear(P2, kLongSword);
    attach(sword, bearer);
    o(akshan).card_counters["__akshan_paid"] = 1;
    sendToTrash(akshan);
    EnginePathAgent agent(EnginePathAgent::choosing(sword));
    fireTriggerThroughEngine(akshan, TriggerType::AsYouPlayMe, &agent);
    EXPECT_EQ(o(sword).controller, P2);
    EXPECT_EQ(o(sword).attached_to, std::optional<GameObjectId>{bearer})
        << "not detached for a move that does not happen";
    EXPECT_EQ(o(bearer).attachments.size(), 1u);
}

// 08 S5: the pick is not the first option, so a first-match card fails.
TEST_F(VerifyCardsTest, StarSpring_MovesTheLaterOfTwoOtherUnits) {
    auto spring = battlefieldCard(0, kStarSpring);
    auto played = addUnit(P1, kShipyardSkulker, 0, 0);
    auto first = addUnit(P1, kShipyardSkulker, 0, 0);
    auto later = addUnit(P1, kShipyardSkulker, 0, 0);
    EnginePathAgent agent(EnginePathAgent::choosing(later));
    fireTriggerThroughEngine(spring, TriggerType::WhenYouPlayAUnit, &agent, played);
    EXPECT_TRUE(agent.wasOffered(first));
    EXPECT_TRUE(o(later).isAtBase());
    EXPECT_TRUE(o(first).isAtBattlefield());
    EXPECT_TRUE(o(played).isAtBattlefield());
}

TEST_F(VerifyCardsTest, AssemblyRig_RecyclesTheSecondUnitWhenThatIsThePick) {
    auto rig = addGear(P1, kAssemblyRig);
    ps(P1).rune_pool.energy = 1;
    power(P1, Domain::Fury) = 1;
    auto u1 = addToTrash(P1, kShipyardSkulker);
    auto u2 = addToTrash(P1, kShipyardSkulker);
    EnginePathAgent agent(EnginePathAgent::choosing(u2));
    activate(rig, &agent);
    EXPECT_TRUE(agent.wasOffered(u1));
    EXPECT_TRUE(inDeck(P1, u2));
    EXPECT_TRUE(inTrash(P1, u1));
    EXPECT_EQ(countNamed(P1, "Mech"), 1);
}


// ═══ Tranche 2, group A ═══════════════════════════════════════════════════════
// ═══ Fixer A: sweep items 191 / 695 / 147 / 765 / 449 / 21 / 680 / 446 / 373 / 855 / 406 ═══

// 191 Maddened Marauder: a unit that can't be moved to base is not a choice.
TEST_F(VerifyCardsTest, MaddenedMarauder_DoesNotOfferAUnitThatCannotBeMovedToBase) {
    auto marauder = addUnit(P1, kMaddenedMarauder);
    state.battlefields[0].blocks_move_to_base = true;    // as Vilemaw's Lair sets it
    auto stuck = addUnit(P2, kShipyardSkulker, 0, 0);
    auto guarded = addUnit(P2, kShipyardSkulker, 0, 1);
    auto cutlass = addGear(P2, kJaggedCutlass);
    attach(cutlass, guarded);
    o(guarded).cant_be_moved_by_enemy = true;            // what the Cutlass's aura sets
    auto free_unit = addUnit(P2, kShipyardSkulker, 0, 1);
    EnginePathAgent agent;
    fireTriggerThroughEngine(marauder, TriggerType::WhenYouPlayMe, &agent);
    ASSERT_EQ(agent.prompts.size(), 1u);
    EXPECT_FALSE(agent.wasOffered(stuck)) << "units can't move from battlefield #0 to base";
    EXPECT_FALSE(agent.wasOffered(guarded)) << "can't be moved by enemy spells and abilities";
    EXPECT_TRUE(agent.wasOffered(free_unit));
    EXPECT_TRUE(o(free_unit).isAtBase());
    EXPECT_EQ(bfOf(stuck), std::optional<BattlefieldId>{0});
    EXPECT_EQ(bfOf(guarded), std::optional<BattlefieldId>{1});
}

// 695 Blast Cone: an enemy unit enemy abilities can't move is not a choice.
TEST_F(VerifyCardsTest, BlastCone_DoesNotOfferAnEnemyUnitItCannotMove) {
    auto cone = addGear(P1, kBlastCone);
    auto guarded = addUnit(P2, kShipyardSkulker, 0, 0);
    auto cutlass = addGear(P2, kJaggedCutlass);
    attach(cutlass, guarded);
    o(guarded).cant_be_moved_by_enemy = true;            // what the Cutlass's aura sets
    auto free_unit = addUnit(P2, kShipyardSkulker, 0, 0);
    // Yes, the unit, to its base (mode 0); the stun "may" that follows is declined.
    EnginePathAgent agent(EnginePathAgent::script({1, static_cast<int>(free_unit), 0}));
    fireTriggerThroughEngine(cone, TriggerType::WhenYouPlayThis, &agent);
    ASSERT_GE(agent.prompts.size(), 2u);
    EXPECT_FALSE(agent.wasOffered(guarded)) << "can't be moved by enemy spells and abilities";
    EXPECT_TRUE(agent.wasOffered(free_unit));
    EXPECT_TRUE(o(free_unit).isAtBase());
    EXPECT_EQ(bfOf(guarded), std::optional<BattlefieldId>{0});
}

TEST_F(VerifyCardsTest, BlastCone_WithOnlyAnUnmovableEnemyAsksNothing) {
    auto cone = addGear(P1, kBlastCone);
    auto guarded = addUnit(P2, kShipyardSkulker, 0, 0);
    auto cutlass = addGear(P2, kJaggedCutlass);
    attach(cutlass, guarded);
    o(guarded).cant_be_moved_by_enemy = true;
    EnginePathAgent agent(EnginePathAgent::accepting());
    fireTriggerThroughEngine(cone, TriggerType::WhenYouPlayThis, &agent);
    EXPECT_TRUE(agent.prompts.empty()) << "no enemy unit it could move: the \"may\" is not offered";
    EXPECT_EQ(bfOf(guarded), std::optional<BattlefieldId>{0});
}

// 147 Wildclaw Shaman: spending a unit's buff is paying a cost, not choosing
// the unit — a "when you choose me" ability does not see it.
TEST_F(VerifyCardsTest, WildclawShaman_SpendingABuffDoesNotChooseTheUnit) {
    auto irelia = addUnit(P1, kIreliaFervent);
    o(irelia).buff_count = 1;
    o(irelia).recomputeMight();
    auto plain = addUnit(P1, kShipyardSkulker);
    o(plain).buff_count = 1;
    o(plain).recomputeMight();
    auto shaman = addToHand(P1, kWildclawShaman);
    EnginePathAgent agent(EnginePathAgent::script({1, static_cast<int>(irelia)}));
    playThroughEngine(shaman, &agent);
    EXPECT_TRUE(agent.wasOffered(plain)) << "the controller picks whose buff is spent";
    EXPECT_EQ(o(irelia).buff_count, 0) << "\"spend a buff\"";
    EXPECT_EQ(o(plain).buff_count, 1);
    EXPECT_EQ(o(irelia).temp_might_bonus, 0)
        << "\"When you choose ... me\" did not trigger: a cost is not a choice";
    EXPECT_EQ(o(shaman).buff_count, 1);
    EXPECT_FALSE(o(shaman).is_exhausted);
}

// 765 Dusk Rose Lab: the kill was replaced as the cost was paid — no draw, and
// the cost is not attempted again as the ability resolves.
TEST_F(VerifyCardsTest, DuskRoseLab_AReplacedKillIsNotTriedAgainAtResolution) {
    auto lab = battlefieldCard(0, kDuskRoseLab);
    addToDeck(P1, kRecruit);
    auto keeper = addUnit(P1, kShipyardSkulker, 0, 0);   // P1 keeps units (and control) here
    auto unit = addUnit(P1, kShipyardSkulker, 0, 0);
    o(unit).death_replacement_recall_pending = true;     // one-shot: recalled instead of dying
    EnginePathAgent agent(EnginePathAgent::choosing(unit));   // yes, then which unit
    fireAndProbe(lab, TriggerType::AtStartOfBeginning, &agent, [&](GameState& live) {
        auto& u = live.getObject(unit);
        EXPECT_TRUE(u.isAtBase()) << "the death was replaced as the cost was paid";
        EXPECT_FALSE(u.death_replacement_recall_pending);
        // In response it comes back to the Lab: nothing protects it now.
        u.location = BattlefieldLocation{0};
        u.zone = ZoneType::BattlefieldZone;
    });
    EXPECT_TRUE(onBoard(unit)) << "the cost is attempted once, not again at resolution";
    EXPECT_TRUE(onBoard(keeper));
    EXPECT_EQ(handSize(P1), 0) << "no kill, no draw";
}

// 449 Overzealous Fan: "kill me to ..." — the Fan's death was replaced, so the
// cost was not paid and the attacker stays.
TEST_F(VerifyCardsTest, OverzealousFan_SurvivingTheKillMovesNobody) {
    auto fan = addUnit(P1, kOverzealousFan, 0, 0);
    auto attacker = addUnit(P2, kShipyardSkulker, 0, 0);
    auto& b = state.battlefields[0];
    b.combat_in_progress = true;
    b.attacker = P2;
    b.defender = P1;
    o(fan).combat_designation = CombatDesignation::Defender;
    o(attacker).combat_designation = CombatDesignation::Attacker;
    o(fan).card_counters["__defend_attacker_id"] = static_cast<int>(attacker);
    o(fan).death_replacement_recall_pending = true;      // recalled instead of dying
    EnginePathAgent agent(EnginePathAgent::choosing(attacker));
    fireTriggerThroughEngine(fan, TriggerType::WhenIDefend, &agent);
    ASSERT_EQ(agent.prompts.size(), 2u) << "the may, the attacker";
    EXPECT_TRUE(onBoard(fan)) << "the death was replaced";
    EXPECT_EQ(bfOf(attacker), std::optional<BattlefieldId>{0}) << "no kill, no move";
}

// 21 Sun Disc: unarmed, playing a unit puts no Sun Disc ability on the chain.
TEST_F(VerifyCardsTest, SunDisc_UnarmedDoesNotTriggerWhenAUnitIsPlayed) {
    auto disc = addGear(P1, kSunDisc);
    // Plays `card` from hand; true if the opponent ever had priority with a
    // Sun Disc ability on the chain.
    auto disc_ability_seen = [&](GameObjectId card) {
        bool saw = false;
        GameState* live = nullptr;
        PickByPredicateAgent p2([&](int, const std::vector<Intent>& legal) {
            if (live)
                for (const auto& ci : live->chain.items)
                    if (ci.source == disc) saw = true;
            for (const auto& i : legal)
                if (i.type == IntentType::PassPriority) return i;
            return legal.front();
        });
        ps(P1).rune_pool.energy += card_db.get(o(card).card_def_id).energy_cost;
        Intent play;
        play.type = IntentType::PlayCard;
        play.player = P1;
        play.card = card;
        play.play_location = LocationId{BaseLocation{P1}};
        withEngine([&](GameEngine& e) {
            live = &e.mutableState();
            e.testHook_executeIntent(play);
            e.testHook_runChain();
            live = nullptr;
        }, nullptr, &p2);
        return saw;
    };
    auto first = addToHand(P1, kRecruit);
    EXPECT_FALSE(disc_ability_seen(first)) << "not armed: the ability does not trigger";
    ASSERT_TRUE(onBoard(first));
    EXPECT_TRUE(o(first).is_exhausted);

    // Armed ([Legion]: a card was played this turn): it triggers for the next unit.
    ASSERT_GE(ps(P1).cards_played_this_turn, 1);
    activate(disc);
    auto second = addToHand(P1, kRecruit);
    EXPECT_TRUE(disc_ability_seen(second)) << "armed: the ability is on the chain";
    ASSERT_TRUE(onBoard(second));
    EXPECT_FALSE(o(second).is_exhausted) << "\"enters ready\"";
}

// 680 Elder Dragon: [Deflect] is owed for every unit chosen — one payable [A]
// buys one Deflect unit, not one per location.
TEST_F(VerifyCardsTest, ElderDragon_DeflectIsCountedAcrossLocations) {
    auto dragon = addUnit(P1, kElderDragon, 0, 1);
    auto in_base = addUnit(P2, kIreliaFervent);           // [Deflect]: [A] to choose
    auto at_bf = addUnit(P2, kIreliaFervent, 0, 0);
    power(P1, Domain::Body) = 1;                          // all P1 can pay
    EnginePathAgent agent(EnginePathAgent::script(
        {static_cast<int>(in_base), static_cast<int>(at_bf)}));
    fireTriggerThroughEngine(dragon, TriggerType::WhenYouPlayMe, &agent);
    ASSERT_EQ(agent.prompts.size(), 1u) << "after the first pick nothing is left to pay with";
    EXPECT_TRUE(agent.wasOffered(in_base));
    EXPECT_FALSE(agent.wasOffered(at_bf)) << "its [A] can't be paid on top of the first pick's";
    EXPECT_EQ(power(P1, Domain::Body), 0) << "the first pick's Deflect was paid";
    EXPECT_TRUE(!onBoard(in_base) || o(in_base).damage_marked >= 1) << "\"Deal 1 to them\"";
    ASSERT_TRUE(onBoard(at_bf));
    EXPECT_EQ(o(at_bf).damage_marked, 0) << "not chosen: its Deflect was never paid";
}

// 446 Fae Porter — NOT A DEFECT: "the same battlefield" is read off the Porter.
// Once it has left the board that information is gone and the move is ignored

// 373 Azir, Ascendant: "Use only once per turn" — an activation is a use even
// if the chosen unit is gone when it resolves.
TEST_F(VerifyCardsTest, AzirAscendant_AnActivationThatDoesNothingIsStillTheUseForTheTurn) {
    auto azir = addUnit(P1, kAzirAscendant, 0);
    auto other = addUnit(P1, kShipyardSkulker, 0, 1);
    auto spare = addUnit(P1, kShipyardSkulker, 0, 0);     // still there to swap with
    power(P1, Domain::Calm) = 1;
    Intent act;
    act.type = IntentType::ActivateAbility;
    act.player = P1;
    act.ability_source = azir;
    EnginePathAgent agent(EnginePathAgent::choosing(other));
    executeAndProbe(act, &agent, [&](GameState& live) {
        auto it = live.getObject(azir).card_counters.find("azir_used_turn");
        ASSERT_TRUE(it != live.getObject(azir).card_counters.end())
            << "used as the ability goes on the chain";
        EXPECT_EQ(it->second, live.turn.turn_number + 1);
        removeFromBoard(live, other);                     // in response: the chosen unit leaves
    });
    EXPECT_TRUE(o(azir).isAtBase()) << "nothing to swap with";
    EXPECT_EQ(bfOf(spare), std::optional<BattlefieldId>{0});
    EXPECT_EQ(o(azir).card_counters["azir_used_turn"], state.turn.turn_number + 1);
    const Card* card = card_registry.get(kAzirAscendant);
    ASSERT_NE(card, nullptr);
    EXPECT_FALSE(card->canActivateAbilityAt(state, P1, azir, 0))
        << "\"Use only once per turn\": not offered again this turn";
}

// 855 Jayce, Brilliant Inventor: "ready something besides me that's exhausted"
// — not only his controller's things.
TEST_F(VerifyCardsTest, JayceBrilliantInventor_MayReadyAnEnemyObject) {
    auto jayce = addUnit(P1, kJayceBrilliantInventor);
    auto theirs = addUnit(P2, kShipyardSkulker, 0, 0);
    o(theirs).is_exhausted = true;
    EnginePathAgent agent(EnginePathAgent::choosing(theirs));
    fireTriggerThroughEngine(jayce, TriggerType::WhenYouPlayMe, &agent);
    ASSERT_EQ(agent.prompts.size(), 2u) << "the may, then what to ready";
    EXPECT_TRUE(agent.wasOffered(theirs)) << "\"something\": no \"friendly\" in the text";
    EXPECT_FALSE(o(theirs).is_exhausted);
}

// 855: the gear trigger's choices are made again after "make new choices for
// it" (GameEngine::makeNewChoicesFor, reached as Rebuttal reaches it:
// EffectExecutor::makeNewChoices), and the ability is not lost because its
// "first gear this turn" stamp was already written when it went on the chain.
TEST_F(VerifyCardsTest, JayceBrilliantInventor_IsAskedAgainAfterNewChoicesAndStillResolves) {
    auto jayce = addUnit(P1, kJayceBrilliantInventor);
    auto first = addUnit(P1, kShipyardSkulker, 0);
    o(first).is_exhausted = true;
    auto second = addUnit(P1, kShipyardSkulker, 0);
    o(second).is_exhausted = true;
    auto gear = addGear(P1, kSerratedDirk);
    // Finalization: yes, `first`. After the new choices: yes, `second`.
    EnginePathAgent agent(EnginePathAgent::script(
        {1, static_cast<int>(first), 1, static_cast<int>(second)}));
    GameEngine* live = nullptr;
    bool rechosen = false;
    PickByPredicateAgent p2([&](int, const std::vector<Intent>& legal) {
        if (!rechosen && live && !live->mutableState().chain.items.empty()) {
            rechosen = true;
            auto& st = live->mutableState();
            EXPECT_EQ(st.getObject(jayce).card_counters["__jayce_first_gear_turn"],
                      st.turn.turn_number + 1) << "stamped as the ability went on the chain";
            live->testHook_executor().makeNewChoices(st.chain.items.back().id);
        }
        for (const auto& i : legal)
            if (i.type == IntentType::PassPriority) return i;
        return legal.front();
    });
    withEngine([&](GameEngine& e) {
        live = &e;
        e.testHook_executor().queueTriggeredAbility(jayce, P1, TriggerType::WhenYouPlayACard, gear);
        e.testHook_runChain();
        live = nullptr;
    }, &agent, &p2);
    ASSERT_TRUE(rechosen) << "the opponent never got priority";
    EXPECT_EQ(agent.prompts.size(), 4u) << "may + what, asked at finalization and again";
    EXPECT_TRUE(o(first).is_exhausted) << "the first answer was forgotten";
    EXPECT_FALSE(o(second).is_exhausted) << "the re-chosen ability still resolved";

    // A second non-token gear this turn: not "the first time", nothing triggers.
    o(second).is_exhausted = true;
    auto gear2 = addGear(P1, kSerratedDirk);
    EnginePathAgent again(EnginePathAgent::accepting());
    fireTriggerThroughEngine(jayce, TriggerType::WhenYouPlayACard, &again, gear2);
    EXPECT_TRUE(again.prompts.empty()) << "the second gear this turn does not trigger";
    EXPECT_TRUE(o(second).is_exhausted);
}

// 406 Jayce, Man of Progress: "ignoring its Energy cost. (You must still pay
// its Power cost.)" — Sun Disc is [2][R].
TEST_F(VerifyCardsTest, JayceManOfProgress_TheGrantedGearStillCostsItsPower) {
    auto jayce = addToHand(P1, kJayceManOfProgress);
    auto kill = addGear(P1, kSerratedDirk);
    auto disc = addToHand(P1, kSunDisc);
    EnginePathAgent agent(EnginePathAgent::choosing(kill));
    playThroughEngine(jayce, &agent);
    ASSERT_FALSE(onBoard(kill));
    ASSERT_EQ(ps(P1).cost_modifiers.size(), 1u) << "the permission";

    // No Power: the Energy is ignored, the [R] is not.
    ps(P1).rune_pool.energy = 0;
    ps(P1).rune_pool.universal_power = 0;
    power(P1, Domain::Fury) = 0;
    withEngine([&](GameEngine& e) {
        EXPECT_FALSE(e.testHook_canAfford(P1, disc)) << "[R] must still be paid";
    });
    // With the Power: affordable, and playing it spends the Power and no Energy.
    power(P1, Domain::Fury) = 1;
    ps(P1).rune_pool.energy = 3;
    withEngine([&](GameEngine& e) {
        EXPECT_TRUE(e.testHook_canAfford(P1, disc));
    });
    playThroughEngine(disc, nullptr, std::nullopt, nullptr, /*fund=*/false);
    ASSERT_TRUE(onBoard(disc));
    EXPECT_EQ(power(P1, Domain::Fury), 0) << "its Power cost was paid";
    EXPECT_EQ(ps(P1).rune_pool.energy, 3) << "its Energy cost was ignored";
    EXPECT_TRUE(ps(P1).cost_modifiers.empty()) << "\"a gear\": the permission is used up";
}


// ═══ Tranche 2, group B ═══════════════════════════════════════════════════════
// (already defined in the file and used below: kArise, kVoidBurrower, kGuards,
//  kGlascMixologist, kWildClaw, kMegaMech, kShipyardSkulker, kMeditation,
//  kSerratedDirk, kForgottenMonument)

// ═══ Sweep B: items 1-7 ═════════════════════════════════════════════════════

// ── 1. Icathian Rain 247: a play-time unit that is gone loses its "Deal 2" ──

TEST_F(VerifyCardsTest, IcathianRain_AUnitGoneBeforeItResolvesLosesOneOfTheSix) {
    // CR 359.3.e.5: the first "Deal 2 to a unit" was aimed as the spell was
    // played. Its unit leaves in response: that instance is lost, the other
    // five are still chosen — not six fresh ones.
    auto spell = addToHand(P1, kIcathianRain);
    auto first = addUnit(P2, kInvalidId, 3, 0);
    auto tough = addUnit(P2, kInvalidId, 30, 1);
    const auto& def = card_db.get(kIcathianRain);
    ps(P1).rune_pool.energy += def.energy_cost;
    ps(P1).rune_pool.universal_power += def.power_cost;
    Intent play;
    play.type = IntentType::PlayCard;
    play.player = P1;
    play.card = spell;
    play.targets = {first};
    EnginePathAgent agent(EnginePathAgent::choosing(tough));
    executeAndProbe(play, &agent, [&](GameState& live) { removeFromBoard(live, first); });
    EXPECT_EQ(agent.prompts.size(), 5u) << "five units left to choose, not six";
    EXPECT_EQ(o(tough).damage_marked, 10) << "5 x 2";
    EXPECT_FALSE(onBoard(first));
}

// ── 2. Bullet Time 263: a battlefield is chosen, not a unit; X is capped ──

TEST_F(VerifyCardsTest, BulletTime_ChoosesABattlefieldNotAUnitThere) {
    // "...to all enemy units at a battlefield": no unit is chosen, so nothing
    // is "chosen" (no Deflect, no "chose an enemy unit this turn") and any
    // battlefield may be named, the empty one included.
    auto spell = addToHand(P1, kBulletTime);
    auto e1 = addUnit(P2, kInvalidId, 9, 1);
    auto e2 = addUnit(P2, kInvalidId, 9, 1);
    ps(P1).rune_pool.universal_power = 2;
    EnginePathAgent agent(EnginePathAgent::script({1, 2}));   // battlefield slot 1; X = 2
    playThroughEngine(spell, &agent);
    ASSERT_EQ(agent.prompts.size(), 2u) << "the battlefield, then X";
    for (const auto& i : agent.prompts[0])
        EXPECT_TRUE(i.chosen_objects.empty()) << "a battlefield is offered, not a unit";
    EXPECT_TRUE(promptOffersValue(agent, 0, 0)) << "a battlefield with no unit may be named";
    EXPECT_TRUE(promptOffersValue(agent, 0, 1));
    EXPECT_FALSE(ps(P1).chose_enemy_unit_this_turn) << "no unit was chosen";
    EXPECT_EQ(o(e1).damage_marked, 2);
    EXPECT_EQ(o(e2).damage_marked, 2);
    EXPECT_EQ(ps(P1).rune_pool.universal_power, 0);
}

TEST_F(VerifyCardsTest, BulletTime_TheBattlefieldIsNamedBeforeTheOpponentCanRespond) {
    // CR 355: chosen as the spell is played; X is asked as it resolves.
    auto spell = addToHand(P1, kBulletTime);
    auto enemy = addUnit(P2, kInvalidId, 9, 1);
    ps(P1).rune_pool.universal_power = 1;
    ps(P1).rune_pool.energy += card_db.get(kBulletTime).energy_cost;
    Intent play;
    play.type = IntentType::PlayCard;
    play.player = P1;
    play.card = spell;
    EnginePathAgent agent(EnginePathAgent::script({1, 1}));
    executeAndProbe(play, &agent, [&](GameState& live) {
        EXPECT_EQ(agent.prompts.size(), 1u) << "the battlefield is already chosen, X is not";
        EXPECT_EQ(live.player(P1).rune_pool.universal_power, 1) << "nothing paid yet";
        EXPECT_EQ(live.getObject(enemy).damage_marked, 0);
    });
    EXPECT_EQ(agent.prompts.size(), 2u);
    EXPECT_EQ(o(enemy).damage_marked, 1);
}

TEST_F(VerifyCardsTest, BulletTime_XIsCappedAtTheFifteenTheActionVocabularyEncodes) {
    // pickXAmount publishes one int-coded choice per X; the action vocabulary
    // has 16 int slots (openspiel::kNumIntChoices): 0..15.
    auto spell = addToHand(P1, kBulletTime);
    auto enemy = addUnit(P2, kInvalidId, 99, 0);
    ps(P1).rune_pool.universal_power = 20;
    EnginePathAgent agent(EnginePathAgent::script({0, 15}));
    playThroughEngine(spell, &agent);
    ASSERT_EQ(agent.prompts.size(), 2u);
    EXPECT_EQ(agent.prompts[1].size(), 16u) << "X = 0..15";
    EXPECT_TRUE(promptOffersValue(agent, 1, 15));
    EXPECT_FALSE(promptOffersValue(agent, 1, 16));
    EXPECT_EQ(o(enemy).damage_marked, 15);
    EXPECT_EQ(ps(P1).rune_pool.universal_power, 5);
}

// ── 3. Promising Future 115: only one of the top five can be banished ──

TEST_F(VerifyCardsTest, PromisingFuture_AnAnswerThatIsNotOneOfTheTopFiveBanishesTheTopCard) {
    auto bystander = addUnit(P1, kShipyardSkulker, 0);
    for (int i = 0; i < 4; ++i) addToDeck(P1, kShipyardSkulker);
    auto top = addToDeck(P1, kShipyardSkulker);
    auto spell = addToHand(P1, kPromisingFuture);
    // An answer naming an object that was never on offer.
    EnginePathAgent agent([&](const std::vector<Intent>& legal) {
        Intent wrong = legal.front();
        wrong.chosen_objects = {bystander};
        return wrong;
    });
    playThroughEngine(spell, &agent);
    ASSERT_EQ(agent.prompts.size(), 1u) << "P2 has no deck: only P1 is asked";
    EXPECT_FALSE(agent.wasOffered(bystander));
    EXPECT_TRUE(onBoard(bystander)) << "a unit on the board is not one of the top 5";
    EXPECT_FALSE(inBanishment(P1, bystander));
    EXPECT_TRUE(o(bystander).isAtBase()) << "it never moved";
    EXPECT_FALSE(inDeck(P1, top)) << "\"banishes one of them\": the top card instead";
    EXPECT_TRUE(onBoard(top) || inBanishment(P1, top)) << "banished, then played if paid for";
    EXPECT_EQ(deckSize(P1), 4) << "the other four were recycled";
    EXPECT_EQ(ps(P1).cards_banished_this_turn, 1);
}

// ── 4. Keeper's Verdict 760: a token's owner has nothing to decide ──

TEST_F(VerifyCardsTest, KeepersVerdict_ATokensOwnerIsNotAskedTopOrBottom) {
    // CR 183.1: a token that leaves the board ceases to exist.
    auto spell = addToHand(P1, kKeepersVerdict);
    auto token = addUnit(P2, kInvalidId, 3, 0);
    o(token).is_token = true;
    o(token).super_type = SuperType::Token;
    addToDeck(P2, kMeditation);
    EnginePathAgent agent(EnginePathAgent::choosing(token));
    EnginePathAgent owner;
    playThroughEngine(spell, &agent, std::nullopt, &owner);
    EXPECT_TRUE(owner.prompts.empty()) << "top or bottom of a deck it never reaches";
    EXPECT_FALSE(onBoard(token));
    EXPECT_FALSE(inDeck(P2, token));
    EXPECT_EQ(deckSize(P2), 1);
    EXPECT_FALSE(inBanishment(P2, token));
}

// ── 5 / 6. Void Burrower 553: "may" and "exhaust me" at finalization; a
//           printed reveal is shown to both players ──

TEST_F(VerifyCardsTest, VoidBurrower_IsExhaustedBeforeTheOpponentCanRespondAndRevealsToBoth) {
    auto legend = addLegend(P1, kVoidBurrower);
    auto below = addToDeck(P1, kMeditation);
    auto top = addToDeck(P1, kMeditation);
    int shown = 0, to_all = 0;
    auto conn = events.on_card_revealed.connect([&](const CardRevealedEvent& e) {
        if (e.card != top && e.card != below) return;
        ++shown;
        if (e.revealed_to_all) ++to_all;
    });
    EnginePathAgent agent(EnginePathAgent::script({1}));   // yes; then banish none
    fireAndProbe(legend, TriggerType::WhenIConquer, &agent, [&](GameState& live) {
        EXPECT_EQ(agent.prompts.size(), 1u) << "the may was decided at finalization";
        EXPECT_TRUE(live.getObject(legend).is_exhausted) << "\"exhaust me\" is the cost (CR 383.3.b)";
        EXPECT_EQ(shown, 0) << "nothing is revealed until it resolves";
        EXPECT_EQ(live.player(P1).main_deck.size(), 2u);
    });
    conn.disconnect();
    EXPECT_EQ(agent.prompts.size(), 2u) << "\"You may banish one\" is asked on resolution";
    EXPECT_EQ(shown, 2);
    EXPECT_EQ(to_all, 2) << "\"reveal\": to every player";
    EXPECT_TRUE(o(legend).is_exhausted);
    EXPECT_EQ(deckSize(P1), 2) << "none banished: both recycled";
}

TEST_F(VerifyCardsTest, VoidBurrower_DecliningAtFinalizationCostsNothing) {
    auto legend = addLegend(P1, kVoidBurrower);
    addToDeck(P1, kMeditation);
    auto top = addToDeck(P1, kMeditation);
    EnginePathAgent agent(EnginePathAgent::script({0}));
    fireTriggerThroughEngine(legend, TriggerType::WhenIConquer, &agent);
    EXPECT_EQ(agent.prompts.size(), 1u);
    EXPECT_FALSE(o(legend).is_exhausted);
    EXPECT_EQ(ps(P1).main_deck.back(), top) << "the deck is untouched";
}

// ── 5. Deceiver 756: "may", discard and exhaust at finalization (and its
//       first engine-path test) ──

TEST_F(VerifyCardsTest, Deceiver_ThroughTheEngine_TheCostIsPaidBeforeTheOpponentCanRespond) {
    auto legend = addLegend(P1, kDeceiver);
    auto bf_card = battlefieldCard(0, kForgottenMonument);   // the battlefield held
    auto model = addUnit(P1, kShipyardSkulker, 0, 0);
    auto elsewhere = addUnit(P1, kShipyardSkulker, 0, 1);
    auto keep = addToHand(P1, kMeditation);
    auto discard = addToHand(P1, kMeditation);
    EnginePathAgent agent(EnginePathAgent::script(
        {1, static_cast<int>(discard), static_cast<int>(model)}));
    fireAndProbe(legend, TriggerType::WhenIConquerOrHold, &agent, [&](GameState& live) {
        EXPECT_EQ(agent.prompts.size(), 2u) << "the may and the discard: at finalization";
        EXPECT_TRUE(live.getObject(legend).is_exhausted) << "\"exhaust me\" (CR 383.3.b)";
        const auto& trash = live.player(P1).trash;
        EXPECT_NE(std::find(trash.begin(), trash.end(), discard), trash.end()) << "\"discard 1\"";
        for (const auto& [id, obj] : live.objects)
            EXPECT_FALSE(obj.isToken()) << "the Reflection is not played yet";
    }, bf_card);
    // "Then do this: It becomes a copy of another unit there." is a reflexive
    // trigger (CR 388): its unit is chosen as the ability resolves.
    ASSERT_EQ(agent.prompts.size(), 3u);
    EXPECT_TRUE(agent.wasOffered(model));
    EXPECT_FALSE(agent.wasOffered(elsewhere)) << "another unit THERE";
    EXPECT_TRUE(inTrash(P1, discard));
    EXPECT_TRUE(inHand(P1, keep));
    GameObjectId token = kInvalidId;
    for (const auto& [id, obj] : state.objects)
        if (obj.isToken() && obj.isUnit() && obj.location.has_value()) token = id;
    ASSERT_NE(token, kInvalidId);
    EXPECT_EQ(bfOf(token), std::optional<BattlefieldId>{0}) << "there";
    EXPECT_EQ(o(token).card_def_id, kShipyardSkulker) << "a copy of the chosen unit";
    EXPECT_TRUE(o(token).keywords.has(Keyword::Temporary));
    EXPECT_FALSE(o(token).is_exhausted) << "a READY Reflection";
}

TEST_F(VerifyCardsTest, Deceiver_ThroughTheEngine_DecliningPaysNothingAndPlaysNothing) {
    auto legend = addLegend(P1, kDeceiver);
    auto bf_card = battlefieldCard(0, kForgottenMonument);
    addUnit(P1, kShipyardSkulker, 0, 0);
    auto card = addToHand(P1, kMeditation);
    EnginePathAgent agent(EnginePathAgent::script({0}));
    fireTriggerThroughEngine(legend, TriggerType::WhenIConquerOrHold, &agent, bf_card);
    EXPECT_EQ(agent.prompts.size(), 1u);
    EXPECT_FALSE(o(legend).is_exhausted);
    EXPECT_TRUE(inHand(P1, card));
    for (const auto& [id, obj] : state.objects) EXPECT_FALSE(obj.isToken());
}

// ── 5 / 7. Rek'Sai, Swarm Queen 491: the first "may" at finalization; "you
//           may play it here" is one more place the unit can go ──

TEST_F(VerifyCardsTest, RekSaiSwarmQueen_TheRevealIsDecidedBeforeTheOpponentCanRespond) {
    auto reksai = addUnit(P1, kRekSaiSwarmQueen, 0, 0);
    addToDeck(P1, kMeditation);
    addToDeck(P1, kMeditation);
    int shown = 0;
    auto conn = events.on_card_revealed.connect([&](const CardRevealedEvent&) { ++shown; });
    EnginePathAgent agent(EnginePathAgent::script({1}));
    fireAndProbe(reksai, TriggerType::WhenIAttack, &agent, [&](GameState&) {
        EXPECT_EQ(agent.prompts.size(), 1u)
            << "CR 383.3.a: a \"you may\" that opens the effect is decided at finalization";
        EXPECT_EQ(shown, 0) << "nothing is revealed until it resolves";
    });
    conn.disconnect();
    EXPECT_EQ(shown, 2);
    EXPECT_EQ(agent.prompts.size(), 2u) << "\"You may banish one\": on resolution";
}

TEST_F(VerifyCardsTest, RekSaiSwarmQueen_TheUnitMayGoToBaseAndNotWhereUnitsCannotBePlayed) {
    // "If it is a unit, you may play it here": a permission (CR 355.2.b), on
    // top of the base and the battlefields its controller controls.
    auto reksai = addUnit(P1, kRekSaiSwarmQueen, 0, 0);       // "here" = battlefield #0
    state.battlefields[1].controller = P1;
    state.battlefields[1].blocks_unit_play = true;
    addUnit(P1, kInvalidId, 1, 1);                            // holds battlefield #1
    addToDeck(P1, kMeditation);
    auto unit = addToDeck(P1, kShipyardSkulker);              // top
    ps(P1).rune_pool.energy = card_db.get(kShipyardSkulker).energy_cost;
    EnginePathAgent agent(EnginePathAgent::script({1, static_cast<int>(unit), 0}));
    fireTriggerThroughEngine(reksai, TriggerType::WhenIAttack, &agent);
    ASSERT_EQ(agent.prompts.size(), 3u) << "the may, the card, where";
    EXPECT_TRUE(promptOffersValue(agent, 2, 0)) << "base";
    EXPECT_TRUE(promptOffersValue(agent, 2, 1)) << "here";
    EXPECT_FALSE(promptOffersValue(agent, 2, 2)) << "units can't be played to battlefield #1";
    ASSERT_TRUE(onBoard(unit));
    EXPECT_TRUE(o(unit).isAtBase()) << "\"you MAY play it here\"";
    EXPECT_EQ(deckSize(P1), 1) << "the rest was recycled";
}

TEST_F(VerifyCardsTest, RekSaiSwarmQueen_TheUnitCanBePlayedHere) {
    auto reksai = addUnit(P1, kRekSaiSwarmQueen, 0, 0);
    addToDeck(P1, kMeditation);
    auto unit = addToDeck(P1, kShipyardSkulker);
    ps(P1).rune_pool.energy = card_db.get(kShipyardSkulker).energy_cost;
    EnginePathAgent agent(EnginePathAgent::script({1, static_cast<int>(unit), 1}));
    fireTriggerThroughEngine(reksai, TriggerType::WhenIAttack, &agent);
    EXPECT_EQ(bfOf(unit), std::optional<BattlefieldId>{0});
    EXPECT_FALSE(inBanishment(P1, unit));
}

// ── 6. Teemo, Strategist 121: "reveal the top 5" is shown to both players ──

TEST_F(VerifyCardsTest, TeemoStrategist_TheRevealIsShownToBothPlayers) {
    auto teemo = addUnit(P1, kTeemoStrategist, 0, 0);
    addToDeck(P1, kMeditation);
    addToDeck(P1, kMeditation);
    int shown = 0, to_all = 0;
    auto conn = events.on_card_revealed.connect([&](const CardRevealedEvent& e) {
        ++shown;
        if (e.revealed_to_all) ++to_all;
    });
    fireTriggerThroughEngine(teemo, TriggerType::WhenIDefend);
    conn.disconnect();
    EXPECT_EQ(shown, 2);
    EXPECT_EQ(to_all, 2) << "\"reveal\": to every player, as the deck bookkeeping records";
}

// ── 5. Forgotten Relic 895: its first engine-path tests ──

TEST_F(VerifyCardsTest, ForgottenRelic_ThroughTheEngine_BurningAUnitGivesItsMightToTheChosenUnit) {
    auto relic = addGear(P1, kForgottenRelic);
    auto first = addUnit(P1, kShipyardSkulker, 0);
    auto second = addUnit(P1, kShipyardSkulker, 0);
    auto theirs = addUnit(P2, kShipyardSkulker, 0);
    addToDeck(P1, kMeditation);
    auto top = addToDeck(P1, kMegaMech);
    EnginePathAgent agent(EnginePathAgent::choosing(second));
    fireTriggerThroughEngine(relic, TriggerType::AtStartOfBeginning, &agent);
    EXPECT_TRUE(inTrash(P1, top)) << "[Burn 1]";
    EXPECT_EQ(deckSize(P1), 1) << "burned once, however often the callback is re-entered";
    ASSERT_EQ(agent.prompts.size(), 1u);
    EXPECT_TRUE(agent.wasOffered(first));
    EXPECT_FALSE(agent.wasOffered(theirs)) << "a FRIENDLY unit";
    EXPECT_EQ(o(second).temp_might_bonus, card_db.get(kMegaMech).might)
        << "+[M] equal to the burned card's Might";
    EXPECT_EQ(o(first).temp_might_bonus, 0);
}

TEST_F(VerifyCardsTest, ForgottenRelic_ThroughTheEngine_PlayingItBurnsOneAndANonUnitGivesNothing) {
    auto relic = addToHand(P1, kForgottenRelic);
    auto unit = addUnit(P1, kShipyardSkulker, 0);
    addToDeck(P1, kMegaMech);
    auto top = addToDeck(P1, kMeditation);
    EnginePathAgent agent;
    playThroughEngine(relic, &agent);
    ASSERT_TRUE(onBoard(relic));
    EXPECT_TRUE(inTrash(P1, top)) << "When you play this, [Burn 1]";
    EXPECT_EQ(deckSize(P1), 1);
    EXPECT_TRUE(agent.prompts.empty()) << "not a unit: nobody gets Might";
    EXPECT_EQ(o(unit).temp_might_bonus, 0);
}

// ── 7. Effect plays respect "units can't be played here" (CR 355.2.a) ──

TEST_F(VerifyCardsTest, GlascMixologist_ChoosesWhereAndIsNotOfferedABattlefieldUnitsCannotBePlayedTo) {
    auto glasc = addUnit(P1, kGlascMixologist, 0);
    auto cheap = addToTrash(P1, kShipyardSkulker);
    state.battlefields[0].controller = P1;
    state.battlefields[0].blocks_unit_play = true;
    state.battlefields[1].controller = P1;
    addUnit(P1, kInvalidId, 1, 0);                            // a unit holding each
    addUnit(P1, kInvalidId, 1, 1);
    EnginePathAgent agent(EnginePathAgent::script({1, static_cast<int>(cheap), 2}));
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) { exec.killObject(glasc); },
                         &agent);
    ASSERT_EQ(agent.prompts.size(), 3u) << "the may, the unit, where";
    EXPECT_TRUE(promptOffersValue(agent, 2, 0)) << "base";
    EXPECT_FALSE(promptOffersValue(agent, 2, 1)) << "units can't be played to battlefield #0";
    EXPECT_TRUE(promptOffersValue(agent, 2, 2));
    ASSERT_TRUE(onBoard(cheap));
    EXPECT_EQ(bfOf(cheap), std::optional<BattlefieldId>{1});
}

TEST_F(VerifyCardsTest, HeedlessResurrection_DoesNotOfferABattlefieldUnitsCannotBePlayedTo) {
    auto spell = addToHand(P1, kHeedlessResurrection);
    auto victim = addUnit(P1, kShipyardSkulker, 0);
    auto corpse = addToTrash(P1, kShipyardSkulker);
    state.battlefields[0].controller = P1;
    state.battlefields[0].blocks_unit_play = true;
    state.battlefields[1].controller = P1;
    addUnit(P1, kInvalidId, 1, 0);                            // a unit holding each
    addUnit(P1, kInvalidId, 1, 1);
    // The unit to kill, the unit to play, where (mode 0 = base, 1 and 2 = the
    // controlled battlefields in order).
    EnginePathAgent agent(EnginePathAgent::script(
        {static_cast<int>(victim), static_cast<int>(corpse), 2}));
    playThroughEngine(spell, &agent);
    ASSERT_EQ(agent.prompts.size(), 3u);
    EXPECT_TRUE(promptOffersValue(agent, 2, 0)) << "base";
    EXPECT_FALSE(promptOffersValue(agent, 2, 1)) << "units can't be played to battlefield #0";
    EXPECT_TRUE(promptOffersValue(agent, 2, 2));
    ASSERT_TRUE(onBoard(corpse));
    EXPECT_EQ(bfOf(corpse), std::optional<BattlefieldId>{1});
    EXPECT_TRUE(inTrash(P1, victim));
}

TEST_F(VerifyCardsTest, ArcaneShift_DoesNotOfferABattlefieldUnitsCannotBePlayedTo) {
    auto spell = addToHand(P1, kArcaneShift);
    auto unit = addUnit(P1, kShipyardSkulker, 0);
    state.battlefields[0].controller = P1;
    state.battlefields[0].blocks_unit_play = true;
    state.battlefields[1].controller = P1;
    addUnit(P1, kInvalidId, 1, 0);                            // a unit holding each
    addUnit(P1, kInvalidId, 1, 1);
    EnginePathAgent agent(EnginePathAgent::script({2}));     // no enemy unit: only "where"
    playAt(spell, {unit}, &agent);
    ASSERT_EQ(agent.prompts.size(), 1u);
    EXPECT_TRUE(promptOffersValue(agent, 0, 0)) << "base";
    EXPECT_FALSE(promptOffersValue(agent, 0, 1)) << "units can't be played to battlefield #0";
    EXPECT_TRUE(promptOffersValue(agent, 0, 2));
    ASSERT_TRUE(onBoard(unit));
    EXPECT_EQ(bfOf(unit), std::optional<BattlefieldId>{1});
}

TEST_F(VerifyCardsTest, Arise_DoesNotOfferABattlefieldUnitsCannotBePlayedTo) {
    auto spell = addToHand(P1, kArise);
    addGear(P1, kSerratedDirk);                               // one Equipment: one token
    state.battlefields[0].controller = P1;
    state.battlefields[0].blocks_unit_play = true;
    state.battlefields[1].controller = P1;
    addUnit(P1, kInvalidId, 1, 0);                            // a unit holding each
    addUnit(P1, kInvalidId, 1, 1);
    EnginePathAgent agent(EnginePathAgent::script({2}));
    playThroughEngine(spell, &agent);
    ASSERT_FALSE(agent.prompts.empty());
    EXPECT_TRUE(promptOffersValue(agent, 0, 0)) << "base";
    EXPECT_FALSE(promptOffersValue(agent, 0, 1)) << "units can't be played to battlefield #0";
    EXPECT_TRUE(promptOffersValue(agent, 0, 2));
    ASSERT_EQ(countNamed(P1, "Sand Soldier"), 1);
    EXPECT_EQ(bfOf(firstNamed(P1, "Sand Soldier")), std::optional<BattlefieldId>{1});
}

TEST_F(VerifyCardsTest, Guards_DoesNotOfferABattlefieldUnitsCannotBePlayedTo) {
    auto spell = addToHand(P1, kGuards);
    state.battlefields[0].controller = P1;
    state.battlefields[0].blocks_unit_play = true;
    state.battlefields[1].controller = P1;
    addUnit(P1, kInvalidId, 1, 0);                            // a unit holding each
    addUnit(P1, kInvalidId, 1, 1);
    EnginePathAgent agent(EnginePathAgent::script({2}));
    playThroughEngine(spell, &agent);
    ASSERT_EQ(agent.prompts.size(), 1u) << "where; no [Y] for the may";
    EXPECT_TRUE(promptOffersValue(agent, 0, 0)) << "base";
    EXPECT_FALSE(promptOffersValue(agent, 0, 1)) << "units can't be played to battlefield #0";
    EXPECT_TRUE(promptOffersValue(agent, 0, 2));
    ASSERT_EQ(countNamed(P1, "Sand Soldier"), 1);
    EXPECT_EQ(bfOf(firstNamed(P1, "Sand Soldier")), std::optional<BattlefieldId>{1});
}

TEST_F(VerifyCardsTest, Guards_APlayerWhoMayOnlyPlayUnitsToBaseIsOfferedOnlyTheBase) {
    // Mageseeker Warden: "While I'm at a battlefield, opponents can only play
    // units to their base." (The flag is rebuilt from the Warden by every
    // cleanup; it is also set here so the test does not depend on one running.)
    auto spell = addToHand(P1, kGuards);
    state.battlefields[0].controller = P1;
    addUnit(P1, kInvalidId, 1, 0);                            // holds battlefield #0
    addUnit(P2, kMageseekerWarden, 0, 1);
    ps(P1).units_play_base_only = true;
    EnginePathAgent agent(EnginePathAgent::script({1}));
    playThroughEngine(spell, &agent);
    ASSERT_EQ(agent.prompts.size(), 1u);
    EXPECT_EQ(agent.prompts[0].size(), 1u) << "the base, nothing else";
    ASSERT_EQ(countNamed(P1, "Sand Soldier"), 1);
    EXPECT_TRUE(o(firstNamed(P1, "Sand Soldier")).isAtBase());
}

TEST_F(VerifyCardsTest, WildClaw_DoesNotOfferABattlefieldUnitsCannotBePlayedTo) {
    addToDeck(P1, kMeditation);
    auto mech = addToDeck(P1, kMegaMech);
    for (int i = 0; i < 4; ++i) addToDeck(P1, kMeditation);
    auto spell = addToHand(P1, kWildClaw);
    state.battlefields[0].controller = P1;
    state.battlefields[0].blocks_unit_play = true;
    state.battlefields[1].controller = P1;
    addUnit(P1, kInvalidId, 1, 0);                            // a unit holding each
    addUnit(P1, kInvalidId, 1, 1);
    ps(P1).rune_pool.energy = 2;
    // The card, where (0 = base, 1 + i = battlefield i), then "yes" to Empower.
    EnginePathAgent agent(EnginePathAgent::script({static_cast<int>(mech), 2, 1}));
    playThroughEngine(spell, &agent);
    ASSERT_GE(agent.prompts.size(), 2u);
    EXPECT_TRUE(promptOffersValue(agent, 1, 0)) << "base";
    EXPECT_FALSE(promptOffersValue(agent, 1, 1)) << "units can't be played to battlefield #0";
    EXPECT_TRUE(promptOffersValue(agent, 1, 2));
    ASSERT_TRUE(onBoard(mech));
    EXPECT_EQ(bfOf(mech), std::optional<BattlefieldId>{1});
}


// ═══ Tranche 2, group C ═══════════════════════════════════════════════════════
// (already defined in the file and used below: kSett164, kSett543, kRecruit,
//  kShipyardSkulker.)

// ═══ Sweep C, item 1: option lists are offered in object-id order ═══════════

// Thirteen cards built the list they hand to pickTarget / pickTargetPair by
// walking GameState::objects (an unordered_map) and offered it as it came:
// the order of the options — what an agent sees at each index, and the
// default first pick — was hash order. Each now sorts its list.
//
// NOTE: this pins the sort; it cannot force the old code to fail. Hash order
// is not under a test's control, so the unsorted code passes whenever the map
// happens to iterate in ascending id order. To make that unlikely each board
// has nine or more candidates-and-bystanders created interleaved (enemy,
// friend, gear, enemy, ...), every round adds to the same map (so it rehashes
// along the way), and thirteen cards are checked. (libstdc++ walks freshly
// inserted integer keys most-recent-first, so in practice the old code fails
// every round — but that is an implementation detail, not a guarantee.)
TEST_F(VerifyCardsTest, OptionLists_AreOfferedInAscendingObjectIdOrder) {
    enum class Kind { Unit, Battlefield, Gear };
    struct Case { CardDefId id; TriggerType trigger; Kind kind; };
    const Case cases[] = {
        {234, TriggerType::WhenYouPlayMe, Kind::Unit},           // Harnessed Dragon
        {238, TriggerType::WhenIAttack, Kind::Unit},             // Leona, Determined
        {330, TriggerType::WhenYouPlayMe, Kind::Unit},           // Gem Jammer
        {361, TriggerType::WhenIMoveToFB, Kind::Unit},           // Ribbon Dancer
        {453, TriggerType::WhenYouPlayMe, Kind::Unit},           // Beast Below (a pair)
        {479, TriggerType::WhenYouPlayMe, Kind::Unit},           // Sandshifter
        {539, TriggerType::WhenIAttackOrDefend, Kind::Unit},     // Ahri, Inquisitive
        {589, TriggerType::WhenIConquer, Kind::Unit},            // Inviolus Vox
        {618, TriggerType::WhenIAttackOrDefend, Kind::Unit},     // Yuumi, Magical Cat
        {278, TriggerType::WhenYouHoldHere, Kind::Battlefield},  // Navori Fighting Pit
        {535, TriggerType::WhenYouConquerHere, Kind::Battlefield},  // Veiled Temple (gear)
        {763, TriggerType::WhenYouHoldHere, Kind::Battlefield},  // Amateur Recital
        {63, TriggerType::WhenYouPlayThis, Kind::Gear},          // Spirit's Refuge
    };
    for (const Case& c : cases) {
        SCOPED_TRACE("card " + std::to_string(c.id) + " " + card_db.get(c.id).name);
        // Everything at battlefield #0 (the gear in base), interleaved, with
        // the source in the middle. Shipyard Skulker: 3 [M], no text.
        std::vector<GameObjectId> made;
        GameObjectId source = kInvalidId;
        for (int i = 0; i < 4; ++i) {
            made.push_back(addUnit(P2, kShipyardSkulker, 0, 0));
            made.push_back(addUnit(P1, kShipyardSkulker, 0, 0));
            if (i < 3) made.push_back(addGear(P1, kInvalidId));
            if (i == 1) {
                if (c.kind == Kind::Unit) source = addUnit(P1, c.id, 0, 0);
                else if (c.kind == Kind::Gear) source = addGear(P1, c.id);
                else source = battlefieldCard(0, c.id, P1);
                made.push_back(source);
            }
        }

        AcceptingEnginePathAgent agent;               // yes to a "may", then the first option
        fireTriggerThroughEngine(source, c.trigger, &agent);

        int object_prompts = 0;
        for (const auto& prompt : agent.prompts) {
            if (prompt.empty() || prompt.front().chosen_objects.empty()) continue;   // a "may"
            ++object_prompts;
            EXPECT_GE(prompt.size(), 3u) << "at least three candidates";
            for (size_t i = 1; i < prompt.size(); ++i) {
                ASSERT_FALSE(prompt[i].chosen_objects.empty());
                EXPECT_LT(prompt[i - 1].chosen_objects.front(), prompt[i].chosen_objects.front())
                    << "option " << i << " is out of object-id order";
            }
        }
        EXPECT_GE(object_prompts, 1) << "the card never asked which object";

        // Clear the board for the next card (the objects stay in the map).
        for (auto id : made) {
            o(id).location = std::nullopt;
            o(id).zone = ZoneType::Banishment;
        }
        for (auto p : {P1, P2}) {
            ps(p).hand.clear();
            ps(p).trash.clear();
        }
        state.battlefields[0].card_object_id = kInvalidId;
        state.battlefields[0].controller = std::nullopt;
    }
}

// ═══ Sweep C, item 2: Soul Sword 601 — the Level 3 bonus needs both on board ═

TEST_F(VerifyCardsTest, SoulSword_OffTheBoardGearOrBearerGivesNoLevelBonus) {
    ps(P1).xp = 3;                                    // [Level 3][>] is on
    // Control: attached and both on the board — +1 [M] printed, +1 [M] more.
    auto bearer = addUnit(P1, kShipyardSkulker, 2);
    auto sword = addGear(P1, kSoulSword);
    attach(sword, bearer);
    // A Soul Sword that left the board with `attached_to` still set.
    auto kept = addUnit(P1, kShipyardSkulker, 2);
    auto gone_sword = addGear(P1, kSoulSword);
    o(gone_sword).attached_to = kept;
    sendToTrash(gone_sword);
    // A Soul Sword on the board whose bearer left it.
    auto gone_bearer = addUnit(P1, kShipyardSkulker, 2);
    auto orphan = addGear(P1, kSoulSword);
    o(orphan).attached_to = gone_bearer;
    sendToTrash(gone_bearer);

    cleanup();

    EXPECT_EQ(o(bearer).current_might, 4) << "2 + the Equipment's +1 + the Level 3 +1";
    EXPECT_EQ(o(kept).aura_might_bonus, 0) << "a Soul Sword in the trash gives nothing";
    EXPECT_EQ(o(kept).current_might, 2);
    EXPECT_TRUE(o(gone_bearer).aura_effects.empty()) << "a card in the trash is not a bearer";
    EXPECT_EQ(o(gone_bearer).aura_might_bonus, 0);
}

// ═══ Sweep C, item 3: Spirit's Refuge 63 next to Allay 603 ══════════════════

// "Friendly buffed units have [Deflect] if they didn't already." The Refuge
// can only see the [Deflect] that exists when ITS hook runs (printed, "this
// turn", Equipment, hooks that ran earlier in the pass). The hooks run in
// GameState::objects order, so next to Allay ("your other units here have
// [Deflect]") the buffed unit ends with ONE Deflect effect if Allay's hook ran
// first and TWO if the Refuge's did — and EffectExecutor::deflectCostFor sums
// them. The right answer is 1 [A]; that needs an engine hook (see the comment
// in gear/0063_spirit_s_refuge.cpp) and is NOT fixed. This test shows the
// behaviour as it is: once the engine orders the two, replace the 1-or-2
// expectations with EXPECT_EQ(..., 1).
TEST_F(VerifyCardsTest, SpiritsRefuge_NextToAllayTheDeflectCostDependsOnHookOrder) {
    state.battlefields[0].controller = P1;
    auto refuge = addGear(P1, kSpiritsRefuge);
    auto allay = addUnit(P1, kAllay, 0, 0);
    auto both = addUnit(P1, kShipyardSkulker, 2, 0);  // buffed, next to Allay
    o(both).buff_count = 1;
    auto plain = addUnit(P1, kShipyardSkulker, 2, 0); // next to Allay, no buff
    auto solo = addUnit(P1, kShipyardSkulker, 2);     // buffed, in base: the Refuge only
    o(solo).buff_count = 1;

    cleanup();

    auto deflectEffects = [&](GameObjectId id, GameObjectId from) {
        int n = 0;
        for (const auto& ae : o(id).aura_effects)
            if (ae.keyword == Keyword::Deflect && (from == kInvalidId || ae.source == from)) ++n;
        return n;
    };
    // Order-independent parts.
    EXPECT_EQ(deflectEffects(solo, kInvalidId), 1);
    EXPECT_EQ(deflectEffects(solo, refuge), 1);
    EXPECT_EQ(deflectCostFor(state, P2, solo), 1);
    EXPECT_EQ(deflectEffects(plain, kInvalidId), 1);
    EXPECT_EQ(deflectEffects(plain, allay), 1);
    EXPECT_EQ(deflectCostFor(state, P2, plain), 1);
    EXPECT_EQ(deflectCostFor(state, P2, allay), 1) << "printed; the Refuge adds nothing";
    EXPECT_EQ(deflectEffects(allay, kInvalidId), 0);

    // The unit both cards reach: Allay's [Deflect] always, the Refuge's only
    // if its hook ran before Allay's — and the cost is their sum.
    EXPECT_EQ(deflectEffects(both, allay), 1);
    const int from_refuge = deflectEffects(both, refuge);
    EXPECT_GE(from_refuge, 0);
    EXPECT_LE(from_refuge, 1);
    EXPECT_EQ(deflectEffects(both, kInvalidId), 1 + from_refuge);
    EXPECT_EQ(deflectCostFor(state, P2, both), 1 + from_refuge)
        << "CURRENT BEHAVIOUR: 1 or 2 by hook order; the card text says 1";
}

// ═══ Sweep C, item 4: what a card does once its object has left the board ═══

// 242 Baited Hook: the unit to kill left the board in response — it is not
// killed, so there is no Might to dig against: no look, no unit played.
TEST_F(VerifyCardsTest, BaitedHook_TheVictimLeftInResponse_NoDigAndNoUnitPlayed) {
    auto hook = addGear(P1, kBaitedHook);
    ps(P1).rune_pool.energy = 1;
    power(P1, Domain::Order) = 1;
    auto victim = addUnit(P1, kInvalidId, 1);
    addToDeck(P1, kInvalidId);
    auto eligible = addToDeck(P1, kRecruit);
    o(eligible).base_might = 1;
    addToDeck(P1, kInvalidId);
    const auto deck_before = ps(P1).main_deck;

    Intent act;
    act.type = IntentType::ActivateAbility;
    act.player = P1;
    act.ability_source = hook;
    act.targets = {victim};
    EnginePathAgent agent(EnginePathAgent::choosing(eligible));
    executeAndProbe(act, &agent, [&](GameState& live) {
        EXPECT_TRUE(live.getObject(hook).is_exhausted) << "activated and paid for";
        removeFromBoard(live, victim);
    });

    EXPECT_EQ(power(P1, Domain::Order), 0);
    EXPECT_TRUE(agent.prompts.empty()) << "nothing is looked at, nothing is offered";
    EXPECT_FALSE(agent.wasOffered(eligible));
    EXPECT_TRUE(inDeck(P1, eligible));
    EXPECT_FALSE(onBoard(eligible));
    EXPECT_EQ(ps(P1).main_deck, deck_before) << "nothing recycled either";
    EXPECT_EQ(ps(P1).cards_banished_this_turn, 0);
    EXPECT_EQ(std::count(ps(P1).trash.begin(), ps(P1).trash.end(), victim), 1)
        << "not killed a second time";
}

// 490 Altar of Memories: the Altar is gone when its trigger is finalized —
// "you may exhaust me" cannot be paid, so the may is not offered.
TEST_F(VerifyCardsTest, AltarOfMemories_OffTheBoardIsNotOfferedAndDrawsNothing) {
    auto altar = addGear(P1, kAltarOfMemories);
    auto dead = addUnit(P1, kInvalidId, 1);
    sendToTrash(dead);
    auto in_hand = addToHand(P1, kInvalidId);
    addToDeck(P1, kInvalidId);
    addToDeck(P1, kInvalidId);
    sendToTrash(altar);                               // left the board with the unit

    EnginePathAgent yes(EnginePathAgent::script({1}));
    fireTriggerThroughEngine(altar, TriggerType::WhenAFriendlyUnitDies, &yes, dead);

    EXPECT_TRUE(yes.prompts.empty()) << "an Altar in the trash cannot be exhausted";
    EXPECT_FALSE(o(altar).is_exhausted);
    EXPECT_EQ(handSize(P1), 1);
    EXPECT_TRUE(inHand(P1, in_hand));
    EXPECT_EQ(deckSize(P1), 2) << "nothing drawn, nothing put back";
}

// 864 Tools of Empire: the chosen unit left the board in response.
TEST_F(VerifyCardsTest, ToolsOfEmpire_AUnitThatLeftInResponseGetsNoMight) {
    auto tools = addGear(P1, kToolsOfEmpire);
    auto unit = addUnit(P1, kShipyardSkulker, 2);
    Intent act;
    act.type = IntentType::ActivateAbility;
    act.player = P1;
    act.ability_source = tools;
    act.targets = {unit};
    executeAndProbe(act, nullptr, [&](GameState& live) {
        EXPECT_TRUE(live.getObject(tools).is_exhausted) << "activated";
        EXPECT_EQ(live.getObject(unit).temp_might_bonus, 0) << "not resolved yet";
        removeFromBoard(live, unit);
    });
    EXPECT_EQ(o(unit).temp_might_bonus, 0) << "a card in the trash gets no Might";
    EXPECT_EQ(o(unit).current_might, 2);

    // Control: the same activation with the unit still there gives +2 [M].
    auto tools2 = addGear(P1, kToolsOfEmpire);
    auto unit2 = addUnit(P1, kShipyardSkulker, 2);
    act.ability_source = tools2;
    act.targets = {unit2};
    executeAndProbe(act, nullptr, [](GameState&) {});
    EXPECT_EQ(o(unit2).temp_might_bonus, 2);
}

// 157 Udyr, Wildman: "Spend my buff" is the cost, and it is Udyr's own buff.
// An Udyr that is not on the board (a stale or hand-built activation; the
// generator never offers one) pays nothing, and then nothing happens — the
// chosen mode does not resolve for free.
TEST_F(VerifyCardsTest, Udyr_OffTheBoardSpendsNoBuffAndDoesNothing) {
    auto udyr = addUnit(P1, kUdyrWildman, 0, 0);
    o(udyr).buff_count = 1;
    auto enemy = addUnit(P2, kShipyardSkulker, 0, 0);
    sendToTrash(udyr);
    Intent act;
    act.type = IntentType::ActivateAbility;
    act.player = P1;
    act.ability_source = udyr;
    EnginePathAgent agent(EnginePathAgent::script({0, static_cast<int>(enemy)}));   // Deal 2
    withEngine([&](GameEngine& e) { e.testHook_executeIntent(act); }, &agent);
    EXPECT_EQ(o(udyr).buff_count, 1) << "the buff of a card in the trash is not spent";
    EXPECT_EQ(o(udyr).card_counters.count("__udyr_modes_mask"), 0u) << "no mode used up";
    EXPECT_EQ(o(enemy).damage_marked, 0) << "no cost paid: no damage";
    EXPECT_TRUE(onBoard(enemy));
    EXPECT_FALSE(onBoard(udyr));
}

// ...and an Udyr that leaves in response, after the buff was spent: "Ready
// me" and "Give me [Ganking]" do nothing to the card in the trash.
TEST_F(VerifyCardsTest, Udyr_LeftInResponse_IsNeitherReadiedNorGivenGanking) {
    for (int mode : {2, 3}) {                         // Ready me / Give me [Ganking]
        SCOPED_TRACE("mode " + std::to_string(mode));
        auto udyr = addUnit(P1, kUdyrWildman);
        o(udyr).buff_count = 1;
        o(udyr).is_exhausted = true;
        Intent act;
        act.type = IntentType::ActivateAbility;
        act.player = P1;
        act.ability_source = udyr;
        EnginePathAgent agent(EnginePathAgent::script({mode}));
        executeAndProbe(act, &agent, [&](GameState& live) {
            EXPECT_EQ(live.getObject(udyr).buff_count, 0) << "spent as the ability was activated";
            removeFromBoard(live, udyr);
        });
        ASSERT_EQ(agent.prompts.size(), 1u) << "the mode";
        EXPECT_TRUE(o(udyr).is_exhausted) << "a card in the trash is not readied";
        EXPECT_FALSE(o(udyr).keywords.has(Keyword::Ganking));
        EXPECT_FALSE(o(udyr).temp_keywords.has(Keyword::Ganking));
        EXPECT_FALSE(onBoard(udyr));
        EXPECT_EQ(o(udyr).zone, ZoneType::Trash);
    }
}

// 164 / 543 Sett, Brawler: activated (the buff is spent), then he leaves the
// board in response — the card in the trash gets no +4 [M].
TEST_F(VerifyCardsTest, SettBrawler_LeftInResponse_GetsNoMightInTheTrash) {
    for (CardDefId def_id : {kSett164, kSett543}) {
        SCOPED_TRACE("card " + std::to_string(def_id));
        auto sett = addUnit(P1, def_id);
        o(sett).buff_count = 1;
        Intent act;
        act.type = IntentType::ActivateAbility;
        act.player = P1;
        act.ability_source = sett;
        executeAndProbe(act, nullptr, [&](GameState& live) {
            EXPECT_EQ(live.getObject(sett).buff_count, 0) << "spent as the ability was activated";
            EXPECT_EQ(live.getObject(sett).temp_might_bonus, 0) << "not resolved yet";
            removeFromBoard(live, sett);
        });
        EXPECT_EQ(o(sett).temp_might_bonus, 0) << "no +4 [M] on a card in the trash";
        EXPECT_FALSE(onBoard(sett));
        EXPECT_EQ(o(sett).zone, ZoneType::Trash);

        // Control: he stays, he gets the +4 [M].
        auto stays = addUnit(P1, def_id);
        o(stays).buff_count = 1;
        act.ability_source = stays;
        executeAndProbe(act, nullptr, [](GameState&) {});
        EXPECT_EQ(o(stays).temp_might_bonus, 4);
        EXPECT_EQ(o(stays).buff_count, 0);
        o(stays).location = std::nullopt;             // off the board for the next printing
        o(stays).zone = ZoneType::Banishment;
    }
}

// ═══ Sweep C, item 5: 108 Convergent Mutation — what keeps it off the trash ═

// The chosen unit leaves the board in response. EffectExecutor::
// giveTemporaryMight is NOT what protects the card in the trash (it only
// checks objectExists and would add the Might): GameEngine::resolveSpell
// re-validates the play-time target against Card::enumerateLegalTargets (a
// friendly unit ON THE BOARD) and drops it, so onResolve gets no target and
// returns before it asks which other unit to copy.
TEST_F(VerifyCardsTest, Pin_Batch08_ConvergentMutation108_AUnitThatLeftInResponseGetsNoMight) {
    auto chosen = addUnit(P1, kShipyardSkulker, 1);
    auto big = addUnit(P1, kShipyardSkulker, 4);
    auto spell = addToHand(P1, kConvergentMutation);
    const auto& def = card_db.get(kConvergentMutation);
    ps(P1).rune_pool.energy += def.energy_cost;
    ps(P1).rune_pool.universal_power += def.power_cost;
    Intent play;
    play.type = IntentType::PlayCard;
    play.player = P1;
    play.card = spell;
    play.targets = {chosen};
    EnginePathAgent agent;
    executeAndProbe(play, &agent, [&](GameState& live) {
        EXPECT_EQ(live.getObject(chosen).temp_might_bonus, 0) << "not resolved yet";
        removeFromBoard(live, chosen);
    });
    EXPECT_FALSE(inHand(P1, spell)) << "the spell was played";
    EXPECT_TRUE(agent.prompts.empty()) << "no target left: the other unit is never asked for";
    EXPECT_EQ(o(chosen).temp_might_bonus, 0) << "a card in the trash gets no Might";
    EXPECT_EQ(o(chosen).current_might, 1);
    EXPECT_EQ(o(big).temp_might_bonus, 0);

    // Control: the same play with the unit still there raises it to 4 [M].
    auto chosen2 = addUnit(P1, kShipyardSkulker, 1);
    auto spell2 = addToHand(P1, kConvergentMutation);
    ps(P1).rune_pool.energy += def.energy_cost;
    ps(P1).rune_pool.universal_power += def.power_cost;
    play.card = spell2;
    play.targets = {chosen2};
    executeAndProbe(play, nullptr, [](GameState&) {});
    EXPECT_EQ(o(chosen2).temp_might_bonus, 3);
    EXPECT_EQ(o(chosen2).current_might, 4);
}

// ═══ Tranche 3: "opponents can only play units to their base" (Mageseeker
// Warden) — the other half of the play-location filter in 704, 515 and 876.
// The flag is rebuilt from the Warden by every cleanup; it is also set here
// so the tests do not depend on one running.

TEST_F(VerifyCardsTest, HeedlessResurrection_APlayerWhoMayOnlyPlayUnitsToBaseIsOfferedOnlyTheBase) {
    auto spell = addToHand(P1, kHeedlessResurrection);
    auto victim = addUnit(P1, kShipyardSkulker, 0);
    auto corpse = addToTrash(P1, kShipyardSkulker);
    state.battlefields[0].controller = P1;
    addUnit(P1, kInvalidId, 1, 0);                            // holds battlefield #0
    addUnit(P2, kMageseekerWarden, 0, 1);
    ps(P1).units_play_base_only = true;
    EnginePathAgent agent(EnginePathAgent::script(
        {static_cast<int>(victim), static_cast<int>(corpse), 1}));
    playThroughEngine(spell, &agent);
    ASSERT_FALSE(agent.prompts.empty());
    EXPECT_FALSE(promptOffersValue(agent, agent.prompts.size() - 1, 1))
        << "battlefield #0 is controlled, but units may only go to the base";
    ASSERT_TRUE(onBoard(corpse));
    EXPECT_TRUE(o(corpse).isAtBase());
}

TEST_F(VerifyCardsTest, ArcaneShift_APlayerWhoMayOnlyPlayUnitsToBaseIsOfferedOnlyTheBase) {
    auto spell = addToHand(P1, kArcaneShift);
    auto unit = addUnit(P1, kShipyardSkulker, 0);
    state.battlefields[0].controller = P1;
    addUnit(P1, kInvalidId, 1, 0);                            // holds battlefield #0
    addUnit(P2, kMageseekerWarden, 0, 1);
    ps(P1).units_play_base_only = true;
    EnginePathAgent agent(EnginePathAgent::script({1, 1, 1}));
    playAt(spell, {unit}, &agent);
    for (size_t n = 0; n < agent.prompts.size(); ++n)
        for (const auto& i : agent.prompts[n])
            if (i.chosen_objects.empty()) {
                EXPECT_TRUE(!i.chosen_value.has_value() || *i.chosen_value == 0)
                    << "a battlefield was offered as a place to play the unit";
            }
    ASSERT_TRUE(onBoard(unit));
    EXPECT_TRUE(o(unit).isAtBase());
}

TEST_F(VerifyCardsTest, WildClaw_APlayerWhoMayOnlyPlayUnitsToBaseIsOfferedOnlyTheBase) {
    addToDeck(P1, kMeditation);
    auto mech = addToDeck(P1, kMegaMech);
    for (int i = 0; i < 4; ++i) addToDeck(P1, kMeditation);
    auto spell = addToHand(P1, kWildClaw);
    state.battlefields[0].controller = P1;
    addUnit(P1, kInvalidId, 1, 0);                            // holds battlefield #0
    addUnit(P2, kMageseekerWarden, 0, 1);
    ps(P1).units_play_base_only = true;
    ps(P1).rune_pool.energy = 2;
    // The card; then "battlefield #0" if a place is asked (it must not be).
    int asked = 0;
    bool bf_offered = false;
    EnginePathAgent agent([&](const std::vector<Intent>& legal) {
        if (asked++ == 0) {
            for (const auto& i : legal)
                if (!i.chosen_objects.empty() && i.chosen_objects.front() == mech) return i;
            return legal.front();
        }
        for (const auto& i : legal)
            if (i.choice_label.find("battlefield") != std::string::npos) { bf_offered = true; return i; }
        return legal.front();
    });
    playThroughEngine(spell, &agent);
    EXPECT_FALSE(bf_offered) << "units may only go to the base";
    ASSERT_TRUE(onBoard(mech));
    EXPECT_TRUE(o(mech).isAtBase());
}

// ═══ Tranche 3: plans/09 section G ═══════════════════════════════════════════
/* REPLACES AN EXISTING TEST
   Delete VerifyCardsTest.Pin_FaePorter_GoneBeforeResolutionMovesNobodyAndThePowerStaysSpent
   (and its three-line comment): it pins the reading 9.G2 overturns and fails
   on the fixed card. FaePorter_GoneBeforeResolution_... below takes its place.
*/

// ═══ 9.G2: Fae Porter 446 — "the same battlefield" is the trigger's ═════════
// "When I move to a battlefield, you may pay [P] to move a unit you control to
// the same battlefield." The battlefield comes from the trigger condition, so
// it is fixed when the condition is fulfilled (CR 359.3.f.3), not read off the
// Porter as the ability resolves.
TEST_F(VerifyCardsTest, FaePorter_MovedOnInResponse_TheUnitGoesToTheBattlefieldOfTheTrigger) {
    auto porter = addUnit(P1, kFaePorter, 0, 0);
    auto ally = addUnit(P1, kShipyardSkulker, 0);
    power(P1, Domain::Chaos) = 1;
    EnginePathAgent agent(EnginePathAgent::choosing(ally));
    fireAndProbe(porter, TriggerType::WhenIMoveToFB, &agent, [&](GameState& live) {
        EXPECT_TRUE(live.getObject(ally).isAtBase()) << "not moved yet";
        // In response the Porter ends up at the other battlefield.
        live.getObject(porter).location = BattlefieldLocation{1};
    });
    EXPECT_EQ(bfOf(porter), std::optional<BattlefieldId>{1});
    EXPECT_EQ(bfOf(ally), std::optional<BattlefieldId>{0})
        << "the battlefield the Porter moved to, not the one it is at now";
}

TEST_F(VerifyCardsTest, FaePorter_BackAtBaseInResponse_TheUnitStillGoesToTheBattlefieldOfTheTrigger) {
    auto porter = addUnit(P1, kFaePorter, 0, 0);
    auto ally = addUnit(P1, kShipyardSkulker, 0);
    power(P1, Domain::Chaos) = 1;
    EnginePathAgent agent(EnginePathAgent::choosing(ally));
    fireAndProbe(porter, TriggerType::WhenIMoveToFB, &agent, [&](GameState& live) {
        auto& p = live.getObject(porter);
        p.location = BaseLocation{P1};
        p.zone = ZoneType::Base;
    });
    EXPECT_TRUE(o(porter).isAtBase());
    EXPECT_EQ(bfOf(ally), std::optional<BattlefieldId>{0});
}

// The CR 359.3.f.3 example: Lillia leaving the board in response "will not
// affect where the Sprite token will be played". Same here.
TEST_F(VerifyCardsTest, FaePorter_GoneBeforeResolution_TheUnitStillGoesToTheBattlefieldOfTheTrigger) {
    auto porter = addUnit(P1, kFaePorter, 0, 0);
    auto ally = addUnit(P1, kShipyardSkulker, 0);
    power(P1, Domain::Chaos) = 1;
    EnginePathAgent agent(EnginePathAgent::choosing(ally));
    fireAndProbe(porter, TriggerType::WhenIMoveToFB, &agent, [&](GameState& live) {
        EXPECT_EQ(live.player(P1).rune_pool.power[static_cast<int>(Domain::Chaos)], 0);
        removeFromBoard(live, porter);                    // in response: the Porter dies
    });
    EXPECT_FALSE(onBoard(porter));
    EXPECT_EQ(power(P1, Domain::Chaos), 0);
    EXPECT_EQ(bfOf(ally), std::optional<BattlefieldId>{0})
        << "the battlefield was noted when the Porter moved there";
}

// A unit that got to that battlefield by itself in the meantime is no longer
// "a unit to move there": a mistarget, and it is not moved anywhere else.
TEST_F(VerifyCardsTest, FaePorter_TheChosenUnitAlreadyAtThatBattlefieldIsNotMovedToThePortersNewOne) {
    auto porter = addUnit(P1, kFaePorter, 0, 0);
    auto ally = addUnit(P1, kShipyardSkulker, 0);
    power(P1, Domain::Chaos) = 1;
    EnginePathAgent agent(EnginePathAgent::choosing(ally));
    fireAndProbe(porter, TriggerType::WhenIMoveToFB, &agent, [&](GameState& live) {
        live.getObject(porter).location = BattlefieldLocation{1};
        auto& a = live.getObject(ally);
        a.location = BattlefieldLocation{0};
        a.zone = ZoneType::BattlefieldZone;
    });
    EXPECT_EQ(bfOf(ally), std::optional<BattlefieldId>{0}) << "not dragged to battlefield #1";
}

// ═══ 9.G3: Guards! 475 from facedown at "Units can't be played here." ═══════
// CR 811.1.d restricts the unit to the battlefield the spell was hidden at;
// Rockfall Path's text forbids playing units there and a rule does not lift a
// card's "can't" (CR 002, 054.1). No location is left: no token (Vendetta FAQ,
// Perched Grimwyrm at Rockfall Path), and nothing to pay [Y] for.
TEST_F(VerifyCardsTest, Guards_HiddenWhereUnitsCannotBePlayed_PlaysNoSandSoldier) {
    state.turn.turn_number = 3;
    auto hide = [&](int bf) {
        auto spell = addToHand(P1, kGuards);
        auto& h = ps(P1).hand;
        h.erase(std::remove(h.begin(), h.end(), spell), h.end());
        auto& card = o(spell);
        card.zone = ZoneType::BattlefieldZone;
        card.is_hidden = true;
        card.hidden_at = static_cast<BattlefieldId>(bf);
        card.hidden_on_turn = 2;
        state.battlefields[bf].facedown.push_back(spell);
        return spell;
    };
    auto playHidden = [&](GameObjectId spell, AgentInterface* agent) {
        Intent play;
        play.type = IntentType::PlayReaction;
        play.player = P1;
        play.card = spell;
        withEngine([&](GameEngine& e) {
            e.testHook_executeIntent(play);
            e.testHook_runChain();
        }, agent);
    };
    state.battlefields[0].controller = P1;
    state.battlefields[0].blocks_unit_play = true;        // Rockfall Path
    state.battlefields[1].controller = P1;

    // The control: hidden at a battlefield units can be played to.
    EnginePathAgent free_agent;
    playHidden(hide(1), &free_agent);
    ASSERT_EQ(countNamed(P1, "Sand Soldier"), 1) << "this setup does play a hidden Guards!";
    EXPECT_EQ(bfOf(firstNamed(P1, "Sand Soldier")), std::optional<BattlefieldId>{1});

    // Hidden at the battlefield units can't be played to.
    power(P1, Domain::Order) = 1;
    EnginePathAgent agent(EnginePathAgent::script({0, 1}));
    playHidden(hide(0), &agent);
    EXPECT_EQ(countNamed(P1, "Sand Soldier"), 1) << "no second token: not there, not anywhere";
    for (const auto& [id, obj] : state.objects)
        if (obj.name == "Sand Soldier" && obj.location.has_value()) {
            EXPECT_NE(obj.battlefieldId(), std::optional<BattlefieldId>{0})
                << "\"Units can't be played here.\"";
        }
    EXPECT_TRUE(agent.prompts.empty()) << "no location to pick, no token to ready";
    EXPECT_EQ(power(P1, Domain::Order), 1) << "[Y] not spent on a token that does not exist";
}

// ═══ 9.G6: Udyr, Wildman 157 — NOT A DEFECT ═════════════════════════════════
// "Deal 2 to a unit at a battlefield." / "Stun a unit at a battlefield." read
// nothing off Udyr. The ability is its own chain item (CR 377.3.a.1); only an
// instruction that checks information about a card that has left is ignored
// (CR 359.3.e.12) — which is the other two modes ("Ready me", "Give me
// [Ganking]", pinned by Udyr_LeftInResponse_IsNeitherReadiedNorGivenGanking).
TEST_F(VerifyCardsTest, Pin_Udyr_LeftInResponse_DealTwoAndStunStillResolve) {
    for (int mode : {0, 1}) {                         // Deal 2 / Stun
        SCOPED_TRACE("mode " + std::to_string(mode));
        auto udyr = addUnit(P1, kUdyrWildman);
        o(udyr).buff_count = 1;
        auto enemy = addUnit(P2, kShipyardSkulker, 0, 0);     // 3 [M]: survives 2
        Intent act;
        act.type = IntentType::ActivateAbility;
        act.player = P1;
        act.ability_source = udyr;
        EnginePathAgent agent(EnginePathAgent::script({mode, static_cast<int>(enemy)}));
        executeAndProbe(act, &agent, [&](GameState& live) {
            EXPECT_EQ(live.getObject(udyr).buff_count, 0) << "spent as the ability was activated";
            EXPECT_EQ(live.getObject(enemy).damage_marked, 0) << "not resolved yet";
            EXPECT_FALSE(live.getObject(enemy).is_stunned) << "not resolved yet";
            removeFromBoard(live, udyr);                      // in response: Udyr leaves
        });
        ASSERT_EQ(agent.prompts.size(), 2u) << "the mode, then its unit";
        EXPECT_FALSE(onBoard(udyr));
        ASSERT_TRUE(onBoard(enemy));
        if (mode == 0) {
            EXPECT_EQ(o(enemy).damage_marked, 2) << "\"Deal 2 to a unit at a battlefield.\"";
            EXPECT_FALSE(o(enemy).is_stunned);
        } else {
            EXPECT_TRUE(o(enemy).is_stunned) << "\"Stun a unit at a battlefield.\"";
            EXPECT_EQ(o(enemy).damage_marked, 0);
        }
    }
}

// ═══ 9.G7: Dusk Rose Lab 765 — banished instead of dying is not killed ══════
// "you may kill a unit you control here to draw 1": the kill is the cost. A
// death replaced by "banish it instead" does not occur (CR 370.1.b,
// 370.1.c.1; the Deathgrip example of CR 359.3.e.14.b), so nothing was paid.
TEST_F(VerifyCardsTest, DuskRoseLab_AUnitBanishedInsteadOfDyingWasNotKilled_NoDraw) {
    auto lab = battlefieldCard(0, kDuskRoseLab);
    addToDeck(P1, kRecruit);
    auto keeper = addUnit(P1, kShipyardSkulker, 0, 0);   // P1 keeps units (and control) here
    auto unit = addUnit(P1, kShipyardSkulker, 0, 0);
    o(unit).banish_if_would_die_this_turn = true;        // Smite: "banish it instead"
    EnginePathAgent agent(EnginePathAgent::choosing(unit));   // yes, then which unit
    fireTriggerThroughEngine(lab, TriggerType::AtStartOfBeginning, &agent);
    EXPECT_FALSE(onBoard(unit));
    EXPECT_TRUE(inBanishment(P1, unit)) << "the replacement applied";
    EXPECT_FALSE(inTrash(P1, unit));
    EXPECT_TRUE(onBoard(keeper)) << "the cost is attempted once";
    EXPECT_EQ(handSize(P1), 0) << "banished, not killed: no draw";
    EXPECT_EQ(deckSize(P1), 1);
}

// The same for a token, which is in the same place banished or dead (CR 183.1).
TEST_F(VerifyCardsTest, DuskRoseLab_ATokenBanishedInsteadOfDyingWasNotKilled_NoDraw) {
    auto lab = battlefieldCard(0, kDuskRoseLab);
    addToDeck(P1, kRecruit);
    auto keeper = addUnit(P1, kShipyardSkulker, 0, 0);
    auto token = addUnit(P1, kShipyardSkulker, 0, 0);
    o(token).is_token = true;
    o(token).banish_if_would_die_this_turn = true;
    EnginePathAgent agent(EnginePathAgent::choosing(token));
    fireTriggerThroughEngine(lab, TriggerType::AtStartOfBeginning, &agent);
    EXPECT_FALSE(onBoard(token));
    EXPECT_TRUE(onBoard(keeper));
    EXPECT_EQ(handSize(P1), 0) << "banished, not killed: no draw";
}

// ...and a token that does die pays the cost like any unit.
TEST_F(VerifyCardsTest, DuskRoseLab_KillingATokenPaysTheCost) {
    auto lab = battlefieldCard(0, kDuskRoseLab);
    addToDeck(P1, kRecruit);
    auto keeper = addUnit(P1, kShipyardSkulker, 0, 0);
    auto token = addUnit(P1, kShipyardSkulker, 0, 0);
    o(token).is_token = true;
    EnginePathAgent agent(EnginePathAgent::choosing(token));
    fireTriggerThroughEngine(lab, TriggerType::AtStartOfBeginning, &agent);
    EXPECT_FALSE(onBoard(token));
    EXPECT_TRUE(onBoard(keeper));
    EXPECT_EQ(handSize(P1), 1) << "killed: draw 1";
}

// ═══ 9.G9: Elder Dragon 680 — its own Deflect filter asks ignoresDeflect() ══
// A pin: Elder Dragon's text has no "ignore [Deflect]", so ignoresDeflect()
// is false and [Deflect] (CR 809) is charged — a unit whose cost can't be paid
// is not offered. (The card's filter now reads the same hook
// Card::offerableChoices does; no card path makes it true for Elder Dragon.)
TEST_F(VerifyCardsTest, Pin_ElderDragon_DoesNotIgnoreDeflect) {
    const Card* card = card_registry.get(kElderDragon);
    ASSERT_NE(card, nullptr);
    EXPECT_FALSE(card->ignoresDeflect()) << "nothing in its text says so";
    auto dragon = addUnit(P1, kElderDragon, 0, 1);
    auto plain = addUnit(P2, kShipyardSkulker, 0);        // in P2's base
    auto deflect = addUnit(P2, kIreliaFervent, 0, 0);     // [Deflect]: [A] to choose
    // P1 has no Power and no rune: the [A] can't be paid.
    EnginePathAgent agent(EnginePathAgent::script({static_cast<int>(plain)}));
    fireTriggerThroughEngine(dragon, TriggerType::WhenYouPlayMe, &agent);
    ASSERT_EQ(agent.prompts.size(), 1u) << "only the location with a unit that can be chosen";
    EXPECT_TRUE(agent.wasOffered(plain));
    EXPECT_FALSE(agent.wasOffered(deflect)) << "its [Deflect] can't be paid";
    EXPECT_TRUE(!onBoard(plain) || o(plain).damage_marked >= 1) << "\"Deal 1 to them\"";
    ASSERT_TRUE(onBoard(deflect));
    EXPECT_EQ(o(deflect).damage_marked, 0);
}

}  // namespace
}  // namespace test_review_fix_verify_cards
}  // namespace riftbound::test
