/// @file test_review_fix_c4.cpp
/// Regression tests for the card findings of the 2026-10 engine review in the
/// range spells/0935 .. units/0218 (plans/cards/batch-04.md, and the entries
/// of plans/03, 07, 08 and cards/batch-09 that name a card in that range).
///
/// Every behavioural test drives the real engine path through the fixture
/// (playThroughEngine / fireTriggerThroughEngine / resolveThroughEngine, or a
/// GameEngine held by withEngine): the card is played or its event happens,
/// the trigger goes through TriggerManager, finalization and the chain, and
/// the agent — not the test — answers the choices.

#include "tests/cards/card_test_fixture.h"
#include "cards/card_helpers.h"
#include "engine/effect_executor.h"
#include "engine/game_engine.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

namespace riftbound::test {
namespace test_review_fix_c4 {  // file-unique: test sources are unity-built
namespace {

constexpr CardDefId kShadowDash = 935;
constexpr CardDefId kAccelerationGate = 937;
constexpr CardDefId kRebuttal = 939;
constexpr CardDefId kFlameChompers = 6;
constexpr CardDefId kMagmaWurm = 11;
constexpr CardDefId kDangerousDuo = 16;
constexpr CardDefId kNoxusSaboteur = 18;
constexpr CardDefId kDariusTrifarian = 27;
constexpr CardDefId kViDestructive = 36;
constexpr CardDefId kImmortalPhoenix = 37;
constexpr CardDefId kSolariShieldbearer = 51;
constexpr CardDefId kBlitzcrank = 67;
constexpr CardDefId kCaitlynPatrolling = 68;
constexpr CardDefId kYasuoRemorseful = 76;
constexpr CardDefId kWizenedElder = 65;
constexpr CardDefId kLeeSinAscetic = 78;      // "[E]: Buff me."
constexpr CardDefId kWhiteflameProtector = 82;
constexpr CardDefId kRiptideRex = 92;
constexpr CardDefId kBlastconeFae = 97;
constexpr CardDefId kGemcraftSeer = 100;
constexpr CardDefId kSpriteMother = 106;
constexpr CardDefId kAvaAchiever = 107;
constexpr CardDefId kDrMundoExpert = 109;
constexpr CardDefId kEkkoRecurrent = 110;
constexpr CardDefId kHeimerdinger = 111;
constexpr CardDefId kKaiSaEvolutionary = 112;
constexpr CardDefId kAhriInquisitive = 119;
constexpr CardDefId kTeemoStrategist = 121;
constexpr CardDefId kCrackshotCorsair = 130;
constexpr CardDefId kFirstMate = 132;
constexpr CardDefId kPitRookie = 136;
constexpr CardDefId kWildclawShaman = 147;
constexpr CardDefId kAniviaPrimal = 148;
constexpr CardDefId kSnapvine = 149;
constexpr CardDefId kUdyrWildman = 157;
constexpr CardDefId kMissFortuneCaptain = 162;
constexpr CardDefId kSettBrawler = 164;
constexpr CardDefId kEmberMonk = 167;
constexpr CardDefId kZauniteBouncer = 188;
constexpr CardDefId kKaynUnleashed = 189;
constexpr CardDefId kMaddenedMarauder = 191;
constexpr CardDefId kNocturne = 194;
constexpr CardDefId kSoulgorger = 196;
constexpr CardDefId kTeemoScout = 197;        // a unit with [Hidden], costs [2]
constexpr CardDefId kTideturner = 199;
constexpr CardDefId kTwistedFate = 200;
constexpr CardDefId kCruelPatron = 208;
constexpr CardDefId kVanguardCaptain = 218;
constexpr CardDefId kIncinerate = 301;        // spell, [2]
constexpr CardDefId kPunchFirst = 419;        // spell

/// Passes / takes the first option. The first time it is asked, it shows the
/// state to `look` — what a player given priority can see before responding.
class SnoopAgent : public AgentInterface {
public:
    std::function<void(const GameState&)> look;
    int asked = 0;
    explicit SnoopAgent(std::function<void(const GameState&)> f) : look(std::move(f)) {}
    Intent selectAction(const GameState& s, const std::vector<Intent>& legal) override {
        if (++asked == 1 && look) look(s);   // the first time it gets priority
        for (const auto& i : legal)
            if (i.type == IntentType::PassPriority || i.type == IntentType::PassFocus) return i;
        return legal.empty() ? Intent{} : legal.front();
    }
};

class C4Test : public CardTestFixture {
protected:
    GameObject& o(GameObjectId id) { return state.getObject(id); }

