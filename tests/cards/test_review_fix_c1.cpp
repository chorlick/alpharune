/// @file test_review_fix_c1.cpp
/// Regression tests for the 2026-10 card review, stream c1: gear 0388.. and
/// legends ..0928 (plans/cards/batch-01.md and the lines of plans 03, 07, 08
/// and batch-08 that name those cards). Behaviour is tested through the real
/// engine path — an activation, a play, or the event that triggers the
/// ability — with an agent that answers the prompts.

#include "tests/cards/card_test_fixture.h"
#include "cards/card_helpers.h"

#include <algorithm>
#include <string>
#include <vector>

namespace test_review_fix_c1 {

using namespace riftbound;
using namespace riftbound::test;

// Gear
constexpr CardDefId kExperimentalHexplate = 396;
constexpr CardDefId kTemporalPortal = 401;
constexpr CardDefId kHextechAnomaly = 405;
constexpr CardDefId kZeroDrive = 412;
constexpr CardDefId kPetriciteMonument = 426;
constexpr CardDefId kEdgeOfNight = 460;
constexpr CardDefId kLastRites = 471;
constexpr CardDefId kEyeOfTheHerald = 474;
constexpr CardDefId kVanguardArmory = 489;
constexpr CardDefId kAltarOfMemories = 490;
constexpr CardDefId kBladeOfTheRuinedKing = 498;
constexpr CardDefId kSpinningAxe = 504;
constexpr CardDefId kRabadonsDeathcrown = 508;
constexpr CardDefId kShurelyasRequiem = 509;
constexpr CardDefId kSealOfStrength = 542;
constexpr CardDefId kGold564 = 564;
constexpr CardDefId kForgottenSignpost = 607;
constexpr CardDefId kHoneyfruit = 611;
constexpr CardDefId kGutterPalace = 650;
constexpr CardDefId kBloodRose = 671;
constexpr CardDefId kBlastCone = 695;
constexpr CardDefId kScryersBloom = 698;
constexpr CardDefId kTheList = 700;
constexpr CardDefId kCursedSarcophagus = 710;
constexpr CardDefId kHextechGauntlets = 748;
// Legends
constexpr CardDefId kLooseCannon = 249;
constexpr CardDefId kNineTailedFox = 252;
constexpr CardDefId kUnforgiven = 256;
constexpr CardDefId kTheBoss = 264;
constexpr CardDefId kRelentlessStorm = 295;
constexpr CardDefId kHandOfNoxus = 296;
constexpr CardDefId kRadiantDawn = 297;
constexpr CardDefId kDarkChild = 315;
constexpr CardDefId kLadyOfLuminosity = 319;
constexpr CardDefId kMightOfDemacia = 321;
constexpr CardDefId kProdigalExplorer = 514;
constexpr CardDefId kBattleMistress = 517;
constexpr CardDefId kGrandmasterAtArms = 554;
constexpr CardDefId kChemBaroness = 556;
constexpr CardDefId kPridestalker = 744;
constexpr CardDefId kGreenFather = 753;
constexpr CardDefId kKeeperOfTheHammer = 759;
constexpr CardDefId kVirtuoso = 782;
constexpr CardDefId kPiltoverEnforcer = 783;
// Props
constexpr CardDefId kPlayfulPhantom = 49;     // vanilla unit, [5], 5 [M]
constexpr CardDefId kShipyardSkulker = 175;   // vanilla unit, [3], 3 [M]
constexpr CardDefId kSkySplitter = 14;        // spell, costs [5] or more
constexpr CardDefId kGold326 = 326;           // the Gold token's card
constexpr CardDefId kBFSword = 482;           // Equipment, +3 [M]
constexpr CardDefId kDoransBlade = 417;       // Equipment, +2 [M]

class ReviewFixC1Test : public CardTestFixture {
protected:
    /// A gear card on the board (base, or battlefield `at_bf`).
    GameObjectId addGear(PlayerId owner, CardDefId def_id, int at_bf = -1) {
        auto id = state.createObject();
        auto& g = state.getObject(id);
        const auto& def = card_db.get(def_id);
        g.owner = g.controller = owner;
        g.card_def_id = def_id;
        g.card_type = CardType::Gear;
        g.name = def.name;
        g.super_type = def.super_type;
        g.keywords = def.keywords;
        g.domains = def.domains;
        g.tags = def.tags;
        g.might_bonus = def.might_bonus;
        if (at_bf >= 0) {
            g.zone = ZoneType::BattlefieldZone;
            g.location = BattlefieldLocation{static_cast<BattlefieldId>(at_bf)};
        } else {
            g.zone = ZoneType::Base;
            g.location = BaseLocation{owner};
        }
        return id;
    }
    /// `def_id` as `owner`'s legend.
    GameObjectId addLegend(PlayerId owner, CardDefId def_id) {
        auto id = state.createObject();
        auto& l = state.getObject(id);
        const auto& def = card_db.get(def_id);
        l.owner = l.controller = owner;
        l.card_def_id = def_id;
        l.card_type = CardType::Legend;
        l.name = def.name;
        l.domains = def.domains;
        l.tags = def.tags;
        l.zone = ZoneType::LegendZone;
        state.player(owner).legend_zone = id;
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
    /// Give battlefield `bf` a card object (what a conquer / hold trigger
    /// names as its subject) and return it.
    GameObjectId bfCard(int bf) {
        auto id = state.createObject();
        auto& o = state.getObject(id);
        o.card_type = CardType::Battlefield;
        o.name = "TestBattlefield";
        state.battlefields[static_cast<size_t>(bf)].card_object_id = id;
        return id;
    }
    /// A unit card in hand with its printed Might (addToHand leaves it at 0).
    GameObjectId unitInHand(PlayerId owner, CardDefId def_id) {
        auto id = addToHand(owner, def_id);
        state.getObject(id).base_might = card_db.get(def_id).might;
        state.getObject(id).current_might = card_db.get(def_id).might;
        return id;
    }
    GameObjectId cardInTrash(PlayerId owner, CardDefId def_id) {
        auto id = addToHand(owner, def_id);
        auto& ps = state.player(owner);
        ps.hand.erase(std::remove(ps.hand.begin(), ps.hand.end(), id), ps.hand.end());
        state.getObject(id).zone = ZoneType::Trash;
        ps.trash.push_back(id);
        return id;
    }
    /// Activate ability `index` of `source` the way a player does: the engine
    /// checks and pays the cost, finalizes, and resolves the chain.
    void activate(GameObjectId source, int index, AgentInterface* p1_agent = nullptr,
                  std::vector<GameObjectId> targets = {}, AgentInterface* p2_agent = nullptr) {
        Intent act;
        act.type = IntentType::ActivateAbility;
        act.player = state.getObject(source).controller;
        act.ability_source = source;
        act.ability_index = index;
        act.targets = std::move(targets);
        withEngine([&](GameEngine& e) { e.testHook_executeIntent(act); },
                   p1_agent, p2_agent, act.player);
    }
    /// Whether the engine offers ability `index` of `source` in the main phase.
    bool offered(GameObjectId source, int index) {
        bool found = false;
        const PlayerId player = state.getObject(source).controller;
        withEngine([&](GameEngine& e) {
            for (const auto& i : e.generateLegalActions())
                if ((i.type == IntentType::ActivateAbility ||
                     i.type == IntentType::ActivateActionAbility ||
                     i.type == IntentType::ActivateReactionAbility) &&
                    i.ability_source == source && i.ability_index == index) found = true;
        }, nullptr, nullptr, player);
        return found;
    }
    int countNamed(PlayerId owner, const std::string& name) {
        int n = 0;
        for (const auto& [id, obj] : state.objects)
            if (obj.controller == owner && obj.location.has_value() && obj.name == name) ++n;
        return n;
    }
    bool has(const GameObject& obj, const std::string& tag) {
        return std::find(obj.tags.begin(), obj.tags.end(), tag) != obj.tags.end();
    }
};

// ═══ Gear ═══════════════════════════════════════════════════════════════════

TEST_F(ReviewFixC1Test, ExperimentalHexplate_BearerIsAMechWhileItIsAttached) {
    // batch-01: "I am a Mech." — nothing added the tag.
    auto unit = addUnit(P1, kInvalidId, 2, 0);
    auto plate = addGear(P1, kExperimentalHexplate);
    state.player(P1).rune_pool.power[static_cast<int>(Domain::Mind)] = 1;

    activate(plate, 0, nullptr, {unit});   // Equip [B]
    ASSERT_EQ(state.getObject(plate).attached_to, std::optional<GameObjectId>{unit});
    EXPECT_TRUE(has(state.getObject(unit), "Mech"));

    withEngine([&](GameEngine& e) { e.testHook_executor().unattachGear(plate); });
    EXPECT_FALSE(has(state.getObject(unit), "Mech")) << "only while it is attached";
}

TEST_F(ReviewFixC1Test, ExperimentalHexplate_AttachedByAnotherEffectStillMakesAMech) {
    // Attached without going through onEquip (Weaponmaster, Grandmaster at
    // Arms): the aura pass puts the tag on.
    auto unit = addUnit(P1, kInvalidId, 2, 0);
    auto plate = addGear(P1, kExperimentalHexplate);
    attach(plate, unit);
    withEngine([&](GameEngine& e) { e.testHook_cleanup(); });
    EXPECT_TRUE(has(state.getObject(unit), "Mech"));
}

TEST_F(ReviewFixC1Test, TemporalPortal_CostIsAnyPowerAndTheNextSpellGetsRepeat) {
    // batch-01: the effect only logged, and [A] was coded as Mind power.
    Card* card = card_registry.get(kTemporalPortal);
    EXPECT_TRUE(card->getActivationCost().power_any_domain);

    auto portal = addGear(P1, kTemporalPortal);
    state.player(P1).rune_pool.power[static_cast<int>(Domain::Fury)] = 1;   // not Mind
    activate(portal, 0);

    EXPECT_TRUE(state.getObject(portal).is_exhausted);
    EXPECT_EQ(state.player(P1).rune_pool.totalPower(), 0) << "a Fury power paid the [A]";
    EXPECT_TRUE(state.player(P1).grant_repeat_base_to_next_spell);
}

TEST_F(ReviewFixC1Test, HextechAnomaly_IsAReactionAndReadyRunesCanPayX) {
    // batch-01: Action timing only; X counted floating power and exhausted
    // runes, so three ready runes offered only X = 0.
    Card* card = card_registry.get(kHextechAnomaly);
    EXPECT_TRUE(card->isReactionAbility());

    auto anomaly = addGear(P1, kHextechAnomaly);
    for (int i = 0; i < 3; ++i) addRune(P1, Domain::Mind);
    EnginePathAgent agent(EnginePathAgent::script({2}));   // X = 2
    activate(anomaly, 0, &agent);

    ASSERT_EQ(agent.prompts.size(), 1u);
    EXPECT_EQ(agent.prompts[0].size(), 4u) << "X from 0 to 3";
    EXPECT_EQ(state.player(P1).rune_pool.energy, 2);
    EXPECT_EQ(state.player(P1).rune_deck.size(), 2u) << "two runes recycled for the [A]";
    EXPECT_EQ(state.player(P1).power_spent_this_turn, 2) << "paid with the engine's accounting";
}

TEST_F(ReviewFixC1Test, ZeroDrive_BearerDiesAndItsOwnDeathTriggersFire) {
    // batch-01 (a): as a replacement the bearer never died. Altar of Memories
    // watches for a friendly death.
    auto altar = addGear(P1, kAltarOfMemories);
    auto unit = addUnit(P1, kInvalidId, 3, 0);
    auto drive = addGear(P1, kZeroDrive);
    attach(drive, unit);
    addToDeck(P1, kInvalidId);

    EnginePathAgent yes(EnginePathAgent::script({1}));
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) { exec.killObject(unit); }, &yes);

