/// @file test_review_fix_c7.cpp
/// Regression tests for the card findings of the 2026-10 engine review in the
/// range units/0710 .. units/0955 (plans/cards/batch-07.md, and the entries of
/// plans/03, 07, 08 and cards/batch-10 that name a card in that range).
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
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace riftbound::test {
namespace test_review_fix_c7 {  // file-unique: test sources are unity-built
namespace {

constexpr CardDefId kPoutyPoro = 13;          // a unit with [Deflect]
constexpr CardDefId kSealOfStrength = 163;    // a non-token gear, [0][O]
constexpr CardDefId kKarthusEternal = 236;
constexpr CardDefId kIncinerate = 301;        // a spell
constexpr CardDefId kDiana = 711;
constexpr CardDefId kVexApathetic = 712;
constexpr CardDefId kCarrionDredger = 715;
constexpr CardDefId kCrimsonPigeons = 716;
constexpr CardDefId kLoyalPoro = 718;         // a 3-cost Poro with no Power cost
constexpr CardDefId kScrutinizingSergeant = 719;
constexpr CardDefId kUltrasoftPoro = 722;
constexpr CardDefId kEnthrallingProtector = 724;
constexpr CardDefId kSafetyInspector = 726;
constexpr CardDefId kStalkingWolf = 728;
constexpr CardDefId kAsheFocused = 731;
constexpr CardDefId kAtakhan = 732;
constexpr CardDefId kLeBlancFragmented = 734;
constexpr CardDefId kViPeacekeeper = 738;     // also: a unit with a Power cost
constexpr CardDefId kRiftHerald = 741;
constexpr CardDefId kDaisy = 754;
constexpr CardDefId kPlunderingPoro = 778;
constexpr CardDefId kBladeTwirler = 789;
constexpr CardDefId kBaccaiReaper = 796;
constexpr CardDefId kZedFromTheShadows = 810;
constexpr CardDefId kAstralHeron = 831;
constexpr CardDefId kJayceBrilliantInventor = 855;
constexpr CardDefId kBaccaiWitherclaw = 865;
constexpr CardDefId kProfiteer = 869;
constexpr CardDefId kCorruptedDragon = 878;
constexpr CardDefId kRenektonBrute = 879;
constexpr CardDefId kShadowOrderDisciple = 882;
constexpr CardDefId kSpiderling = 884;
constexpr CardDefId kGustMonk = 888;
constexpr CardDefId kRavenbloomPrefect = 889;
constexpr CardDefId kTailCloakedMatriarch = 891;
constexpr CardDefId kIllaoiProphet = 896;
constexpr CardDefId kKharox = 901;
constexpr CardDefId kNoxianEmissary = 915;
constexpr CardDefId kShadowClone = 955;
constexpr CardDefId kDredgeUpForOrdinal = 836;   // any spell

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

class C7Test : public CardTestFixture {
protected:
    GameObject& o(GameObjectId id) { return state.getObject(id); }

    /// A P1 spell that was played as the `ordinal`-th card of this turn (as
    /// TriggerManager::onCardPlayed stamps it).
    GameObjectId playedAs(int ordinal) {
        auto id = inTrashOf(P1, kDredgeUpForOrdinal);
        o(id).card_counters["__play_ordinal"] = ordinal;
        o(id).card_counters["__play_ordinal_turn"] = state.turn.turn_number + 1;
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
    /// A unit `p` controls with exactly `tags`.
    GameObjectId tagged(PlayerId p, std::vector<std::string> tags, int at_bf = -1) {
        auto id = addUnit(p, kInvalidId, 2, at_bf);
        o(id).tags = std::move(tags);
        return id;
    }
    /// An enemy unit that can't be chosen by enemy spells and abilities.
    GameObjectId shrouded(PlayerId p, int might, int at_bf) {
        auto id = addUnit(p, kInvalidId, might, at_bf);
        o(id).untargetable_by_enemy_this_turn = true;
        o(id).untargetable_by_enemy = true;
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
            e.testHook_executeIntent(a);
            e.testHook_runChain();
        }, agent, nullptr, a.player);
    }
    /// Run the engine's cleanup (aura pass included).
    void cleanup() {
        withEngine([](GameEngine& e) { e.testHook_cleanup(); });
    }
    /// A cleanup, then whatever it put on the chain.
    void settle() {
        withEngine([](GameEngine& e) { e.testHook_cleanup(); e.testHook_runChain(); });
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
    bool canActivate(const std::vector<Intent>& legal, GameObjectId source) {
        return std::any_of(legal.begin(), legal.end(), [&](const Intent& i) {
            return i.type == IntentType::ActivateAbility && i.ability_source == source;
        });
    }
    std::vector<GameObjectId> tokensNamed(PlayerId p, const std::string& name) {
        std::vector<GameObjectId> out;
        for (const auto& [id, obj] : state.objects)
            if (obj.isToken() && obj.controller == p && obj.location.has_value() &&
                obj.name == name) out.push_back(id);
        std::sort(out.begin(), out.end());
        return out;
    }
    bool banished(PlayerId owner, GameObjectId id) {
        const auto& b = state.player(owner).banishment;
        return std::find(b.begin(), b.end(), id) != b.end();
    }
};

// ─── batch-07 S2: the source must still be on the board ─────────────────────

TEST_F(C7Test, Diana_GetsTheMightOnlyWhileOnTheBoard) {
    auto diana = addUnit(P1, kDiana, 0, 0);
    fireTriggerThroughEngine(diana, TriggerType::WhenYouPlayASpell);
    EXPECT_EQ(o(diana).temp_might_bonus, 2);

    auto gone = addUnit(P1, kDiana);
    o(gone).location = std::nullopt;
    o(gone).zone = ZoneType::Trash;
    state.player(P1).trash.push_back(gone);
    fireTriggerThroughEngine(gone, TriggerType::WhenYouPlayASpell);
    EXPECT_EQ(o(gone).temp_might_bonus, 0) << "a Diana in the trash is not given Might";
}

TEST_F(C7Test, ShadowClone_OffTheBoardBanishesNothing) {
    auto clone = addUnit(P1, kShadowClone);
    o(clone).location = std::nullopt;
    o(clone).zone = ZoneType::Banishment;
    auto corpse = inTrashOf(P1, kLoyalPoro);
    EnginePathAgent yes(EnginePathAgent::choosing(corpse));
    fireTriggerThroughEngine(clone, TriggerType::WhenIAttack, &yes);
    EXPECT_TRUE(inTrash(P1, corpse));
}

// ─── 03 item 3.15: Vex stuns the unit that was played, if it is still there ──

TEST_F(C7Test, VexApathetic_StunsTheUnitAnOpponentPlays) {
    auto vex = addUnit(P2, kVexApathetic, 0, 0);
    auto card = addToHand(P1, kLoyalPoro);
    playThroughEngine(card);
    EXPECT_TRUE(o(card).is_stunned);

    // The remembered unit has left the board (back in hand): nothing happens.
    auto left = addToHand(P1, kLoyalPoro);
    fireTriggerThroughEngine(vex, TriggerType::WhenOpponentPlaysAUnit, nullptr, /*subject=*/left);
    EXPECT_FALSE(o(left).is_stunned);
}

// ─── 08 item 8.8: keywords that belong to the token, or are missing ─────────

TEST_F(C7Test, DeflectBelongsToTheBirdTokensOnly) {
    for (CardDefId id : {kCarrionDredger, kUltrasoftPoro}) {
        const auto& def = card_db.get(id);
        EXPECT_FALSE(def.keywords.has(Keyword::Deflect)) << def.name;
        EXPECT_EQ(def.deflect_value, 0) << def.name;
    }
    auto dredger = addUnit(P1, kCarrionDredger);
    resolveThroughEngine([&](EffectExecutor& ex, GameState&) { ex.killObject(dredger); });
    auto birds = tokensNamed(P1, "Bird");
    ASSERT_EQ(birds.size(), 1u);
    EXPECT_TRUE(o(birds[0]).keywords.has(Keyword::Deflect));
    EXPECT_TRUE(o(birds[0]).isAtBase()) << "\"to your base\"";
}

TEST_F(C7Test, EmpoweredDeathknellCardsCarryTheDeathknellKeyword) {
    EXPECT_TRUE(card_db.get(kBaccaiWitherclaw).keywords.has(Keyword::Deathknell));
    EXPECT_TRUE(card_db.get(kNoxianEmissary).keywords.has(Keyword::Deathknell));
}

// Karthus repeats only abilities of objects that carry [Deathknell].
TEST_F(C7Test, Karthus_RepeatsAnEmpoweredDeathknell) {
    addUnit(P1, kKarthusEternal, 0, 0);
    auto emissary = addUnit(P1, kNoxianEmissary);
    o(emissary).empowered = 1;
    withEngine([&](GameEngine& e) {
        e.testHook_cleanup();   // Karthus's aura
        e.testHook_executor().killObject(emissary);
        e.testHook_runChain();
    });
    EXPECT_EQ(tokensNamed(P1, "Recruit").size(), 4u) << "two Recruits, twice";
}

// XP is gained through gainXp, which keeps "XP gained this turn".
TEST_F(C7Test, HuntAndScrutinizingSergeant_CountAsXpGainedThisTurn) {
    auto fae = addUnit(P1, kEnthrallingProtector, 0, 0);
    fireTriggerThroughEngine(fae, TriggerType::WhenIConquerOrHold);
    EXPECT_EQ(state.player(P1).xp, 1);
    EXPECT_EQ(state.player(P1).xp_gained_this_turn, 1);
    auto sergeant = addUnit(P1, kScrutinizingSergeant);
    fireTriggerThroughEngine(sergeant, TriggerType::WhenYouPlayMe);
    EXPECT_EQ(state.player(P1).xp, 3) << "1 XP for each friendly unit";
    EXPECT_EQ(state.player(P1).xp_gained_this_turn, 3);
}

// ─── batch-07: Ultrasoft Poro ───────────────────────────────────────────────

TEST_F(C7Test, UltrasoftPoro_OnlyThePoroAtABattlefieldCanUseItsAbility) {
    auto at_bf = addUnit(P1, kUltrasoftPoro, 0, 0);
    auto in_base = addUnit(P1, kUltrasoftPoro);
    auto legal = legalActions();
    EXPECT_TRUE(canActivate(legal, at_bf));
    EXPECT_FALSE(canActivate(legal, in_base)) << "\"only while I'm at a battlefield\"";
}

TEST_F(C7Test, UltrasoftPoro_EachBirdGoesWhereItsControllerChooses) {
    auto poro = addUnit(P1, kUltrasoftPoro, 0, 0);
    state.battlefields[0].controller = P1;
    // First Bird to the battlefield (option 1), second to the base (option 0).
    EnginePathAgent agent(EnginePathAgent::script({1, 0}));
    activate(poro, &agent);
    ASSERT_EQ(agent.prompts.size(), 2u) << "one location prompt per token";
    auto birds = tokensNamed(P1, "Bird");
    ASSERT_EQ(birds.size(), 2u);
    EXPECT_TRUE(o(birds[0]).isAtBattlefield());
    EXPECT_TRUE(o(birds[1]).isAtBase());
    EXPECT_TRUE(o(birds[0]).keywords.has(Keyword::Deflect));
    EXPECT_TRUE(o(poro).is_exhausted);
}

// ─── 08 S3 / S4: fixed by the engine, verified per card ─────────────────────

TEST_F(C7Test, TwoCrimsonPigeonsAttackingTogether_EachGetPlusTwoOnce) {
    auto a = addUnit(P1, kCrimsonPigeons, 0, 0);
    auto b = addUnit(P1, kCrimsonPigeons, 0, 0);
    addUnit(P2, kInvalidId, 2, 0);
    // A combat at their battlefield, P1 attacking (CR 323.2 designations).
    state.battlefields[0].combat_in_progress = true;
    state.battlefields[0].attacker = P1;
    state.battlefields[0].defender = P2;
    cleanup();
    EXPECT_EQ(o(a).current_might, 5);
    EXPECT_EQ(o(b).current_might, 5);
}

TEST_F(C7Test, EnthrallingProtector_ASecondActivationAddsNoSecondBuff) {
    auto fae = addUnit(P1, kEnthrallingProtector, 0, 0);
    setXp(P1, 4);
    activate(fae);
    activate(fae);
    EXPECT_EQ(o(fae).buff_count, 1) << "a unit with a Buff is not buffed again (CR 427.1.b)";
    EXPECT_EQ(state.player(P1).xp, 0);
}

// ─── batch-10: Safety Inspector ─────────────────────────────────────────────

TEST_F(C7Test, SafetyInspector_EachPlayerChoosesTheirOwnUnit) {
    auto mine = addUnit(P1, kInvalidId, 2);
    auto weak = addUnit(P2, kInvalidId, 1, 0);
    auto strong = addUnit(P2, kInvalidId, 5, 0);
    auto card = addToHand(P1, kSafetyInspector);
    EnginePathAgent me(EnginePathAgent::choosing(mine));
    me.pick = [mine](const std::vector<Intent>& legal) {   // no XP: only the unit prompt
        for (const auto& i : legal)
            if (!i.chosen_objects.empty() && i.chosen_objects.front() == mine) return i;
        return legal.front();
    };
    EnginePathAgent them(EnginePathAgent::choosing(strong));
    playThroughEngine(card, &me, std::nullopt, &them);
    EXPECT_TRUE(them.wasOffered(weak));
    EXPECT_FALSE(them.wasOffered(mine)) << "a player chooses among their own units";
    EXPECT_FALSE(o(strong).location.has_value()) << "the opponent's own choice dies";
    EXPECT_TRUE(o(weak).location.has_value());
    EXPECT_FALSE(o(mine).location.has_value());
    EXPECT_TRUE(o(card).location.has_value());
}

TEST_F(C7Test, SafetyInspector_ThreeXpPaidUpFrontSparesYourUnit) {
    auto mine = addUnit(P1, kInvalidId, 2);
    auto theirs = addUnit(P2, kInvalidId, 5, 0);
    auto card = addToHand(P1, kSafetyInspector);
    setXp(P1, 3);
    EnginePathAgent me(EnginePathAgent::script({1}));       // yes
    // By the time the opponent can respond the XP is spent (CR 383.3).
    SnoopAgent them([&](const GameState& s) { EXPECT_EQ(s.player(P1).xp, 0); });
    playThroughEngine(card, &me, std::nullopt, &them);
    EXPECT_GE(them.asked, 1);
    EXPECT_EQ(state.player(P1).xp, 0) << "paid once";
    EXPECT_TRUE(o(mine).location.has_value()) << "\"you don't kill a unit this way\"";
    EXPECT_TRUE(o(card).location.has_value());
    EXPECT_FALSE(o(theirs).location.has_value());
}

// ─── 07 item 7.6: Stalking Wolf's additional cost ───────────────────────────

TEST_F(C7Test, StalkingWolf_NeedsABirdCatDogOrPoroAndKillsTheChosenOne) {
    Card* wolf = card_registry.get(kStalkingWolf);
    ASSERT_NE(wolf, nullptr);
    addUnit(P1, kInvalidId, 2);
    EXPECT_FALSE(wolf->canBePlayed(state, P1)) << "nothing to kill: the cost can't be paid";
    auto cat = tagged(P1, {"Cat"});
    auto poro = tagged(P1, {"Poro"});
    EXPECT_TRUE(wolf->canBePlayed(state, P1));

    auto card = addToHand(P1, kStalkingWolf);
    EnginePathAgent agent(EnginePathAgent::choosing(poro));
    // The kill is a cost: done before the opponent can respond.
    SnoopAgent them([&](const GameState& s) {
        EXPECT_FALSE(s.getObject(poro).location.has_value());
    });
    playThroughEngine(card, &agent, std::nullopt, &them);
    EXPECT_FALSE(o(poro).location.has_value());
    EXPECT_TRUE(o(cat).location.has_value());
    EXPECT_TRUE(o(card).location.has_value());
}

// ─── batch-07: Ashe, Focused ────────────────────────────────────────────────

TEST_F(C7Test, AsheFocused_TheBanishedCardReturnsWhenTheyHold) {
    auto kept = addToHand(P2, kLoyalPoro);
    auto taken = addToHand(P2, kCarrionDredger);
    auto ashe = addToHand(P1, kAsheFocused);
    EnginePathAgent agent(EnginePathAgent::choosing(taken));
    playThroughEngine(ashe, &agent);
    ASSERT_TRUE(agent.wasOffered(kept));
    ASSERT_TRUE(banished(P2, taken));
    EXPECT_FALSE(inHand(P2, taken));
    EXPECT_TRUE(inHand(P2, kept));

    // A conquer is not a hold.
    addUnit(P2, kInvalidId, 2, 0);
    withEngine([&](GameEngine& e) {
        e.testHook_scoreConquer(P2, 0);
        e.testHook_runChain();
    });
    EXPECT_TRUE(banished(P2, taken));

    // "(even if I'm no longer on the board)"
    resolveThroughEngine([&](EffectExecutor& ex, GameState&) { ex.killObject(ashe); });
    state.battlefields[1].controller = P2;
    withEngine([&](GameEngine& e) {
        e.testHook_scoreHold(P2, 1);
        e.testHook_runChain();
    }, nullptr, nullptr, P2);
    EXPECT_TRUE(inHand(P2, taken)) << "when they hold, it returns to their hand";
    EXPECT_FALSE(banished(P2, taken));
    EXPECT_EQ(o(taken).zone, ZoneType::Hand);
}

// ─── batch-07 S5: Atakhan's defender chooses ────────────────────────────────

TEST_F(C7Test, Atakhan_TheDefenderChoosesWhichOfTheirUnitsHereDies) {
    auto atakhan = addUnit(P1, kAtakhan, 0, 0);
    auto weak = addUnit(P2, kInvalidId, 1, 0);
    auto strong = addUnit(P2, kInvalidId, 6, 0);
    auto elsewhere = addUnit(P2, kInvalidId, 1, 1);
    EnginePathAgent defender(EnginePathAgent::choosing(strong));
    fireTriggerThroughEngine(atakhan, TriggerType::WhenIAttack, nullptr, kInvalidId, &defender);
    ASSERT_EQ(defender.prompts.size(), 1u) << "the defender is asked";
    EXPECT_TRUE(defender.wasOffered(weak));
    EXPECT_FALSE(defender.wasOffered(elsewhere)) << "\"here\"";
    EXPECT_FALSE(o(strong).location.has_value());
    EXPECT_TRUE(o(weak).location.has_value());
}

// ─── batch-07: LeBlanc, Fragmented ──────────────────────────────────────────

TEST_F(C7Test, LeBlancFragmented_DrawsTwoOnlyInHerControllersBeginningPhase) {
    for (int i = 0; i < 6; ++i) addToDeck(P1, kLoyalPoro);
    // Her controller's Scoring step is part of their Beginning Phase.
    auto mine = addUnit(P1, kLeBlancFragmented);
    state.turn.phase = TurnPhase::ScoringStep;
    state.turn.turn_player = P1;
    resolveThroughEngine([&](EffectExecutor& ex, GameState&) { ex.killObject(mine); });
    EXPECT_EQ(handSize(P1), 2);

    // The opponent's Beginning Phase is not "your Beginning Phase".
    auto other = addUnit(P1, kLeBlancFragmented);
    state.turn.phase = TurnPhase::BeginningStep;
    state.turn.turn_player = P2;
    resolveThroughEngine([&](EffectExecutor& ex, GameState&) { ex.killObject(other); },
                         nullptr, nullptr, P2);
    EXPECT_EQ(handSize(P1), 3);
}

// ─── 07 item 7.1: Vi, Peacekeeper chooses her own target ────────────────────

TEST_F(C7Test, ViPeacekeeper_StunsTheEnemyUnitHereItsControllerChooses) {
    auto vi = addUnit(P1, kViPeacekeeper, 0, 0);
    auto first = addUnit(P2, kInvalidId, 3, 0);
    auto chosen = addUnit(P2, kInvalidId, 3, 0);
    auto elsewhere = addUnit(P2, kInvalidId, 3, 1);
    auto hidden = shrouded(P2, 3, 0);
    EnginePathAgent agent(EnginePathAgent::choosing(chosen));
    fireTriggerThroughEngine(vi, TriggerType::WhenIAttack, &agent);
    ASSERT_EQ(agent.prompts.size(), 1u) << "the controller is asked which unit";
    EXPECT_TRUE(agent.wasOffered(first));
    EXPECT_FALSE(agent.wasOffered(elsewhere)) << "\"an enemy unit here\"";
    EXPECT_FALSE(agent.wasOffered(hidden)) << "can't be chosen by enemy abilities";
    EXPECT_TRUE(o(chosen).is_stunned);
    EXPECT_FALSE(o(first).is_stunned);
}

// ─── batch-07 S5: Daisy! ────────────────────────────────────────────────────

TEST_F(C7Test, Daisy_NeedsAllFourTagsAndStunsTheChosenEnemyUnitHere) {
    auto daisy = addUnit(P1, kDaisy, 0, 0);
    auto first = addUnit(P2, kInvalidId, 3, 0);
    auto chosen = addUnit(P2, kInvalidId, 3, 0);
    tagged(P1, {"Bird"});
    tagged(P1, {"Cat"});
    tagged(P1, {"Dog"});
    EnginePathAgent quiet;
    fireTriggerThroughEngine(daisy, TriggerType::WhenIAttack, &quiet);
    EXPECT_TRUE(quiet.prompts.empty()) << "three tags: the ability does not trigger";
    EXPECT_FALSE(o(first).is_stunned);

    tagged(P1, {"Poro"});
    EnginePathAgent agent(EnginePathAgent::choosing(chosen));
    fireTriggerThroughEngine(daisy, TriggerType::WhenIAttack, &agent);
    ASSERT_EQ(agent.prompts.size(), 1u);
    EXPECT_TRUE(o(chosen).is_stunned);
    EXPECT_FALSE(o(first).is_stunned);
}

// ─── batch-10 / 07 items 7.7, 7.10: Rift Herald ─────────────────────────────

TEST_F(C7Test, RiftHerald_MayDrawTheUnitItsControllerChoosesFromTheTopThree) {
    auto filler_a = addToDeck(P1, kIncinerate);
    auto filler_b = addToDeck(P1, kIncinerate);
    auto wanted = addToDeck(P1, kLoyalPoro);
    auto spell = addToDeck(P1, kIncinerate);
    auto top_unit = addToDeck(P1, kCarrionDredger);   // top of the deck
    auto herald = addUnit(P1, kRiftHerald, 0, 0);

    int shown_to_all = 0;
    auto conn = events.on_card_revealed.connect([&](const CardRevealedEvent& e) {
        if (e.revealed_to_all) ++shown_to_all;
    });
    EnginePathAgent agent(EnginePathAgent::choosing(wanted));   // yes, then that unit
    fireTriggerThroughEngine(herald, TriggerType::WhenIMoveToFB, &agent);
    conn.disconnect();

    ASSERT_EQ(agent.prompts.size(), 2u) << "the may, then which unit";
    EXPECT_TRUE(agent.wasOffered(top_unit));
    EXPECT_FALSE(agent.wasOffered(spell)) << "\"a unit\"";
    EXPECT_TRUE(inHand(P1, wanted));
    EXPECT_EQ(shown_to_all, 1) << "only the unit drawn is revealed to the opponent";
    EXPECT_EQ(state.player(P1).draws_this_turn, 1) << "it is drawn";
    // "Recycle the rest": the other two are under the rest of the deck.
    const auto& deck = state.player(P1).main_deck;
    ASSERT_EQ(deck.size(), 4u);
    EXPECT_TRUE(deck.back() == filler_a || deck.back() == filler_b);
    EXPECT_TRUE(deck[0] == spell || deck[0] == top_unit);
    EXPECT_TRUE(deck[1] == spell || deck[1] == top_unit);
}

TEST_F(C7Test, RiftHerald_DecliningRecyclesAllThree) {
    auto filler = addToDeck(P1, kIncinerate);
    addToDeck(P1, kLoyalPoro);
    addToDeck(P1, kIncinerate);
    addToDeck(P1, kCarrionDredger);
    auto herald = addUnit(P1, kRiftHerald, 0, 0);
    EnginePathAgent decline;   // "No" is the first option
    fireTriggerThroughEngine(herald, TriggerType::WhenIMoveToFB, &decline);
    ASSERT_EQ(decline.prompts.size(), 1u) << "\"You may\"";
    EXPECT_EQ(handSize(P1), 0);
    ASSERT_EQ(deckSize(P1), 4);
    EXPECT_EQ(state.player(P1).main_deck.back(), filler);
}

TEST_F(C7Test, RiftHerald_DeathknellPlaysAHandUnitThatStillPaysItsPower) {
    auto free_unit = addToHand(P1, kLoyalPoro);        // [3], no Power
    auto power_unit = addToHand(P1, kViPeacekeeper);   // [5][Y]
    auto herald = addUnit(P1, kRiftHerald);
    EnginePathAgent agent;
    resolveThroughEngine([&](EffectExecutor& ex, GameState&) { ex.killObject(herald); }, &agent);
    EXPECT_FALSE(agent.wasOffered(power_unit)) << "its Power cost can't be paid";
    EXPECT_TRUE(o(free_unit).isAtBase()) << "played to base, ignoring its Energy cost";
    EXPECT_FALSE(inHand(P1, free_unit));
    EXPECT_TRUE(inHand(P1, power_unit));

    // With the Power available the unit can be chosen, and the Power is spent.
    auto second = addUnit(P1, kRiftHerald);
    state.player(P1).rune_pool.universal_power = 1;
    resolveThroughEngine([&](EffectExecutor& ex, GameState&) { ex.killObject(second); });
    EXPECT_TRUE(o(power_unit).isAtBase());
    EXPECT_FALSE(inHand(P1, power_unit));
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 0) << "\"You must still pay its Power cost\"";
}

// ─── 03 item 3.18: Plundering Poro ──────────────────────────────────────────

TEST_F(C7Test, PlunderingPoro_TheGoldEntersExhausted) {
    auto poro = addUnit(P1, kPlunderingPoro, 0, 0);
    fireTriggerThroughEngine(poro, TriggerType::WhenIConquer);
    auto gold = tokensNamed(P1, "Gold");
    ASSERT_EQ(gold.size(), 1u);
    EXPECT_TRUE(o(gold[0]).is_exhausted);
}

// ─── 03 item 3.13: "the first time ... each turn" without a flag ────────────

TEST_F(C7Test, BladeTwirler_OnlyTheFirstMoveOfTheTurnBurns) {
    auto twirler = addUnit(P1, kBladeTwirler);
    for (int i = 0; i < 3; ++i) addToDeck(P2, kIncinerate);
    EnginePathAgent agent;   // mode 0: the opponent burns
    resolveThroughEngine([&](EffectExecutor& ex, GameState&) { ex.moveToBattlefield(twirler, 0); },
                         &agent);
    EXPECT_EQ(agent.prompts.size(), 1u) << "choose a player";
    EXPECT_EQ(trashSize(P2), 1);
    resolveThroughEngine([&](EffectExecutor& ex, GameState&) { ex.moveToBattlefield(twirler, 1); });
    EXPECT_EQ(trashSize(P2), 1) << "the second move of the turn does not trigger";
}

TEST_F(C7Test, JayceBrilliantInventor_OnlyTheFirstNonTokenGearOfTheTurn) {
    auto jayce = addUnit(P1, kJayceBrilliantInventor);
    auto tired = addUnit(P1, kInvalidId, 2);
    o(tired).is_exhausted = true;
    (void)jayce;

    auto first = addToHand(P1, kSealOfStrength);
    EnginePathAgent agent(EnginePathAgent::choosing(tired));
    playThroughEngine(first, &agent);
    EXPECT_FALSE(o(tired).is_exhausted) << "first non-token gear: may ready something";

    o(tired).is_exhausted = true;
    auto second = addToHand(P1, kSealOfStrength);
    EnginePathAgent again(EnginePathAgent::choosing(tired));
    playThroughEngine(second, &again);
    EXPECT_TRUE(again.prompts.empty()) << "the second gear this turn does not trigger";
    EXPECT_TRUE(o(tired).is_exhausted);
}

// ─── 07 item 7.5: costs are paid as the ability goes on the chain ───────────

TEST_F(C7Test, BaccaiReaper_PaysFuryByRecyclingAnExhaustedRuneUpFront) {
    auto reaper = addUnit(P1, kBaccaiReaper, 0, 0);
    auto rune = addRune(P1, Domain::Fury, /*exhausted=*/true);
    EnginePathAgent agent(EnginePathAgent::script({1}));
    SnoopAgent them([&](const GameState& s) {
        EXPECT_FALSE(s.getObject(rune).location.has_value()) << "[R] paid before any response";
    });
    fireTriggerThroughEngine(reaper, TriggerType::WhenIAttack, &agent, kInvalidId, &them);
    ASSERT_EQ(agent.prompts.size(), 1u) << "offered with only an exhausted Fury rune (CR 164.2)";
    EXPECT_GE(them.asked, 1);
    EXPECT_FALSE(o(rune).location.has_value());
    EXPECT_TRUE(o(reaper).hasKeyword(Keyword::Assault));
}

TEST_F(C7Test, BaccaiReaper_WithNoFuryToPayIsNotOffered) {
    auto reaper = addUnit(P1, kBaccaiReaper, 0, 0);
    addRune(P1, Domain::Calm);
    EnginePathAgent agent(EnginePathAgent::script({1}));
    fireTriggerThroughEngine(reaper, TriggerType::WhenIAttack, &agent);
    EXPECT_TRUE(agent.prompts.empty());
    EXPECT_FALSE(o(reaper).hasKeyword(Keyword::Assault));
}

TEST_F(C7Test, ShadowOrderDisciple_BurnsUpFront) {
    auto disciple = addUnit(P1, kShadowOrderDisciple, 0, 0);
    addToDeck(P1, kIncinerate);
    addToDeck(P1, kIncinerate);
    EnginePathAgent agent(EnginePathAgent::script({1}));
    SnoopAgent them([&](const GameState& s) {
        EXPECT_EQ(s.player(P1).trash.size(), 1u) << "[Burn 1] paid before any response";
        EXPECT_EQ(s.getObject(disciple).temp_might_bonus, 0) << "the effect has not happened yet";
    });
    fireTriggerThroughEngine(disciple, TriggerType::WhenIMove, &agent, kInvalidId, &them);
    EXPECT_GE(them.asked, 1);
    EXPECT_EQ(trashSize(P1), 1) << "burned once";
    EXPECT_EQ(o(disciple).temp_might_bonus, 1);
}

TEST_F(C7Test, GustMonk_BanishesTheTrashCardUpFrontThroughTheExecutor) {
    auto monk = addUnit(P1, kGustMonk);
    o(monk).card_counters["__gust_monk_paid"] = 1;
    auto ally = addUnit(P1, kInvalidId, 2, 0);
    auto corpse = inTrashOf(P1, kLoyalPoro);
    EnginePathAgent agent(EnginePathAgent::choosing(ally));
    SnoopAgent them([&](const GameState& s) {
        EXPECT_EQ(s.getObject(corpse).zone, ZoneType::Banishment) << "banished before any response";
    });
    fireTriggerThroughEngine(monk, TriggerType::WhenYouPlayMe, &agent, kInvalidId, &them);
    EXPECT_GE(them.asked, 1);
    EXPECT_TRUE(banished(P1, corpse));
    EXPECT_FALSE(inTrash(P1, corpse));
    EXPECT_EQ(state.player(P1).cards_banished_this_turn, 1) << "counted as a banish";
    EXPECT_TRUE(o(ally).hasKeyword(Keyword::Assault));
    EXPECT_FALSE(o(monk).hasKeyword(Keyword::Assault));
}

TEST_F(C7Test, Profiteer_DisempowersUpFrontAndEmpowersTheChosenThing) {
    auto profiteer = addUnit(P1, kProfiteer);
    auto empowered = addUnit(P1, kInvalidId, 2);
    o(empowered).empowered = 1;
    auto target = addUnit(P1, kInvalidId, 2, 0);
    EnginePathAgent agent(EnginePathAgent::choosing(target));   // yes, then the recipient
    SnoopAgent them([&](const GameState& s) {
        EXPECT_FALSE(s.getObject(empowered).isEmpowered()) << "disempowered before any response";
        EXPECT_FALSE(s.getObject(target).isEmpowered());
    });
    fireTriggerThroughEngine(profiteer, TriggerType::WhenYouPlayMe, &agent, kInvalidId, &them);
    EXPECT_GE(them.asked, 1);
    EXPECT_FALSE(o(empowered).isEmpowered());
    EXPECT_TRUE(o(target).isEmpowered());
}

TEST_F(C7Test, RavenbloomPrefect_IsBanishedAsTheCost) {
    auto prefect = addUnit(P2, kRavenbloomPrefect, 0, 0);
    auto gear = addToHand(P1, kSealOfStrength);
    EnginePathAgent them(EnginePathAgent::script({1}));
    playThroughEngine(gear, nullptr, std::nullopt, &them);
    ASSERT_EQ(them.prompts.size(), 1u);
    EXPECT_TRUE(banished(P2, prefect));
    EXPECT_FALSE(o(prefect).location.has_value());
    EXPECT_TRUE(banished(P1, gear));
    EXPECT_FALSE(o(gear).location.has_value());
}

// ─── 08 item 8.13 / CR 355.2.a: effect-played units ─────────────────────────

TEST_F(C7Test, Kharox_PlaysTheUnitWhereItsNewControllerChooses) {
    auto kharox = addUnit(P1, kKharox, 0, 0);
    state.battlefields[0].controller = P1;
    for (int i = 0; i < 3; ++i) addToDeck(P2, kIncinerate);
    auto corpse = inTrashOf(P2, kLoyalPoro);
    // yes; the unit; battlefield #0
    EnginePathAgent agent(EnginePathAgent::script({1, static_cast<int>(corpse), 1}));
    fireTriggerThroughEngine(kharox, TriggerType::WhenIBecomeEmpowered, &agent);
    EXPECT_EQ(trashSize(P2), 3) << "burned 3 exactly once; the unit left the trash";
    EXPECT_FALSE(inTrash(P2, corpse));
    EXPECT_EQ(o(corpse).controller, P1);
    EXPECT_EQ(o(corpse).battlefieldId(), std::optional<BattlefieldId>{0});
}

TEST_F(C7Test, TailCloakedMatriarch_PlaysTheUnitToBaseAndOutOfTheTrash) {
    auto matriarch = addUnit(P1, kTailCloakedMatriarch, 0, 0);
    state.battlefields[0].controller = P1;
    auto corpse = inTrashOf(P1, kLoyalPoro);
    EnginePathAgent agent(EnginePathAgent::script({1}));
    fireTriggerThroughEngine(matriarch, TriggerType::WhenIBecomeEmpowered, &agent);
    EXPECT_TRUE(o(corpse).isAtBase()) << "\"Play it to your base\"";
    EXPECT_FALSE(inTrash(P1, corpse));
}

TEST_F(C7Test, ZedAndIllaoi_TheTokenGoesWhereItsControllerChooses) {
    state.battlefields[0].controller = P1;
    auto zed = addUnit(P1, kZedFromTheShadows);
    o(zed).card_counters["zed_discard_paid"] = 1;
    EnginePathAgent to_bf(EnginePathAgent::script({1}));    // battlefield #0
    fireTriggerThroughEngine(zed, TriggerType::WhenYouPlayMe, &to_bf);
    auto clones = tokensNamed(P1, "Shadow Clone");
    ASSERT_EQ(clones.size(), 1u);
    EXPECT_EQ(o(clones[0]).battlefieldId(), std::optional<BattlefieldId>{0});
    EXPECT_EQ(o(clones[0]).card_def_id, kShadowClone);

    auto illaoi = addUnit(P1, kIllaoiProphet);
    EnginePathAgent to_bf2(EnginePathAgent::script({1}));
    fireTriggerThroughEngine(illaoi, TriggerType::WhenYouPlayMe, &to_bf2);
    auto tentacles = tokensNamed(P1, "Tentacle");
    ASSERT_EQ(tentacles.size(), 1u);
    EXPECT_EQ(o(tentacles[0]).battlefieldId(), std::optional<BattlefieldId>{0});
}

// ─── batch-07: missing effects ──────────────────────────────────────────────

TEST_F(C7Test, Spiderling_PlusOneForEachOtherSpiderlingHere) {
    auto a = addUnit(P1, kSpiderling, 0, 0);
    auto b = addUnit(P1, kSpiderling, 0, 0);
    auto c = addUnit(P1, kSpiderling, 0, 0);
    auto alone = addUnit(P1, kSpiderling, 0, 1);
    auto enemy = addUnit(P2, kSpiderling);
    cleanup();
    EXPECT_EQ(o(a).current_might, 3);
    EXPECT_EQ(o(b).current_might, 3);
    EXPECT_EQ(o(c).current_might, 3);
    EXPECT_EQ(o(alone).current_might, 1) << "\"here\"";
    EXPECT_EQ(o(enemy).current_might, 1) << "\"you control\"";
}

TEST_F(C7Test, RenektonBrute_BecomesEmpoweredWhenHisAbilityTakesHimToTen) {
    auto renekton = addUnit(P1, kRenektonBrute, 9, 0);
    state.player(P1).rune_pool.energy = 1;
    activate(renekton);
    EXPECT_EQ(o(renekton).current_might, 10);
    // "When my Might becomes 10 or more" is a triggered ability of its own
    // (WhenMyMightReachesThreshold), queued when the state settles.
    settle();
    EXPECT_TRUE(o(renekton).isEmpowered()) << "\"When my Might becomes 10 or more, empower me\"";
    cleanup();
    EXPECT_TRUE(o(renekton).hasKeyword(Keyword::Ganking)) << "[Empowered][>] [Deflect] and [Ganking]";

    auto small = addUnit(P1, kRenektonBrute, 4, 0);
    state.player(P1).rune_pool.energy = 1;
    activate(small);
    settle();
    EXPECT_FALSE(o(small).isEmpowered());
}

// "When my Might becomes 10 or more": whatever raised it (card text; the
// engine fires WhenMyMightReachesThreshold on the edge).
TEST_F(C7Test, RenektonBrute_IsEmpoweredWhateverTakesHimToTen) {
    auto buffed = addUnit(P1, kRenektonBrute, 9, 0);
    settle();
    EXPECT_FALSE(o(buffed).isEmpowered());
    resolveThroughEngine([&](EffectExecutor& ex, GameState&) { ex.buffUnit(buffed); });
    settle();
    EXPECT_TRUE(o(buffed).isEmpowered()) << "the buff took him to 10";

    auto low = addUnit(P1, kRenektonBrute, 4, 0);
    resolveThroughEngine([&](EffectExecutor& ex, GameState&) { ex.buffUnit(low); });
    settle();
    EXPECT_FALSE(o(low).isEmpowered());
}

// ─── batch-07: Astral Heron ─────────────────────────────────────────────────

TEST_F(C7Test, AstralHeron_DiscountsOnceForTheFirstCardOfTheTurn) {
    auto heron = addUnit(P1, kAstralHeron, 0, 0);
    auto discounts = [&]() {
        int n = 0;
        for (const auto& m : state.player(P1).cost_modifiers)
            if (m.source == heron) ++n;
        return n;
    };
    playThroughEngine(addToHand(P1, kLoyalPoro));
    EXPECT_EQ(discounts(), 1) << "first card: the next card costs [2][A][A] less";
    playThroughEngine(addToHand(P1, kLoyalPoro));
    EXPECT_EQ(discounts(), 0) << "used by the second card, and not given again";
    playThroughEngine(addToHand(P1, kLoyalPoro));
    EXPECT_EQ(discounts(), 0);
}

// "Your first card each turn" is the card played first (its play ordinal,
// stamped by TriggerManager::onCardPlayed), whatever order the plays are
// announced in: a spell is announced when it resolves (CR 420.4.a), after the
// Reaction played in response to it.
TEST_F(C7Test, AstralHeron_AFirstSpellAnsweredByYourOwnReactionStillCounts) {
    auto heron = addUnit(P1, kAstralHeron, 0, 0);
    auto first = playedAs(1), reaction = playedAs(2), third = playedAs(3);
    state.player(P1).cards_played_this_turn = 2;
    fireTriggerThroughEngine(heron, TriggerType::WhenYouPlayACard, nullptr, reaction);
    EXPECT_TRUE(state.player(P1).cost_modifiers.empty()) << "the Reaction is the second card";
    fireTriggerThroughEngine(heron, TriggerType::WhenYouPlayACard, nullptr, first);
    EXPECT_EQ(state.player(P1).cost_modifiers.size(), 1u) << "the first card, now resolved";
    state.player(P1).cards_played_this_turn = 3;
    fireTriggerThroughEngine(heron, TriggerType::WhenYouPlayACard, nullptr, third);
    EXPECT_EQ(state.player(P1).cost_modifiers.size(), 1u);
}

// 9.D1: the response to the first card is countered, so it is never announced
// as played — the first card is still the first card.
TEST_F(C7Test, AstralHeron_AFirstSpellWhoseResponseWasCounteredStillCounts) {
    auto heron = addUnit(P1, kAstralHeron, 0, 0);
    auto first = playedAs(1);
    state.player(P1).cards_played_this_turn = 2;       // the countered Reaction was the second
    fireTriggerThroughEngine(heron, TriggerType::WhenYouPlayACard, nullptr, first);
    EXPECT_EQ(state.player(P1).cost_modifiers.size(), 1u);
}

// A card stamped on an earlier turn is not this turn's first card.
TEST_F(C7Test, AstralHeron_APlayOrdinalFromAnEarlierTurnDoesNotCount) {
    auto heron = addUnit(P1, kAstralHeron, 0, 0);
    auto old_card = playedAs(1);
    o(old_card).card_counters["__play_ordinal_turn"] = state.turn.turn_number;   // last turn
    fireTriggerThroughEngine(heron, TriggerType::WhenYouPlayACard, nullptr, old_card);
    EXPECT_TRUE(state.player(P1).cost_modifiers.empty());
}

TEST_F(C7Test, AstralHeron_InBaseForTheFirstCardGivesNoDiscountLater) {
    auto heron = addUnit(P1, kAstralHeron);
    auto first = playedAs(1), second = playedAs(2);
    state.player(P1).cards_played_this_turn = 1;
    fireTriggerThroughEngine(heron, TriggerType::WhenYouPlayACard, nullptr, first);
    EXPECT_TRUE(state.player(P1).cost_modifiers.empty()) << "\"if I'm at a battlefield\"";
    o(heron).location = BattlefieldLocation{0};
    o(heron).zone = ZoneType::BattlefieldZone;
    state.player(P1).cards_played_this_turn = 2;
    fireTriggerThroughEngine(heron, TriggerType::WhenYouPlayACard, nullptr, second);
    EXPECT_TRUE(state.player(P1).cost_modifiers.empty()) << "the second card is not the first";
}

// ─── batch-10: Corrupted Dragon ─────────────────────────────────────────────

TEST_F(C7Test, CorruptedDragon_ReadsTheVictoryScoreInEffect) {
    Card* dragon = card_registry.get(kCorruptedDragon);
    ASSERT_NE(dragon, nullptr);
    setScore(P1, 5);
    ASSERT_EQ(state.mode.victory_score, 8);
    EXPECT_FALSE(dragon->entersReadyOnPlay(state, P1)) << "3 away: within 3 points";
    state.turn.victory_score_bonus = 1;   // "+1 points needed to win" battlefield
    EXPECT_TRUE(dragon->entersReadyOnPlay(state, P1)) << "4 away of 9";
}

TEST_F(C7Test, CorruptedDragon_MovesOnlyUnitsItCanChooseAndPaysDeflect) {
    auto dragon = addUnit(P1, kCorruptedDragon, 0, 0);
    auto small = addUnit(P2, kInvalidId, 3, 0);
    auto big = addUnit(P2, kInvalidId, 6, 0);
    auto hidden = shrouded(P2, 2, 0);
    auto deflector = addUnit(P2, kPoutyPoro, 0, 0);
    ASSERT_TRUE(o(deflector).keywords.has(Keyword::Deflect));

    // No Power: the [Deflect] unit can't be chosen.
    EnginePathAgent agent(EnginePathAgent::choosing(small));
    fireTriggerThroughEngine(dragon, TriggerType::WhenIAttack, &agent);
    EXPECT_FALSE(agent.wasOffered(big)) << "5 [M] or less";
    EXPECT_FALSE(agent.wasOffered(hidden)) << "can't be chosen by enemy abilities";
    EXPECT_FALSE(agent.wasOffered(deflector)) << "its Deflect can't be paid";
    EXPECT_TRUE(o(small).isAtBase());
    EXPECT_TRUE(o(deflector).isAtBattlefield());

    // With [A] to pay, it can — and the Power is spent.
    state.player(P1).rune_pool.universal_power = 1;
    EnginePathAgent pays(EnginePathAgent::choosing(deflector));
    fireTriggerThroughEngine(dragon, TriggerType::WhenIAttack, &pays);
    EXPECT_TRUE(o(deflector).isAtBase());
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 0) << "Deflect paid (CR 809)";
    EXPECT_TRUE(state.player(P1).chose_enemy_unit_this_turn);
}

}  // namespace
}  // namespace test_review_fix_c7
}  // namespace riftbound::test
