/// @file test_review_fix_c5.cpp
/// Regression tests for the 2026-10 engine review, card stream c5: the unit
/// cards units/0219 .. units/0494 (plans/cards/batch-05.md and the entries of
/// plans 02, 03, 07, 08, batch-09 and batch-10 that name them).
///
/// Every behavioural test goes through a real GameEngine on the fixture's
/// state (playThroughEngine / fireTriggerThroughEngine / withEngine): the card
/// is played or its trigger is queued, finalized and resolved by the engine,
/// and the agent — not the test — answers the choices. Data-only fixes are
/// one assertion on the card's def.

#include "tests/cards/card_test_fixture.h"
#include "cards/card_helpers.h"
#include "engine/effect_executor.h"
#include "engine/game_engine.h"

#include <gtest/gtest.h>

#include <algorithm>

namespace riftbound::test {
namespace test_review_fix_c5 {  // file-unique: test sources are unity-built
namespace {

constexpr CardDefId kMeditation = 48;          // spell, [2]
constexpr CardDefId kFallingComet = 85;        // spell, [5]
constexpr CardDefId kMegaMech = 88;            // unit, [7], Mech, 8 [M]
constexpr CardDefId kMobilize = 134;           // spell, [2]
constexpr CardDefId kKrakenHunter = 150;       // unit, [3][O][O]
constexpr CardDefId kDazzlingAurora = 160;     // gear, [9]
constexpr CardDefId kShipyardSkulker = 175;    // unit, [3], 3 [M], no text
constexpr CardDefId kScrapheap = 182;          // gear: "When this is ... killed, draw 1."
constexpr CardDefId kSpectralMatron = 226;
constexpr CardDefId kAlbusFerros = 230;
constexpr CardDefId kFioraVictorious = 232;
constexpr CardDefId kHarnessedDragon = 234;
constexpr CardDefId kKarmaChanneler = 235;
constexpr CardDefId kKarthusEternal = 236;
constexpr CardDefId kLeonaDetermined = 238;
constexpr CardDefId kSettKingpin = 240;
constexpr CardDefId kViktorLeader = 246;
constexpr CardDefId kLuxIlluminated = 304;
constexpr CardDefId kLuxCrownguard = 312;
constexpr CardDefId kTibbers = 316;
constexpr CardDefId kArmedAssailant = 324;
constexpr CardDefId kGemJammer = 330;
constexpr CardDefId kSentinelAdept = 331;
constexpr CardDefId kSerratedDirk = 332;       // gear, Equipment, [1]
constexpr CardDefId kBlastCorpsCadet = 336;
constexpr CardDefId kMinotaurReckoner = 337;
constexpr CardDefId kVoidHatchling = 341;
constexpr CardDefId kDravenVanquisher = 343;
constexpr CardDefId kRellMagnetic = 347;
constexpr CardDefId kRumbleHotheaded = 349;
constexpr CardDefId kDoransShield = 356;       // gear, Equipment, [1]
constexpr CardDefId kRibbonDancer = 361;
constexpr CardDefId kApprenticeSmith = 364;
constexpr CardDefId kLegionQuartermaster = 367;
constexpr CardDefId kAphelios = 372;
constexpr CardDefId kAzir = 373;
constexpr CardDefId kJannaSavior = 376;
constexpr CardDefId kJaxUnmatched = 377;
constexpr CardDefId kNeedlesslyLargeYordle = 378;
constexpr CardDefId kIreliaFervent = 380;
constexpr CardDefId kOrnnBlacksmith = 381;
constexpr CardDefId kAspiringEngineer = 384;
constexpr CardDefId kBubbleBot = 385;
constexpr CardDefId kBreakneckMech = 394;
constexpr CardDefId kPickpocket = 397;
constexpr CardDefId kCardSharp = 403;
constexpr CardDefId kJayceManOfProgress = 406;
constexpr CardDefId kOrnnForgeGod = 407;
constexpr CardDefId kRenataMastermind = 410;
constexpr CardDefId kBuhruCaptain = 413;
constexpr CardDefId kDirewing = 416;
constexpr CardDefId kVeteranPoro = 421;
constexpr CardDefId kYordleExplorer = 422;
constexpr CardDefId kFaeDragon = 423;
constexpr CardDefId kAkshan = 431;
constexpr CardDefId kFioraPeerless = 432;
constexpr CardDefId kLucianMerciless = 435;
constexpr CardDefId kSivirAmbitious = 441;
constexpr CardDefId kCorruptEnforcer = 444;
constexpr CardDefId kFaePorter = 446;
constexpr CardDefId kLoyalPup = 447;
constexpr CardDefId kOverzealousFan = 449;
constexpr CardDefId kTreasureHunter = 451;
constexpr CardDefId kBeastBelow = 453;
constexpr CardDefId kHarpoonSquad = 458;
constexpr CardDefId kWindsinger = 459;
constexpr CardDefId kFizzTrickster = 461;
constexpr CardDefId kJaeMedarda = 463;
constexpr CardDefId kSivirMercenary = 464;
constexpr CardDefId kHonestBroker = 476;
constexpr CardDefId kSandshifter = 479;
constexpr CardDefId kTrustyRamhound = 480;
constexpr CardDefId kZaunPunk = 481;
constexpr CardDefId kGlascMixologist = 486;
constexpr CardDefId kRekSai = 491;
constexpr CardDefId kRenataIndustrialist = 492;

class C5Test : public CardTestFixture {
protected:
    /// A gear on the board in `owner`'s base. With a def id it is that card;
    /// without one it is a card-less gear (as a token is).
    GameObjectId addGear(PlayerId owner, CardDefId def_id = kInvalidId, bool equipment = false) {
        auto id = state.createObject();
        auto& g = state.getObject(id);
        g.owner = g.controller = owner;
        g.card_type = CardType::Gear;
        g.card_def_id = def_id;
        g.name = "TestGear";
        if (def_id != kInvalidId) {
            const auto& def = card_db.get(def_id);
            g.name = def.name;
            g.tags = def.tags;
            g.keywords = def.keywords;
        }
        if (equipment) g.tags.push_back("Equipment");
        g.zone = ZoneType::Base;
        g.location = BaseLocation{owner};
        return id;
    }
    /// Attach `gear` to `unit` (board bookkeeping only).
    void attach(GameObjectId gear, GameObjectId unit) {
        state.getObject(gear).attached_to = unit;
        state.getObject(gear).location = state.getObject(unit).location;
        state.getObject(unit).attachments.push_back(gear);
    }
    /// A card in `owner`'s trash.
    GameObjectId addToTrash(PlayerId owner, CardDefId def_id) {
        auto id = addToHand(owner, def_id);
        auto& ps = state.player(owner);
        ps.hand.erase(std::remove(ps.hand.begin(), ps.hand.end(), id), ps.hand.end());
        state.getObject(id).zone = ZoneType::Trash;
        ps.trash.push_back(id);
        return id;
    }
    /// A unit with a tag and no card of its own.
    GameObjectId addTaggedUnit(PlayerId owner, const std::string& tag, int might, int at_bf = -1) {
        auto id = addUnit(owner, kInvalidId, might, at_bf);
        state.getObject(id).tags = {tag};
        return id;
    }
    /// Move a unit off the board into its owner's trash (as if it had died).
    void sendToTrash(GameObjectId id) {
        auto& o = state.getObject(id);
        o.location = std::nullopt;
        o.zone = ZoneType::Trash;
        state.player(o.owner).trash.push_back(id);
    }
    /// A combat at `bf`: cleanup keeps Attacker / Defender designations in
    /// step with it (CR 323.2).
    void startCombat(BattlefieldId bf, PlayerId attacker, PlayerId defender) {
        auto& b = state.battlefields[bf];
        b.combat_in_progress = true;
        b.attacker = attacker;
        b.defender = defender;
        for (auto& [id, o] : state.objects)
            if (o.isUnit() && o.battlefieldId() == std::optional<BattlefieldId>{bf})
                o.combat_designation = o.controller == attacker ? CombatDesignation::Attacker
                                                                : CombatDesignation::Defender;
    }
    void endCombat(BattlefieldId bf) {
        state.battlefields[bf].combat_in_progress = false;
        for (auto& [id, o] : state.objects) o.combat_designation = CombatDesignation::None;
    }
    /// One cleanup (the aura pass included) on a real engine.
    void cleanup() { withEngine([](GameEngine& e) { e.testHook_cleanup(); }); }
    bool onBoard(GameObjectId id) { return state.getObject(id).location.has_value(); }
    bool has(GameObjectId id, Keyword kw) { return state.getObject(id).hasKeyword(kw); }
    int might(GameObjectId id) { return state.getObject(id).current_might; }
    int goldCount(PlayerId p, bool only_exhausted = false) {
        int n = 0;
        for (auto& [id, o] : state.objects)
            if (o.isGear() && o.controller == p && o.location.has_value() && o.name == "Gold" &&
                (!only_exhausted || o.is_exhausted)) ++n;
        return n;
    }
    const Card& card(CardDefId id) { return *card_registry.get(id); }
    bool defHas(CardDefId id, Keyword kw) { return card_db.get(id).keywords.has(kw); }
};

// ─── Data ───────────────────────────────────────────────────────────────────

TEST_F(C5Test, KeywordsTheTextOnlyMentionsAreNotOnTheDef) {
    // batch-09 Gem Jammer / 8.8: the text GRANTS [Ganking]; the Jammer has none.
    EXPECT_FALSE(defHas(kGemJammer, Keyword::Ganking));
    // batch-05 Karthus: he has no [Deathknell] ability of his own.
    EXPECT_FALSE(defHas(kKarthusEternal, Keyword::Deathknell));
    // batch-05 Sivir, Mercenary: [Ganking] only "if you've spent [A][A]".
    EXPECT_FALSE(defHas(kSivirMercenary, Keyword::Ganking));
    // batch-05 Breakneck Mech: [Deflect] / [Ganking] come from "Your Mechs have".
    EXPECT_FALSE(defHas(kBreakneckMech, Keyword::Deflect));
    EXPECT_FALSE(defHas(kBreakneckMech, Keyword::Ganking));
    EXPECT_EQ(card_db.get(kBreakneckMech).deflect_value, 0);
    // batch-05 Lux, Crownguard / Jax, Unmatched: only an ability (or the
    // Equipment) is a [Reaction]; the unit is not.
    EXPECT_FALSE(defHas(kLuxCrownguard, Keyword::Reaction));
    EXPECT_FALSE(defHas(kJaxUnmatched, Keyword::Reaction));
    EXPECT_FALSE(defHas(kJaxUnmatched, Keyword::QuickDraw));
    // 03 item 3.21: a [Weaponmaster] unit has no [Equip] ability.
    for (CardDefId id : {kArmedAssailant, kSentinelAdept, kOrnnForgeGod, kVeteranPoro,
                         kLucianMerciless})
        EXPECT_FALSE(defHas(id, Keyword::Equip)) << id;
    // batch-10 Windsinger: printed [Hidden].
    EXPECT_TRUE(defHas(kWindsinger, Keyword::Hidden));
}

TEST_F(C5Test, KarmaChanneler235IsAKarma) {
    // 03 item 3.19 / STATUS carry-over: a champion unit carries its champion's
    // tag; "Vi" made her a legal Chosen Champion for a Vi legend.
    const auto& tags = card_db.get(kKarmaChanneler).tags;
    EXPECT_NE(std::find(tags.begin(), tags.end(), "Karma"), tags.end());
    EXPECT_EQ(std::find(tags.begin(), tags.end(), "Vi"), tags.end());
}

// ─── 7.1 — triggers that choose their own targets ───────────────────────────

TEST_F(C5Test, HarnessedDragonKillsTheChosenEnemyUnit) {
    auto dragon = addToHand(P1, kHarnessedDragon);
    auto spared = addUnit(P2, kInvalidId, 3, 0);
    auto victim = addUnit(P2, kInvalidId, 3);
    auto mine = addUnit(P1, kInvalidId, 3);
    EnginePathAgent agent(EnginePathAgent::choosing(victim));
    playThroughEngine(dragon, &agent);
    ASSERT_FALSE(agent.prompts.empty()) << "the controller is asked";
    EXPECT_FALSE(agent.wasOffered(mine)) << "an ENEMY unit";
    EXPECT_FALSE(onBoard(victim));
    EXPECT_TRUE(onBoard(spared));
}

TEST_F(C5Test, LeonaDeterminedStunsAnEnemyUnitHere) {
    auto leona = addUnit(P1, kLeonaDetermined, 0, 0);
    auto here = addUnit(P2, kInvalidId, 3, 0);
    auto elsewhere = addUnit(P2, kInvalidId, 3, 1);
    EnginePathAgent agent;
    fireTriggerThroughEngine(leona, TriggerType::WhenIAttack, &agent);
    ASSERT_FALSE(agent.prompts.empty());
    EXPECT_FALSE(agent.wasOffered(elsewhere)) << "an enemy unit HERE";
    EXPECT_TRUE(state.getObject(here).is_stunned);
    EXPECT_FALSE(state.getObject(elsewhere).is_stunned);
}

TEST_F(C5Test, GemJammerGivesTheChosenUnitGanking) {
    auto jammer = addToHand(P1, kGemJammer);
    auto ally = addUnit(P1, kInvalidId, 2);
    EnginePathAgent agent(EnginePathAgent::choosing(ally));
    playThroughEngine(jammer, &agent);
    ASSERT_FALSE(agent.prompts.empty());
    EXPECT_TRUE(state.getObject(ally).keywords.has(Keyword::Ganking));
    EXPECT_FALSE(state.getObject(jammer).keywords.has(Keyword::Ganking));
}

TEST_F(C5Test, RibbonDancerBuffsAnotherFriendlyUnit) {
    auto dancer = addUnit(P1, kRibbonDancer, 0, 0);
    auto ally = addUnit(P1, kInvalidId, 2);
    EnginePathAgent agent;
    fireTriggerThroughEngine(dancer, TriggerType::WhenIMoveToFB, &agent);
    ASSERT_FALSE(agent.prompts.empty());
    EXPECT_FALSE(agent.wasOffered(dancer)) << "ANOTHER friendly unit";
    EXPECT_EQ(state.getObject(ally).temp_might_bonus, 1);
}

TEST_F(C5Test, AspiringEngineerReturnsAGearFromTheTrash) {
    // The old body bounced a gear that was on the board.
    auto engineer = addToHand(P1, kAspiringEngineer);
    auto on_board = addGear(P1, kSerratedDirk);
    auto trashed = addToTrash(P1, kDoransShield);
    EnginePathAgent agent;
    playThroughEngine(engineer, &agent);
    ASSERT_FALSE(agent.prompts.empty());
    EXPECT_FALSE(agent.wasOffered(on_board));
    EXPECT_TRUE(inHand(P1, trashed));
    EXPECT_FALSE(inTrash(P1, trashed));
    EXPECT_TRUE(onBoard(on_board));
}

TEST_F(C5Test, BeastBelowReturnsAFriendlyAndAnEnemyUnit) {
    auto beast = addToHand(P1, kBeastBelow);
    auto ally = addUnit(P1, kShipyardSkulker, 0);
    auto enemy = addUnit(P2, kShipyardSkulker, 0, 0);
    EnginePathAgent agent;
    playThroughEngine(beast, &agent);
    EXPECT_GE(agent.prompts.size(), 2u) << "two units are chosen";
    EXPECT_FALSE(agent.wasOffered(beast)) << "ANOTHER friendly unit";
    EXPECT_TRUE(inHand(P1, ally));
    EXPECT_TRUE(inHand(P2, enemy));
    EXPECT_TRUE(onBoard(beast));
}

TEST_F(C5Test, BeastBelowAloneStillReturnsAnEnemyUnit) {
    auto beast = addToHand(P1, kBeastBelow);
    auto enemy = addUnit(P2, kShipyardSkulker, 0, 0);
    EnginePathAgent agent;
    playThroughEngine(beast, &agent);
    EXPECT_TRUE(inHand(P2, enemy));
}

TEST_F(C5Test, SandshifterKillsAnEnemyUnitWithThreeMightOrLess) {
    auto shifter = addToHand(P1, kSandshifter);
    auto big = addUnit(P2, kInvalidId, 4, 0);
    auto small = addUnit(P2, kInvalidId, 3, 0);
    EnginePathAgent agent;
    playThroughEngine(shifter, &agent);
    ASSERT_FALSE(agent.prompts.empty());
    EXPECT_FALSE(agent.wasOffered(big));
    EXPECT_FALSE(onBoard(small));
    EXPECT_TRUE(onBoard(big));
}

// ─── batch-05 findings ──────────────────────────────────────────────────────

TEST_F(C5Test, FioraVictoriousHasHerKeywordsOnlyWhileMighty) {
    // CR 478.4 examples: 4 [M] and no keywords; buffed, 5 [M] with all three;
    // buffed and defending, 6 [M]; the buff removed while she defends, straight
    // back to 4 [M] with no keywords (her own [Shield] does not keep her Mighty).
    auto fiora = addUnit(P1, kFioraVictorious, 0, 0);
    cleanup();
    EXPECT_FALSE(has(fiora, Keyword::Deflect));
    EXPECT_FALSE(has(fiora, Keyword::Ganking));
    EXPECT_FALSE(has(fiora, Keyword::Shield));
    EXPECT_EQ(might(fiora), 4);

    addUnit(P2, kInvalidId, 1, 0);
    startCombat(0, P2, P1);
    cleanup();
    cleanup();
    EXPECT_EQ(might(fiora), 4) << "defending at 4 [M] does not make her Mighty";
    EXPECT_FALSE(has(fiora, Keyword::Shield));
    endCombat(0);

    state.getObject(fiora).buff_count = 1;
    state.getObject(fiora).recomputeMight();
    cleanup();
    EXPECT_TRUE(has(fiora, Keyword::Deflect));
    EXPECT_TRUE(has(fiora, Keyword::Ganking));
    EXPECT_TRUE(has(fiora, Keyword::Shield));
    EXPECT_EQ(might(fiora), 5);

    startCombat(0, P2, P1);
    state.getObject(fiora).recomputeMight();
    cleanup();
    cleanup();
    EXPECT_EQ(might(fiora), 6);
    EXPECT_TRUE(has(fiora, Keyword::Shield)) << "still Mighty on the next pass";

    state.getObject(fiora).buff_count = 0;
    state.getObject(fiora).recomputeMight();
    cleanup();
    EXPECT_EQ(might(fiora), 4);
    EXPECT_FALSE(has(fiora, Keyword::Deflect));
    EXPECT_FALSE(has(fiora, Keyword::Shield));
}

TEST_F(C5Test, KarthusRepeatsOnlyDeathknellAbilities) {
    addUnit(P1, kKarthusEternal, 0);
    auto scrapheap = addGear(P1, kScrapheap);   // "When this is ... killed, draw 1." — not a Deathknell
    auto broker = addUnit(P1, kHonestBroker, 0);   // "[Deathknell] — Play a Gold gear token exhausted."
    for (int i = 0; i < 4; ++i) addToDeck(P1, kShipyardSkulker);
    withEngine([&](GameEngine& e) {
        e.testHook_cleanup();
        e.testHook_executor().killObject(scrapheap);
        e.testHook_runChain();
    });
    EXPECT_EQ(handSize(P1), 1) << "a plain 'when killed' ability triggers once";
    withEngine([&](GameEngine& e) {
        e.testHook_cleanup();
        e.testHook_executor().killObject(broker);
        e.testHook_runChain();
    });
    EXPECT_EQ(goldCount(P1), 2) << "the [Deathknell] triggers an additional time";
    EXPECT_EQ(goldCount(P1, /*only_exhausted=*/true), 2) << "Gold gear token EXHAUSTED";
}

TEST_F(C5Test, SettKingpinCountsHimself) {
    // "+1 [M] for each buffed friendly unit at my battlefield" — no "other".
    auto sett = addUnit(P1, kSettKingpin, 0, 0);
    state.getObject(sett).buff_count = 1;
    cleanup();
    EXPECT_EQ(might(sett), 7);   // 5 + his buff + 1 for himself
}

TEST_F(C5Test, ViktorLeaderIgnoresRecruitsDying) {
    addUnit(P1, kViktorLeader, 0);
    auto recruit = addTaggedUnit(P1, "Recruit", 1);
    auto other = addUnit(P1, kShipyardSkulker, 0);
    auto recruits = [&]() {
        int n = 0;
        for (auto& [id, o] : state.objects)
            if (o.isUnit() && o.location.has_value() && o.controller == P1 &&
                std::find(o.tags.begin(), o.tags.end(), "Recruit") != o.tags.end()) ++n;
        return n;
    };
    resolveThroughEngine([&](EffectExecutor& ex, GameState&) { ex.killObject(recruit); });
    EXPECT_EQ(recruits(), 0) << "a Recruit dying makes no new Recruit";
    resolveThroughEngine([&](EffectExecutor& ex, GameState&) { ex.killObject(other); });
    EXPECT_EQ(recruits(), 1) << "another non-Recruit unit dying does";
}

TEST_F(C5Test, LuxIlluminatedReadsTheSpellThatWasPlayed) {
    auto lux = addUnit(P1, kLuxIlluminated, 0);
    // The 5+ spell was banished on resolution; an older cheap spell is the
    // newest one in the trash.
    addToTrash(P1, kMeditation);
    auto big = addToHand(P1, kFallingComet);
    state.player(P1).hand.clear();
    state.getObject(big).zone = ZoneType::Banishment;
    state.player(P1).banishment.push_back(big);
    fireTriggerThroughEngine(lux, TriggerType::WhenYouPlayASpell, nullptr, big);
    EXPECT_EQ(state.getObject(lux).temp_might_bonus, 3);

    auto cheap = addToTrash(P1, kMeditation);
    fireTriggerThroughEngine(lux, TriggerType::WhenYouPlayASpell, nullptr, cheap);
    EXPECT_EQ(state.getObject(lux).temp_might_bonus, 3) << "a [2] spell gives nothing";
}

TEST_F(C5Test, TibbersSparesUnitsInBases) {
    auto tibbers = addToHand(P1, kTibbers);
    auto in_base = addUnit(P1, kInvalidId, 3);
    auto enemy_base = addUnit(P2, kInvalidId, 3);
    auto at_bf = addUnit(P2, kInvalidId, 3, 0);
    playThroughEngine(tibbers);
    EXPECT_TRUE(onBoard(in_base));
    EXPECT_EQ(state.getObject(in_base).damage_marked, 0);
    EXPECT_TRUE(onBoard(enemy_base));
    EXPECT_FALSE(onBoard(at_bf)) << "deal 3 to all units at battlefields";
    EXPECT_EQ(state.getObject(tibbers).damage_marked, 0) << "Tibbers himself is in base";
}

TEST_F(C5Test, MinotaurReckonerStopsMovesToBaseOnlyWhileOnBoard) {
    auto reckoner = addUnit(P1, kMinotaurReckoner, 0, 0);
    auto unit = addUnit(P2, kInvalidId, 3, 1);
    state.battlefields[1].controller = P2;
    auto can_go_home = [&]() {
        bool found = false;
        withEngine([&](GameEngine& e) {
            e.testHook_cleanup();
            for (const auto& i : e.generateLegalActions())
                if (i.type == IntentType::StandardMove &&
                    std::find(i.units_to_move.begin(), i.units_to_move.end(), unit) != i.units_to_move.end() &&
                    i.move_destination.has_value() &&
                    std::holds_alternative<BaseLocation>(*i.move_destination))
                    found = true;
        }, nullptr, nullptr, P2);
        return found;
    };
    state.turn.phase = TurnPhase::MainPhase;
    state.turn.turn_player = P2;
    EXPECT_FALSE(can_go_home()) << "Units can't move to base.";
    EXPECT_TRUE(state.getObject(unit).aura_cant_move_to_base);
    sendToTrash(reckoner);
    EXPECT_TRUE(can_go_home()) << "the restriction ends when the Reckoner leaves";
    EXPECT_FALSE(state.getObject(unit).aura_cant_move_to_base);
}

TEST_F(C5Test, HarpoonSquadOnlyWhenItMovesFromABattlefield) {
    auto squad = addUnit(P1, kHarpoonSquad, 0, 0);
    const Card& c = card(kHarpoonSquad);
    state.getObject(squad).card_counters["__moved_from_bf"] = -1;   // out of a base
    EXPECT_FALSE(c.triggerCondition(state, squad, P1, TriggerType::WhenIMove, kInvalidId));
    state.getObject(squad).card_counters["__moved_from_bf"] = 1;
    EXPECT_TRUE(c.triggerCondition(state, squad, P1, TriggerType::WhenIMove, kInvalidId));
    // Through the engine: base -> battlefield gives nothing, battlefield ->
    // base gives +2.
    auto mover = addUnit(P1, kHarpoonSquad, 0);
    resolveThroughEngine([&](EffectExecutor& ex, GameState&) { ex.moveToBattlefield(mover, 1); });
    EXPECT_EQ(state.getObject(mover).temp_might_bonus, 0);
    resolveThroughEngine([&](EffectExecutor& ex, GameState&) { ex.moveToBase(mover); });
    EXPECT_EQ(state.getObject(mover).temp_might_bonus, 2);
}

TEST_F(C5Test, SivirAmbitiousDealsTheExcessDamage) {
    auto sivir = addUnit(P1, kSivirAmbitious, 0, 0);
    auto enemy = addUnit(P2, kInvalidId, 6, 1);
    auto& ps = state.player(P1);
    ps.last_combat_bf = 0;
    ps.last_combat_attacker = true;
    ps.excess_combat_damage = 4;
    EXPECT_FALSE(card(kSivirAmbitious).triggerCondition(state, sivir, P1, TriggerType::WhenIConquer,
                                                        kInvalidId)) << "5 or more";
    ps.excess_combat_damage = 6;
    EXPECT_TRUE(card(kSivirAmbitious).triggerCondition(state, sivir, P1, TriggerType::WhenIConquer,
                                                       kInvalidId));
    EnginePathAgent agent(EnginePathAgent::choosing(enemy));
    fireTriggerThroughEngine(sivir, TriggerType::WhenIConquer, &agent);
    ASSERT_GE(agent.prompts.size(), 2u) << "the may, then the unit";
    EXPECT_FALSE(onBoard(enemy)) << "6 excess damage kills a 6 [M] unit";
}

TEST_F(C5Test, GoldTokensEnterExhaustedUnlessRenataReadiesThem) {
    auto hunter = addUnit(P1, kTreasureHunter, 0, 0);
    fireTriggerThroughEngine(hunter, TriggerType::WhenIMove);
    EXPECT_EQ(goldCount(P1), 1);
    EXPECT_EQ(goldCount(P1, /*only_exhausted=*/true), 1) << "play a Gold gear token EXHAUSTED";

    // "Your tokens enter ready." replaces how the token enters (CR 184.1).
    addUnit(P1, kRenataIndustrialist, 0);
    withEngine([&](GameEngine& e) {
        e.testHook_cleanup();
        e.testHook_executor().queueTriggeredAbility(hunter, P1, TriggerType::WhenIMove);
        e.testHook_runChain();
    });
    EXPECT_EQ(goldCount(P1), 2);
    EXPECT_EQ(goldCount(P1, /*only_exhausted=*/true), 1) << "the second Gold entered ready";
}

TEST_F(C5Test, BuhruCaptainMayDrawOrBuffOrNeither) {
    for (int i = 0; i < 3; ++i) addToDeck(P1, kShipyardSkulker);
    auto buff_me = addToHand(P1, kBuhruCaptain);
    EnginePathAgent buff(EnginePathAgent::script({1}));
    playThroughEngine(buff_me, &buff);
    ASSERT_EQ(buff.prompts.size(), 1u);
    EXPECT_EQ(buff.prompts[0].size(), 3u) << "draw 1, buff me, or neither";
    EXPECT_EQ(state.getObject(buff_me).buff_count, 1);
    EXPECT_EQ(handSize(P1), 0);

    auto draw = addToHand(P1, kBuhruCaptain);
    EnginePathAgent draws(EnginePathAgent::script({0}));
    playThroughEngine(draw, &draws);
    EXPECT_EQ(handSize(P1), 1);
    EXPECT_EQ(state.getObject(draw).buff_count, 0);

    auto neither = addToHand(P1, kBuhruCaptain);
    EnginePathAgent declines(EnginePathAgent::script({2}));
    playThroughEngine(neither, &declines);
    EXPECT_EQ(handSize(P1), 1);
    EXPECT_EQ(state.getObject(neither).buff_count, 0);
}

TEST_F(C5Test, AkshanHasWeaponmasterAndChoosesTheGearHeSteals) {
    auto akshan = addUnit(P1, kAkshan, 0, 0);
    auto plain = addGear(P2);
    auto equipment = addGear(P2, kInvalidId, /*equipment=*/true);
    state.getObject(akshan).card_counters["__akshan_paid"] = 1;
    EnginePathAgent agent(EnginePathAgent::choosing(equipment));
    fireTriggerThroughEngine(akshan, TriggerType::AsYouPlayMe, &agent);
    EXPECT_TRUE(agent.wasOffered(plain)) << "the controller chooses among the enemy gear";
    EXPECT_EQ(state.getObject(equipment).controller, P1);
    EXPECT_EQ(state.getObject(equipment).attached_to, std::optional<GameObjectId>{akshan});
    EXPECT_EQ(state.getObject(plain).controller, P2);
    EXPECT_EQ(state.getObject(akshan).card_counters.count("__akshan_paid"), 0u)
        << "the payment covers one play";

    // [Weaponmaster]: a may, with a choice of Equipment.
    auto own = addGear(P1, kInvalidId, /*equipment=*/true);
    EnginePathAgent wm(EnginePathAgent::choosing(own));
    fireTriggerThroughEngine(akshan, TriggerType::WhenYouPlayMe, &wm);
    ASSERT_FALSE(wm.prompts.empty());
    EXPECT_EQ(state.getObject(own).attached_to, std::optional<GameObjectId>{akshan});
}

TEST_F(C5Test, WeaponmasterUnitsUseTheSharedMay) {
    // Ornn, Forge God and Lucian, Merciless carried their own forced copies.
    for (CardDefId def : {kOrnnForgeGod, kLucianMerciless, kSentinelAdept}) {
        auto gear = addGear(P1, kInvalidId, /*equipment=*/true);
        auto unit = addToHand(P1, def);
        EnginePathAgent no(EnginePathAgent::script({0}));
        playThroughEngine(unit, &no);
        ASSERT_TRUE(onBoard(unit)) << def;
        ASSERT_FALSE(no.prompts.empty()) << def << ": the may is asked";
        EXPECT_FALSE(state.getObject(gear).attached_to.has_value()) << def;
    }
    // A unit that left the board is not equipped.
    auto gear = addGear(P1, kInvalidId, /*equipment=*/true);
    auto dead = addUnit(P1, kOrnnForgeGod, 0);
    sendToTrash(dead);
    fireTriggerThroughEngine(dead, TriggerType::WhenYouPlayMe);
    EXPECT_FALSE(state.getObject(gear).attached_to.has_value());
    EXPECT_TRUE(onBoard(gear));
}

TEST_F(C5Test, JaxUnmatchedGivesEquipmentQuickDraw) {
    addUnit(P1, kJaxUnmatched, 0);
    auto in_hand = addToHand(P1, kSerratedDirk);
    cleanup();
    EXPECT_TRUE(has(in_hand, Keyword::QuickDraw)) << "Your Equipment EVERYWHERE";
}

TEST_F(C5Test, NeedlesslyLargeYordleCostsLessPowerToo) {
    state.player(P1).hold_points_this_turn = 2;
    EXPECT_EQ(card(kNeedlesslyLargeYordle).selfCostReduction(state, P1), 4);
    EXPECT_EQ(card(kNeedlesslyLargeYordle).selfPowerCostReduction(state, P1), 2);
}

TEST_F(C5Test, IreliaFerventOnlyWhenSheIsTheOneChosen) {
    auto irelia = addUnit(P1, kIreliaFervent, 0);
    auto other = addUnit(P1, kInvalidId, 2);
    resolveThroughEngine([&](EffectExecutor&, GameState&) {
        events.emit(ObjectStateChangedEvent{other, "chosen_by_controller"});
    });
    EXPECT_EQ(state.getObject(irelia).temp_might_bonus, 0);
    resolveThroughEngine([&](EffectExecutor&, GameState&) {
        events.emit(ObjectStateChangedEvent{irelia, "chosen_by_controller"});
    });
    EXPECT_EQ(state.getObject(irelia).temp_might_bonus, 1);
    // "or ready me": her controller readying her in the Awaken Phase counts
    // (CR 416.3.a) — that arrives as "became_ready".
    state.getObject(irelia).temp_might_bonus = 0;
    resolveThroughEngine([&](EffectExecutor&, GameState&) {
        events.emit(ObjectStateChangedEvent{irelia, "became_ready"});
    });
    EXPECT_EQ(state.getObject(irelia).temp_might_bonus, 1);
}

TEST_F(C5Test, JaeMedardaOnlyWhenASpellChoosesHer) {
    auto jae = addUnit(P1, kJaeMedarda, 0);
    auto other = addUnit(P1, kInvalidId, 2);
    const Card& c = card(kJaeMedarda);
    const auto t = TriggerType::WhenYouChooseAFriendlyUnit;
    ChainItem spell;
    spell.is_spell = true;
    spell.controller = P1;
    spell.targets = {jae};
    state.chain.items.push_back(spell);
    EXPECT_TRUE(c.triggerCondition(state, jae, P1, t, jae));
    EXPECT_FALSE(c.triggerCondition(state, jae, P1, t, other)) << "another unit was chosen";
    // A spell that chooses as it resolves is the resolving item.
    state.chain.items.clear();
    spell.targets.clear();
    spell.chosen_objects = {jae};
    state.chain.resuming = spell;
    EXPECT_TRUE(c.triggerCondition(state, jae, P1, t, jae));
    // An ability choosing her is not "with a spell".
    state.chain.resuming->is_spell = false;
    EXPECT_FALSE(c.triggerCondition(state, jae, P1, t, jae));
    state.chain.resuming.reset();
}

TEST_F(C5Test, BreakneckMechIsOneOfYourMechs) {
    auto mech = addUnit(P1, kBreakneckMech, 0, 0);
    state.getObject(mech).keywords = card_db.get(kBreakneckMech).keywords;
    cleanup();
    EXPECT_TRUE(has(mech, Keyword::Deflect));
    EXPECT_TRUE(has(mech, Keyword::Ganking));
    withEngine([&](GameEngine& e) {
        EXPECT_EQ(e.testHook_executor().deflectCostFor(P2, mech), 1)
            << "a lone Breakneck Mech has Deflect once";
    });
    // "I enter ready if you control another Mech" — another Breakneck Mech is one.
    auto second = addUnit(P1, kBreakneckMech, 0);
    EXPECT_TRUE(card(kBreakneckMech).entersReadyOnPlay(state, P1, state.getObject(second)));
    sendToTrash(mech);
    EXPECT_FALSE(card(kBreakneckMech).entersReadyOnPlay(state, P1, state.getObject(second)));
}

TEST_F(C5Test, DirewingEntersReadyNextToAnotherDirewing) {
    auto second = addUnit(P1, kDirewing, 0);
    EXPECT_FALSE(card(kDirewing).entersReadyOnPlay(state, P1, state.getObject(second)));
    addUnit(P1, kDirewing, 0);
    EXPECT_TRUE(card(kDirewing).entersReadyOnPlay(state, P1, state.getObject(second)));
}

TEST_F(C5Test, RenataMastermindOnlyTheOneAtABattlefield) {
    auto at_bf = addUnit(P1, kRenataMastermind, 0, 0);
    auto in_base = addUnit(P1, kRenataMastermind, 0);
    EXPECT_TRUE(card(kRenataMastermind).canActivateAbilityAt(state, P1, at_bf, 0));
    EXPECT_FALSE(card(kRenataMastermind).canActivateAbilityAt(state, P1, in_base, 0));
}

TEST_F(C5Test, TrustyRamhoundHasCompanyInBaseToo) {
    auto hound = addUnit(P1, kTrustyRamhound, 0);
    cleanup();
    const int alone = might(hound);
    addUnit(P1, kInvalidId, 1);
    cleanup();
    EXPECT_EQ(might(hound), alone + 1) << "'here' can be a base";
}

TEST_F(C5Test, FioraPeerlessDoublesHerMightForTheCombatOnly) {
    auto fiora = addUnit(P1, kFioraPeerless, 0, 0);
    addUnit(P2, kInvalidId, 3, 0);
    startCombat(0, P1, P2);
    const int base = might(fiora);
    fireTriggerThroughEngine(fiora, TriggerType::WhenIAttackOrDefend);
    cleanup();
    EXPECT_EQ(might(fiora), 2 * base);
    EXPECT_EQ(state.getObject(fiora).temp_might_bonus, 0) << "not a 'this turn' bonus";
    // The combat ends.
    endCombat(0);
    cleanup();
    EXPECT_EQ(might(fiora), base);
}

TEST_F(C5Test, CardSharpAsksTheOpponent) {
    auto sharp = addToHand(P1, kCardSharp);
    EnginePathAgent yes(EnginePathAgent::script({1}));
    EnginePathAgent opp_no(EnginePathAgent::script({0}));
    playThroughEngine(sharp, &yes, std::nullopt, &opp_no);
    ASSERT_EQ(opp_no.prompts.size(), 1u) << "each opponent MAY";
    EXPECT_EQ(goldCount(P1), 1);
    EXPECT_EQ(goldCount(P2), 0);

    auto again = addToHand(P1, kCardSharp);
    EnginePathAgent yes2(EnginePathAgent::script({1}));
    EnginePathAgent opp_yes(EnginePathAgent::script({1}));
    playThroughEngine(again, &yes2, std::nullopt, &opp_yes);
    EXPECT_EQ(goldCount(P1), 3) << "one more of my own, and one for the opponent who did";
    EXPECT_EQ(goldCount(P2), 1);
}

TEST_F(C5Test, OrnnBlacksmithLetsThePlayerChooseTheGearOrNone) {
    auto ornn = addUnit(P1, kOrnnBlacksmith, 0, 0);
    addToDeck(P1, kShipyardSkulker);
    auto gear_b = addToDeck(P1, kDoransShield);
    auto gear_a = addToDeck(P1, kSerratedDirk);
    addToDeck(P1, kMeditation);   // top
    EnginePathAgent agent(EnginePathAgent::choosing(gear_b));
    fireTriggerThroughEngine(ornn, TriggerType::WhenIHold, &agent);
    EXPECT_TRUE(agent.wasOffered(gear_a));
    EXPECT_TRUE(inHand(P1, gear_b));
    EXPECT_FALSE(inHand(P1, gear_a));
    EXPECT_EQ(deckSize(P1), 3);

    EnginePathAgent no(EnginePathAgent::script({0}));
    fireTriggerThroughEngine(ornn, TriggerType::WhenIHold, &no);
    EXPECT_EQ(handSize(P1), 1) << "You MAY reveal a gear";
    EXPECT_EQ(deckSize(P1), 3);
}

TEST_F(C5Test, FaeDragonBuffsTheUnitsThePlayerChooses) {
    auto dragon = addToHand(P1, kFaeDragon);
    auto a = addUnit(P1, kInvalidId, 2);
    auto b = addUnit(P1, kInvalidId, 2);
    auto c = addUnit(P1, kInvalidId, 2);
    int asked = 0;
    EnginePathAgent agent([&](const std::vector<Intent>& legal) {
        if (asked++ == 0)
            for (const auto& i : legal)
                if (!i.chosen_objects.empty() && i.chosen_objects.front() == b) return i;
        return legal.back();   // "stop"
    });
    playThroughEngine(dragon, &agent);
    EXPECT_EQ(state.getObject(b).buff_count, 1);
    EXPECT_EQ(state.getObject(a).buff_count, 0) << "up to four: the player stopped after one";
    EXPECT_EQ(state.getObject(c).buff_count, 0);
}

// ─── Costs, "may"s and choices (03 / 07) ────────────────────────────────────

TEST_F(C5Test, SpectralMatronLimitsThePrintedCostAndAsksWhere) {
    auto matron = addToHand(P1, kSpectralMatron);
    auto cheap = addToTrash(P1, kShipyardSkulker);     // [3]
    auto powered = addToTrash(P1, kKrakenHunter);      // [3][O][O]: more than [A]
    state.battlefields[0].controller = P1;
    EnginePathAgent agent(EnginePathAgent::script({1, static_cast<int>(cheap), 1}));
    playThroughEngine(matron, &agent);
    EXPECT_FALSE(agent.wasOffered(powered)) << "no more than [3] AND no more than [A]";
    ASSERT_TRUE(onBoard(cheap));
    EXPECT_EQ(state.getObject(cheap).battlefieldId(), std::optional<BattlefieldId>{0})
        << "CR 355.2.a: base or a battlefield you control";
    EXPECT_FALSE(inTrash(P1, cheap));
}

TEST_F(C5Test, GlascMixologistUsesTheSameCostLimit) {
    auto glasc = addUnit(P1, kGlascMixologist, 0);
    auto powered = addToTrash(P1, kKrakenHunter);
    sendToTrash(glasc);
    EnginePathAgent agent;
    fireTriggerThroughEngine(glasc, TriggerType::WhenIDie, &agent);
    EXPECT_TRUE(agent.prompts.empty()) << "a [3][O][O] unit costs more than [A]";
    EXPECT_TRUE(inTrash(P1, powered));
}

TEST_F(C5Test, AlbusFerrosSpendsTheBuffsThePlayerChooses) {
    auto albus = addToHand(P1, kAlbusFerros);
    auto a = addUnit(P1, kInvalidId, 2);
    auto b = addUnit(P1, kInvalidId, 2);
    state.getObject(a).buff_count = 1;
    state.getObject(b).buff_count = 1;
    for (int i = 0; i < 3; ++i) {
        auto rune = addRune(P1, Domain::Order);
        state.getObject(rune).location = std::nullopt;
        state.getObject(rune).zone = ZoneType::RuneDeck;
        state.player(P1).rune_deck.push_back(rune);
    }
    int asked = 0;
    EnginePathAgent agent([&](const std::vector<Intent>& legal) {
        if (asked++ == 0)
            for (const auto& i : legal)
                if (!i.chosen_objects.empty() && i.chosen_objects.front() == b) return i;
        return legal.back();   // "stop"
    });
    playThroughEngine(albus, &agent);
    EXPECT_TRUE(agent.wasOffered(a));
    EXPECT_EQ(state.getObject(b).buff_count, 0);
    EXPECT_EQ(state.getObject(a).buff_count, 1) << "the player, not map order, picks the buff";
    EXPECT_EQ(runesControlled(state, P1), 1) << "one rune per buff actually spent";
}

TEST_F(C5Test, BlastCorpsCadetPaymentCoversOnePlay) {
    auto cadet = addUnit(P1, kBlastCorpsCadet, 0);
    auto enemy = addUnit(P2, kInvalidId, 5, 0);
    state.getObject(cadet).card_counters["__blastcorps_paid"] = 1;
    fireTriggerThroughEngine(cadet, TriggerType::WhenYouPlayMe);
    EXPECT_EQ(state.getObject(enemy).damage_marked, 2);
    // Returned to hand and played again without paying: no second 2 damage.
    fireTriggerThroughEngine(cadet, TriggerType::WhenYouPlayMe);
    EXPECT_EQ(state.getObject(enemy).damage_marked, 2);
}

TEST_F(C5Test, DravenVanquisherPaysWithFloatingPower) {
    auto draven = addUnit(P1, kDravenVanquisher, 0, 0);
    state.player(P1).rune_pool.power[static_cast<int>(Domain::Fury)] = 1;
    EnginePathAgent agent(EnginePathAgent::script({1}));
    fireTriggerThroughEngine(draven, TriggerType::WhenIAttackOrDefend, &agent);
    ASSERT_EQ(agent.prompts.size(), 1u) << "floating [R] is enough to be asked";
    EXPECT_EQ(state.getObject(draven).temp_might_bonus, 2);
    EXPECT_EQ(state.player(P1).rune_pool.power[static_cast<int>(Domain::Fury)], 0);
}

TEST_F(C5Test, RellMagneticPlaysTheEquipmentOutOfHand) {
    auto rell = addUnit(P1, kRellMagnetic, 0, 0);
    auto dirk = addToHand(P1, kSerratedDirk);
    EnginePathAgent agent(EnginePathAgent::choosing(dirk));
    fireTriggerThroughEngine(rell, TriggerType::WhenIAttack, &agent);
    EXPECT_FALSE(inHand(P1, dirk));
    EXPECT_EQ(state.getObject(dirk).attached_to, std::optional<GameObjectId>{rell});

    // Rell left the board before the trigger resolved: the Equipment is
    // played but does not follow her to the trash.
    auto gone = addUnit(P1, kRellMagnetic, 0, 0);
    auto shield = addToHand(P1, kDoransShield);
    sendToTrash(gone);
    EnginePathAgent agent2(EnginePathAgent::choosing(shield));
    fireTriggerThroughEngine(gone, TriggerType::WhenIAttack, &agent2);
    // 9.B3: the checks on the Equipment were inside `if (onBoard(shield))`, so
    // a Rell that did nothing at all passed. "You may play an Equipment ..."
    // does not depend on her; only "attach it to me" does.
    EXPECT_FALSE(agent2.prompts.empty()) << "the may is still offered";
    EXPECT_FALSE(inHand(P1, shield)) << "played out of the hand";
    ASSERT_TRUE(onBoard(shield)) << "the Equipment is played although Rell is gone";
    EXPECT_EQ(state.getObject(shield).zone, ZoneType::Base);
    EXPECT_FALSE(state.getObject(shield).attached_to.has_value());
    EXPECT_TRUE(state.getObject(gone).attachments.empty());
}

TEST_F(C5Test, RumbleHotheadedLetsThePlayerChooseBothCards) {
    auto rumble = addUnit(P1, kRumbleHotheaded, 0, 0);
    auto small = addUnit(P1, kShipyardSkulker, 0);       // 3 [M]
    auto big = addUnit(P1, kShipyardSkulker, 5);         // 5 [M]
    auto mech = addToTrash(P1, kMegaMech);               // [7]
    state.player(P1).rune_pool.energy = 2;               // 7 - 5
    EnginePathAgent agent(EnginePathAgent::script(
        {1, static_cast<int>(big), static_cast<int>(mech)}));
    fireTriggerThroughEngine(rumble, TriggerType::WhenIConquer, &agent);
    EXPECT_TRUE(agent.wasOffered(small));
    EXPECT_TRUE(onBoard(mech)) << "its Energy cost is reduced by the recycled unit's Might";
    EXPECT_FALSE(inTrash(P1, mech));
    EXPECT_TRUE(inDeck(P1, big)) << "the chosen unit was recycled";
    EXPECT_TRUE(onBoard(small));
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0);
}

TEST_F(C5Test, ApprenticeSmithRevealHonoursVoidHatchling) {
    addUnit(P1, kVoidHatchling, 0);
    auto smith = addUnit(P1, kApprenticeSmith, 0);
    auto gear = addToDeck(P1, kSerratedDirk);
    auto spell = addToDeck(P1, kMeditation);   // top
    // "look at the top card first. You may recycle it." — recycle the spell,
    // so the reveal finds the gear.
    EnginePathAgent agent(EnginePathAgent::script({1}));
    withEngine([&](GameEngine& e) {
        e.testHook_cleanup();
        e.testHook_executor().queueTriggeredAbility(smith, P1, TriggerType::WhenIMove);
        e.testHook_runChain();
    }, &agent);
    EXPECT_TRUE(inHand(P1, gear));
    EXPECT_TRUE(inDeck(P1, spell));
}

TEST_F(C5Test, LegionQuartermasterNeedsAGearAndReturnsIt) {
    auto qm = addToHand(P1, kLegionQuartermaster);
    state.player(P1).rune_pool.energy = 10;
    withEngine([&](GameEngine& e) {
        EXPECT_FALSE(e.testHook_canAfford(P1, qm)) << "no friendly gear: the cost can't be paid";
    });
    auto gear = addGear(P1, kSerratedDirk);
    withEngine([&](GameEngine& e) { EXPECT_TRUE(e.testHook_canAfford(P1, qm)); });
    playThroughEngine(qm);
    EXPECT_TRUE(onBoard(qm));
    EXPECT_TRUE(inHand(P1, gear));
}

TEST_F(C5Test, ApheliosModeCannotBeChosenTwiceInATurn) {
    auto aphelios = addUnit(P1, kAphelios, 0);
    EnginePathAgent first(EnginePathAgent::script({1}));
    fireTriggerThroughEngine(aphelios, TriggerType::WhenEquipmentAttachedToMe, &first);
    ASSERT_EQ(first.prompts.size(), 1u);
    EXPECT_EQ(first.prompts[0].size(), 3u);
    EnginePathAgent second;
    fireTriggerThroughEngine(aphelios, TriggerType::WhenEquipmentAttachedToMe, &second);
    ASSERT_EQ(second.prompts.size(), 1u);
    EXPECT_EQ(second.prompts[0].size(), 2u);
    for (const auto& i : second.prompts[0])
        EXPECT_NE(i.chosen_value.value_or(-1), 1) << "mode 1 was chosen this turn";
}

TEST_F(C5Test, AzirOncePerTurnIsPerAzir) {
    auto used = addUnit(P1, kAzir, 0, 0);
    auto fresh = addUnit(P1, kAzir, 0);
    state.getObject(used).card_counters["azir_used_turn"] = state.turn.turn_number + 1;
    EXPECT_FALSE(card(kAzir).canActivateAbilityAt(state, P1, used, 0));
    EXPECT_TRUE(card(kAzir).canActivateAbilityAt(state, P1, fresh, 0))
        << "the other Azir has not used its ability";
    const auto targets = card(kAzir).enumerateLegalTargets(state, P1, 0, fresh);
    EXPECT_EQ(std::find(targets.begin(), targets.end(), fresh), targets.end())
        << "an Azir does not swap with itself";
}

TEST_F(C5Test, AzirMayLeaveTheEquipmentWhereItIs) {
    auto swap_with = addUnit(P1, kShipyardSkulker, 0, 1);
    auto gear = addGear(P1, kInvalidId, /*equipment=*/true);
    attach(gear, swap_with);
    auto activate = [&](GameObjectId azir, EnginePathAgent& agent) {
        state.player(P1).rune_pool.power[static_cast<int>(Domain::Calm)] = 1;
        Intent act;
        act.type = IntentType::ActivateAbility;
        act.player = P1;
        act.ability_source = azir;
        act.ability_index = 0;
        withEngine([&](GameEngine& e) {
            e.testHook_executeIntent(act);
            e.testHook_runChain();
        }, &agent);
    };
    auto azir = addUnit(P1, kAzir, 0);
    EnginePathAgent no(EnginePathAgent::script({0, static_cast<int>(swap_with)}));
    activate(azir, no);
    EXPECT_EQ(state.getObject(azir).battlefieldId(), std::optional<BattlefieldId>{1});
    EXPECT_TRUE(state.getObject(swap_with).isAtBase());
    EXPECT_EQ(state.getObject(gear).attached_to, std::optional<GameObjectId>{swap_with})
        << "you MAY attach one of its Equipment";

    auto azir2 = addUnit(P1, kAzir, 0, 0);
    EnginePathAgent yes(EnginePathAgent::script(
        {1, static_cast<int>(swap_with), static_cast<int>(gear)}));
    activate(azir2, yes);
    EXPECT_EQ(state.getObject(gear).attached_to, std::optional<GameObjectId>{azir2});
    EXPECT_TRUE(state.getObject(azir2).isAtBase());
}

TEST_F(C5Test, JannaSaviorHealsInBaseAndMayMoveNoOne) {
    auto hurt = addUnit(P1, kShipyardSkulker, 0);
    state.getObject(hurt).damage_marked = 2;
    auto in_base = addToHand(P1, kJannaSavior);
    playThroughEngine(in_base);
    EXPECT_EQ(state.getObject(hurt).damage_marked, 0) << "'here' can be a base";

    state.battlefields[0].controller = P1;
    auto enemy = addUnit(P2, kShipyardSkulker, 0, 0);
    auto at_bf = addToHand(P1, kJannaSavior);
    EnginePathAgent no(EnginePathAgent::script({0}));
    playThroughEngine(at_bf, &no, LocationId{BattlefieldLocation{0}});
    ASSERT_FALSE(no.prompts.empty());
    EXPECT_EQ(state.getObject(enemy).battlefieldId(), std::optional<BattlefieldId>{0})
        << "UP TO one enemy unit";

    auto mover = addToHand(P1, kJannaSavior);
    EnginePathAgent yes(EnginePathAgent::choosing(enemy));
    playThroughEngine(mover, &yes, LocationId{BattlefieldLocation{0}});
    EXPECT_TRUE(state.getObject(enemy).isAtBase());
}

TEST_F(C5Test, BubbleBotReadiesTheMechThePlayerChooses) {
    auto bot = addToHand(P1, kBubbleBot);
    auto first = addTaggedUnit(P1, "Mech", 3);
    auto second = addTaggedUnit(P1, "Mech", 3);
    state.getObject(first).is_exhausted = true;
    state.getObject(second).is_exhausted = true;
    EnginePathAgent agent(EnginePathAgent::choosing(second));
    playThroughEngine(bot, &agent);
    EXPECT_FALSE(state.getObject(second).is_exhausted);
    EXPECT_TRUE(state.getObject(first).is_exhausted);
}

TEST_F(C5Test, PickpocketCanKillAGearTokenWithoutACard) {
    auto pickpocket = addToHand(P1, kPickpocket);
    auto token = addGear(P2);   // no card_def_id
    EnginePathAgent agent(EnginePathAgent::choosing(token));
    EXPECT_NO_THROW(playThroughEngine(pickpocket, &agent));
    EXPECT_FALSE(onBoard(token));
    EXPECT_EQ(goldCount(P1), 1);
}

TEST_F(C5Test, JayceKillsTheChosenGearAndGrantsAPlayThisTurn) {
    auto jayce = addToHand(P1, kJayceManOfProgress);
    auto keep = addGear(P1, kSerratedDirk);
    auto kill = addGear(P1, kDoransShield);
    auto in_hand = addToHand(P1, kScrapheap);
    EnginePathAgent agent(EnginePathAgent::choosing(kill));
    playThroughEngine(jayce, &agent);
    EXPECT_TRUE(agent.wasOffered(keep)) << "the player chooses which gear";
    EXPECT_FALSE(onBoard(kill));
    EXPECT_TRUE(onBoard(keep));
    EXPECT_TRUE(inHand(P1, in_hand)) << "'you may play a gear ... this turn': nothing is forced out";
    // The permission: the next gear this turn ignores its Energy cost.
    withEngine([&](GameEngine& e) {
        EXPECT_TRUE(e.testHook_canAfford(P1, in_hand)) << "[2] gear, no Energy available";
    });
    const auto& mods = state.player(P1).cost_modifiers;
    ASSERT_EQ(mods.size(), 1u);
    EXPECT_TRUE(mods[0].gear_only);
    EXPECT_TRUE(mods[0].this_turn_only);
    // "Energy cost no more than [7]": a [9] gear gets nothing.
    auto dear = addToHand(P1, kDazzlingAurora);
    state.player(P1).rune_pool.energy = 2;
    state.player(P1).rune_pool.universal_power = 2;
    withEngine([&](GameEngine& e) {
        EXPECT_FALSE(e.testHook_canAfford(P1, dear)) << "[9][A][A] with [2][A][A] available";
    });
}

TEST_F(C5Test, YordleExplorerDrawsForTheCardThatWasPlayed) {
    auto explorer = addUnit(P1, kYordleExplorer, 0);
    for (int i = 0; i < 3; ++i) addToDeck(P1, kShipyardSkulker);
    auto powered = addUnit(P1, kKrakenHunter, 0);    // Power cost [O][O]
    auto plain = addUnit(P1, kShipyardSkulker, 0);
    fireTriggerThroughEngine(explorer, TriggerType::WhenYouPlayACard, nullptr, plain);
    EXPECT_EQ(handSize(P1), 0);
    fireTriggerThroughEngine(explorer, TriggerType::WhenYouPlayACard, nullptr, powered);
    EXPECT_EQ(handSize(P1), 1) << "a card with Power cost [A][A] or more";
}

TEST_F(C5Test, YordleExplorerDrawsWhenAPoweredUnitIsPlayed) {
    addUnit(P1, kYordleExplorer, 0);
    for (int i = 0; i < 3; ++i) addToDeck(P1, kShipyardSkulker);
    auto powered = addToHand(P1, kKrakenHunter);
    EnginePathAgent agent(EnginePathAgent::script({0}));   // decline Accelerate
    playThroughEngine(powered, &agent);
    ASSERT_TRUE(onBoard(powered));
    EXPECT_EQ(handSize(P1), 1) << "played with an empty chain: the draw still happens";
}

TEST_F(C5Test, CorruptEnforcerDiscardsTheChosenCard) {
    auto enforcer = addUnit(P1, kCorruptEnforcer, 0, 0);
    auto keep = addToHand(P1, kShipyardSkulker);
    auto toss = addToHand(P1, kMeditation);
    EnginePathAgent agent(EnginePathAgent::choosing(toss));
    fireTriggerThroughEngine(enforcer, TriggerType::WhenIMoveToFB, &agent);
    EXPECT_TRUE(inTrash(P1, toss));
    EXPECT_TRUE(inHand(P1, keep));
}

TEST_F(C5Test, FaePorterPaysWhenTheTriggerIsFinalized) {
    auto porter = addUnit(P1, kFaePorter, 0, 0);
    auto ally = addUnit(P1, kShipyardSkulker, 0);
    EnginePathAgent broke;
    fireTriggerThroughEngine(porter, TriggerType::WhenIMoveToFB, &broke);
    EXPECT_TRUE(broke.prompts.empty()) << "no [P] to pay: not offered";
    EXPECT_TRUE(state.getObject(ally).isAtBase());

    state.player(P1).rune_pool.power[static_cast<int>(Domain::Chaos)] = 1;
    EnginePathAgent agent(EnginePathAgent::choosing(ally));
    fireTriggerThroughEngine(porter, TriggerType::WhenIMoveToFB, &agent);
    EXPECT_EQ(state.getObject(ally).battlefieldId(), std::optional<BattlefieldId>{0});
    EXPECT_EQ(state.player(P1).rune_pool.power[static_cast<int>(Domain::Chaos)], 0);
}

TEST_F(C5Test, LoyalPupIsNotMovedOutOfTheTrash) {
    auto pup = addUnit(P1, kLoyalPup, 0);
    state.getObject(pup).card_counters["__defend_bf"] = 0;
    sendToTrash(pup);
    EnginePathAgent agent;
    fireTriggerThroughEngine(pup, TriggerType::WhenYouDefendAtABattlefield, &agent);
    EXPECT_TRUE(agent.prompts.empty());
    EXPECT_FALSE(onBoard(pup));
}

TEST_F(C5Test, OverzealousFanMovesTheAttackerToItsBase) {
    auto fan = addUnit(P1, kOverzealousFan, 0, 0);
    auto attacker = addUnit(P2, kShipyardSkulker, 0, 0);
    auto other = addUnit(P2, kShipyardSkulker, 0, 0);
    startCombat(0, P2, P1);
    state.getObject(attacker).buff_count = 1;
    state.getObject(fan).card_counters["__defend_attacker_id"] = static_cast<int>(other);
    EnginePathAgent agent(EnginePathAgent::choosing(attacker));
    fireTriggerThroughEngine(fan, TriggerType::WhenIDefend, &agent);
    EXPECT_TRUE(agent.wasOffered(other)) << "AN attacking unit: the controller chooses";
    EXPECT_FALSE(onBoard(fan)) << "kill me";
    EXPECT_FALSE(inHand(P2, attacker)) << "moved to its base, not returned to hand";
    EXPECT_TRUE(state.getObject(attacker).isAtBase());
    EXPECT_EQ(state.getObject(attacker).buff_count, 1) << "it keeps its buff";
    EXPECT_EQ(state.getObject(other).battlefieldId(), std::optional<BattlefieldId>{0});
}

TEST_F(C5Test, FizzPlaysTheSpellThePlayerChooses) {
    auto fizz = addToHand(P1, kFizzTrickster);
    auto older = addToTrash(P1, kMobilize);
    auto newer = addToTrash(P1, kMeditation);
    addToTrash(P1, kFallingComet);   // [5]: too dear
    for (int i = 0; i < 3; ++i) addToDeck(P1, kShipyardSkulker);   // Mobilize draws 1
    EnginePathAgent agent(EnginePathAgent::script({1, static_cast<int>(older)}));
    playThroughEngine(fizz, &agent);
    EXPECT_TRUE(agent.wasOffered(newer));
    EXPECT_TRUE(inTrash(P1, newer));
    EXPECT_FALSE(inTrash(P1, older));
    EXPECT_TRUE(inDeck(P1, older)) << "Then recycle it.";
    EXPECT_EQ(std::count(state.player(P1).main_deck.begin(), state.player(P1).main_deck.end(),
                         older), 1) << "in the deck once (02 item 2.22)";
}

TEST_F(C5Test, ZaunPunkKillsTheCostAndTheChosenGear) {
    auto punk = addToHand(P1, kZaunPunk);
    auto mine = addGear(P1, kSerratedDirk);
    auto theirs = addGear(P2, kDoransShield);
    EnginePathAgent agent(EnginePathAgent::script(
        {1, static_cast<int>(mine), static_cast<int>(theirs)}));
    playThroughEngine(punk, &agent);
    EXPECT_FALSE(onBoard(mine));
    EXPECT_FALSE(onBoard(theirs));

    auto declined = addToHand(P1, kZaunPunk);
    auto mine2 = addGear(P1, kSerratedDirk);
    EnginePathAgent no(EnginePathAgent::script({0}));
    playThroughEngine(declined, &no);
    EXPECT_TRUE(onBoard(mine2));
}

TEST_F(C5Test, RekSaiRevealIsAMayAndTheBanishIsCounted) {
    auto reksai = addUnit(P1, kRekSai, 0, 0);
    addToDeck(P1, kMeditation);
    auto unit = addToDeck(P1, kShipyardSkulker);   // top
    EnginePathAgent no(EnginePathAgent::script({0}));
    fireTriggerThroughEngine(reksai, TriggerType::WhenIAttack, &no);
    EXPECT_EQ(deckSize(P1), 2) << "you MAY reveal";
    EXPECT_EQ(no.prompts.size(), 1u);

    state.player(P1).rune_pool.energy = 3;
    // "you may play it here": where it goes is asked (mode 1 = battlefield #0, here).
    EnginePathAgent yes(EnginePathAgent::script({1, static_cast<int>(unit), 1}));
    fireTriggerThroughEngine(reksai, TriggerType::WhenIAttack, &yes);
    EXPECT_EQ(state.getObject(unit).battlefieldId(), std::optional<BattlefieldId>{0})
        << "If it is a unit, you may play it here.";
    EXPECT_EQ(state.player(P1).cards_banished_this_turn, 1);
    EXPECT_TRUE(state.player(P1).banishment.empty()) << "it was played out of banishment";
    EXPECT_EQ(deckSize(P1), 1) << "the rest was recycled";
}

}  // namespace
}  // namespace test_review_fix_c5
}  // namespace riftbound::test