    EXPECT_TRUE(state.getObject(altar).is_exhausted) << "a friendly unit died";
    EXPECT_EQ(state.getObject(unit).zone, ZoneType::Banishment);
    EXPECT_EQ(state.getObject(drive).tracked_objects.size(), 1u);
}

TEST_F(ReviewFixC1Test, ZeroDrive_ATokenBearerIsNotRecordedOrReplayed) {
    // batch-01 (b): a token that died ceased to exist (CR 183.1).
    auto drive = addGear(P1, kZeroDrive);
    GameObjectId token = kInvalidId;
    withEngine([&](GameEngine& e) {
        token = e.testHook_executor().createToken(P1, CardType::Unit, "Recruit", 1, {"Recruit"},
                                                  KeywordSet{}, BattlefieldLocation{0});
    });
    attach(drive, token);
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) { exec.killObject(token); });

    EXPECT_TRUE(state.getObject(drive).tracked_objects.empty());
    EXPECT_FALSE(card_registry.get(kZeroDrive)->canActivateAbilityAt(state, P1, drive, 0));
}

TEST_F(ReviewFixC1Test, ZeroDrive_ReplayCostsThreeEnergyAndAMindPower) {
    // batch-01 (c): only [3] was charged. (d): the gate looks at the copy
    // that is activating, not the first Zero Drive in the game.
    const auto cost = card_registry.get(kZeroDrive)->getActivationCost();
    EXPECT_EQ(cost.energy, 3);
    EXPECT_EQ(cost.power, 1);
    EXPECT_EQ(cost.power_domain, Domain::Mind);

    auto bearer = addUnit(P2, kInvalidId, 2, 0);
    auto theirs = addGear(P2, kZeroDrive);       // an attached copy the opponent controls
    attach(theirs, bearer);
    auto mine = addGear(P1, kZeroDrive);
    auto banished = cardInTrash(P1, kShipyardSkulker);
    {
        auto& ps = state.player(P1);
        ps.trash.clear();
        state.getObject(banished).zone = ZoneType::Banishment;
        ps.banishment.push_back(banished);
        state.getObject(mine).tracked_objects.push_back(banished);
    }
    EXPECT_TRUE(card_registry.get(kZeroDrive)->canActivateAbilityAt(state, P1, mine, 0));
    EXPECT_FALSE(card_registry.get(kZeroDrive)->canActivateAbilityAt(state, P2, theirs, 0));

    state.player(P1).rune_pool.energy = 3;
    EXPECT_FALSE(offered(mine, 0)) << "no Mind power: [3] alone does not pay it";
    state.player(P1).rune_pool.power[static_cast<int>(Domain::Mind)] = 1;
    activate(mine, 0);

    EXPECT_TRUE(state.getObject(banished).location.has_value()) << "played back";
    EXPECT_EQ(state.getObject(mine).zone, ZoneType::Banishment) << "Banish this";
    EXPECT_EQ(state.player(P1).rune_pool.totalPower(), 0);
}

TEST_F(ReviewFixC1Test, GearDefs_DeflectBelongsToTheUnitsNotTheGear) {
    // batch-01: Petricite Monument / Gutter Palace carried Deflect themselves.
    for (CardDefId id : {kPetriciteMonument, kGutterPalace}) {
        const auto& def = card_db.get(id);
        EXPECT_FALSE(def.keywords.has(Keyword::Deflect)) << def.name;
        EXPECT_EQ(def.deflect_value, 0) << def.name;
    }
}

TEST_F(ReviewFixC1Test, EdgeOfNight_TheControllerChoosesTheUnitHere) {
    // batch-01 S5: attached to the first unit found.
    auto a = addUnit(P1, kInvalidId, 2, 0);
    auto b = addUnit(P1, kInvalidId, 2, 0);
    auto elsewhere = addUnit(P1, kInvalidId, 2, 1);
    auto edge = addGear(P1, kEdgeOfNight, /*at_bf=*/0);

    EnginePathAgent agent(EnginePathAgent::choosing(b));
    fireTriggerThroughEngine(edge, TriggerType::WhenYouPlayThis, &agent);

    EXPECT_TRUE(agent.wasOffered(a));
    EXPECT_FALSE(agent.wasOffered(elsewhere)) << "only units here";
    EXPECT_EQ(state.getObject(edge).attached_to, std::optional<GameObjectId>{b});
}

TEST_F(ReviewFixC1Test, LastRites_ThePlayedUnitIsPaidFor) {
    // batch-01: played with playIgnoringCost ("You still pay its costs.").
    auto bearer = addUnit(P1, kInvalidId, 3, 0);
    auto rites = addGear(P1, kLastRites);
    attach(rites, bearer);
    state.battlefields[0].controller = P1;
    auto in_trash = cardInTrash(P1, kShipyardSkulker);   // costs [3]

    // Can't pay: not even offered.
    EnginePathAgent broke(EnginePathAgent::choosing(in_trash));
    withEngine([&](GameEngine& e) {
        e.testHook_scoreHold(P1, e.mutableState().battlefields[0].id);
        e.testHook_runChain();
    }, &broke);
    EXPECT_TRUE(broke.prompts.empty());
    EXPECT_TRUE(inTrash(P1, in_trash));

    // Can pay: the Energy is spent.
    state.player(P1).battlefields_scored_this_turn.clear();
    state.player(P1).rune_pool.energy = 4;
    EnginePathAgent rich(EnginePathAgent::choosing(in_trash));
    withEngine([&](GameEngine& e) {
        e.testHook_scoreHold(P1, e.mutableState().battlefields[0].id);
        e.testHook_runChain();
    }, &rich);
    EXPECT_TRUE(state.getObject(in_trash).location.has_value());
    EXPECT_FALSE(inTrash(P1, in_trash));
    EXPECT_EQ(state.player(P1).rune_pool.energy, 1) << "its [3] was paid";
}