    /// A unit `p` controls, built from `def`, in a non-board zone.
    GameObjectId offBoard(PlayerId p, CardDefId def, ZoneType zone) {
        auto id = addUnit(p, def);
        o(id).location = std::nullopt;
        o(id).zone = zone;
        if (zone == ZoneType::Trash) state.player(p).trash.push_back(id);
        if (zone == ZoneType::MainDeck) state.player(p).main_deck.push_back(id);
        if (zone == ZoneType::Banishment) state.player(p).banishment.push_back(id);
        return id;
    }
    /// A card of `def` in `p`'s trash.
    GameObjectId inTrashOf(PlayerId p, CardDefId def) {
        auto id = addToHand(p, def);
        auto& h = state.player(p).hand;
        h.erase(std::remove(h.begin(), h.end(), id), h.end());
        o(id).zone = ZoneType::Trash;
        state.player(p).trash.push_back(id);
        return id;
    }
    GameObjectId legend(PlayerId p) {
        auto id = state.createObject();
        auto& l = o(id);
        l.owner = l.controller = p;
        l.card_type = CardType::Legend;
        l.name = "TestLegend";
        l.zone = ZoneType::LegendZone;
        return id;
    }
    /// Activate `source`'s ability through the engine and resolve the chain.
    void activate(GameObjectId source, AgentInterface* agent = nullptr, int index = 0) {
        Intent a;
        a.type = IntentType::ActivateAbility;
        a.player = o(source).controller;
        a.ability_source = source;
        a.ability_index = index;
        withEngine([&](GameEngine& e) {
            e.testHook_cleanup();
            e.testHook_executeIntent(a); settleLikeTheEngine(e);
            e.testHook_runChain();
        }, agent, nullptr, a.player);
    }
    /// What the engine offers the turn player after a cleanup.
    std::vector<Intent> legalActions() {
        std::vector<Intent> out;
        withEngine([&](GameEngine& e) {
            e.testHook_cleanup();
            out = e.generateLegalActions();
        });
        return out;
    }
    bool canActivate(const std::vector<Intent>& legal, GameObjectId source,
                     CardDefId granted_by = kInvalidId) {
        return std::any_of(legal.begin(), legal.end(), [&](const Intent& i) {
            return i.type == IntentType::ActivateAbility && i.ability_source == source &&
                   i.granted_ability_def == granted_by;
        });
    }
    int unitsNamed(const std::string& name) {
        int n = 0;
        for (const auto& [id, obj] : state.objects)
            if (obj.isUnit() && obj.location.has_value() && obj.name == name) ++n;
        return n;
    }
};

// ─── 07 item 7.1: the trigger chooses its own target ────────────────────────

TEST_F(C4Test, SolariShieldbearer_StunsTheUnitItsControllerChooses) {
    auto a = addUnit(P2, kInvalidId, 3, 0);
    auto b = addUnit(P2, kInvalidId, 3, 0);
    auto card = addToHand(P1, kSolariShieldbearer);
    EnginePathAgent agent(EnginePathAgent::choosing(b));
    playThroughEngine(card, &agent);
    ASSERT_FALSE(agent.prompts.empty()) << "the controller is asked which unit";
    EXPECT_TRUE(o(b).is_stunned);
    EXPECT_FALSE(o(a).is_stunned);
}

TEST_F(C4Test, WhiteflameProtector_GivesTheChosenUnitPlusEight) {
    auto a = addUnit(P1, kInvalidId, 2);
    auto b = addUnit(P1, kInvalidId, 2);
    auto card = addToHand(P1, kWhiteflameProtector);
    EnginePathAgent agent(EnginePathAgent::choosing(b));
    playThroughEngine(card, &agent);
    EXPECT_EQ(o(b).temp_might_bonus, 8);
    EXPECT_EQ(o(a).temp_might_bonus, 0);
}

TEST_F(C4Test, RiptideRex_DealsSixToAnEnemyUnitAtABattlefield) {
    auto at_bf = addUnit(P2, kInvalidId, 3, 0);
    auto big = addUnit(P2, kInvalidId, 9, 1);
    auto in_base = addUnit(P2, kInvalidId, 3);
    auto mine = addUnit(P1, kInvalidId, 3, 0);
    auto card = addToHand(P1, kRiptideRex);
    EnginePathAgent agent(EnginePathAgent::choosing(at_bf));
    playThroughEngine(card, &agent);
    EXPECT_TRUE(agent.wasOffered(big));
    EXPECT_FALSE(agent.wasOffered(in_base)) << "\"at a battlefield\"";
    EXPECT_FALSE(agent.wasOffered(mine)) << "\"an enemy unit\"";
    EXPECT_FALSE(o(at_bf).location.has_value()) << "6 damage kills a 3-Might unit";
    EXPECT_EQ(o(big).damage_marked, 0);
}

TEST_F(C4Test, BlastconeFae_ShrinksTheChosenUnitToAMinimumOfOne) {
    auto small = addUnit(P2, kInvalidId, 2, 0);
    auto other = addUnit(P2, kInvalidId, 5, 0);
    auto card = addToHand(P1, kBlastconeFae);
    EnginePathAgent agent(EnginePathAgent::choosing(small));
    playThroughEngine(card, &agent);
    EXPECT_EQ(o(small).current_might, 1) << "-2 [M], to a minimum of 1";
    EXPECT_EQ(o(other).current_might, 5);
}

TEST_F(C4Test, DangerousDuo_NeedsLegionAndThenChooses) {
    auto ally = addUnit(P1, kInvalidId, 2);
    auto first = addToHand(P1, kDangerousDuo);
    EnginePathAgent quiet;
    playThroughEngine(first, &quiet);
    EXPECT_TRUE(quiet.prompts.empty()) << "first card this turn: no Legion, nothing to choose";
    EXPECT_EQ(o(ally).temp_might_bonus, 0);

    // The first Duo was the first card; this one is the second.
    auto second = addToHand(P1, kDangerousDuo);
    EnginePathAgent agent(EnginePathAgent::choosing(ally));
    playThroughEngine(second, &agent);
    ASSERT_FALSE(agent.prompts.empty());
    EXPECT_EQ(o(ally).temp_might_bonus, 2);
}

TEST_F(C4Test, FirstMate_ReadiesAnotherUnitNeverItself) {
    auto tired = addUnit(P1, kInvalidId, 2);
    o(tired).is_exhausted = true;
    auto mate = addUnit(P1, kFirstMate, 0, 0);
    o(mate).is_exhausted = true;
    EnginePathAgent agent(EnginePathAgent::choosing(tired));
    fireTriggerThroughEngine(mate, TriggerType::WhenYouPlayMe, &agent);
    ASSERT_FALSE(agent.prompts.empty());
    EXPECT_FALSE(agent.wasOffered(mate)) << "\"another unit\"";
    EXPECT_FALSE(o(tired).is_exhausted);
    EXPECT_TRUE(o(mate).is_exhausted);
}

TEST_F(C4Test, ZauniteBouncer_ReturnsAnotherUnitAtABattlefield) {
    auto bouncer = addUnit(P1, kZauniteBouncer, 0, 0);
    auto at_bf = addUnit(P2, kInvalidId, 3, 0);
    auto in_base = addUnit(P2, kInvalidId, 3);
    EnginePathAgent agent(EnginePathAgent::choosing(at_bf));
    fireTriggerThroughEngine(bouncer, TriggerType::WhenYouPlayMe, &agent);
    EXPECT_FALSE(agent.wasOffered(bouncer)) << "\"another unit\"";
    EXPECT_FALSE(agent.wasOffered(in_base)) << "\"at a battlefield\"";
    EXPECT_TRUE(inHand(P2, at_bf));
    EXPECT_FALSE(o(at_bf).location.has_value());
}

TEST_F(C4Test, MaddenedMarauder_MovesAUnitFromABattlefieldToItsBase) {
    auto at_bf = addUnit(P2, kInvalidId, 3, 1);
    auto in_base = addUnit(P2, kInvalidId, 3);
    auto card = addToHand(P1, kMaddenedMarauder);
    EnginePathAgent agent(EnginePathAgent::choosing(at_bf));
    playThroughEngine(card, &agent);
    EXPECT_FALSE(agent.wasOffered(in_base)) << "a unit already in its base is not \"from a battlefield\"";
    EXPECT_TRUE(o(at_bf).isAtBase());
}

TEST_F(C4Test, YasuoRemorseful_HitsOnlyAnEnemyUnitHere) {
    auto yasuo = addUnit(P1, kYasuoRemorseful, 0, 0);     // 6 Might
    auto here = addUnit(P2, kInvalidId, 4, 0);
    auto elsewhere = addUnit(P2, kInvalidId, 4, 1);
    EnginePathAgent agent(EnginePathAgent::choosing(here));
    fireTriggerThroughEngine(yasuo, TriggerType::WhenIAttack, &agent);
    ASSERT_FALSE(agent.prompts.empty());
    EXPECT_FALSE(agent.wasOffered(elsewhere)) << "\"an enemy unit here\"";
    EXPECT_FALSE(o(here).location.has_value()) << "6 damage kills a 4-Might unit";
    EXPECT_EQ(o(elsewhere).damage_marked, 0);
}

TEST_F(C4Test, CrackshotCorsair_DealsOneToAnEnemyUnitHere) {
    auto corsair = addUnit(P1, kCrackshotCorsair, 0, 0);
    auto here = addUnit(P2, kInvalidId, 4, 0);
    auto elsewhere = addUnit(P2, kInvalidId, 4, 1);
    EnginePathAgent agent;
    fireTriggerThroughEngine(corsair, TriggerType::WhenIAttack, &agent);
    ASSERT_FALSE(agent.prompts.empty());
    EXPECT_FALSE(agent.wasOffered(elsewhere));
    EXPECT_EQ(o(here).damage_marked, 1);
}

TEST_F(C4Test, Tideturner_SwapsWithTheChosenUnitOrNotAtAll) {
    auto away = addUnit(P1, kInvalidId, 2, 0);
    auto beside = addUnit(P1, kInvalidId, 2);          // in base, where Tideturner lands
    auto card = addToHand(P1, kTideturner);
    EnginePathAgent agent(EnginePathAgent::choosing(away, EnginePathAgent::May::Yes));   // yes, then `away`
    playThroughEngine(card, &agent);
    ASSERT_EQ(agent.prompts.size(), 2u) << "the may, then the unit";
    EXPECT_FALSE(agent.wasOffered(beside)) << "\"at another location\"";
    EXPECT_FALSE(agent.wasOffered(card));
    EXPECT_EQ(o(card).battlefieldId(), std::optional<BattlefieldId>{BattlefieldId::fromRaw(0)});
    EXPECT_TRUE(o(away).isAtBase());

    auto second = addToHand(P1, kTideturner);
    EnginePathAgent no(EnginePathAgent::script({0}));
    playThroughEngine(second, &no);
    EXPECT_EQ(no.prompts.size(), 1u);
    EXPECT_TRUE(o(second).isAtBase()) << "declined: nobody moves";
}

TEST_F(C4Test, PitRookie_BuffsTheFriendlyUnitItsControllerChooses) {
    auto a = addUnit(P1, kInvalidId, 2);
    auto b = addUnit(P1, kInvalidId, 2);
    auto enemy = addUnit(P2, kInvalidId, 2);
    auto card = addToHand(P1, kPitRookie);
    EnginePathAgent agent(EnginePathAgent::choosing(b));
    playThroughEngine(card, &agent);
    ASSERT_FALSE(agent.prompts.empty()) << "S5: the unit is a choice, not the first in map order";
    EXPECT_FALSE(agent.wasOffered(enemy));
    EXPECT_FALSE(agent.wasOffered(card)) << "\"another friendly unit\"";
    EXPECT_EQ(o(b).buff_count, 1);
    EXPECT_EQ(o(a).buff_count, 0);
}

// ─── batch-04 / 08 item 8.8: keywords the text only mentions ────────────────

TEST_F(C4Test, Defs_DoNotCarryKeywordsTheTextOnlyMentions) {
    EXPECT_FALSE(card_db.get(kNoxusSaboteur).keywords.has(Keyword::Hidden));
    EXPECT_FALSE(card_db.get(kEmberMonk).keywords.has(Keyword::Hidden));
    EXPECT_FALSE(card_db.get(kAvaAchiever).keywords.has(Keyword::Hidden));
    EXPECT_FALSE(card_db.get(kSpriteMother).keywords.has(Keyword::Temporary));
    EXPECT_FALSE(card_db.get(kUdyrWildman).keywords.has(Keyword::Ganking));
}

TEST_F(C4Test, SpriteMother_SurvivesHerBeginningPhase_TheSpriteDoesNot) {
    auto card = addToHand(P1, kSpriteMother);
    playThroughEngine(card);
    ASSERT_EQ(unitsNamed("Sprite"), 1);
    withEngine([&](GameEngine& e) { e.testHook_beginningStep(); });
    EXPECT_TRUE(o(card).location.has_value()) << "Sprite Mother is not [Temporary]";
    EXPECT_EQ(unitsNamed("Sprite"), 0) << "her token is";
}

// ─── batch-04 findings ──────────────────────────────────────────────────────

TEST_F(C4Test, VanguardCaptain_NeedsLegion) {
    auto first = addToHand(P1, kVanguardCaptain);
    playThroughEngine(first);
    EXPECT_EQ(unitsNamed("Recruit"), 0) << "first card this turn: no Legion";
    auto second = addToHand(P1, kVanguardCaptain);
    playThroughEngine(second);
    EXPECT_EQ(unitsNamed("Recruit"), 2);
}

TEST_F(C4Test, AniviaPrimal_DamagesOnlyEnemyUnitsHere) {
    auto anivia = addUnit(P1, kAniviaPrimal, 0, 0);
    auto here = addUnit(P2, kInvalidId, 3, 0);
    auto elsewhere = addUnit(P2, kInvalidId, 3, 1);
    auto in_base = addUnit(P2, kInvalidId, 3);
    fireTriggerThroughEngine(anivia, TriggerType::WhenIAttack);
    EXPECT_FALSE(o(here).location.has_value()) << "3 damage kills it";
    EXPECT_TRUE(o(elsewhere).location.has_value()) << "the other battlefield is untouched";
    EXPECT_EQ(o(elsewhere).damage_marked, 0);
    EXPECT_EQ(o(in_base).damage_marked, 0);
}

TEST_F(C4Test, MagmaWurm_OtherFriendlyUnitsEnterReady) {
    // Alone, the Wurm itself enters exhausted.
    auto wurm = addToHand(P1, kMagmaWurm);
    playThroughEngine(wurm);
    ASSERT_TRUE(o(wurm).location.has_value());
    EXPECT_TRUE(o(wurm).is_exhausted) << "\"other\": the Wurm does not ready itself";

    // With the Wurm on the board, a unit card and a unit token enter ready.
    auto unit = addToHand(P1, kFirstMate);
    auto theirs = addToHand(P2, kFirstMate);
    GameObjectId token = kInvalidId;
    withEngine([&](GameEngine& e) {
        e.testHook_cleanup();
        Intent play;
        play.type = IntentType::PlayCard;
        play.player = P1;
        play.card = unit;
        play.play_location = LocationId{BaseLocation{P1}};
        e.mutableState().player(P1).rune_pool.energy += 3;
        e.testHook_executeIntent(play); settleLikeTheEngine(e);
        token = e.testHook_executor().createToken(P1, CardType::Unit, "Recruit", 1, {"Recruit"},
                                                  KeywordSet{}, LocationId{BaseLocation{P1}}, false);
    });
    EXPECT_FALSE(o(unit).is_exhausted) << "entered ready";
    EXPECT_FALSE(o(token).is_exhausted) << "tokens are units too";
    playThroughEngine(theirs);
    EXPECT_TRUE(o(theirs).is_exhausted) << "\"friendly\": the opponent's units are unaffected";
}

TEST_F(C4Test, NoxusSaboteur_OpponentsCantPlayTheirHiddenCardsHere) {
    // P2 hid a card at battlefield 0 on an earlier turn and now holds priority
    // in a Closed State: it may be played from facedown (CR 811.6).
    state.turn.turn_number = 3;
    state.battlefields[0].controller = P2;
    addUnit(P2, kInvalidId, 3, 0);
    auto hidden = addToHand(P2, kTeemoScout);
    auto& h = state.player(P2).hand;
    h.erase(std::remove(h.begin(), h.end(), hidden), h.end());
    o(hidden).zone = ZoneType::FacedownZone;
    o(hidden).is_hidden = true;
    o(hidden).hidden_at = BattlefieldId::fromRaw(0);
    o(hidden).hidden_on_turn = 1;
    state.battlefields[0].facedown.push_back(hidden);
    pushSpellOnChain(P1, kPunchFirst);

    auto offered = [&]() {
        bool found = false;
        withEngine([&](GameEngine& e) {
            e.testHook_cleanup();
            auto& st = e.mutableState();
            st.turn.oc_state = OpenClosedState::Closed;
            st.turn.priority_holder = P2;
            for (const auto& i : e.generateLegalActions())
                if (i.type == IntentType::PlayReaction && i.card == hidden) found = true;
            st.turn.oc_state = OpenClosedState::Open;
            st.turn.priority_holder.reset();
        });
        return found;
    };
    ASSERT_TRUE(offered()) << "without a Saboteur the hidden card can be played";

    addUnit(P1, kNoxusSaboteur, 0, 0);
    EXPECT_FALSE(offered()) << "\"Your opponents' [Hidden] cards can't be revealed here.\"";
    EXPECT_TRUE(state.battlefields[0].opp_hidden_unrevealable[playerIndex(P2)]);
    EXPECT_FALSE(state.battlefields[0].opp_hidden_unrevealable[playerIndex(P1)])
        << "only the Saboteur's opponents";
    EXPECT_FALSE(state.battlefields[1].opp_hidden_unrevealable[playerIndex(P2)]) << "\"here\"";
}

TEST_F(C4Test, DariusTrifarian_SecondCardIsJudgedWhenItIsPlayed) {
    auto darius = addUnit(P1, kDariusTrifarian);
    o(darius).is_exhausted = true;
    withEngine([&](GameEngine& e) {
        auto& st = e.mutableState();
        st.player(P1).cards_played_this_turn = 2;
        e.testHook_executor().queueTriggeredAbility(darius, P1, TriggerType::WhenYouPlayASpell);
        // A third card is played in response, before the trigger resolves.
        st.player(P1).cards_played_this_turn = 3;
        e.testHook_runChain();
    });
    EXPECT_EQ(o(darius).temp_might_bonus, 2);
    EXPECT_FALSE(o(darius).is_exhausted);

    // The third card itself does not trigger him.
    o(darius).is_exhausted = true;
    withEngine([&](GameEngine& e) {
        e.mutableState().player(P1).cards_played_this_turn = 3;
        e.testHook_executor().queueTriggeredAbility(darius, P1, TriggerType::WhenYouPlayASpell);
        EXPECT_FALSE(e.mutableState().chain.exists()) << "not the second card: no trigger";
        e.testHook_runChain();
    });
    EXPECT_EQ(o(darius).temp_might_bonus, 2);
    EXPECT_TRUE(o(darius).is_exhausted);
}

TEST_F(C4Test, EkkoRecurrent_IsRecycledToReadyTheRunes) {
    auto ekko = addUnit(P1, kEkkoRecurrent, 0, 0);
    o(ekko).keywords = card_db.get(kEkkoRecurrent).keywords;
    auto r1 = addRune(P1, Domain::Mind, /*exhausted=*/true);
    auto r2 = addRune(P1, Domain::Mind, /*exhausted=*/true);
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) { exec.killObject(ekko); });
    EXPECT_FALSE(inTrash(P1, ekko)) << "\"Recycle me\": not left in the trash";
    EXPECT_TRUE(inDeck(P1, ekko));
    EXPECT_FALSE(o(r1).is_exhausted);
    EXPECT_FALSE(o(r2).is_exhausted);
}

TEST_F(C4Test, EkkoRecurrent_ThatCantBeRecycledReadiesNothing) {
    // Banished instead of trashed: there is no Ekko to recycle.
    auto ekko = offBoard(P1, kEkkoRecurrent, ZoneType::Banishment);
    auto r1 = addRune(P1, Domain::Mind, /*exhausted=*/true);
    fireTriggerThroughEngine(ekko, TriggerType::WhenIDie);
    EXPECT_TRUE(o(r1).is_exhausted);
    EXPECT_FALSE(inDeck(P1, ekko));
}

TEST_F(C4Test, CaitlynPatrolling_OnlyTheCopyAtABattlefieldMayShoot) {
    auto at_bf = addUnit(P1, kCaitlynPatrolling, 0, 0);
    auto in_base = addUnit(P1, kCaitlynPatrolling);
    addUnit(P2, kInvalidId, 3, 0);
    const auto legal = legalActions();
    EXPECT_TRUE(canActivate(legal, at_bf));
    EXPECT_FALSE(canActivate(legal, in_base)) << "\"only while I'm at a battlefield\"";
}

TEST_F(C4Test, SettBrawler_OnlyABuffedSettSpendsItsBuff) {
    auto buffed = addUnit(P1, kSettBrawler);
    auto plain = addUnit(P1, kSettBrawler);
    o(buffed).buff_count = 1;
    o(buffed).recomputeMight();
    const auto legal = legalActions();
    EXPECT_TRUE(canActivate(legal, buffed));
    EXPECT_FALSE(canActivate(legal, plain)) << "\"Spend my buff\": the unbuffed copy has none";

    activate(buffed);
    EXPECT_EQ(o(buffed).buff_count, 0);
    EXPECT_EQ(o(buffed).temp_might_bonus, 4);
    EXPECT_EQ(o(buffed).current_might, 4 + 4) << "base 4, the buff is gone, +4 this turn";
    EXPECT_FALSE(canActivate(legalActions(), buffed));
}

TEST_F(C4Test, UdyrWildman_SpendsItsBuffAndHonoursCantBeChosen) {
    auto udyr = addUnit(P1, kUdyrWildman, 0, 0);
    auto plain = addUnit(P1, kUdyrWildman);
    o(udyr).buff_count = 1;
    o(udyr).recomputeMight();
    auto target = addUnit(P2, kInvalidId, 5, 1);
    auto shrouded = addUnit(P2, kInvalidId, 5, 1);
    o(shrouded).untargetable_by_enemy_this_turn = true;
    o(shrouded).untargetable_by_enemy = true;

    const auto legal = legalActions();
    EXPECT_TRUE(canActivate(legal, udyr));
    EXPECT_FALSE(canActivate(legal, plain)) << "the unbuffed copy has no buff to spend";

    EnginePathAgent agent(EnginePathAgent::script({0, static_cast<int>(target.raw())}));  // "Deal 2"
    activate(udyr, &agent);
    ASSERT_EQ(agent.prompts.size(), 2u) << "the mode, then the unit";
    EXPECT_TRUE(agent.wasOffered(target));
    EXPECT_FALSE(agent.wasOffered(shrouded)) << "can't be chosen by enemy abilities";
    EXPECT_EQ(o(target).damage_marked, 2);
    EXPECT_EQ(o(udyr).buff_count, 0) << "the buff was spent, once";

    // The mode is used up for the turn: with a new buff, "Deal 2" is not offered.
    o(udyr).buff_count = 1;
    EnginePathAgent again(EnginePathAgent::script({2}));   // "Ready me"
    o(udyr).is_exhausted = true;
    activate(udyr, &again);
    ASSERT_FALSE(again.prompts.empty());
    for (const auto& i : again.prompts.front())
        EXPECT_NE(i.chosen_value.value_or(-1), 0) << "\"one you've not chosen this turn\"";
    EXPECT_FALSE(o(udyr).is_exhausted);
    EXPECT_FALSE(o(udyr).hasKeyword(Keyword::Ganking)) << "Ganking is only what mode 4 gives";
}

TEST_F(C4Test, Heimerdinger_HasOnlyTheExhaustAbilities) {
    auto heimer = addUnit(P1, kHeimerdinger);
    o(heimer).buff_count = 1;                 // Sett's "Spend my buff" would be payable
    addUnit(P1, kSettBrawler);
    addUnit(P1, kLeeSinAscetic);
    addUnit(P1, kLeeSinAscetic);              // a second copy grants nothing more
    const auto legal = legalActions();
    EXPECT_FALSE(canActivate(legal, heimer, kSettBrawler))
        << "\"Spend my buff: ...\" is not an [E] ability";
    EXPECT_EQ(std::count_if(legal.begin(), legal.end(), [&](const Intent& i) {
        return i.type == IntentType::ActivateAbility && i.ability_source == heimer &&
               i.granted_ability_def == kLeeSinAscetic; }), 1)
        << "\"[E]: Buff me.\" is granted, once";
}

TEST_F(C4Test, Rebuttal_ChoosesASpellUnderAnAbilityOnTheChain) {
    // "Choose a spell": a finalized spell anywhere on the chain (CR
    // 355.9.a.2; testing phase D21 — this pinned the old peek-and-pop rule,
    // under which an ability on top made Rebuttal unplayable).
    Card* rebuttal = card_registry.get(kRebuttal);
    ASSERT_NE(rebuttal, nullptr);
    EXPECT_FALSE(rebuttal->hasLegalTargets(state, P1)) << "empty chain";
    pushSpellOnChain(P2, kIncinerate);
    EXPECT_TRUE(rebuttal->hasLegalTargets(state, P1));
    ChainItem ability;
    ability.id = state.chain.allocateId();
    ability.status = ChainItemStatus::Finalized;
    ability.controller = P2;
    ability.is_ability = true;
    state.chain.items.push_back(ability);
    EXPECT_TRUE(rebuttal->hasLegalTargets(state, P1))
        << "the spell under the ability can still be chosen";
}

// ─── S5: the player chooses ─────────────────────────────────────────────────

TEST_F(C4Test, ViDestructive_RecyclesTheTrashCardItsControllerChooses) {
    auto vi = addUnit(P1, kViDestructive);
    auto oldest = inTrashOf(P1, kIncinerate);
    auto chosen = inTrashOf(P1, kPunchFirst);
    EnginePathAgent agent(EnginePathAgent::choosing(chosen));
    activate(vi, &agent);
    ASSERT_FALSE(agent.prompts.empty()) << "which card is a choice, not the oldest";
    EXPECT_TRUE(inDeck(P1, chosen));
    EXPECT_FALSE(inTrash(P1, chosen));
    EXPECT_TRUE(inTrash(P1, oldest));
    EXPECT_EQ(o(vi).temp_might_bonus, 1);
}

TEST_F(C4Test, DrMundoExpert_ChoosesWhichThreeCardsToRecycle) {
    auto mundo = addUnit(P1, kDrMundoExpert);
    std::vector<GameObjectId> trash;
    for (int i = 0; i < 5; ++i) trash.push_back(inTrashOf(P1, kIncinerate));
    // Keep the two oldest: pick the three newest.
    EnginePathAgent agent(EnginePathAgent::script(
        {static_cast<int>(trash[4].raw()), static_cast<int>(trash[3].raw()), static_cast<int>(trash[2].raw())}));
    fireTriggerThroughEngine(mundo, TriggerType::AtStartOfBeginning, &agent);
    EXPECT_EQ(agent.prompts.size(), 3u);
    EXPECT_TRUE(inTrash(P1, trash[0]));
    EXPECT_TRUE(inTrash(P1, trash[1]));
    for (int i = 2; i < 5; ++i) {
        EXPECT_FALSE(inTrash(P1, trash[i]));
        EXPECT_TRUE(inDeck(P1, trash[i]));
    }
    EXPECT_EQ(trashSize(P1), 2);
}

// Batch M1: the three cards are targets chosen as the ability is finalized
// (CR 355.10.a, 403.2), one prompt each like every target (exactly three
// there: all three, in the order the player names them); with fewer than
// three the ability is removed (CR 403.4, judge question 15). This test was
// "ThreeOrFewerAreAllRecycledUnasked" (two cards: both recycled, nothing asked).
TEST_F(C4Test, DrMundoExpert_ExactlyThreeAreAllRecycled) {
    auto mundo = addUnit(P1, kDrMundoExpert);
    inTrashOf(P1, kIncinerate);
    inTrashOf(P1, kIncinerate);
    inTrashOf(P1, kIncinerate);
    EnginePathAgent agent;
    fireTriggerThroughEngine(mundo, TriggerType::AtStartOfBeginning, &agent);
    EXPECT_EQ(agent.prompts.size(), 3u);
    EXPECT_EQ(trashSize(P1), 0);
    EXPECT_EQ(deckSize(P1), 3);
}

TEST_F(C4Test, DrMundoExpert_Pin_FewerThanThreeAreNotRecycled) {
    auto mundo = addUnit(P1, kDrMundoExpert);
    inTrashOf(P1, kIncinerate);
    inTrashOf(P1, kIncinerate);
    EnginePathAgent agent;
    fireTriggerThroughEngine(mundo, TriggerType::AtStartOfBeginning, &agent);
    EXPECT_EQ(trashSize(P1), 2);
    EXPECT_EQ(deckSize(P1), 0);
}

// ─── 03 / 07: choices, costs and re-entry ───────────────────────────────────

TEST_F(C4Test, AvaAchiever_PaysAsTheAbilityGoesOnTheChain) {
    auto ava = addUnit(P1, kAvaAchiever, 0, 0);
    auto hidden = addToHand(P1, kTeemoScout);
    state.player(P1).rune_pool.power[static_cast<int>(Domain::Mind)] = 1;
    EnginePathAgent agent(EnginePathAgent::choosing(hidden, EnginePathAgent::May::Yes));   // yes, then the card
    // The opponent gets priority after the ability is finalized and before it
    // resolves: by then the [C] is already gone (CR 383.3) and the card is not
    // yet played.
    SnoopAgent opponent([&](const GameState& s) {
        EXPECT_EQ(s.player(P1).rune_pool.power[static_cast<int>(Domain::Mind)], 0);
        EXPECT_FALSE(s.getObject(hidden).location.has_value());
    });
    fireTriggerThroughEngine(ava, TriggerType::WhenIAttack, &agent, kInvalidId, &opponent);
    EXPECT_GE(opponent.asked, 1) << "the opponent had a chance to respond";
    EXPECT_EQ(o(hidden).battlefieldId(), std::optional<BattlefieldId>{BattlefieldId::fromRaw(0)}) << "a unit: played here";
    EXPECT_FALSE(inHand(P1, hidden));
    EXPECT_EQ(state.player(P1).rune_pool.power[static_cast<int>(Domain::Mind)], 0) << "paid once";
}

TEST_F(C4Test, WildclawShaman_SpendsABuffToBuffAndReadyItself) {
    auto buffed = addUnit(P1, kInvalidId, 2);
    o(buffed).buff_count = 1;
    o(buffed).recomputeMight();
    auto card = addToHand(P1, kWildclawShaman);
    EnginePathAgent agent(EnginePathAgent::choosing(buffed, EnginePathAgent::May::Yes));   // yes, then whose buff
    playThroughEngine(card, &agent);
    EXPECT_EQ(o(buffed).buff_count, 0) << "\"spend a buff\"";
    EXPECT_EQ(o(card).buff_count, 1);
    EXPECT_FALSE(o(card).is_exhausted);
}

TEST_F(C4Test, WildclawShaman_WithNoBuffToSpendIsNotOffered) {
    addUnit(P1, kInvalidId, 2);
    auto card = addToHand(P1, kWildclawShaman);
    EnginePathAgent agent;
    playThroughEngine(card, &agent);
    EXPECT_TRUE(agent.prompts.empty());
    EXPECT_EQ(o(card).buff_count, 0);
    EXPECT_TRUE(o(card).is_exhausted);
}

TEST_F(C4Test, MissFortuneCaptain_FirstMoveOnly_AndLegendsCount) {
    auto mf = addUnit(P1, kMissFortuneCaptain);
    auto mine = legend(P1);
    o(mine).is_exhausted = true;
    state.battlefields[0].controller = P1;

    EnginePathAgent agent(EnginePathAgent::choosing(mine, EnginePathAgent::May::Yes));
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) { exec.moveToBattlefield(mf, BattlefieldId::fromRaw(0)); },
                         &agent);
    ASSERT_EQ(agent.prompts.size(), 2u) << "the may, then what to ready";
    EXPECT_TRUE(agent.wasOffered(mine)) << "an exhausted legend is \"something else that's exhausted\"";
    EXPECT_FALSE(o(mine).is_exhausted);

    // Her second move this turn does not trigger.
    o(mine).is_exhausted = true;
    EnginePathAgent second;
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) { exec.moveToBase(mf); }, &second);
    EXPECT_TRUE(second.prompts.empty()) << "\"the first time I move each turn\"";
    EXPECT_TRUE(o(mine).is_exhausted);
}

