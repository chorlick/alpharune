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
constexpr CardDefId kVayne035 = 35;
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

}  // namespace
}  // namespace test_review_fix_verify_cards
}  // namespace riftbound::test