TEST_F(ReviewFixC1Test, LastRites_EquipRecyclesTheTwoCardsTheControllerChooses) {
    // batch-01 S5 + 02 item 2.15: the last two trash cards were recycled with
    // no choice and no "recycled" announcement.
    auto unit = addUnit(P1, kInvalidId, 3, 0);
    auto rites = addGear(P1, kLastRites);
    auto keep = cardInTrash(P1, kShipyardSkulker);
    auto r1 = cardInTrash(P1, kPlayfulPhantom);
    auto r2 = cardInTrash(P1, kSkySplitter);
    auto also_keep = cardInTrash(P1, kShipyardSkulker);
    state.player(P1).rune_pool.power[static_cast<int>(Domain::Chaos)] = 1;

    int recycled_events = 0;
    auto conn = events.on_object_state_changed.connect(
        [&](const ObjectStateChangedEvent& e) {
            if (e.what_changed == "recycled_main") ++recycled_events;
        });
    EnginePathAgent agent(EnginePathAgent::script({static_cast<int>(r1), static_cast<int>(r2)}));
    activate(rites, 0, &agent, {unit});
    conn.disconnect();

    EXPECT_EQ(state.getObject(rites).attached_to, std::optional<GameObjectId>{unit});
    EXPECT_TRUE(inTrash(P1, keep));
    EXPECT_TRUE(inTrash(P1, also_keep));
    EXPECT_TRUE(inDeck(P1, r1));
    EXPECT_TRUE(inDeck(P1, r2));
    EXPECT_EQ(recycled_events, 1);
}

TEST_F(ReviewFixC1Test, EyeOfTheHerald_TheRecruitEntersExhausted) {
    // batch-01: createToken(..., enter_ready = true); units enter exhausted.
    auto unit = addUnit(P1, kInvalidId, 3, 0);
    auto eye = addGear(P1, kEyeOfTheHerald);
    attach(eye, unit);

    resolveThroughEngine([&](EffectExecutor& exec, GameState&) { exec.moveToBattlefield(unit, 1); });

    GameObjectId recruit = kInvalidId;
    for (const auto& [id, obj] : state.objects)
        if (obj.name == "Recruit" && obj.location.has_value()) recruit = id;
    ASSERT_NE(recruit, kInvalidId);
    EXPECT_TRUE(state.getObject(recruit).is_exhausted);
    EXPECT_EQ(state.getObject(recruit).battlefieldId(), std::optional<BattlefieldId>{1}) << "here";
}

TEST_F(ReviewFixC1Test, VanguardArmory_TokensGoWhereTheControllerPutsThem) {
    // batch-01: all three always went to base.
    auto armory = addGear(P1, kVanguardArmory);
    state.battlefields[1].controller = P1;
    // 0 = base, 1 + i = battlefield i: base, battlefield 1, battlefield 1.
    EnginePathAgent agent(EnginePathAgent::script({0, 2, 2}));
    activate(armory, 0, &agent);

    ASSERT_EQ(agent.prompts.size(), 3u);
    EXPECT_EQ(agent.prompts[0].size(), 2u) << "base or the battlefield you control";
    int at_base = 0, at_bf1 = 0;
    for (const auto& [id, obj] : state.objects) {
        if (obj.name != "Recruit" || !obj.location.has_value()) continue;
        if (obj.isAtBase()) ++at_base;
        else if (obj.battlefieldId() == std::optional<BattlefieldId>{1}) ++at_bf1;
    }
    EXPECT_EQ(at_base, 1);
    EXPECT_EQ(at_bf1, 2);
}

TEST_F(ReviewFixC1Test, VanguardArmory_WithOnlyABaseNothingIsAsked) {
    auto armory = addGear(P1, kVanguardArmory);
    EnginePathAgent agent;
    activate(armory, 0, &agent);
    EXPECT_TRUE(agent.prompts.empty());
    EXPECT_EQ(countNamed(P1, "Recruit"), 3);
}

TEST_F(ReviewFixC1Test, AltarOfMemories_WorksOnEveryDeathNotOnlyTheFirst) {
    // batch-01 / 03 item 3.7: the `__altar_drew` guard was never cleared, so
    // from the second death on "yes" neither exhausted nor drew, but still
    // took a hand card.
    auto altar = addGear(P1, kAltarOfMemories);
    auto first = addUnit(P1, kInvalidId, 1);
    auto second = addUnit(P1, kInvalidId, 1);
    for (int i = 0; i < 4; ++i) addToDeck(P1, kInvalidId);
    addToHand(P1, kInvalidId);

    EnginePathAgent yes1(EnginePathAgent::script({1}));
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) { exec.killObject(first); }, &yes1);
    EXPECT_TRUE(state.getObject(altar).is_exhausted);
    EXPECT_EQ(handSize(P1), 1) << "drew 1, put 1 back";
    EXPECT_EQ(deckSize(P1), 4);

    state.getObject(altar).is_exhausted = false;   // a later turn
    EnginePathAgent yes2(EnginePathAgent::script({1}));
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) { exec.killObject(second); }, &yes2);
    EXPECT_EQ(yes2.prompts.size(), 3u) << "the may, the hand card, top/bottom";
    EXPECT_TRUE(state.getObject(altar).is_exhausted) << "exhausted again";
    EXPECT_EQ(handSize(P1), 1) << "drew 1 again, put 1 back";
    EXPECT_EQ(deckSize(P1), 4);
}

TEST_F(ReviewFixC1Test, AltarOfMemories_AnExhaustedAltarIsNotOffered) {
    auto altar = addGear(P1, kAltarOfMemories);
    state.getObject(altar).is_exhausted = true;
    auto doomed = addUnit(P1, kInvalidId, 1);
    auto in_hand = addToHand(P1, kInvalidId);
    addToDeck(P1, kInvalidId);

    EnginePathAgent yes(EnginePathAgent::script({1}));
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) { exec.killObject(doomed); }, &yes);
    EXPECT_TRUE(yes.prompts.empty());
    EXPECT_TRUE(inHand(P1, in_hand)) << "no card is taken from the hand";
}

TEST_F(ReviewFixC1Test, BladeOfTheRuinedKing_TheControllerChoosesWhichUnitDies) {
    // batch-01: pickTarget outside the chain took the first unit in map order.
    auto bearer = addUnit(P1, kInvalidId, 3, 0);
    auto champion = addUnit(P1, kInvalidId, 6, 0);
    auto fodder = addUnit(P1, kInvalidId, 1, 0);
    auto blade = addGear(P1, kBladeOfTheRuinedKing);
    state.player(P1).rune_pool.power[static_cast<int>(Domain::Order)] = 1;

    EnginePathAgent agent(EnginePathAgent::choosing(fodder));
    activate(blade, 0, &agent, {bearer});

    EXPECT_TRUE(agent.wasOffered(champion));
    EXPECT_FALSE(agent.wasOffered(bearer)) << "not the unit being equipped";
    EXPECT_FALSE(state.getObject(fodder).location.has_value());
    EXPECT_TRUE(state.getObject(champion).location.has_value());
    EXPECT_EQ(state.getObject(blade).attached_to, std::optional<GameObjectId>{bearer});
    EXPECT_EQ(state.player(P1).rune_pool.totalPower(), 0) << "floating [Y] paid it";
}

TEST_F(ReviewFixC1Test, BladeOfTheRuinedKing_NothingIsPaidWithoutThePower) {
    auto bearer = addUnit(P1, kInvalidId, 3, 0);
    auto fodder = addUnit(P1, kInvalidId, 1, 0);
    auto blade = addGear(P1, kBladeOfTheRuinedKing);
    activate(blade, 0, nullptr, {bearer});
    EXPECT_TRUE(state.getObject(fodder).location.has_value());
    EXPECT_FALSE(state.getObject(blade).attached_to.has_value());
}

TEST_F(ReviewFixC1Test, HextechGauntlets_EquipIsPaidWithFloatingResources) {
    // 03 item 3.24: hand-rolled rune recycle; floating Energy / Power ignored.
    auto unit = addUnit(P1, kInvalidId, 1, 0);          // 1 [M]: Energy cost 3 - 1 = 2
    auto gauntlets = addGear(P1, kHextechGauntlets);
    state.player(P1).rune_pool.energy = 2;
    state.player(P1).rune_pool.universal_power = 1;
    activate(gauntlets, 0, nullptr, {unit});
    EXPECT_EQ(state.getObject(gauntlets).attached_to, std::optional<GameObjectId>{unit});
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0);
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 0);
}