TEST_F(C4Test, TwistedFateGambler_RevealsOneRuneHoweverOftenItIsReEntered) {
    auto tf = addUnit(P1, kTwistedFate, 0, 0);
    auto here = addUnit(P2, kInvalidId, 5, 0);
    auto also_here = addUnit(P2, kInvalidId, 5, 0);
    // Rune deck: a Mind rune under a Fury rune (top = back).
    std::vector<GameObjectId> runes;
    for (Domain d : {Domain::Mind, Domain::Fury}) {
        auto r = addRune(P1, d);
        o(r).location = std::nullopt;
        o(r).zone = ZoneType::RuneDeck;
        state.player(P1).rune_deck.push_back(r);
        runes.push_back(r);
    }
    EnginePathAgent agent(EnginePathAgent::choosing(here));
    fireTriggerThroughEngine(tf, TriggerType::WhenIAttack, &agent);
    ASSERT_EQ(agent.prompts.size(), 1u);
    // Only the Fury rune was revealed and recycled: the Mind rune is now on top.
    ASSERT_EQ(state.player(P1).rune_deck.size(), 2u);
    EXPECT_EQ(state.player(P1).rune_deck.back(), runes[0]) << "the second rune was not revealed";
    EXPECT_EQ(state.player(P1).rune_deck.front(), runes[1]);
    EXPECT_EQ(handSize(P1), 0) << "no [B] payoff from a second reveal";
    EXPECT_EQ(o(here).damage_marked, 2);
    EXPECT_EQ(o(also_here).damage_marked, 1);
}

