/// @file test_review_fix_c6.cpp
/// Regression tests for the 2026-10 card review, stream c6: units 495..709
/// (plans/cards/batch-06.md and the plan items that point into that range).
/// Each test drives the real engine path — a card played with executeIntent,
/// a trigger queued through TriggerManager and resolved off the chain, an
/// ability activated through executeIntent, an aura through cleanup — and
/// names the card it pins.

#include "card_test_fixture.h"
#include "cards/card_helpers.h"
#include "engine/effect_executor.h"
#include "engine/game_engine.h"

#include <gtest/gtest.h>

namespace riftbound::test {
namespace test_review_fix_c6 {  // file-unique: test sources are unity-built
namespace {

constexpr CardDefId kLongSword = 345;          // gear, Equipment
constexpr CardDefId kGold = 326;
constexpr CardDefId kXinZhao = 496;
constexpr CardDefId kAzirSovereign = 497;
constexpr CardDefId kFioraWorthy = 500;
constexpr CardDefId kAhri = 539;
constexpr CardDefId kBard = 540;
constexpr CardDefId kSett = 543;
constexpr CardDefId kYone = 544;
constexpr CardDefId kKarma = 548;
constexpr CardDefId kBird = 559;
constexpr CardDefId kPreparedNeophyte = 563;
constexpr CardDefId kRevna = 565;
constexpr CardDefId kReflection = 566;
constexpr CardDefId kSprite = 569;
constexpr CardDefId kLordBroadmane = 574;
constexpr CardDefId kScorchclaw = 578;
constexpr CardDefId kYetiBrawler = 580;
constexpr CardDefId kGrimApothecary = 583;
constexpr CardDefId kKatarina = 585;
constexpr CardDefId kXerath = 588;
constexpr CardDefId kInviolusVox = 589;        // a Dragon
constexpr CardDefId kPykeButcher = 590;
constexpr CardDefId kFriskyHunter = 595;
constexpr CardDefId kHeraldOfSpring = 596;
constexpr CardDefId kMutatedMouser = 598;      // a Cat
constexpr CardDefId kAllay = 603;
constexpr CardDefId kMosstomper = 609;
constexpr CardDefId kTrevor = 610;
constexpr CardDefId kIascylla = 612;
constexpr CardDefId kIvern = 613;
constexpr CardDefId kNami = 614;
constexpr CardDefId kScuttleCrab = 615;
constexpr CardDefId kYuumi = 618;
constexpr CardDefId kAlphaWildclaw = 619;
constexpr CardDefId kLilliaProtector = 620;
constexpr CardDefId kYiUnstoppable = 621;
constexpr CardDefId kRuinedRex = 629;
constexpr CardDefId kSpectralCentaur = 630;
constexpr CardDefId kChakramDancer = 633;
constexpr CardDefId kGustwalker = 637;
constexpr CardDefId kPetalPixie = 638;
constexpr CardDefId kKeeperOfMasks = 643;
constexpr CardDefId kLilliaFawn = 644;
constexpr CardDefId kSpriteQueen = 646;
constexpr CardDefId kZilean = 648;
constexpr CardDefId kBlueSentinel = 649;
constexpr CardDefId kLeBlanc = 652;
constexpr CardDefId kDiplomat = 654;           // 2-cost unit: "When you play me, gain 1 XP."
constexpr CardDefId kDragonsoulSage = 655;
constexpr CardDefId kCrowdFavorite = 664;
constexpr CardDefId kGemdragon = 666;
constexpr CardDefId kImposingChallenger = 667;
constexpr CardDefId kNewtfish = 670;
constexpr CardDefId kFaefolk = 674;
constexpr CardDefId kYiTempered = 675;
constexpr CardDefId kPoppy = 678;
constexpr CardDefId kArachnoidHorror = 679;
constexpr CardDefId kElderDragon = 680;
constexpr CardDefId kKhaZixHunter = 681;
constexpr CardDefId kCrescentGuardian = 684;
constexpr CardDefId kMegatusk = 688;
constexpr CardDefId kMaduli = 706;
constexpr CardDefId kPykeReturned = 707;
constexpr CardDefId kSyndra = 708;
constexpr CardDefId kBaronNashor = 709;

class C6Test : public CardTestFixture {
protected:
    GameObject& o(GameObjectId id) { return state.getObject(id); }
    PlayerState& ps(PlayerId p) { return state.player(p); }