TEST_F(ReviewFixC1Test, SpinningAxe_TemporaryOnlyKillsItWhileUnattached) {
    // batch-01: the Beginning Step killed every [Temporary] gear, attached or
    // not ("If this is unattached, kill it ..."; CR 718.2).
    auto draven = addUnit(P1, kInvalidId, 3, 0);
    auto worn = addGear(P1, kSpinningAxe);
    attach(worn, draven);
    auto loose = addGear(P1, kSpinningAxe);

    withEngine([&](GameEngine& e) { e.testHook_beginningStep(); });

    EXPECT_TRUE(state.getObject(worn).location.has_value());
    EXPECT_EQ(state.getObject(worn).attached_to, std::optional<GameObjectId>{draven});
    EXPECT_FALSE(state.getObject(loose).location.has_value());
}

TEST_F(ReviewFixC1Test, RabadonsDeathcrown_GivesThreeBonusDamageWhileAttached) {
    // batch-01: nothing set PlayerState::bonus_damage_dealt.
    auto unit = addUnit(P1, kInvalidId, 3, 0);
    auto crown = addGear(P1, kRabadonsDeathcrown);
    withEngine([&](GameEngine& e) { e.testHook_cleanup(); });
    EXPECT_EQ(state.player(P1).bonus_damage_dealt, 0) << "unattached";

    attach(crown, unit);
    withEngine([&](GameEngine& e) { e.testHook_cleanup(); });
    EXPECT_EQ(state.player(P1).bonus_damage_dealt, 3);
    EXPECT_EQ(state.player(P2).bonus_damage_dealt, 0);
}

TEST_F(ReviewFixC1Test, ShurelyasRequiem_HereIsWhereTheBearerIs) {
    // batch-01: "here" was read from the gear's own location.
    auto bearer = addUnit(P1, kInvalidId, 3, 1);
    auto with_it = addUnit(P1, kInvalidId, 2, 1);
    auto away = addUnit(P1, kInvalidId, 2, 0);
    auto requiem = addGear(P1, kShurelyasRequiem);
    attach(requiem, bearer);
    state.getObject(requiem).location = BattlefieldLocation{0};   // stale

    withEngine([&](GameEngine& e) { e.testHook_cleanup(); });
    EXPECT_TRUE(state.getObject(with_it).hasKeyword(Keyword::Ganking));
    EXPECT_FALSE(state.getObject(away).hasKeyword(Keyword::Ganking));
}

TEST_F(ReviewFixC1Test, SealOfStrength_AddsBodyPower) {
    // batch-01 / 03 item 3.17: [O] is Body; the card added Order.
    auto seal = addGear(P1, kSealOfStrength);
    activate(seal, 0);
    EXPECT_EQ(state.player(P1).rune_pool.power[static_cast<int>(Domain::Body)], 1);
    EXPECT_EQ(state.player(P1).rune_pool.power[static_cast<int>(Domain::Order)], 0);
}

TEST_F(ReviewFixC1Test, Gold564_IsATokenReferenceCard) {
    EXPECT_EQ(card_db.get(kGold564).super_type, SuperType::Token);
}

TEST_F(ReviewFixC1Test, ForgottenSignpost_ExhaustsOneUnitAndMovesADifferentOne) {
    // batch-01 / 03 item 3.1: two pickTarget calls returned the same unit, so
    // the exhausted unit was "moved" to where it already was.
    auto anchor = addUnit(P1, kInvalidId, 2, 1);
    auto mover = addUnit(P1, kInvalidId, 2);          // in base
    auto third = addUnit(P1, kInvalidId, 2, 0);
    auto post = addGear(P1, kForgottenSignpost);

    EnginePathAgent agent(EnginePathAgent::script({static_cast<int>(anchor),
                                                   static_cast<int>(mover)}));
    activate(post, 0, &agent);

    ASSERT_EQ(agent.prompts.size(), 2u);
    EXPECT_TRUE(state.getObject(post).is_exhausted);
    EXPECT_TRUE(state.getObject(anchor).is_exhausted) << "the unit exhausted as the cost";
    EXPECT_FALSE(state.getObject(mover).is_exhausted);
    EXPECT_EQ(state.getObject(mover).battlefieldId(), std::optional<BattlefieldId>{1});
    EXPECT_EQ(state.getObject(anchor).battlefieldId(), std::optional<BattlefieldId>{1});
    EXPECT_EQ(state.getObject(third).battlefieldId(), std::optional<BattlefieldId>{0});
    // The second prompt never offers the unit that was exhausted.
    for (const auto& i : agent.prompts[1])
        EXPECT_NE(i.chosen_objects.front(), anchor);
}

TEST_F(ReviewFixC1Test, ForgottenSignpost_NeedsAReadyUnitAndADifferentOneElsewhere) {
    auto post = addGear(P1, kForgottenSignpost);
    auto only = addUnit(P1, kInvalidId, 2, 1);
    EXPECT_FALSE(offered(post, 0)) << "one unit: nothing to move to it";
    auto other = addUnit(P1, kInvalidId, 2, 1);
    EXPECT_FALSE(offered(post, 0)) << "both already at the same place";
    state.getObject(other).location = BaseLocation{P1};
    state.getObject(other).zone = ZoneType::Base;
    EXPECT_TRUE(offered(post, 0));
    state.getObject(only).is_exhausted = true;
    state.getObject(other).is_exhausted = true;
    EXPECT_FALSE(offered(post, 0)) << "no ready unit to exhaust";
}

TEST_F(ReviewFixC1Test, Honeyfruit_EntersExhaustedAndTheLevelAbilityIsGated) {
    // batch-01 (a) (b) (c).
    Card* card = card_registry.get(kHoneyfruit);
    EXPECT_TRUE(card->entersExhausted());

    auto fruit = addGear(P1, kHoneyfruit);
    setXp(P1, 3);
    EXPECT_FALSE(card->canActivateAbilityAt(state, P1, fruit, 1)) << "needs 6+ XP";
    EXPECT_TRUE(card->canActivateAbilityAt(state, P1, fruit, 0));

    setXp(P1, 6);
    ASSERT_TRUE(card->canActivateAbilityAt(state, P1, fruit, 1));
    activate(fruit, 1);
    EXPECT_EQ(state.player(P1).rune_pool.energy, 1) << "[1]";
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 1) << "[A]";
}

TEST_F(ReviewFixC1Test, Honeyfruit_PlayedFromHandItIsExhausted) {
    auto fruit = addToHand(P1, kHoneyfruit);
    playThroughEngine(fruit);
    ASSERT_TRUE(state.getObject(fruit).location.has_value());
    EXPECT_TRUE(state.getObject(fruit).is_exhausted);
}

TEST_F(ReviewFixC1Test, BloodRose_ReadiesAnyUnit) {
    // batch-08: "Ready a unit." was limited to friendly units — and with no
    // target list of its own the ability was never offered at all.
    auto rose = addGear(P1, kBloodRose);
    auto enemy = addUnit(P2, kInvalidId, 2, 0);
    state.getObject(enemy).is_exhausted = true;
    setXp(P1, 3);
    ASSERT_TRUE(offered(rose, 0));

    EnginePathAgent agent(EnginePathAgent::choosing(enemy));
    activate(rose, 0, &agent);
    EXPECT_FALSE(state.getObject(enemy).is_exhausted);
    EXPECT_EQ(state.player(P1).xp, 0);
}

TEST_F(ReviewFixC1Test, BlastCone_TheControllerChoosesTheUnitAndWhereItGoes) {
    // 07 item 7.2 / batch-08: the first enemy in map order, always to base.
    auto cone = addGear(P1, kBlastCone);
    auto other = addUnit(P2, kInvalidId, 2, 0);
    auto enemy = addUnit(P2, kInvalidId, 2, 0);
    // May: yes. Unit: `enemy`. Destination: battlefield 1 (mode 2). Stun: no.
    EnginePathAgent agent(EnginePathAgent::script({1, static_cast<int>(enemy), 2, 0}));
    fireTriggerThroughEngine(cone, TriggerType::WhenYouPlayThis, &agent);

    EXPECT_TRUE(agent.wasOffered(other));
    EXPECT_EQ(state.getObject(enemy).battlefieldId(), std::optional<BattlefieldId>{1});
    EXPECT_EQ(state.getObject(other).battlefieldId(), std::optional<BattlefieldId>{0});
    ASSERT_EQ(agent.prompts.size(), 4u) << "moving it triggers the stun ability, with its own may";
    EXPECT_FALSE(state.getObject(cone).is_exhausted) << "declined";
    EXPECT_FALSE(state.getObject(enemy).is_stunned);
}