TEST_F(C4Test, CruelPatron_NeedsAFriendlyUnitAndKillsTheChosenOne) {
    Card* patron = card_registry.get(kCruelPatron);
    ASSERT_NE(patron, nullptr);
    EXPECT_FALSE(patron->canBePlayed(state, P1)) << "no friendly unit: the cost can't be paid";
    auto a = addUnit(P1, kInvalidId, 2);
    auto b = addUnit(P1, kInvalidId, 2);
    EXPECT_TRUE(patron->canBePlayed(state, P1));

    auto card = addToHand(P1, kCruelPatron);
    EnginePathAgent agent(EnginePathAgent::choosing(b));
    playThroughEngine(card, &agent);
    EXPECT_FALSE(o(b).location.has_value());
    EXPECT_TRUE(o(a).location.has_value());
    EXPECT_TRUE(o(card).location.has_value());
}

TEST_F(C4Test, Blitzcrank_PullsTheEnemyUnitItsControllerChooses) {
    state.battlefields[0].controller = P1;
    auto first = addUnit(P2, kInvalidId, 3, 1);
    auto chosen = addUnit(P2, kInvalidId, 3);
    auto shrouded = addUnit(P2, kInvalidId, 3, 1);
    o(shrouded).untargetable_by_enemy_this_turn = true;
    o(shrouded).untargetable_by_enemy = true;
    auto card = addToHand(P1, kBlitzcrank);
    EnginePathAgent agent(EnginePathAgent::choosing(chosen, EnginePathAgent::May::Yes));
    playThroughEngine(card, &agent, LocationId{BattlefieldLocation{0}});
    ASSERT_EQ(agent.prompts.size(), 2u) << "the may, then the unit";
    EXPECT_TRUE(agent.wasOffered(first));
    EXPECT_FALSE(agent.wasOffered(shrouded)) << "can't be chosen by enemy abilities";
    EXPECT_EQ(o(chosen).battlefieldId(), std::optional<BattlefieldId>{BattlefieldId::fromRaw(0)});
    EXPECT_EQ(o(first).battlefieldId(), std::optional<BattlefieldId>{BattlefieldId::fromRaw(1)});
}