    /// A unit with no card behind it (no abilities), `m` Might.
    GameObjectId vanilla(PlayerId p, int m, int bf = -1) { return addUnit(p, kInvalidId, m, bf); }
    GameObjectId token(PlayerId p, int m, int bf = -1) {
        auto id = vanilla(p, m, bf);
        o(id).is_token = true;
        o(id).super_type = SuperType::Token;
        return id;
    }
    GameObjectId gear(PlayerId p, CardDefId def_id) {
        auto id = state.createObject();
        auto& g = o(id);
        const auto& def = card_db.get(def_id);
        g.owner = g.controller = p;
        g.card_def_id = def_id;
        g.card_type = CardType::Gear;
        g.name = def.name;
        g.tags = def.tags;
        g.keywords = def.keywords;
        g.might_bonus = def.might_bonus;
        g.zone = ZoneType::Base;
        g.location = BaseLocation{p};
        return id;
    }
    GameObjectId legend(PlayerId p) {
        auto id = state.createObject();
        auto& l = o(id);
        l.owner = l.controller = p;
        l.card_type = CardType::Legend;
        l.name = "TestLegend";
        l.zone = ZoneType::LegendZone;
        ps(p).legend_zone = id;
        return id;
    }
    void cleanup() {
        withEngine([](GameEngine& e) { e.testHook_cleanup(); });
    }
    std::vector<Intent> legalActions() {
        std::vector<Intent> out;
        withEngine([&](GameEngine& e) { out = e.generateLegalActions(); });
        return out;
    }
    bool canActivate(GameObjectId source) {
        for (const auto& i : legalActions())
            if (i.type == IntentType::ActivateAbility && i.ability_source == source) return true;
        return false;
    }
    void activate(GameObjectId source, AgentInterface* agent = nullptr) {
        Intent act;
        act.type = IntentType::ActivateAbility;
        act.player = o(source).controller;
        act.ability_source = source;
        withEngine([&](GameEngine& e) { e.testHook_executeIntent(act); }, agent, nullptr,
                   act.player);
    }
    /// `from`: who controlled the battlefield before P1 conquered it.
    void conquer(int bf, AgentInterface* agent = nullptr,
                 std::optional<PlayerId> from = std::nullopt) {
        withEngine([&](GameEngine& e) {
            e.testHook_scoreConquer(P1, static_cast<BattlefieldId>(bf), from);
            e.testHook_runChain();
        }, agent);
    }
    void hold(int bf, AgentInterface* agent = nullptr) {
        withEngine([&](GameEngine& e) {
            e.testHook_scoreHold(P1, static_cast<BattlefieldId>(bf));
            e.testHook_runChain();
        }, agent);
    }
    /// Queue `source`'s trigger and run the chain. `probe` is handed the live
    /// state the first time the opponent gets priority — the ability has been
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
    int might(GameObjectId id) { return o(id).current_might; }
    bool atBf(GameObjectId id, int bf) {
        auto b = o(id).battlefieldId();
        return b.has_value() && static_cast<int>(*b) == bf;
    }
    int goldTokens(PlayerId p) {
        int n = 0;
        for (auto& [id, obj] : state.objects)
            if (obj.isGear() && obj.controller == p && obj.location.has_value() &&
                obj.card_def_id == kGold) ++n;
        return n;
    }
};

// ── Data ────────────────────────────────────────────────────────────────────

TEST_F(C6Test, Defs_KeywordsThatBelongToSomethingElseAreGone) {
    struct Row { CardDefId id; Keyword kw; const char* why; };
    const Row rows[] = {
        {kLordBroadmane, Keyword::Assault, "gives [Assault] to other units"},
        {kYetiBrawler, Keyword::Reaction, "[Reaction] is in the Gold reminder text"},
        {kFriskyHunter, Keyword::Deflect, "[Deflect] is the Bird token's"},
        {kMosstomper, Keyword::Deflect, "[Deflect] only at Level 3"},
        {kTrevor, Keyword::Temporary, "[Temporary] is the Sprite token's"},
        {kYuumi, Keyword::Tank, "[Tank] is given to another unit"},
        {kLilliaProtector, Keyword::Tank, "[Tank] is her tokens'"},
        {kChakramDancer, Keyword::Shield, "gives [Shield] to other units"},
        {kGustwalker, Keyword::Ganking, "[Ganking] only at Level 3"},
        {kPetalPixie, Keyword::Temporary, "counts [Temporary] units"},
        {kLilliaFawn, Keyword::Temporary, "[Temporary] is the Sprite token's"},
        {kSpriteQueen, Keyword::Temporary, "[Temporary] is the Sprite token's"},
        {kLeBlanc, Keyword::Temporary, "names other cards' [Temporary] effects"},
        {kDragonsoulSage, Keyword::Reaction, "the ability is a [Reaction], the card is not"},
        {kYiTempered, Keyword::Deflect, "[Deflect] only at Level 6"},
        {kYiTempered, Keyword::Ganking, "[Ganking] only at Level 6"},
        {kMegatusk, Keyword::Ganking, "[Ganking] is what the ability gives"},
        {kPykeReturned, Keyword::Reaction, "[Reaction] is in the Gold reminder text"},
        {kSyndra, Keyword::Repeat, "[Repeat] is what she gives spells"},
    };
    for (const auto& r : rows)
        EXPECT_FALSE(card_db.get(r.id).keywords.has(r.kw))
            << card_db.get(r.id).name << ": " << r.why;
    EXPECT_EQ(card_db.get(kFriskyHunter).deflect_value, 0);
    EXPECT_EQ(card_db.get(kMosstomper).deflect_value, 0);
    EXPECT_EQ(card_db.get(kYiTempered).deflect_value, 0);
    EXPECT_EQ(card_db.get(kLordBroadmane).assault_value, 0);
    EXPECT_EQ(card_db.get(kChakramDancer).shield_value, 0);
}

TEST_F(C6Test, Defs_TokenReferenceCardsAreTokensAndAllayIsNotAChampion) {
    EXPECT_EQ(card_db.get(kBird).super_type, SuperType::Token);
    EXPECT_EQ(card_db.get(kReflection).super_type, SuperType::Token);
    EXPECT_EQ(card_db.get(kSprite).super_type, SuperType::Token);
    // UNL-041/219 prints no supertype and no champion tag.
    EXPECT_EQ(card_db.get(kAllay).super_type, SuperType::None);
}

TEST_F(C6Test, Trevor_SurvivesHisControllersBeginningPhase) {
    auto trevor = addUnit(P1, kTrevor, 0, 0);
    state.turn.turn_player = P1;
    withEngine([](GameEngine& e) { e.testHook_beginningStep(); });
    EXPECT_TRUE(o(trevor).location.has_value());
}

// ── 496 Xin Zhao, Vigilant ──────────────────────────────────────────────────

TEST_F(C6Test, XinZhao_AnotherXinZhaoCountsAsAnOtherUnit) {
    addUnit(P1, kXinZhao);          // in base
    vanilla(P1, 2);                 // in base
    auto second = addToHand(P1, kXinZhao);
    playThroughEngine(second);
    EXPECT_FALSE(o(second).is_exhausted);
}

TEST_F(C6Test, XinZhao_DoesNotCountItself) {
    vanilla(P1, 2);
    auto xin = addToHand(P1, kXinZhao);
    playThroughEngine(xin);
    EXPECT_TRUE(o(xin).is_exhausted);
}

// ── 497 Azir, Sovereign ─────────────────────────────────────────────────────

TEST_F(C6Test, AzirSovereign_MovesOnlyTheTokensThePlayerNames) {
    auto azir = addUnit(P1, kAzirSovereign, 0, 0);
    auto t1 = token(P1, 1);
    auto t2 = token(P1, 1, 1);
    EnginePathAgent agent(EnginePathAgent::script({static_cast<int>(t1), 0}));
    fireTriggerThroughEngine(azir, TriggerType::WhenIAttack, &agent);
    EXPECT_TRUE(atBf(t1, 0));
    EXPECT_TRUE(atBf(t2, 1)) << "the token holding the other battlefield stays";
    EXPECT_TRUE(agent.wasOffered(t2));
}

// ── 500 Fiora, Worthy ───────────────────────────────────────────────────────

TEST_F(C6Test, FioraWorthy_PaysTheOrderPowerBeforeAnyoneCanRespond) {
    auto fiora = addUnit(P1, kFioraWorthy, 0, 0);
    auto big = vanilla(P1, 5, 0);
    o(big).is_exhausted = true;
    ps(P1).rune_pool.power[static_cast<int>(Domain::Order)] = 1;
    EnginePathAgent agent(EnginePathAgent::script({1}));
    fireAndProbe(fiora, TriggerType::WhenAUnitBecomesMighty, &agent, [&](GameState& live) {
        EXPECT_EQ(live.player(P1).rune_pool.power[static_cast<int>(Domain::Order)], 0);
        EXPECT_TRUE(live.getObject(big).is_exhausted);
    }, big);
    EXPECT_FALSE(o(big).is_exhausted);
}

TEST_F(C6Test, FioraWorthy_IsNotOfferedWithoutOrderPower) {
    auto fiora = addUnit(P1, kFioraWorthy, 0, 0);
    auto big = vanilla(P1, 5, 0);
    o(big).is_exhausted = true;
    EnginePathAgent agent(EnginePathAgent::script({1}));
    fireTriggerThroughEngine(fiora, TriggerType::WhenAUnitBecomesMighty, &agent, big);
    EXPECT_TRUE(agent.prompts.empty());
    EXPECT_TRUE(o(big).is_exhausted);
}

// ── 539 Ahri, Inquisitive (7.1) ─────────────────────────────────────────────

TEST_F(C6Test, AhriInquisitive_WeakensTheEnemyHereThePlayerPicks) {
    auto ahri = addUnit(P1, kAhri, 0, 0);
    auto a = vanilla(P2, 4, 0);
    auto b = vanilla(P2, 3, 0);
    auto elsewhere = vanilla(P2, 4, 1);
    EnginePathAgent agent(EnginePathAgent::choosing(b));
    fireTriggerThroughEngine(ahri, TriggerType::WhenIAttackOrDefend, &agent);
    EXPECT_EQ(might(b), 1);
    EXPECT_EQ(might(a), 4);
    EXPECT_FALSE(agent.wasOffered(elsewhere));
}

// ── 540 Bard, Mercurial ─────────────────────────────────────────────────────

TEST_F(C6Test, Bard_MovesTheUnitsThePlayerNamesToTheOpenBattlefield) {
    auto leg = legend(P1);
    vanilla(P2, 2, 0);              // battlefield 0 is occupied: only 1 is open
    auto u1 = vanilla(P1, 2);
    auto u2 = vanilla(P1, 2);
    auto bard = addToHand(P1, kBard);
    EnginePathAgent agent(EnginePathAgent::script({1, static_cast<int>(u1), 0}));
    playThroughEngine(bard, &agent);
    EXPECT_TRUE(o(leg).is_exhausted);
    EXPECT_TRUE(atBf(u1, 1));
    EXPECT_TRUE(o(u2).isAtBase());
    EXPECT_TRUE(o(bard).isAtBase());
}

TEST_F(C6Test, Bard_DecliningTheCostMovesNothing) {
    auto leg = legend(P1);
    auto u1 = vanilla(P1, 2);
    auto bard = addToHand(P1, kBard);
    EnginePathAgent agent;          // answers "No"
    playThroughEngine(bard, &agent);
    EXPECT_FALSE(o(leg).is_exhausted);
    EXPECT_TRUE(o(u1).isAtBase());
}

// ── 543 Sett, Brawler ───────────────────────────────────────────────────────

TEST_F(C6Test, Sett543_NeedsItsOwnBuffToActivate) {
    auto buffed = addUnit(P1, kSett, 0, 0);
    auto plain = addUnit(P1, kSett, 0, 0);
    o(buffed).buff_count = 1;
    o(buffed).recomputeMight();
    EXPECT_TRUE(canActivate(buffed));
    EXPECT_FALSE(canActivate(plain));
}

TEST_F(C6Test, Sett543_SpendsTheBuffAsTheAbilityIsActivated) {
    auto sett = addUnit(P1, kSett, 0, 0);
    o(sett).buff_count = 1;
    o(sett).recomputeMight();
    GameState* live = nullptr;
    bool probed = false;
    PickByPredicateAgent p2([&](int, const std::vector<Intent>& legal) {
        if (!probed && live) {
            probed = true;
            EXPECT_EQ(live->getObject(sett).buff_count, 0) << "the buff is the cost";
            EXPECT_EQ(live->getObject(sett).temp_might_bonus, 0) << "not resolved yet";
        }
        for (const auto& i : legal)
            if (i.type == IntentType::PassPriority) return i;
        return legal.front();
    });
    Intent act;
    act.type = IntentType::ActivateAbility;
    act.player = P1;
    act.ability_source = sett;
    withEngine([&](GameEngine& e) {
        live = &e.mutableState();
        e.testHook_executeIntent(act);
        live = nullptr;
    }, nullptr, &p2);
    EXPECT_TRUE(probed);
    EXPECT_EQ(o(sett).buff_count, 0);
    EXPECT_EQ(o(sett).temp_might_bonus, 4);
}

// ── 544 Yone, Blademaster ───────────────────────────────────────────────────

TEST_F(C6Test, Yone_ConqueringAnUncontrolledBattlefieldHitsTheChosenBaseUnit) {
    auto yone = addUnit(P1, kYone, 0, 0);
    auto e1 = vanilla(P2, 9);
    auto e2 = vanilla(P2, 9);
    cleanup();                       // Yone sees battlefield 0 uncontrolled
    state.battlefields[0].controller = P1;
    EnginePathAgent agent(EnginePathAgent::choosing(e2));
    conquer(0, &agent);
    EXPECT_EQ(o(e2).damage_marked, might(yone));
    EXPECT_EQ(o(e1).damage_marked, 0);
}

TEST_F(C6Test, Yone_ConqueringAnEnemyBattlefieldDoesNothing) {
    addUnit(P1, kYone, 0, 0);
    auto e1 = vanilla(P2, 9);
    auto defender = vanilla(P2, 1, 0);
    state.battlefields[0].controller = P2;
    cleanup();                       // Yone sees battlefield 0 held by the enemy
    ASSERT_EQ(state.battlefields[0].controller, std::optional<PlayerId>(P2));
    o(defender).location = std::nullopt;         // the defender dies in the combat
    o(defender).zone = ZoneType::Trash;
    state.battlefields[0].controller = P1;
    EnginePathAgent agent(EnginePathAgent::choosing(e1));
    conquer(0, &agent, P2);          // conquered from P2 (the scoring step records it)
    EXPECT_EQ(o(e1).damage_marked, 0);
    EXPECT_TRUE(agent.prompts.empty());
}

TEST_F(C6Test, Yone_WeaponmasterIsAMay) {
    auto sword = gear(P1, kLongSword);
    auto yone = addToHand(P1, kYone);
    EnginePathAgent no;
    playThroughEngine(yone, &no);
    EXPECT_FALSE(o(sword).attached_to.has_value());

    auto sword2 = gear(P2, kLongSword);
    o(sword2).owner = o(sword2).controller = P1;
    o(sword2).location = BaseLocation{P1};
    auto yone2 = addToHand(P1, kYone);
    EnginePathAgent yes(EnginePathAgent::choosing(sword2));
    playThroughEngine(yone2, &yes);
    ASSERT_TRUE(o(sword2).attached_to.has_value());
    EXPECT_EQ(*o(sword2).attached_to, yone2);
}

// ── 548 Karma, Channeler ────────────────────────────────────────────────────

TEST_F(C6Test, Karma548_BuffsAChosenUnitWhenACardIsRecycled) {
    addUnit(P1, kKarma, 0, 0);
    auto a = vanilla(P1, 2, 0);
    auto b = vanilla(P1, 2, 0);
    auto card = addToHand(P1, kDiplomat);
    ps(P1).hand.pop_back();
    EnginePathAgent agent(EnginePathAgent::choosing(b));
    resolveThroughEngine([&](EffectExecutor& ex, GameState&) { ex.recycleCards(P1, {card}); },
                         &agent);
    EXPECT_EQ(o(b).buff_count, 1);
    EXPECT_EQ(o(a).buff_count, 0);
    // [Vision] is the engine's: the card has no play trigger of its own.
    EXPECT_FALSE(card_registry.get(kKarma)->firesOn(TriggerType::WhenYouPlayMe));
}

// ── 563 Prepared Neophyte / 565 Revna ───────────────────────────────────────

TEST_F(C6Test, PreparedNeophyte_KeepsTheBonusAfterACheaperSpell) {
    auto n = addUnit(P1, kPreparedNeophyte, 0, 0);
    ps(P1).max_spell_spent_this_turn = 5;
    ps(P1).last_spell_energy_spent = 1;
    cleanup();
    EXPECT_EQ(might(n), 5);
}

TEST_F(C6Test, Revna_ReadsTheTriggeringSpellNotTheLatestOne) {
    auto revna = addUnit(P1, kRevna, 0, 0);
    o(revna).is_exhausted = true;
    ps(P1).last_spell_energy_spent = 5;   // another, dearer spell
    // The trigger is for a spell that cost nothing.
    fireTriggerThroughEngine(revna, TriggerType::WhenYouPlayASpell);
    EXPECT_TRUE(o(revna).is_exhausted);
}

// ── 574 Lord Broadmane ──────────────────────────────────────────────────────

TEST_F(C6Test, LordBroadmane_HereIsTheBaseWhenPlayedThere) {
    auto ally = vanilla(P1, 2);
    auto away = vanilla(P1, 2, 0);
    auto lord = addToHand(P1, kLordBroadmane);
    playThroughEngine(lord);
    EXPECT_TRUE(o(ally).keywords.has(Keyword::Assault));
    EXPECT_FALSE(o(away).keywords.has(Keyword::Assault));
    EXPECT_FALSE(o(lord).keywords.has(Keyword::Assault));
}

// ── 578 Scorchclaw ──────────────────────────────────────────────────────────

TEST_F(C6Test, Scorchclaw_HuntIsNotGatedByLevel) {
    auto claw = addUnit(P1, kScorchclaw, 0, 0);
    o(claw).is_exhausted = true;
    state.battlefields[0].controller = P1;
    conquer(0);
    EXPECT_EQ(ps(P1).xp, 2);
    EXPECT_TRUE(o(claw).is_exhausted) << "nothing on the card readies it on a conquer";
}

TEST_F(C6Test, Scorchclaw_LevelThreeGivesMightAndEntersReady) {
    setXp(P1, 3);
    auto claw = addUnit(P1, kScorchclaw, 0, 0);
    cleanup();
    EXPECT_EQ(might(claw), 4);
    auto played = addToHand(P1, kScorchclaw);
    playThroughEngine(played);
    EXPECT_FALSE(o(played).is_exhausted);

    setXp(P1, 0);
    auto low = addToHand(P1, kScorchclaw);
    playThroughEngine(low);
    EXPECT_TRUE(o(low).is_exhausted);
}

// ── 580 Yeti Brawler ────────────────────────────────────────────────────────

TEST_F(C6Test, YetiBrawler_NeedsThreeExcessDamage) {
    addUnit(P1, kYetiBrawler, 0, 0);
    state.battlefields[0].controller = P1;
    conquer(0);
    EXPECT_EQ(goldTokens(P1), 0);
}

TEST_F(C6Test, YetiBrawler_PlaysTwoExhaustedGoldWithThreeExcess) {
    addUnit(P1, kYetiBrawler, 0, 0);
    state.battlefields[0].controller = P1;
    ps(P1).last_combat_bf = 0;
    ps(P1).excess_combat_damage = 3;
    conquer(0);
    EXPECT_EQ(goldTokens(P1), 2);
    for (auto& [id, obj] : state.objects)
        if (obj.isGear() && obj.card_def_id == kGold) { EXPECT_TRUE(obj.is_exhausted); }
}

// ── 583 Grim Apothecary ─────────────────────────────────────────────────────

TEST_F(C6Test, GrimApothecary_ReturnsTheUnitThePlayerPicks) {
    auto a = vanilla(P1, 2, 0);
    auto b = vanilla(P1, 2, 1);
    auto grim = addToHand(P1, kGrimApothecary);
    EnginePathAgent agent(EnginePathAgent::choosing(b));
    playThroughEngine(grim, &agent);
    EXPECT_FALSE(o(b).location.has_value());
    EXPECT_TRUE(o(a).location.has_value());
}

// ── 585 Katarina, Reckless ──────────────────────────────────────────────────

TEST_F(C6Test, Katarina_CannotChooseAnUntargetableEnemy) {
    auto kat = addUnit(P1, kKatarina, 0, 0);
    auto baron = addUnit(P2, kBaronNashor, 0, 1);
    auto other = vanilla(P2, 5, 1);
    cleanup();
    ASSERT_TRUE(o(baron).untargetable_by_enemy);
    EnginePathAgent agent(EnginePathAgent::choosing(baron));
    fireTriggerThroughEngine(kat, TriggerType::WhenYouPlayFromFacedown, &agent);
    EXPECT_FALSE(agent.wasOffered(baron));
    EXPECT_EQ(o(baron).damage_marked, 0);
    EXPECT_EQ(o(other).damage_marked, 2);
}

// ── 588 Xerath, Freed ───────────────────────────────────────────────────────

TEST_F(C6Test, Xerath_OnlyTheOneAtABattlefieldCanActivate) {
    auto at_bf = addUnit(P1, kXerath, 0, 0);
    auto in_base = addUnit(P1, kXerath);
    vanilla(P2, 3, 1);
    ps(P1).rune_pool.power[static_cast<int>(Domain::Fury)] = 2;
    EXPECT_TRUE(canActivate(at_bf));
    EXPECT_FALSE(canActivate(in_base));
}

// ── 589 Inviolus Vox (7.1) ──────────────────────────────────────────────────

TEST_F(C6Test, InviolusVox_GivesEightMightToTheChosenUnit) {
    addUnit(P1, kInviolusVox, 0, 0);
    auto ally = vanilla(P1, 2);
    state.battlefields[0].controller = P1;
    EnginePathAgent agent(EnginePathAgent::choosing(ally));
    conquer(0, &agent);
    EXPECT_EQ(might(ally), 10);
}

// ── 590 Pyke, Dockside Butcher ──────────────────────────────────────────────

TEST_F(C6Test, PykeButcher_ThePaidFlagIsUsedOnce) {
    auto pyke = addUnit(P1, kPykeButcher, 0, 0);
    o(pyke).is_exhausted = true;
    o(pyke).card_counters["__pyke_db_paid"] = 1;
    fireTriggerThroughEngine(pyke, TriggerType::WhenYouPlayMe);
    EXPECT_FALSE(o(pyke).is_exhausted);
    EXPECT_EQ(o(pyke).temp_might_bonus, 2);
    // Replayed later without paying: nothing.
    o(pyke).is_exhausted = true;
    fireTriggerThroughEngine(pyke, TriggerType::WhenYouPlayMe);
    EXPECT_TRUE(o(pyke).is_exhausted);
    EXPECT_EQ(o(pyke).temp_might_bonus, 2);
}

// ── 596 Herald of Spring / gainXp ───────────────────────────────────────────

TEST_F(C6Test, HeraldOfSpring_HuntGainsOneXpOnAHold) {
    addUnit(P1, kHeraldOfSpring, 0, 0);
    state.battlefields[0].controller = P1;
    hold(0);
    EXPECT_EQ(ps(P1).xp, 1);
    EXPECT_EQ(ps(P1).xp_gained_this_turn, 1);
}

// ── 609 Mosstomper / 637 Gustwalker / 675 Master Yi, Tempered ───────────────

TEST_F(C6Test, LevelKeywords_OnlyAtTheirLevel) {
    auto moss = addUnit(P1, kMosstomper, 0, 0);
    auto gust = addUnit(P1, kGustwalker, 0, 0);
    auto yi = addUnit(P1, kYiTempered, 0, 0);
    cleanup();
    EXPECT_EQ(deflectCostFor(state, P2, moss), 0);
    EXPECT_FALSE(o(gust).hasKeyword(Keyword::Ganking));
    EXPECT_EQ(deflectCostFor(state, P2, yi), 0);
    EXPECT_FALSE(o(yi).hasKeyword(Keyword::Ganking));

    setXp(P1, 6);
    cleanup();
    EXPECT_EQ(deflectCostFor(state, P2, moss), 1);
    EXPECT_TRUE(o(gust).hasKeyword(Keyword::Ganking));
    EXPECT_EQ(deflectCostFor(state, P2, yi), 1);
    EXPECT_TRUE(o(yi).hasKeyword(Keyword::Ganking));
}

// ── 612 Iascylla ────────────────────────────────────────────────────────────

TEST_F(C6Test, Iascylla_PullsAnEnemyOutOfItsBase) {
    auto ia = addUnit(P1, kIascylla, 0, 0);
    auto in_base = vanilla(P2, 3);
    state.battlefields[0].controller = P1;
    hold(0);
    ASSERT_EQ(state.delayed_abilities.size(), 1u);
    EnginePathAgent agent(EnginePathAgent::choosing(in_base));
    fireTriggerThroughEngine(ia, TriggerType::AtStartOfMain, &agent);
    EXPECT_TRUE(atBf(in_base, 0));
}

// ── 613 Ivern, Nurturer ─────────────────────────────────────────────────────

TEST_F(C6Test, Ivern_DrawsTheUnitThePlayerPicksAndBuffsTheChosenUnit) {
    auto ivern = addUnit(P1, kIvern, 0, 0);
    auto ally = vanilla(P1, 2, 0);
    addToDeck(P1, kDiplomat);                  // below the top 3
    auto third = addToDeck(P1, kDiplomat);
    auto cat = addToDeck(P1, kMutatedMouser);
    auto top = addToDeck(P1, kDiplomat);       // the first unit seen
    EnginePathAgent agent(EnginePathAgent::script({static_cast<int>(cat), static_cast<int>(ally)}));
    fireTriggerThroughEngine(ivern, TriggerType::WhenIHold, &agent);
    EXPECT_TRUE(inHand(P1, cat));
    EXPECT_TRUE(inDeck(P1, top));
    EXPECT_TRUE(inDeck(P1, third));
    EXPECT_EQ(handSize(P1), 1);
    EXPECT_EQ(deckSize(P1), 3);
    EXPECT_EQ(o(ally).buff_count, 1);
    EXPECT_EQ(o(ivern).buff_count, 0);
}

TEST_F(C6Test, Ivern_MayDrawNothing) {
    auto ivern = addUnit(P1, kIvern, 0, 0);
    addToDeck(P1, kDiplomat);
    addToDeck(P1, kDiplomat);
    EnginePathAgent agent(EnginePathAgent::script({0}));   // "None"
    fireTriggerThroughEngine(ivern, TriggerType::WhenIHold, &agent);
    EXPECT_EQ(handSize(P1), 0);
    EXPECT_EQ(deckSize(P1), 2);
}

// ── 614 Nami, Headstrong (7.12) ─────────────────────────────────────────────

TEST_F(C6Test, Nami_ReadiesAndBuffsTheUnitThatWasPlayed) {
    auto nami = addUnit(P1, kNami, 0, 0);
    auto bystander = vanilla(P1, 2);
    o(bystander).is_exhausted = true;
    state.battlefields[0].controller = P1;
    hold(0);
    auto played = addToHand(P1, kDiplomat);
    playThroughEngine(played);
    EXPECT_FALSE(o(played).is_exhausted);
    EXPECT_EQ(o(played).buff_count, 1);
    EXPECT_TRUE(o(bystander).is_exhausted);
    EXPECT_EQ(o(bystander).buff_count, 0);
    // One-shot: the next unit gets nothing.
    auto second = addToHand(P1, kDiplomat);
    playThroughEngine(second);
    EXPECT_TRUE(o(second).is_exhausted);
    (void)nami;
}

TEST_F(C6Test, Nami_DoesNothingForAUnitPlayedWithoutAHold) {
    addUnit(P1, kNami, 0, 0);
    auto played = addToHand(P1, kDiplomat);
    playThroughEngine(played);
    EXPECT_TRUE(o(played).is_exhausted);
    EXPECT_EQ(o(played).buff_count, 0);
}

// ── 615 Scuttle Crab ────────────────────────────────────────────────────────

TEST_F(C6Test, ScuttleCrab_ShowsTheOpponentsFacedownCards) {
    auto crab = addUnit(P1, kScuttleCrab, 0, 0);
    auto hidden = addToHand(P2, kDiplomat);
    ps(P2).hand.pop_back();
    o(hidden).zone = ZoneType::FacedownZone;
    o(hidden).is_hidden = true;
    state.battlefields[1].facedown.push_back(hidden);
    std::vector<GameObjectId> shown;
    auto conn = events.on_card_revealed.connect(
        [&](const CardRevealedEvent& e) { shown.push_back(e.card); });
    resolveThroughEngine([&](EffectExecutor& ex, GameState&) { ex.killObject(crab); });
    conn.disconnect();
    EXPECT_NE(std::find(shown.begin(), shown.end(), hidden), shown.end());
    EXPECT_EQ(ps(P1).xp, 1);
    EXPECT_EQ(ps(P1).xp_gained_this_turn, 1);
}

// ── 618 Yuumi, Magical Cat (7.1) ────────────────────────────────────────────

TEST_F(C6Test, Yuumi_GivesAnotherUnitHereMightAndTank) {
    auto yuumi = addUnit(P1, kYuumi, 0, 0);
    auto ally = vanilla(P1, 2, 0);
    auto away = vanilla(P1, 2, 1);
    EnginePathAgent agent(EnginePathAgent::choosing(ally));
    fireTriggerThroughEngine(yuumi, TriggerType::WhenIAttackOrDefend, &agent);
    EXPECT_EQ(might(ally), 5);
    EXPECT_TRUE(o(ally).hasKeyword(Keyword::Tank));
    EXPECT_FALSE(o(yuumi).hasKeyword(Keyword::Tank));
    EXPECT_FALSE(agent.wasOffered(away));
    EXPECT_FALSE(agent.wasOffered(yuumi));
}

// ── 619 Alpha Wildclaw ──────────────────────────────────────────────────────

TEST_F(C6Test, AlphaWildclaw_ProtectsSmallerFriendlyUnitsHere) {
    auto alpha = addUnit(P1, kAlphaWildclaw, 0, 0);
    auto small = vanilla(P1, 2, 0);
    auto big = vanilla(P1, 9, 0);
    auto away = vanilla(P1, 2, 1);
    auto enemy = vanilla(P2, 2, 0);
    cleanup();
    EXPECT_TRUE(o(small).untargetable_by_enemy);
    EXPECT_FALSE(o(big).untargetable_by_enemy);
    EXPECT_FALSE(o(away).untargetable_by_enemy);
    EXPECT_FALSE(o(enemy).untargetable_by_enemy);
    EXPECT_FALSE(o(alpha).untargetable_by_enemy);
    // The protection ends when the unit leaves.
    o(small).location = BattlefieldLocation{1};
    cleanup();
    EXPECT_FALSE(o(small).untargetable_by_enemy);
}

// ── 621 Master Yi, Unstoppable ──────────────────────────────────────────────

TEST_F(C6Test, YiUnstoppable_LevelElevenRemovesThePowerCostToo) {
    auto yi = addToHand(P1, kYiUnstoppable);
    ps(P1).rune_pool.energy = 6;               // [12][G][G][G] less [6][G][G][G]
    bool affordable = false;
    withEngine([&](GameEngine& e) { affordable = e.testHook_canAfford(P1, yi); });
    EXPECT_FALSE(affordable);
    setXp(P1, 11);
    withEngine([&](GameEngine& e) { affordable = e.testHook_canAfford(P1, yi); });
    EXPECT_TRUE(affordable);
}

TEST_F(C6Test, YiUnstoppable_LevelSixteenCannotBeChosen) {
    auto yi = addUnit(P1, kYiUnstoppable, 0, 0);
    setXp(P1, 15);
    cleanup();
    EXPECT_FALSE(o(yi).untargetable_by_enemy);
    setXp(P1, 16);
    cleanup();
    EXPECT_TRUE(o(yi).untargetable_by_enemy);
}

// ── 629 Ruined Rex / 630 Spectral Centaur ───────────────────────────────────

TEST_F(C6Test, RuinedRex_DealsFourToTheChosenEnemyEvenInItsBase) {
    auto rex = addUnit(P1, kRuinedRex, 0, 0);
    auto at_bf = vanilla(P2, 9, 0);
    auto in_base = vanilla(P2, 9);
    EnginePathAgent agent(EnginePathAgent::choosing(in_base));
    resolveThroughEngine([&](EffectExecutor& ex, GameState&) { ex.killObject(rex); }, &agent);
    EXPECT_EQ(o(in_base).damage_marked, 4);
    EXPECT_EQ(o(at_bf).damage_marked, 0);
}

TEST_F(C6Test, SpectralCentaur_GetsNothingOnceItHasLeftTheBoard) {
    auto centaur = addToHand(P1, kSpectralCentaur);
    fireTriggerThroughEngine(centaur, TriggerType::WhenAFriendlyUnitDies);
    EXPECT_EQ(o(centaur).temp_might_bonus, 0);
}

// ── 638 Petal Pixie ─────────────────────────────────────────────────────────

TEST_F(C6Test, PetalPixie_CountsTemporaryUnitsNotHerself) {
    auto pixie = addUnit(P1, kPetalPixie, 0, 0);
    cleanup();
    EXPECT_EQ(might(pixie), 2);
    auto sprite = token(P1, 3, 0);
    o(sprite).keywords.set(Keyword::Temporary);
    cleanup();
    EXPECT_EQ(might(pixie), 3);
}

// ── 643 Keeper of Masks (+ 648 Zilean) ──────────────────────────────────────

TEST_F(C6Test, KeeperOfMasks_ZileansExtraReflectionBecomesACopyToo) {
    addUnit(P1, kZilean, 0, 0);
    cleanup();                       // zilean_present
    auto keeper = addToHand(P1, kKeeperOfMasks);
    playThroughEngine(keeper);
    int reflections = 0;
    for (auto& [id, obj] : state.objects) {
        if (!obj.isToken() || !obj.isUnit() || !obj.location.has_value()) continue;
        ++reflections;
        EXPECT_EQ(obj.card_def_id, kKeeperOfMasks) << "a Reflection was left blank";
    }
    EXPECT_EQ(reflections, 3);
}

// ── 644 Lillia, Fae Fawn ────────────────────────────────────────────────────

TEST_F(C6Test, LilliaFawn_PlaysTheSpriteWhereSheMovedFromExhausted) {
    auto lillia = addUnit(P1, kLilliaFawn);
    resolveThroughEngine([&](EffectExecutor& ex, GameState&) { ex.moveToBattlefield(lillia, 0); });
    GameObjectId sprite = kInvalidId;
    for (auto& [id, obj] : state.objects)
        if (obj.isToken() && obj.isUnit() && obj.name == "Sprite") sprite = id;
    ASSERT_NE(sprite, kInvalidId);
    EXPECT_TRUE(o(sprite).isAtBase()) << "\"there\" is where she moved from";
    EXPECT_TRUE(o(sprite).is_exhausted);
    EXPECT_TRUE(o(sprite).keywords.has(Keyword::Temporary));
}

// ── 649 Blue Sentinel ───────────────────────────────────────────────────────

TEST_F(C6Test, BlueSentinel_AddsThePowerAtTheNextMainPhaseNotAtTheHold) {
    auto blue = addUnit(P1, kBlueSentinel, 0, 0);
    state.battlefields[0].controller = P1;
    hold(0);
    EXPECT_EQ(ps(P1).rune_pool.universal_power, 0);
    ASSERT_EQ(state.delayed_abilities.size(), 1u);
    EXPECT_EQ(state.delayed_abilities[0].trigger, TriggerType::AtStartOfMain);
    fireTriggerThroughEngine(blue, TriggerType::AtStartOfMain);
    EXPECT_EQ(ps(P1).rune_pool.universal_power, 1);
}

// ── 664 Crowd Favorite / 688 Megatusk ───────────────────────────────────────

TEST_F(C6Test, CrowdFavorite_SpendsTwoXpToBuffItself) {
    auto fav = addUnit(P1, kCrowdFavorite, 0, 0);
    setXp(P1, 1);
    EXPECT_FALSE(canActivate(fav));
    setXp(P1, 2);
    ASSERT_TRUE(canActivate(fav));
    activate(fav);
    EXPECT_EQ(ps(P1).xp, 0);
    EXPECT_EQ(o(fav).buff_count, 1);
}

TEST_F(C6Test, Megatusk_GivesGankingToUnitsHereIncludingInBase) {
    auto tusk = addUnit(P1, kMegatusk);
    auto ally = vanilla(P1, 2);
    auto away = vanilla(P1, 2, 0);
    cleanup();
    EXPECT_FALSE(o(tusk).hasKeyword(Keyword::Ganking));
    setXp(P1, 2);
    EXPECT_FALSE(canActivate(tusk));
    setXp(P1, 3);
    ASSERT_TRUE(canActivate(tusk));
    activate(tusk);
    EXPECT_EQ(ps(P1).xp, 0);
    EXPECT_TRUE(o(tusk).hasKeyword(Keyword::Ganking));
    EXPECT_TRUE(o(ally).hasKeyword(Keyword::Ganking));
    EXPECT_FALSE(o(away).hasKeyword(Keyword::Ganking));
}

// ── 666 Gentle Gemdragon ────────────────────────────────────────────────────

TEST_F(C6Test, GentleGemdragon_OnlyAnotherDragonReadiesRunes) {
    addUnit(P1, kGemdragon, 0, 0);
    auto r1 = addRune(P1, Domain::Body, /*exhausted=*/true);
    auto r2 = addRune(P1, Domain::Body, /*exhausted=*/true);
    auto plain = addToHand(P1, kDiplomat);
    playThroughEngine(plain);
    EXPECT_TRUE(o(r1).is_exhausted);
    EXPECT_TRUE(o(r2).is_exhausted);
    auto dragon = addToHand(P1, kInviolusVox);
    playThroughEngine(dragon);
    EXPECT_FALSE(o(r1).is_exhausted);
    EXPECT_FALSE(o(r2).is_exhausted);
}

// ── 667 Imposing Challenger ─────────────────────────────────────────────────

TEST_F(C6Test, ImposingChallenger_ThePlayerPicksTheDestination) {
    addBattlefield();                // a third battlefield (#2)
    auto imp = addUnit(P1, kImposingChallenger, 0, 0);
    auto enemy = vanilla(P2, 2, 0);
    // Yes; the enemy unit; then destination #2 (the second of {1, 2}).
    EnginePathAgent agent(EnginePathAgent::script({1, static_cast<int>(enemy), 1}));
    fireTriggerThroughEngine(imp, TriggerType::WhenIMove, &agent);
    EXPECT_TRUE(atBf(enemy, 2));
}

// ── 670 Wily Newtfish ───────────────────────────────────────────────────────

TEST_F(C6Test, WilyNewtfish_SeesXpGainedThisTurn) {
    auto newt = addUnit(P1, kNewtfish, 0, 0);
    cleanup();
    EXPECT_EQ(might(newt), 4);
    auto diplomat = addToHand(P1, kDiplomat);   // "When you play me, gain 1 XP."
    playThroughEngine(diplomat);
    cleanup();
    EXPECT_EQ(might(newt), 5);
    EXPECT_TRUE(o(newt).hasKeyword(Keyword::Ganking));
}

// ── 674 Irresistible Faefolk ────────────────────────────────────────────────

TEST_F(C6Test, Faefolk_PullsTheEnemyThePlayerPicks) {
    auto fae = addUnit(P1, kFaefolk, 0, 0);
    auto a = vanilla(P2, 2, 1);
    auto b = vanilla(P2, 2);
    EnginePathAgent agent(EnginePathAgent::choosing(b));
    fireTriggerThroughEngine(fae, TriggerType::WhenIMoveToFB, &agent);
    EXPECT_TRUE(atBf(b, 0));
    EXPECT_TRUE(atBf(a, 1));
}

// ── 678 Poppy, Paragon ──────────────────────────────────────────────────────

TEST_F(C6Test, Poppy_ReadiesAndGainsXpOnlyNearTheVictoryScore) {
    setScore(P2, state.mode.victory_score - 4);
    auto far = addToHand(P1, kPoppy);
    playThroughEngine(far);
    EXPECT_TRUE(o(far).is_exhausted);
    EXPECT_EQ(ps(P1).xp, 0);

    setScore(P2, state.mode.victory_score - 3);
    auto near = addToHand(P1, kPoppy);
    playThroughEngine(near);
    EXPECT_FALSE(o(near).is_exhausted);
    EXPECT_EQ(ps(P1).xp, 3);
}

// ── 679 Arachnoid Horror ────────────────────────────────────────────────────

TEST_F(C6Test, ArachnoidHorror_CanBePlayedWhereAnEnemyUnitIsAlone) {
    vanilla(P2, 2, 0);               // alone at battlefield 0
    vanilla(P2, 2, 1);
    vanilla(P2, 2, 1);               // two at battlefield 1
    state.battlefields[0].controller = P2;
    state.battlefields[1].controller = P2;
    auto horror = addToHand(P1, kArachnoidHorror);
    const auto& def = card_db.get(kArachnoidHorror);
    ps(P1).rune_pool.energy = def.energy_cost;
    ps(P1).rune_pool.universal_power = def.power_cost;
    bool base = false, lone = false, crowded = false;
    for (const auto& i : legalActions()) {
        if (i.type != IntentType::PlayCard || i.card != horror || !i.play_location) continue;
        if (*i.play_location == LocationId{BaseLocation{P1}}) base = true;
        if (*i.play_location == LocationId{BattlefieldLocation{0}}) lone = true;
        if (*i.play_location == LocationId{BattlefieldLocation{1}}) crowded = true;
    }
    EXPECT_TRUE(base);
    EXPECT_TRUE(lone);
    EXPECT_FALSE(crowded);
}

// ── 680 Elder Dragon ────────────────────────────────────────────────────────

TEST_F(C6Test, ElderDragon_ChoosesUpToOnePerLocation) {
    auto dragon = addUnit(P1, kElderDragon, 0, 0);
    auto base_a = vanilla(P2, 5);
    auto base_b = vanilla(P2, 5);
    auto at_bf = vanilla(P2, 5, 0);
    // The base: base_b. Battlefield 0: nobody.
    EnginePathAgent agent(EnginePathAgent::script({static_cast<int>(base_b), 0}));
    fireTriggerThroughEngine(dragon, TriggerType::WhenYouPlayMe, &agent);
    EXPECT_EQ(agent.prompts.size(), 2u);
    EXPECT_EQ(o(base_a).damage_marked, 0);
    EXPECT_EQ(o(at_bf).damage_marked, 0);
    EXPECT_EQ(o(base_b).damage_marked, 1);
}

// ── 681 Kha'Zix, Evolving Hunter ────────────────────────────────────────────

TEST_F(C6Test, KhaZix_SpendsTheXpBeforeAnyoneCanRespond) {
    auto kha = addUnit(P1, kKhaZixHunter, 0, 0);
    auto enemy = vanilla(P2, 9, 0);
    setXp(P1, 3);
    EnginePathAgent agent(EnginePathAgent::choosing(enemy));
    fireAndProbe(kha, TriggerType::WhenIAttack, &agent, [&](GameState& live) {
        EXPECT_EQ(live.player(P1).xp, 0);
        EXPECT_EQ(live.getObject(enemy).damage_marked, 0);
    });
    EXPECT_EQ(ps(P1).xp, 0);
    EXPECT_EQ(o(enemy).damage_marked, might(kha));
}

// ── 684 Crescent Guardian ───────────────────────────────────────────────────

TEST_F(C6Test, CrescentGuardian_TheCostIsOfferedOnlyAfterASpell) {
    auto first = addToHand(P1, kCrescentGuardian);
    ps(P1).rune_pool.power[static_cast<int>(Domain::Chaos)] = 1;
    EnginePathAgent yes(EnginePathAgent::script({1, 1}));
    playThroughEngine(first, &yes);
    EXPECT_TRUE(yes.prompts.empty());
    EXPECT_TRUE(o(first).is_exhausted);
    EXPECT_EQ(ps(P1).rune_pool.power[static_cast<int>(Domain::Chaos)], 1);

    // A spell that cost nothing still counts as a spell played.
    ps(P1).played_spell_this_turn = true;
    ps(P1).last_spell_energy_spent = 0;
    auto second = addToHand(P1, kCrescentGuardian);
    EnginePathAgent yes2(EnginePathAgent::script({1, 1}));
    playThroughEngine(second, &yes2);
    EXPECT_FALSE(o(second).is_exhausted);
    EXPECT_EQ(ps(P1).rune_pool.power[static_cast<int>(Domain::Chaos)], 0);
    EXPECT_EQ(o(second).card_counters.count("__crescent_paid"), 0u);
}

// ── 706 Maduli the Gatekeeper ───────────────────────────────────────────────

TEST_F(C6Test, Maduli_CannotBeReadied) {
    auto maduli = addUnit(P1, kMaduli, 0, 0);
    auto other = vanilla(P1, 2, 0);
    o(maduli).is_exhausted = true;
    o(other).is_exhausted = true;
    state.turn.turn_player = P1;
    withEngine([&](GameEngine& e) { e.testHook_awakenPhase(); });
    EXPECT_TRUE(o(maduli).is_exhausted);
    EXPECT_FALSE(o(other).is_exhausted);
    withEngine([&](GameEngine& e) { e.testHook_executor().readyObject(maduli); });
    EXPECT_TRUE(o(maduli).is_exhausted);
}

TEST_F(C6Test, Maduli_EachCopyIsJudgedOnItsOwnMight) {
    auto strong = addUnit(P1, kMaduli);
    auto weak = addUnit(P1, kMaduli, 1);
    vanilla(P2, 3, 0);
    state.battlefields[0].controller = P2;
    ps(P1).rune_pool.power[static_cast<int>(Domain::Chaos)] = 2;
    EXPECT_TRUE(canActivate(strong));
    EXPECT_FALSE(canActivate(weak));
}

// ── 707 Pyke, Returned ──────────────────────────────────────────────────────

TEST_F(C6Test, PykeReturned_PlaysOneExhaustedGoldWhenAnEnemyDies) {
    addUnit(P1, kPykeReturned, 0, 0);
    auto e1 = vanilla(P2, 1, 1);
    auto e2 = vanilla(P2, 1, 1);
    resolveThroughEngine([&](EffectExecutor& ex, GameState&) {
        ex.killObject(e1);
        ex.killObject(e2);
    });
    EXPECT_EQ(goldTokens(P1), 1) << "once each turn";
    for (auto& [id, obj] : state.objects)
        if (obj.isGear() && obj.card_def_id == kGold) { EXPECT_TRUE(obj.is_exhausted); }
}

}  // namespace
}  // namespace test_review_fix_c6
}  // namespace riftbound::test