TEST_F(ReviewFixC1Test, BlastCone_TheMoveCanBeDeclined) {
    auto cone = addGear(P1, kBlastCone);
    auto enemy = addUnit(P2, kInvalidId, 2, 0);
    EnginePathAgent agent(EnginePathAgent::script({0}));
    fireTriggerThroughEngine(cone, TriggerType::WhenYouPlayThis, &agent);
    EXPECT_EQ(agent.prompts.size(), 1u);
    EXPECT_EQ(state.getObject(enemy).battlefieldId(), std::optional<BattlefieldId>{0});
}

TEST_F(ReviewFixC1Test, BlastCone_StunsAUnitMovedByAnotherOfYourAbilities) {
    // batch-08: "When you move an enemy unit, you may exhaust this to [Stun]
    // it" is a standing trigger, not a tail of the play effect. A second
    // Blast Cone's play effect does the moving here.
    auto standing = addGear(P1, kBlastCone);
    auto played = addGear(P1, kBlastCone);
    state.getObject(played).is_exhausted = true;      // it cannot stun
    auto enemy = addUnit(P2, kInvalidId, 2, 0);

    EnginePathAgent agent(EnginePathAgent::choosing(enemy));   // yes to every may
    fireTriggerThroughEngine(played, TriggerType::WhenYouPlayThis, &agent);

    EXPECT_TRUE(state.getObject(enemy).isAtBase());
    EXPECT_TRUE(state.getObject(standing).is_exhausted);
    EXPECT_TRUE(state.getObject(enemy).is_stunned);
}

TEST_F(ReviewFixC1Test, BlastCone_AUnitThatMovesItselfDoesNotTrigger) {
    auto cone = addGear(P1, kBlastCone);
    auto enemy = addUnit(P2, kInvalidId, 2, 0);
    EnginePathAgent agent(EnginePathAgent::choosing(enemy));
    // No spell or ability of P1's is resolving: nobody "moved" it.
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) { exec.moveToBase(enemy); },
                         &agent);
    EXPECT_TRUE(agent.prompts.empty());
    EXPECT_FALSE(state.getObject(cone).is_exhausted);
}

TEST_F(ReviewFixC1Test, ScryersBloom_KeptCardsGoBackInTheChosenOrder) {
    // batch-08: keeping both always reversed them; "Kill this" was paid as
    // the ability resolved (07 item 7.25).
    auto bloom = addGear(P1, kScryersBloom);
    auto bottom = addToDeck(P1, kInvalidId);
    auto second = addToDeck(P1, kInvalidId);
    auto top = addToDeck(P1, kInvalidId);            // top of the deck = back
    state.player(P1).rune_pool.energy = 1;

    // Keep the first, keep the second, then put the ORIGINAL top back on top.
    EnginePathAgent agent([&](const std::vector<Intent>& legal) {
        if (legal.size() == 2 && legal[1].chosen_objects.empty()) return legal[1];   // keep
        for (const auto& i : legal)
            if (!i.chosen_objects.empty() && i.chosen_objects.front() == top) return i;
        return legal.front();
    });
    activate(bloom, 0, &agent);

    ASSERT_EQ(agent.prompts.size(), 3u) << "two keep/recycle prompts and the order";
    EXPECT_FALSE(state.getObject(bloom).location.has_value()) << "Kill this";
    EXPECT_TRUE(inHand(P1, top)) << "the card put back on top is the one drawn";
    EXPECT_EQ(state.player(P1).main_deck.back(), second);
    EXPECT_EQ(state.player(P1).main_deck.front(), bottom);
    EXPECT_EQ(state.player(P1).xp, 1);
}

TEST_F(ReviewFixC1Test, ScryersBloom_RecyclingAnnouncesTheRecycle) {
    // 02 item 2.15: the hand-rolled Predict never announced "recycled_main".
    auto bloom = addGear(P1, kScryersBloom);
    addToDeck(P1, kInvalidId);
    addToDeck(P1, kInvalidId);
    auto top = addToDeck(P1, kInvalidId);
    state.player(P1).rune_pool.energy = 1;

    int recycled_events = 0;
    auto conn = events.on_object_state_changed.connect(
        [&](const ObjectStateChangedEvent& e) {
            if (e.what_changed == "recycled_main") ++recycled_events;
        });
    EnginePathAgent agent;   // first option of each prompt: recycle
    activate(bloom, 0, &agent);
    conn.disconnect();

    EXPECT_EQ(recycled_events, 1);
    EXPECT_EQ(state.player(P1).main_deck.size(), 2u) << "two recycled, the third drawn";
    EXPECT_NE(std::find(state.player(P1).main_deck.begin(), state.player(P1).main_deck.end(), top),
              state.player(P1).main_deck.end());
}

TEST_F(ReviewFixC1Test, TheList_TheControllerNamesTheTag) {
    // batch-01: the tag was the most common enemy tag; the player never chose.
    auto list = addToHand(P1, kTheList);
    auto poro = addUnit(P2, kInvalidId, 4, 0);
    state.getObject(poro).tags = {"Poro"};
    auto poro2 = addUnit(P2, kInvalidId, 4, 0);
    state.getObject(poro2).tags = {"Poro"};
    auto yordle = addUnit(P2, kInvalidId, 4, 0);
    state.getObject(yordle).tags = {"Yordle"};

    // Offered: Poro (index 0, most common), Yordle (index 1). Name Yordle.
    EnginePathAgent agent(EnginePathAgent::script({1}));
    playThroughEngine(list, &agent);

    ASSERT_TRUE(state.getObject(list).location.has_value());
    EXPECT_EQ(state.getObject(list).string_state["named_tag"], "Yordle");

    // The ability only offers units with the named tag.
    state.getObject(list).is_exhausted = false;
    EnginePathAgent pick;
    activate(list, 0, &pick);
    EXPECT_TRUE(pick.wasOffered(yordle));
    EXPECT_FALSE(pick.wasOffered(poro));
    EXPECT_EQ(state.getObject(yordle).current_might, 2);
}

TEST_F(ReviewFixC1Test, TheList_IsNotOfferedWithNoUnitOfTheNamedTag) {
    // batch-01: the broad gate offered the ability and exhausted for nothing.
    auto list = addGear(P1, kTheList);
    auto yordle = addUnit(P2, kInvalidId, 4, 0);
    state.getObject(yordle).tags = {"Yordle"};
    EXPECT_FALSE(offered(list, 0)) << "no tag named";
    state.getObject(list).string_state["named_tag"] = "Poro";
    EXPECT_FALSE(offered(list, 0)) << "nobody is a Poro";
    state.getObject(list).string_state["named_tag"] = "Yordle";
    EXPECT_TRUE(offered(list, 0));
}

TEST_F(ReviewFixC1Test, CursedSarcophagus_AnUnpaidUnitStaysBanishedWithIt) {
    // 07 item 7.35 / 08 item 8.13: the unit was erased from banishment and the
    // tracking list before the cost was known to be paid.
    auto coffin = addGear(P1, kCursedSarcophagus);
    auto unit = cardInTrash(P1, kShipyardSkulker);    // costs [3]
    {
        auto& ps = state.player(P1);
        ps.trash.clear();
        state.getObject(unit).zone = ZoneType::Banishment;
        ps.banishment.push_back(unit);
        state.getObject(coffin).tracked_objects.push_back(unit);
    }
    EnginePathAgent agent(EnginePathAgent::choosing(unit));
    activate(coffin, 0, &agent);                      // nothing to pay with
    EXPECT_FALSE(state.getObject(unit).location.has_value());
    EXPECT_EQ(state.player(P1).banishment.size(), 1u);
    EXPECT_EQ(state.getObject(coffin).tracked_objects.size(), 1u);

    state.getObject(coffin).is_exhausted = false;
    state.player(P1).rune_pool.energy = 3;
    EnginePathAgent again(EnginePathAgent::choosing(unit));
    activate(coffin, 0, &again);
    EXPECT_TRUE(state.getObject(unit).location.has_value());
    EXPECT_TRUE(state.player(P1).banishment.empty());
    EXPECT_TRUE(state.getObject(coffin).tracked_objects.empty());
}