TEST_F(C4Test, CantBeChosenUnitsAreNotOfferedToAhriTeemoOrSnapvine) {
    auto shrouded = addUnit(P2, kInvalidId, 5, 0);
    o(shrouded).untargetable_by_enemy_this_turn = true;
    o(shrouded).untargetable_by_enemy = true;
    auto open = addUnit(P2, kInvalidId, 5, 0);

    auto ahri = addUnit(P1, kAhriInquisitive, 0, 0);
    EnginePathAgent a1;
    fireTriggerThroughEngine(ahri, TriggerType::WhenIAttackOrDefend, &a1);
    EXPECT_TRUE(a1.wasOffered(open));
    EXPECT_FALSE(a1.wasOffered(shrouded));

    auto teemo = addUnit(P1, kTeemoStrategist, 0, 0);
    EnginePathAgent a2;
    fireTriggerThroughEngine(teemo, TriggerType::WhenIDefend, &a2);
    EXPECT_TRUE(a2.wasOffered(open));
    EXPECT_FALSE(a2.wasOffered(shrouded));

    auto vine = addUnit(P1, kSnapvine, 0, 1);
    EnginePathAgent a3;
    fireTriggerThroughEngine(vine, TriggerType::WhenYouPlayMe, &a3);
    EXPECT_TRUE(a3.wasOffered(open));
    EXPECT_FALSE(a3.wasOffered(shrouded));
}