TEST_F(ReviewFixC1Test, RageAmplifier_GivesOneMightOrTwoWhenEmpowered) {
    // batch-01 (already fixed by the aura rewrite, 02 item 2.1): the engine's
    // text-matched pass added another +1 on top of the card's own hook.
    auto amp = addGear(P1, 805);
    auto unit = addUnit(P1, kInvalidId, 2, 0);
    withEngine([&](GameEngine& e) { e.testHook_cleanup(); });
    EXPECT_EQ(state.getObject(unit).current_might, 3);
    state.getObject(amp).empowered = 1;
    withEngine([&](GameEngine& e) { e.testHook_cleanup(); });
    EXPECT_EQ(state.getObject(unit).current_might, 4);
}

// ═══ Legends ════════════════════════════════════════════════════════════════

TEST_F(ReviewFixC1Test, WujuMaster_LevelSixIsGatedOnXp) {
    // batch-01 (already fixed by the aura rewrite, 02 item 2.1): +1 [M] at any
    // XP from the engine's text match, +2 at Level 6.
    addLegend(P1, 784);
    auto unit = addUnit(P1, kInvalidId, 2, 0);
    withEngine([&](GameEngine& e) { e.testHook_cleanup(); });
    EXPECT_EQ(state.getObject(unit).current_might, 2) << "0 XP: no bonus";
    setXp(P1, 6);
    withEngine([&](GameEngine& e) { e.testHook_cleanup(); });
    EXPECT_EQ(state.getObject(unit).current_might, 3) << "Level 6: +1, once";
}

TEST_F(ReviewFixC1Test, LooseCannon_DrawsOnlyWithOneOrFewerCardsInHand) {
    // batch-01: drew every turn regardless of hand size.
    auto jinx = addLegend(P1, kLooseCannon);
    for (int i = 0; i < 4; ++i) addToDeck(P1, kInvalidId);
    for (int i = 0; i < 5; ++i) addToHand(P1, kInvalidId);
    fireTriggerThroughEngine(jinx, TriggerType::AtStartOfBeginning);
    EXPECT_EQ(handSize(P1), 5);

    state.player(P1).hand.resize(1);
    fireTriggerThroughEngine(jinx, TriggerType::AtStartOfBeginning);
    EXPECT_EQ(handSize(P1), 2);
}

TEST_F(ReviewFixC1Test, NineTailedFox_AttackersOfYourBattlefieldGetMinusOne) {
    // batch-01: the legend was a blank card.
    addLegend(P1, kNineTailedFox);
    state.battlefields[0].controller = P1;
    auto defender = addUnit(P1, kInvalidId, 3, 0);
    auto big = addUnit(P2, kInvalidId, 4, 0);
    auto small = addUnit(P2, kInvalidId, 1, 0);
    state.getObject(defender).combat_designation = CombatDesignation::Defender;
    state.getObject(big).combat_designation = CombatDesignation::Attacker;
    state.getObject(small).combat_designation = CombatDesignation::Attacker;

    withEngine([&](GameEngine& e) {
        events.emit(CombatStartedEvent{e.mutableState().battlefields[0].id, P2, P1});
        e.testHook_runChain();
    }, nullptr, nullptr, P2);

    EXPECT_EQ(state.getObject(big).current_might, 3);
    EXPECT_EQ(state.getObject(small).current_might, 1) << "to a minimum of 1 [M]";
    EXPECT_EQ(state.getObject(defender).current_might, 3);
}

TEST_F(ReviewFixC1Test, NineTailedFox_NotAtABattlefieldYouDoNotControl) {
    addLegend(P1, kNineTailedFox);
    auto defender = addUnit(P1, kInvalidId, 3, 0);
    auto attacker = addUnit(P2, kInvalidId, 4, 0);
    state.getObject(defender).combat_designation = CombatDesignation::Defender;
    state.getObject(attacker).combat_designation = CombatDesignation::Attacker;
    withEngine([&](GameEngine& e) {
        events.emit(CombatStartedEvent{e.mutableState().battlefields[0].id, P2, P1});
        e.testHook_runChain();
    }, nullptr, nullptr, P2);
    EXPECT_EQ(state.getObject(attacker).current_might, 4);
}

TEST_F(ReviewFixC1Test, Unforgiven_MovesAUnitFromItsBaseToAChosenBattlefield) {
    // batch-01: always moveToBase — the "from its base" half did not exist.
    auto yasuo = addLegend(P1, kUnforgiven);
    auto unit = addUnit(P1, kInvalidId, 3);          // in base
    state.player(P1).rune_pool.energy = 2;
    EnginePathAgent agent(EnginePathAgent::script({1}));   // battlefield 1
    activate(yasuo, 0, &agent, {unit});
    EXPECT_EQ(state.getObject(unit).battlefieldId(), std::optional<BattlefieldId>{1});
    EXPECT_TRUE(state.getObject(yasuo).is_exhausted);
}

TEST_F(ReviewFixC1Test, Unforgiven_MovesAUnitAtABattlefieldToItsBase) {
    auto yasuo = addLegend(P1, kUnforgiven);
    auto unit = addUnit(P1, kInvalidId, 3, 1);
    state.player(P1).rune_pool.energy = 2;
    EnginePathAgent agent;
    activate(yasuo, 0, &agent, {unit});
    EXPECT_TRUE(state.getObject(unit).isAtBase());
    EXPECT_TRUE(agent.prompts.empty());
}

TEST_F(ReviewFixC1Test, TheBoss_TheReplacementCanBeDeclined) {
    // batch-01: the "may" was never asked — a player who wanted the unit to
    // die could not let it.
    auto sett = addLegend(P1, kTheBoss);
    auto unit = addUnit(P1, kInvalidId, 2, 0);
    state.getObject(unit).buff_count = 1;
    state.player(P1).rune_pool.power[static_cast<int>(Domain::Body)] = 1;
    EnginePathAgent no(EnginePathAgent::script({0}));
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) { exec.killObject(unit); }, &no);

    EXPECT_EQ(no.prompts.size(), 1u);
    EXPECT_FALSE(state.getObject(unit).location.has_value()) << "it died";
    EXPECT_FALSE(state.getObject(sett).is_exhausted);
    EXPECT_EQ(state.player(P1).rune_pool.totalPower(), 1) << "nothing was paid";
}

TEST_F(ReviewFixC1Test, TheBoss_NeedsThePowerToPay) {
    // batch-01: [C] was never charged — the unit was saved with no power.
    auto sett = addLegend(P1, kTheBoss);
    auto unit = addUnit(P1, kInvalidId, 2, 0);
    state.getObject(unit).buff_count = 1;
    state.player(P1).rune_pool.power[static_cast<int>(Domain::Fury)] = 1;   // not Body / Order
    EnginePathAgent yes(EnginePathAgent::script({1}));
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) { exec.killObject(unit); }, &yes);

    EXPECT_TRUE(yes.prompts.empty()) << "not offered: the cost can't be paid";
    EXPECT_FALSE(state.getObject(unit).location.has_value());
    EXPECT_FALSE(state.getObject(sett).is_exhausted);
}

TEST_F(ReviewFixC1Test, TheBoss_PaysAndRecallsOutOfCombat) {
    auto sett = addLegend(P1, kTheBoss);
    auto unit = addUnit(P1, kInvalidId, 2, 0);
    state.getObject(unit).buff_count = 1;
    state.getObject(unit).combat_designation = CombatDesignation::Defender;
    addRune(P1, Domain::Body);
    EnginePathAgent yes(EnginePathAgent::script({1}));
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) { exec.killObject(unit); }, &yes);

    EXPECT_TRUE(state.getObject(unit).isAtBase());
    EXPECT_TRUE(state.getObject(unit).is_exhausted);
    EXPECT_EQ(state.getObject(unit).buff_count, 0);
    EXPECT_EQ(state.getObject(unit).combat_designation, CombatDesignation::None);
    EXPECT_TRUE(state.getObject(sett).is_exhausted);
    EXPECT_EQ(state.player(P1).rune_deck.size(), 1u) << "a Body rune was recycled for [C]";
}

TEST_F(ReviewFixC1Test, RelentlessStorm_OnlyWhenTheUnitPlayedIsMighty) {
    // 07 item 7.13: fired on any unit play while any Mighty friendly unit was
    // on the board.
    auto voli = addLegend(P1, kRelentlessStorm);
    addUnit(P1, kInvalidId, 7, 0);                     // a Mighty unit already in play
    state.player(P1).rune_deck.push_back(addRune(P1, Domain::Fury));
    {
        auto& r = state.getObject(state.player(P1).rune_deck.back());
        r.location = std::nullopt;
        r.zone = ZoneType::RuneDeck;
    }
    auto small = unitInHand(P1, kShipyardSkulker);
    EnginePathAgent agent(EnginePathAgent::script({1}));
    playThroughEngine(small, &agent);
    EXPECT_TRUE(agent.prompts.empty()) << "a 3 [M] unit is not a Mighty unit";
    EXPECT_FALSE(state.getObject(voli).is_exhausted);

    auto mighty = unitInHand(P1, kPlayfulPhantom);
    EnginePathAgent yes(EnginePathAgent::script({1}));
    playThroughEngine(mighty, &yes);
    EXPECT_EQ(yes.prompts.size(), 1u);
    EXPECT_TRUE(state.getObject(voli).is_exhausted);
    EXPECT_TRUE(state.player(P1).rune_deck.empty()) << "channeled 1 rune";
}

TEST_F(ReviewFixC1Test, HandOfNoxus_NoEnergyWithoutLegion) {
    // batch-01 / 08 item 8.7: requiresLegion() is read by nothing.
    auto darius = addLegend(P1, kHandOfNoxus);
    activate(darius, 0);
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0) << "no card played this turn";

    state.getObject(darius).is_exhausted = false;
    state.player(P1).cards_played_this_turn = 1;
    activate(darius, 0);
    EXPECT_EQ(state.player(P1).rune_pool.energy, 1);
}

TEST_F(ReviewFixC1Test, RadiantDawn_TheControllerChoosesTheUnitToBuff) {
    // batch-01 S5: buffed the first friendly unit in map order.
    addLegend(P1, kRadiantDawn);
    auto a = addUnit(P1, kInvalidId, 2, 0);
    auto b = addUnit(P1, kInvalidId, 2, 0);
    auto enemy = addUnit(P2, kInvalidId, 2, 0);
    EnginePathAgent agent(EnginePathAgent::choosing(b));
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) { exec.stunUnitBy(enemy, a); },
                         &agent);
    EXPECT_TRUE(agent.wasOffered(a));
    EXPECT_EQ(state.getObject(b).buff_count, 1);
    EXPECT_EQ(state.getObject(a).buff_count, 0);
}

TEST_F(ReviewFixC1Test, DarkChild_ReadiesTwoRunes) {
    auto annie = addLegend(P1, kDarkChild);
    for (int i = 0; i < 3; ++i) addRune(P1, Domain::Fury, /*exhausted=*/true);
    fireTriggerThroughEngine(annie, TriggerType::AtEndOfTurn);
    EXPECT_EQ(readyRuneCount(P1), 2);
    EXPECT_EQ(exhaustedRuneCount(P1), 1);
}

TEST_F(ReviewFixC1Test, LadyOfLuminosity_LooksAtTheSpellThatWasPlayed) {
    // batch-01: looked up the newest spell in the trash instead.
    auto lux = addLegend(P1, kLadyOfLuminosity);
    for (int i = 0; i < 3; ++i) addToDeck(P1, kInvalidId);
    ASSERT_GE(card_db.get(kSkySplitter).energy_cost, 5);

    // A cheap spell on top of the trash; the spell played (cost [5]+) has
    // gone elsewhere (banished for Flow).
    auto big = cardInTrash(P1, kSkySplitter);
    {
        auto& ps = state.player(P1);
        ps.trash.clear();
        state.getObject(big).zone = ZoneType::Banishment;
        ps.banishment.push_back(big);
    }
    auto cheap = cardInTrash(P1, 389);   // Frigid Touch, [2]
    ASSERT_LT(card_db.get(389).energy_cost, 5);

    fireTriggerThroughEngine(lux, TriggerType::WhenYouPlayASpell, nullptr, big);
    EXPECT_EQ(handSize(P1), 1) << "the [5]+ spell draws";
    fireTriggerThroughEngine(lux, TriggerType::WhenYouPlayASpell, nullptr, cheap);
    EXPECT_EQ(handSize(P1), 1) << "the [2] spell does not";
}

TEST_F(ReviewFixC1Test, MightOfDemacia_CountsUnitsAtTheConqueredBattlefield) {
    // batch-01: any battlefield scored this turn with 4+ units drew.
    auto garen = addLegend(P1, kMightOfDemacia);
    for (int i = 0; i < 4; ++i) addUnit(P1, kInvalidId, 2, 0);   // held earlier
    addUnit(P1, kInvalidId, 2, 1);                               // the conquest
    for (int i = 0; i < 4; ++i) addToDeck(P1, kInvalidId);
    state.player(P1).battlefields_scored_this_turn.insert(0);
    bfCard(0);
    auto conquered = bfCard(1);

    fireTriggerThroughEngine(garen, TriggerType::WhenIConquer, nullptr, conquered);
    EXPECT_EQ(handSize(P1), 0) << "one unit at that battlefield";
}

TEST_F(ReviewFixC1Test, ProdigalExplorer_IsAReactionWithAGate) {
    // batch-01: Action timing, and no "Use only if" gate at all.
    Card* card = card_registry.get(kProdigalExplorer);
    EXPECT_TRUE(card->isReactionAbility());
    EXPECT_FALSE(card->isActionAbility());

    auto ezreal = addLegend(P1, kProdigalExplorer);
    addToDeck(P1, kInvalidId);
    EXPECT_FALSE(offered(ezreal, 0)) << "has chosen nothing this turn";
    state.player(P1).chose_enemy_unit_this_turn = true;
    EXPECT_TRUE(offered(ezreal, 0));
}

TEST_F(ReviewFixC1Test, BattleMistress_RecyclingARuneOffersAGold) {
    // batch-01: the first sentence was not wired at all.
    auto sivir = addLegend(P1, kBattleMistress);
    addRune(P1, Domain::Body);
    EnginePathAgent yes(EnginePathAgent::script({1}));
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) { exec.payAnyPower(P1, 1); },
                         &yes);

    EXPECT_EQ(yes.prompts.size(), 1u);
    EXPECT_TRUE(state.getObject(sivir).is_exhausted);
    GameObjectId gold = kInvalidId;
    for (const auto& [id, obj] : state.objects)
        if (obj.name == "Gold" && obj.location.has_value()) gold = id;
    ASSERT_NE(gold, kInvalidId);
    EXPECT_TRUE(state.getObject(gold).is_exhausted);
    EXPECT_EQ(state.getObject(gold).card_def_id, kGold326);
}

TEST_F(ReviewFixC1Test, BattleMistress_AnExhaustedLegendIsNotOffered) {
    auto sivir = addLegend(P1, kBattleMistress);
    state.getObject(sivir).is_exhausted = true;
    addRune(P1, Domain::Body);
    EnginePathAgent yes(EnginePathAgent::script({1}));
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) { exec.payAnyPower(P1, 1); },
                         &yes);
    EXPECT_TRUE(yes.prompts.empty());
    EXPECT_EQ(countNamed(P1, "Gold"), 0);
}

TEST_F(ReviewFixC1Test, GrandmasterAtArms_OnlyEquipmentOnTheBoardAndTheControllerChooses) {
    // batch-01: an Equipment still in the deck counted as "detached", and the
    // first match was attached with no choice of Equipment.
    auto jax = addLegend(P1, kGrandmasterAtArms);
    auto in_deck = addToDeck(P1, kBFSword);
    auto unit = addUnit(P1, kInvalidId, 2, 0);
    state.player(P1).rune_pool.energy = 1;
    EXPECT_FALSE(offered(jax, 0)) << "no Equipment on the board";

    auto blade = addGear(P1, kDoransBlade);
    auto sword = addGear(P1, kBFSword);
    ASSERT_TRUE(offered(jax, 0));
    EnginePathAgent agent(EnginePathAgent::script({static_cast<int>(sword),
                                                   static_cast<int>(unit)}));
    activate(jax, 0, &agent);

    EXPECT_TRUE(agent.wasOffered(blade));
    EXPECT_FALSE(agent.wasOffered(in_deck));
    EXPECT_EQ(state.getObject(sword).attached_to, std::optional<GameObjectId>{unit});
    EXPECT_FALSE(state.getObject(blade).attached_to.has_value());
    EXPECT_FALSE(state.getObject(in_deck).attached_to.has_value());
    EXPECT_EQ(state.getObject(unit).attachment_might_bonus, 3);
}