// "When I defend, choose an enemy unit here and reveal the top 5 cards of your
// Main Deck. ..." With no enemy unit he can choose, CR 403.4 removes the whole
// ability as it would be finalized ("If there are not enough options to make
// legal choices for a Triggered Ability that has been put on the chain, remove
// it from the Chain now") — the reveal and the recycle included. This test
// used to pin the opposite (the reveal happened all the same, by analogy with
// a mistarget, CR 359.3.e); the strict reading is implemented since finding D4
// and the case is with the judges (docs/judge-questions-2026-10.md, 6).
TEST_F(C4Test, TeemoStrategist_NoUnitToChoose_TheAbilityLeavesTheChainAndNothingIsRevealed) {
    auto teemo = addUnit(P1, kTeemoStrategist, 0, 0);
    auto top = addToDeck(P1, kTeemoScout);
    int revealed = 0, finalized = 0;
    auto conn = events.on_card_revealed.connect([&](const CardRevealedEvent& e) {
        if (e.card == top) ++revealed;
    });
    auto fin = events.on_chain_item_finalized.connect(
        [&](const ChainItemFinalizedEvent&) { ++finalized; });
    fireTriggerThroughEngine(teemo, TriggerType::WhenIDefend);
    conn.disconnect();
    fin.disconnect();
    EXPECT_EQ(finalized, 0) << "removed, never finalized (CR 403.4)";
    EXPECT_EQ(revealed, 0);
    ASSERT_EQ(deckSize(P1), 1);
    EXPECT_EQ(state.player(P1).main_deck.back(), top) << "the deck is untouched";
}

TEST_F(C4Test, TeemoStrategist_WithAUnitToChoose_RevealsAndRecycles) {
    auto teemo = addUnit(P1, kTeemoStrategist, 0, 0);
    auto enemy = addUnit(P2, kInvalidId, 3, 0);
    auto top = addToDeck(P1, kTeemoScout);
    int revealed = 0;
    auto conn = events.on_card_revealed.connect([&](const CardRevealedEvent& e) {
        if (e.card == top) ++revealed;
    });
    EnginePathAgent agent(EnginePathAgent::choosing(enemy));
    fireTriggerThroughEngine(teemo, TriggerType::WhenIDefend, &agent);
    conn.disconnect();
    EXPECT_EQ(revealed, 1);
    ASSERT_EQ(deckSize(P1), 1);
    EXPECT_TRUE(inDeck(P1, top)) << "and recycled";
}