TEST_F(ReviewFixC1Test, GrandmasterAtArms_MovesAnAttachedEquipmentToAnotherUnit) {
    auto jax = addLegend(P1, kGrandmasterAtArms);
    auto from = addUnit(P1, kInvalidId, 2, 0);
    auto to = addUnit(P1, kInvalidId, 2, 1);
    auto sword = addGear(P1, kBFSword);
    attach(sword, from);
    EnginePathAgent agent(EnginePathAgent::script({static_cast<int>(sword),
                                                   static_cast<int>(to)}));
    activate(jax, 1, &agent);

    EXPECT_FALSE(agent.wasOffered(from)) << "not the unit it is already on";
    EXPECT_EQ(state.getObject(sword).attached_to, std::optional<GameObjectId>{to});
    EXPECT_EQ(state.getObject(from).attachment_might_bonus, 0);
    EXPECT_EQ(state.getObject(to).attachment_might_bonus, 3);
    EXPECT_EQ(state.getObject(sword).battlefieldId(), std::optional<BattlefieldId>{1});
}

TEST_F(ReviewFixC1Test, ChemBaroness_DoesNotTriggerOnAConquer) {
    // batch-08: "When you or an ally hold" was coded as conquer-or-hold.
    Card* card = card_registry.get(kChemBaroness);
    EXPECT_TRUE(card->firesOn(TriggerType::WhenIHold));
    EXPECT_FALSE(card->firesOn(TriggerType::WhenIConquerOrHold));
    EXPECT_FALSE(card->firesOn(TriggerType::WhenIConquer));

    auto renata = addLegend(P1, kChemBaroness);
    addUnit(P1, kInvalidId, 2, 0);
    EnginePathAgent yes(EnginePathAgent::script({1}));
    withEngine([&](GameEngine& e) {
        e.testHook_scoreConquer(P1, e.mutableState().battlefields[0].id);
        e.testHook_runChain();
    }, &yes);
    EXPECT_TRUE(yes.prompts.empty());
    EXPECT_FALSE(state.getObject(renata).is_exhausted);
    EXPECT_EQ(countNamed(P1, "Gold"), 0);
}

TEST_F(ReviewFixC1Test, ChemBaroness_OnlyAReadyLegendCanPay) {
    // 03 item 3.8: with no ready check two holds made two Gold.
    auto renata = addLegend(P1, kChemBaroness);
    auto bf0 = bfCard(0);
    EnginePathAgent first(EnginePathAgent::script({1}));
    fireTriggerThroughEngine(renata, TriggerType::WhenIHold, &first, bf0);
    EXPECT_EQ(countNamed(P1, "Gold"), 1);
    EXPECT_TRUE(state.getObject(renata).is_exhausted);

    EnginePathAgent second(EnginePathAgent::script({1}));
    fireTriggerThroughEngine(renata, TriggerType::WhenIHold, &second, bf0);
    EXPECT_TRUE(second.prompts.empty()) << "already exhausted";
    EXPECT_EQ(countNamed(P1, "Gold"), 1);
}

TEST_F(ReviewFixC1Test, ChemBaroness_GoldAddsAnExtraEnergyNearTheVictoryScore) {
    // batch-08: "While your score is within 3 points of the Victory Score,
    // your Gold [ADD] an additional [1]." was not implemented.
    addLegend(P1, kChemBaroness);
    GameObjectId gold = kInvalidId, late_gold = kInvalidId;
    withEngine([&](GameEngine& e) {
        CardContext ctx{e.mutableState(), events, e.testHook_executor(), P1, kInvalidId};
        gold = createGoldExhausted(ctx);
        late_gold = createGoldExhausted(ctx);
    });
    state.getObject(gold).is_exhausted = false;
    state.getObject(late_gold).is_exhausted = false;

    setScore(P1, state.mode.victory_score - 4);
    withEngine([&](GameEngine& e) { e.testHook_cleanup(); });
    activate(gold, 0);
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 1);
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0) << "4 points away: no bonus";

    setScore(P1, state.mode.victory_score - 3);
    withEngine([&](GameEngine& e) { e.testHook_cleanup(); });
    activate(late_gold, 0);
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 2);
    EXPECT_EQ(state.player(P1).rune_pool.energy, 1) << "within 3: an additional [1]";
}

TEST_F(ReviewFixC1Test, Pridestalker_TheControllerChoosesAnyUnit) {
    // 07 item 7.2: read the played unit from targets[0], which is never set.
    addLegend(P1, kPridestalker);
    auto at_base = addUnit(P1, kInvalidId, 2);
    auto enemy = addUnit(P2, kInvalidId, 2, 0);
    auto played = unitInHand(P1, kShipyardSkulker);
    EnginePathAgent agent(EnginePathAgent::choosing(at_base));
    playThroughEngine(played, &agent);

    EXPECT_TRUE(agent.wasOffered(enemy)) << "a unit: any unit";
    EXPECT_TRUE(agent.wasOffered(played));
    EXPECT_EQ(state.getObject(at_base).current_might, 3);
    EXPECT_EQ(state.getObject(played).current_might, 3);
}

TEST_F(ReviewFixC1Test, GreenFather_ReplacesTheBattlefieldThatWasScored) {
    // 07 item 7.14: replaced the first controlled battlefield, not "that
    // battlefield".
    auto ivern = addLegend(P1, kGreenFather);
    state.battlefields[0].controller = P1;
    state.battlefields[1].controller = P1;
    bfCard(0);
    auto held = bfCard(1);
    EnginePathAgent yes(EnginePathAgent::script({1}));
    fireTriggerThroughEngine(ivern, TriggerType::WhenIConquerOrHold, &yes, held);

    EXPECT_TRUE(state.getObject(ivern).is_exhausted);
    EXPECT_FALSE(state.battlefields[0].was_replaced);
    EXPECT_TRUE(state.battlefields[1].was_replaced);
}

TEST_F(ReviewFixC1Test, KeeperOfTheHammer_GainsXpOnAHoldOnly) {
    // batch-01: WhenIConquerOrHold — a conquer gave the XP too.
    Card* card = card_registry.get(kKeeperOfTheHammer);
    EXPECT_TRUE(card->firesOn(TriggerType::WhenIHold));
    EXPECT_FALSE(card->firesOn(TriggerType::WhenIConquerOrHold));
    EXPECT_FALSE(card->firesOn(TriggerType::WhenIConquer));

    addLegend(P1, kKeeperOfTheHammer);
    addUnit(P1, kInvalidId, 2, 0);
    addUnit(P1, kInvalidId, 2, 1);
    state.battlefields[1].controller = P1;
    withEngine([&](GameEngine& e) {
        e.testHook_scoreConquer(P1, e.mutableState().battlefields[0].id);
        e.testHook_runChain();
    });
    EXPECT_EQ(state.player(P1).xp, 0);
    withEngine([&](GameEngine& e) {
        e.testHook_scoreHold(P1, e.mutableState().battlefields[1].id);
        e.testHook_runChain();
    });
    EXPECT_EQ(state.player(P1).xp, 1);
}

TEST_F(ReviewFixC1Test, Virtuoso_BanishesTheSpellThatWasPlayedAndCountsIt) {
    // 07 item 7.25: rescanned the trash, so a spell played in response was
    // banished instead. batch-08: the banish skipped cards_banished_this_turn.
    auto jhin = addLegend(P1, kVirtuoso);
    auto played = cardInTrash(P1, kSkySplitter);
    auto in_response = cardInTrash(P1, 389);          // newer in the trash
    state.player(P1).last_spell_energy_spent = 5;

    EnginePathAgent yes(EnginePathAgent::script({1}));
    fireTriggerThroughEngine(jhin, TriggerType::WhenYouPlayASpell, &yes, played);

    EXPECT_EQ(state.getObject(played).zone, ZoneType::Banishment);
    EXPECT_TRUE(inTrash(P1, in_response));
    EXPECT_EQ(state.player(P1).cards_banished_this_turn, 1);
    ASSERT_EQ(state.getObject(jhin).tracked_objects.size(), 1u);
    EXPECT_EQ(state.getObject(jhin).tracked_objects.front(), played);
}

TEST_F(ReviewFixC1Test, PiltoverEnforcer_CanReadyAnEnemyUnit) {
    // batch-08: "ready a unit" was limited to friendly units.
    auto vi = addLegend(P1, kPiltoverEnforcer);
    auto conquered = bfCard(0);
    state.player(P1).last_combat_bf = state.battlefields[0].id;
    state.player(P1).excess_combat_damage = 3;
    auto mine = addUnit(P1, kInvalidId, 2, 0);
    auto enemy = addUnit(P2, kInvalidId, 2, 1);
    state.getObject(enemy).is_exhausted = true;

    EnginePathAgent agent(EnginePathAgent::choosing(enemy));
    fireTriggerThroughEngine(vi, TriggerType::WhenIConquer, &agent, conquered);
    EXPECT_TRUE(agent.wasOffered(mine));
    EXPECT_FALSE(state.getObject(enemy).is_exhausted);
    EXPECT_TRUE(state.getObject(vi).is_exhausted);
}

}  // namespace test_review_fix_c1