TEST_F(C4Test, CarnivorousSnapvine_ThatLeftTheBoardFightsNobody) {
    // More Might than the Snapvine: a killed-and-reset unit cannot pass for "no damage".
    auto enemy = addUnit(P2, kInvalidId, 9, 0);
    auto vine = addUnit(P1, kSnapvine);
    EnginePathAgent agent(EnginePathAgent::choosing(enemy));
    withEngine([&](GameEngine& e) {
        e.testHook_executor().queueTriggeredAbility(vine, P1, TriggerType::WhenYouPlayMe);
        e.testHook_executor().bounceToHand(vine);     // returned to hand in response
        e.testHook_runChain();
    }, &agent);
    EXPECT_TRUE(o(enemy).location.has_value());
    EXPECT_EQ(o(enemy).damage_marked, 0);
    EXPECT_TRUE(inHand(P1, vine));
    EXPECT_FALSE(inTrash(P1, vine)) << "not also \"killed\" from the hand";
}

TEST_F(C4Test, KaiSaEvolutionary_PlaysTheSpellItsControllerChooses) {
    auto kaisa = addUnit(P1, kKaiSaEvolutionary, 0, 0);
    setScore(P1, 5);
    auto older = inTrashOf(P1, kPunchFirst);
    auto newest = inTrashOf(P1, kIncinerate);
    auto ally = addUnit(P1, kInvalidId, 2);
    const auto& def = card_db.get(kPunchFirst);
    state.player(P1).rune_pool.universal_power += def.power_cost;
    EnginePathAgent agent([&](const std::vector<Intent>& legal) {
        for (const auto& i : legal)
            if (!i.chosen_objects.empty() && i.chosen_objects.front() == older) return i;
        for (const auto& i : legal)
            if (!i.chosen_objects.empty() && i.chosen_objects.front() == ally) return i;
        for (const auto& i : legal)
            if (i.chosen_value.has_value() && *i.chosen_value == 1) return i;
        return legal.front();
    });
    fireTriggerThroughEngine(kaisa, TriggerType::WhenIConquer, &agent);
    EXPECT_TRUE(agent.wasOffered(newest));
    EXPECT_TRUE(agent.wasOffered(older)) << "any eligible spell, not only the most recent";
    EXPECT_TRUE(inTrash(P1, newest)) << "the spell that was not chosen stays";
    EXPECT_FALSE(inTrash(P1, older));
    EXPECT_TRUE(inDeck(P1, older)) << "\"Then recycle it.\"";
    EXPECT_EQ(o(ally).temp_might_bonus, 5) << "Punch First resolved";
}

// ─── batch-09: a unit played by an effect may go to a battlefield you control ─

TEST_F(C4Test, Soulgorger_PlaysTheChosenUnitWhereItsControllerChooses) {
    state.battlefields[1].controller = P1;
    addUnit(P1, kInvalidId, 1, 1);   // held: control without a unit there is lost at the next Cleanup (CR 323.5)
    auto dead = inTrashOf(P1, kFirstMate);
    auto card = addToHand(P1, kSoulgorger);
    // yes; the unit; battlefield 1 (mode 2).
    EnginePathAgent agent(EnginePathAgent::script({
        1,                        // Soulgorger: "may play a unit from your trash": yes
        static_cast<int>(dead.raw()),   // which unit (First Mate, the only one)
        2,                        // where: mode 2 = battlefield #1 (mode 0 = base)
        static_cast<int>(card.raw()),   // the played First Mate's "ready another unit": Soulgorger,
                                  // the only other unit on the board
    }));
    playThroughEngine(card, &agent);
    // (A fourth prompt is the revived First Mate's own play trigger.)
    ASSERT_GE(agent.prompts.size(), 3u) << "the may, the unit, where";
    EXPECT_EQ(o(dead).battlefieldId(), std::optional<BattlefieldId>{BattlefieldId::fromRaw(1)});
    EXPECT_FALSE(inTrash(P1, dead));
}

TEST_F(C4Test, FlameChompers_PaysAndChoosesItsLandingPlace) {
    state.battlefields[0].controller = P1;
    auto chompers = offBoard(P1, kFlameChompers, ZoneType::Trash);
    state.player(P1).rune_pool.power[static_cast<int>(Domain::Fury)] = 1;
    EnginePathAgent agent(EnginePathAgent::script({1, 1}));   // yes; battlefield 0 (mode 1)
    fireTriggerThroughEngine(chompers, TriggerType::WhenIAmDiscarded, &agent);
    EXPECT_EQ(o(chompers).battlefieldId(), std::optional<BattlefieldId>{BattlefieldId::fromRaw(0)});
    EXPECT_FALSE(inTrash(P1, chompers));
    EXPECT_EQ(state.player(P1).rune_pool.power[static_cast<int>(Domain::Fury)], 0);
}

TEST_F(C4Test, ImmortalPhoenix_PaysFromThePoolAndOneRuneCoversBothHalves) {
    // Floating Energy and Fury Power pay [1][R]: no rune needed (7.35).
    auto phoenix = offBoard(P1, kImmortalPhoenix, ZoneType::Trash);
    state.player(P1).rune_pool.energy = 1;
    state.player(P1).rune_pool.power[static_cast<int>(Domain::Fury)] = 1;
    EnginePathAgent yes(EnginePathAgent::script({1}));
    fireTriggerThroughEngine(phoenix, TriggerType::WhenYouKillAUnitWithASpell, &yes);
    EXPECT_TRUE(o(phoenix).isAtBase());
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0);
    EXPECT_EQ(state.player(P1).rune_pool.power[static_cast<int>(Domain::Fury)], 0);

    // A single ready Fury rune: exhausted for the [1], then recycled for the [R].
    auto second = offBoard(P1, kImmortalPhoenix, ZoneType::Trash);
    auto rune = addRune(P1, Domain::Fury);
    EnginePathAgent yes2(EnginePathAgent::script({1}));
    fireTriggerThroughEngine(second, TriggerType::WhenYouKillAUnitWithASpell, &yes2);
    EXPECT_TRUE(o(second).isAtBase());
    EXPECT_FALSE(o(rune).location.has_value()) << "recycled for the Power";

    // Nothing to pay with: not offered.
    auto third = offBoard(P1, kImmortalPhoenix, ZoneType::Trash);
    EnginePathAgent none;
    fireTriggerThroughEngine(third, TriggerType::WhenYouKillAUnitWithASpell, &none);
    EXPECT_TRUE(none.prompts.empty());
    EXPECT_TRUE(inTrash(P1, third));
}

// (Testing phase D71, by design: the ability is a replacement applied as he is
// seen — EffectExecutor::seenOnTopOfDeck — no longer a WhenIRevealedFromTop
// triggered ability fired by hand. Same prompts, same answers.)
TEST_F(C4Test, Nocturne_PlayingCostsOnePower_AndBanishingAloneIsAllowed) {
    auto nocturne = offBoard(P1, kNocturne, ZoneType::MainDeck);
    state.player(P1).rune_pool.universal_power = 1;
    EnginePathAgent agent(EnginePathAgent::script({1, 1}));   // banish; play to base
    resolveThroughEngine([&](EffectExecutor& ex, GameState&) { ex.seenOnTopOfDeck(P1, nocturne); },
                         &agent);
    EXPECT_TRUE(o(nocturne).isAtBase());
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 0) << "\"play me for [A]\" is paid";
    const auto& bz = state.player(P1).banishment;
    EXPECT_EQ(std::count(bz.begin(), bz.end(), nocturne), 0);
    EXPECT_FALSE(inDeck(P1, nocturne));

    // No Power: the banish is still a "may", playing is not on offer.
    auto second = offBoard(P1, kNocturne, ZoneType::MainDeck);
    EnginePathAgent poor(EnginePathAgent::script({
        1,   // "may banish me": yes
        0,   // "play me for [A]?": mode 0 = "Don't play me", the only mode on offer
    }));
    resolveThroughEngine([&](EffectExecutor& ex, GameState&) { ex.seenOnTopOfDeck(P1, second); },
                         &poor);
    // Only "don't play me" is open: a choice of one is not asked.
    ASSERT_EQ(poor.prompts.size(), 1u) << "the banish only";
    EXPECT_FALSE(o(second).location.has_value());
    EXPECT_EQ(o(second).zone, ZoneType::Banishment);
    EXPECT_FALSE(inDeck(P1, second));
}

// ─── 07 items 7.11 / 7.30: spells ───────────────────────────────────────────

TEST_F(C4Test, ShadowDash_AsksForTheEnemyUnitOnceAndPaysItsDeflect) {
    addUnit(P1, kInvalidId, 2, 0);
    auto lone = addUnit(P1, kInvalidId, 2, 0);
    (void)lone;
    auto enemy = addUnit(P2, kInvalidId, 3, 1);
    o(enemy).keywords.set(Keyword::Deflect);
    o(enemy).deflect_value = 1;
    auto other = addUnit(P2, kInvalidId, 3, 1);
    state.player(P1).rune_pool.universal_power = 1;   // the Deflect; the card's cost is funded
    auto spell = addToHand(P1, kShadowDash);
    EnginePathAgent agent(EnginePathAgent::choosing(enemy));
    playThroughEngine(spell, &agent);
    ASSERT_EQ(agent.prompts.size(), 2u)
        << "the unit is chosen once, as the spell is played — not again as it resolves";
    EXPECT_TRUE(agent.wasOffered(other));
    EXPECT_TRUE(agent.prompts[1].front().chosen_objects.empty()) << "then only the battlefield";
    EXPECT_EQ(o(enemy).battlefieldId(), std::optional<BattlefieldId>{BattlefieldId::fromRaw(0)});
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 0) << "Deflect was paid for the choice";
    EXPECT_TRUE(state.player(P1).chose_enemy_unit_this_turn);
}

TEST_F(C4Test, ShadowDash_ExactlyTwoUnitsThereEachGetPlusOne) {
    auto a = addUnit(P1, kInvalidId, 2, 0);
    auto b = addUnit(P1, kInvalidId, 2, 0);
    auto enemy = addUnit(P2, kInvalidId, 3, 1);
    auto spell = addToHand(P1, kShadowDash);
    EnginePathAgent agent(EnginePathAgent::choosing(enemy));
    playThroughEngine(spell, &agent);
    EXPECT_EQ(o(enemy).battlefieldId(), std::optional<BattlefieldId>{BattlefieldId::fromRaw(0)});
    EXPECT_EQ(o(a).temp_might_bonus, 1);
    EXPECT_EQ(o(b).temp_might_bonus, 1);
}

TEST_F(C4Test, AccelerationGate_ChargesDeflectForAnEnemyUnitItReadies) {
    auto mine = addUnit(P1, kInvalidId, 2);
    o(mine).is_exhausted = true;
    auto enemy = addUnit(P2, kInvalidId, 3, 0);
    o(enemy).is_exhausted = true;
    o(enemy).keywords.set(Keyword::Deflect);
    o(enemy).deflect_value = 1;
    auto spell = addToHand(P1, kAccelerationGate);

    // Without the [A] for its Deflect the enemy unit is not offered.
    EnginePathAgent poor(EnginePathAgent::choosing(mine));
    playThroughEngine(spell, &poor);
    EXPECT_FALSE(poor.wasOffered(enemy));
    EXPECT_FALSE(o(mine).is_exhausted);

    auto again = addToHand(P1, kAccelerationGate);
    state.player(P1).rune_pool.universal_power = 1;
    EnginePathAgent rich(EnginePathAgent::choosing(enemy));
    playThroughEngine(again, &rich);
    EXPECT_FALSE(o(enemy).is_exhausted);
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 0) << "Deflect paid for the pick";
    EXPECT_TRUE(state.player(P1).chose_enemy_unit_this_turn);
}

// ─── 08 item 8.4: aura-granted Vision ───────────────────────────────────────

TEST_F(C4Test, GemcraftSeer_UnitsPlayedBesideItGetTheirVisionLook) {
    // Teemo, Scout has no Vision of his own: played alone, there is no look
    // and the top card stays. Beside Gemcraft Seer he has one, and the
    // controller (here: always "recycle") puts the card on the bottom.
    addToDeck(P1, kFirstMate);
    auto top = addToDeck(P1, kIncinerate);
    auto alone = addToHand(P1, kTeemoScout);
    playThroughEngine(alone);
    ASSERT_EQ(state.player(P1).main_deck.back(), top);

    addUnit(P1, kGemcraftSeer);
    auto unit = addToHand(P1, kTeemoScout);
    EnginePathAgent recycle(EnginePathAgent::first());
    playThroughEngine(unit, &recycle);
    EXPECT_EQ(recycle.prompts.size(), 1u) << "the Vision look asked once";
    EXPECT_EQ(state.player(P1).main_deck.front(), top)
        << "\"Other friendly units have [Vision]\": the played unit looked at the top card";
}

// ─── 08 S3 / 02 item 2.2: one aura hook call covers every copy, once each ───

TEST_F(C4Test, TwoWizenedElders_EachGetTheirOwnPlusOneOnce) {
    auto e1 = addUnit(P1, kWizenedElder);
    auto e2 = addUnit(P1, kWizenedElder);
    auto plain = addUnit(P1, kWizenedElder);
    o(e1).buff_count = 1;
    o(e2).buff_count = 1;
    withEngine([&](GameEngine& e) { e.testHook_cleanup(); });
    EXPECT_EQ(o(e1).current_might, 4 + 1 + 1) << "buff, and \"an additional +1\" — not +2";
    EXPECT_EQ(o(e2).current_might, 4 + 1 + 1);
    EXPECT_EQ(o(plain).current_might, 4) << "not buffed: no bonus";
}

// ─── batch-04: Kayn, Unleashed counts moves made by effects (02 item 2.22) ──

TEST_F(C4Test, KaynUnleashed_MovedTwiceByEffectsTakesNoDamage) {
    auto kayn = addUnit(P1, kKaynUnleashed);
    withEngine([&](GameEngine& e) {
        auto& exec = e.testHook_executor();
        exec.moveToBattlefield(kayn, BattlefieldId::fromRaw(0));
        e.testHook_cleanup();
        exec.dealDamage(kayn, 1, kInvalidId);
        EXPECT_EQ(e.mutableState().getObject(kayn).damage_marked, 1) << "one move: not immune";
        exec.moveToBase(kayn);
        e.testHook_cleanup();
        exec.dealDamage(kayn, 3, kInvalidId);
    });
    EXPECT_EQ(o(kayn).damage_marked, 1) << "moved twice this turn: \"I don't take damage\"";
}

}  // namespace
}  // namespace test_review_fix_c4
}  // namespace riftbound::test
