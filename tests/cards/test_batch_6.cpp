/// Testing phase, batch 6 (plans/remaining/10-testing-phase.md items M5 and
/// R1-R15; report plans/reports/batch-6.md).
///
/// M5: where an effect plays a card or a token is a choice of that play (CR
/// 355.2), made as the played object is finalized, which is while the spell or
/// ability that plays it resolves (CR 420.3.b.1-420.3.b.2, Rek'Sai's example).
/// It is asked after the opponent has had priority, so a battlefield its
/// controller gains in response is offered. The unit's own play permissions
/// and its player's grants apply (CR 355.2.b, 822.1.c: "Treat all steps of
/// Play as normal", 420.3.b).

#include <gtest/gtest.h>

#include "batch_m1_agents.h"
#include "card_test_fixture.h"
#include "cards/card_helpers.h"

#include <algorithm>
#include <optional>
#include <regex>
#include <string>
#include <vector>

namespace riftbound::test::batch_6 {

using batch_m1::Me;
using batch_m1::Them;
using batch_m1::raw;

constexpr CardDefId kFlameChompers = 6;
constexpr CardDefId kImmortalPhoenix = 37;
constexpr CardDefId kReinforce = 62;
constexpr CardDefId kPromisingFuture = 115;
constexpr CardDefId kDazzlingAurora = 160;
constexpr CardDefId kShipyardSkulker = 175;   // unit, [3], 3 [M], no text
constexpr CardDefId kAcceptableLosses = 179;
constexpr CardDefId kWhirlwind = 187;
constexpr CardDefId kSoulgorger = 196;
constexpr CardDefId kSiphonPower = 261;
constexpr CardDefId kHeraldOfTheArcane = 298;
constexpr CardDefId kFirestorm = 300;
constexpr CardDefId kIncinerate = 301;        // a spell
constexpr CardDefId kRecruitTheVanguard = 313;
constexpr CardDefId kDauntlessVanguard = 415;  // "You may play me to an occupied enemy battlefield."
constexpr CardDefId kGuards = 475;
constexpr CardDefId kVanguardArmory = 489;
constexpr CardDefId kBackOff = 604;
constexpr CardDefId kZilean = 648;
constexpr CardDefId kLunarBoon = 687;
constexpr CardDefId kMaduli = 706;
constexpr CardDefId kCursedSarcophagus = 710;
constexpr CardDefId kLoyalPoro = 718;
constexpr CardDefId kSafetyInspector = 726;
constexpr CardDefId kUndyingLoyalty = 730;
constexpr CardDefId kZedFromTheShadows = 810;
constexpr CardDefId kIterativeDesign = 838;
constexpr CardDefId kNasusGuardian = 850;      // "when an enemy unit here dies, channel 1 rune exhausted"
constexpr CardDefId kWildClaw = 876;
constexpr CardDefId kKharox = 901;
constexpr CardDefId kDragonForm = 903;
constexpr CardDefId kLightningRush = 943;
constexpr CardDefId kPoutyPoro = 13;           // [Deflect]
constexpr CardDefId kArcaneShift = 515;
constexpr CardDefId kDoubleTrouble = 594;
constexpr CardDefId kHereToHelp = 433;

constexpr int kRest = EnginePathAgent::kRest;

class Batch6Test : public CardTestFixture {
protected:
    GameObject& o(GameObjectId id) { return state.getObject(id); }
    std::optional<BattlefieldId> bfOf(GameObjectId id) { return o(id).battlefieldId(); }
    static std::optional<BattlefieldId> bf(int i) { return BattlefieldId::fromRaw(i); }
    GameObjectId addToTrash(PlayerId owner, CardDefId def_id) {
        auto id = addToHand(owner, def_id);
        auto& h = state.player(owner).hand;
        h.erase(std::remove(h.begin(), h.end(), id), h.end());
        o(id).zone = ZoneType::Trash;
        state.player(owner).trash.push_back(id);
        return id;
    }
    GameObjectId addGear(PlayerId owner, CardDefId def_id) {
        auto id = state.createObject();
        auto& g = o(id);
        const auto& d = card_db.get(def_id);
        g.owner = g.controller = owner;
        g.card_type = CardType::Gear;
        g.card_def_id = def_id;
        g.name = d.name;
        g.tags = d.tags;
        g.keywords = d.keywords;
        g.zone = ZoneType::Base;
        g.location = BaseLocation{owner};
        return id;
    }
    GameObjectId legendOf(PlayerId p, CardDefId def) {
        auto id = state.createObject();
        auto& l = o(id);
        const auto& d = card_db.get(def);
        l.owner = l.controller = p;
        l.card_def_id = def;
        l.name = d.name;
        l.card_type = d.card_type;
        l.zone = ZoneType::LegendZone;
        state.player(p).legend_zone = id;
        return id;
    }
    void fund(PlayerId p, int energy, int power) {
        state.player(p).rune_pool.energy += energy;
        state.player(p).rune_pool.universal_power += power;
    }
    std::vector<GameObjectId> tokensNamed(PlayerId p, const std::string& name) {
        std::vector<GameObjectId> out;
        for (auto& [id, obj] : state.objects)
            if (obj.isToken() && obj.controller == p && obj.name == name &&
                obj.location.has_value())
                out.push_back(id);
        std::sort(out.begin(), out.end());
        return out;
    }
    /// The opponent, at its first priority with something on the chain,
    /// hands battlefield 0 to P1 (a battlefield gained in response).
    static void givesBattlefieldZero(Them& them) {
        them.respond = [](GameEngine& e) {
            e.mutableState().battlefields[0].controller = PlayerId::Player1;
        };
    }
    void fire(GameObjectId source, TriggerType trigger, Me& me, Them& them,
              GameObjectId subject = kInvalidId) {
        me.other = &them;
        withEngineDefaultTurn([&](GameEngine& e) {
            them.engine = &e;
            e.testHook_executor().queueTriggeredAbility(source, P1, trigger, subject);
            settleLikeTheEngine(e);
            them.engine = nullptr;
        }, &me, &them, P1);
    }
    void activate(GameObjectId source, Me& me, Them& them, int index = 0) {
        me.other = &them;
        Intent i;
        i.type = IntentType::ActivateAbility;
        i.player = P1;
        i.ability_source = source;
        i.ability_index = index;
        withEngine([&](GameEngine& e) {
            them.engine = &e;
            e.testHook_executeIntent(i);
            settleLikeTheEngine(e);
            them.engine = nullptr;
        }, &me, &them, P1);
    }
    void play(GameObjectId card, Me& me, Them& them) {
        me.other = &them;
        withEngineDefaultTurn([&](GameEngine& e) {
            them.engine = &e;
            Intent i;
            i.type = IntentType::PlayCard;
            i.player = P1;
            i.card = card;
            const auto& def = card_db.get(o(card).card_def_id);
            e.mutableState().player(P1).rune_pool.energy += def.energy_cost;
            e.mutableState().player(P1).rune_pool.universal_power += def.power_cost;
            e.testHook_executeIntent(i);
            settleLikeTheEngine(e);
            them.engine = nullptr;
        }, &me, &them, P1);
    }
};

// ═══ M5: an effect play's location is chosen as it resolves ═════════════════

TEST_F(Batch6Test, M5_FlameChompers_WhereItIsPlayedIsAskedAsTheAbilityResolves) {
    auto chompers = addToTrash(P1, kFlameChompers);
    state.player(P1).rune_pool.power[static_cast<int>(Domain::Fury)] = 1;
    Them them;
    givesBattlefieldZero(them);
    Me me(EnginePathAgent::script({1, 1}));   // pay [R]: yes; battlefield #0
    fire(chompers, TriggerType::WhenIAmDiscarded, me, them);
    ASSERT_EQ(me.prompts.size(), 2u);
    EXPECT_EQ(me.other_priority_at[0], 0) << "the may and [R]: as it is finalized (CR 383.3)";
    EXPECT_GT(me.other_priority_at[1], 0) << "where: as it resolves (CR 420.3.b.2)";
    EXPECT_EQ(bfOf(chompers), bf(0)) << "a battlefield gained in response is offered";
}

TEST_F(Batch6Test, M5_ImmortalPhoenix_WhereItIsPlayedIsAskedAsTheAbilityResolves) {
    auto phoenix = addToTrash(P1, kImmortalPhoenix);
    state.player(P1).rune_pool.energy = 1;
    state.player(P1).rune_pool.power[static_cast<int>(Domain::Fury)] = 1;
    Them them;
    givesBattlefieldZero(them);
    Me me(EnginePathAgent::script({1, 1}));
    fire(phoenix, TriggerType::WhenYouKillAUnitWithASpell, me, them);
    ASSERT_EQ(me.prompts.size(), 2u);
    EXPECT_EQ(me.other_priority_at[0], 0);
    EXPECT_GT(me.other_priority_at[1], 0);
    EXPECT_EQ(bfOf(phoenix), bf(0));
}

TEST_F(Batch6Test, M5_Soulgorger_TheUnitAtFinalizationWhereItGoesAsItResolves) {
    auto soul = addUnit(P1, kSoulgorger, 0, -1);
    auto dead = addToTrash(P1, kShipyardSkulker);
    Them them;
    givesBattlefieldZero(them);
    Me me(EnginePathAgent::script({1, raw(dead), 1}));
    fire(soul, TriggerType::WhenYouPlayMe, me, them);
    ASSERT_EQ(me.prompts.size(), 3u);
    EXPECT_EQ(me.other_priority_at[0], 0) << "the may";
    EXPECT_EQ(me.other_priority_at[1], 0) << "the unit in the trash, a target (CR 355.10.a.1)";
    EXPECT_GT(me.other_priority_at[2], 0) << "where it is played";
    EXPECT_EQ(bfOf(dead), bf(0));
}

TEST_F(Batch6Test, M5_HeraldOfTheArcane_TheRecruitsPlaceIsAskedAsTheAbilityResolves) {
    auto herald = legendOf(P1, kHeraldOfTheArcane);
    fund(P1, 1, 0);
    Them them;
    givesBattlefieldZero(them);
    Me me(EnginePathAgent::script({1}));      // tokenPlayDests code: 1 = battlefield #0
    activate(herald, me, them);
    ASSERT_EQ(me.prompts.size(), 1u) << "base or the battlefield gained in response";
    EXPECT_GT(me.other_priority_at[0], 0);
    const auto recruits = tokensNamed(P1, "Recruit");
    ASSERT_EQ(recruits.size(), 1u);
    EXPECT_EQ(bfOf(recruits[0]), bf(0));
}

TEST_F(Batch6Test, M5_Zed_TheShadowClonesPlaceIsAskedAsTheAbilityResolves) {
    auto zed = addUnit(P1, kZedFromTheShadows, 0, -1);
    o(zed).card_counters["zed_discard_paid"] = 1;
    Them them;
    givesBattlefieldZero(them);
    Me me(EnginePathAgent::script({1}));
    fire(zed, TriggerType::WhenYouPlayMe, me, them);
    ASSERT_EQ(me.prompts.size(), 1u);
    EXPECT_GT(me.other_priority_at[0], 0);
    const auto clones = tokensNamed(P1, "Shadow Clone");
    ASSERT_EQ(clones.size(), 1u);
    EXPECT_EQ(bfOf(clones[0]), bf(0));
}

TEST_F(Batch6Test, M5_UndyingLoyalty_WhereTheUnitIsPlayedIsAskedAsTheSpellResolves) {
    auto spell = addToHand(P1, kUndyingLoyalty);
    auto poro = addToTrash(P1, kPoutyPoro);
    Them them;
    givesBattlefieldZero(them);
    Me me(EnginePathAgent::script({raw(poro), 1}));   // the unit (a cost step); where
    play(spell, me, them);
    ASSERT_EQ(me.prompts.size(), 2u);
    EXPECT_EQ(me.other_priority_at[0], 0) << "the unit, as the spell is played";
    EXPECT_GT(me.other_priority_at[1], 0) << "where, as it resolves";
    EXPECT_EQ(bfOf(poro), bf(0));
}

TEST_F(Batch6Test, M5_Kharox_TheUnitAtTheReflexivesFinalizationWhereItGoesAsItResolves) {
    auto kharox = addUnit(P1, kKharox, 0, -1);
    for (int i = 0; i < 3; ++i) addToDeck(P2, kIncinerate);
    auto corpse = addToTrash(P2, kLoyalPoro);
    Them them;
    givesBattlefieldZero(them);
    Me me(EnginePathAgent::script({1, raw(corpse), 1}));
    fire(kharox, TriggerType::WhenIBecomeEmpowered, me, them);
    ASSERT_EQ(me.prompts.size(), 3u);
    EXPECT_EQ(me.other_priority_at[2] > me.other_priority_at[1], true)
        << "the place is asked after the reflexive's own finalization (CR 388.4, 420.3.b.2)";
    EXPECT_EQ(o(corpse).controller, P1);
    EXPECT_EQ(bfOf(corpse), bf(0));
}

// "Treat all steps of Play as normal" (CR 420.3.b): the played card's own
// permission counts (CR 355.2.b). Dauntless Vanguard: "You may play me to an
// occupied enemy battlefield."
TEST_F(Batch6Test, M5_AnEffectPlayOffersWhereTheCardItselfMayBePlayed) {
    auto kharox = addUnit(P1, kKharox, 0, -1);
    for (int i = 0; i < 3; ++i) addToDeck(P2, kIncinerate);
    auto vanguard = addToTrash(P2, kDauntlessVanguard);
    addUnit(P2, kShipyardSkulker, 3, 1);
    state.battlefields[1].controller = P2;
    Them them;
    Me me(EnginePathAgent::script({1, raw(vanguard), 2}));   // battlefield #1
    fire(kharox, TriggerType::WhenIBecomeEmpowered, me, them);
    EXPECT_EQ(o(vanguard).controller, P1);
    EXPECT_EQ(bfOf(vanguard), bf(1)) << "the occupied enemy battlefield it may be played to";
}

TEST_F(Batch6Test, M5_ATokenHasNoPermissionOfItsOwn) {
    addUnit(P2, kShipyardSkulker, 3, 1);
    state.battlefields[1].controller = P2;
    auto herald = legendOf(P1, kHeraldOfTheArcane);
    fund(P1, 1, 0);
    Them them;
    Me me(EnginePathAgent::script({}));
    activate(herald, me, them);
    EXPECT_TRUE(me.prompts.empty()) << "only the base: nothing to choose";
}

// ═══ R1: Lunar Boon's draws are its controller's to read ════════════════════
// (the leak case is HiddenOrderLeak.LunarBoon_TheOpponentReadsNoCardItDrawsOrKeeps)

// ═══ R2: "Its base Might becomes 5 this turn" (CR 143.2.a, 479.2) ═══════════

TEST_F(Batch6Test, R2_DragonForm_TwoOnOneUnitLeaveItsBaseAtFive) {
    auto small = addUnit(P1, kInvalidId, 2, 0);
    auto big = addUnit(P1, kInvalidId, 7, 0);
    for (auto u : {small, small, big, big}) {
        auto df = addToHand(P1, kDragonForm);
        EnginePathAgent agent(EnginePathAgent::script({raw(u)}));
        playThroughEngine(df, &agent);
    }
    EXPECT_EQ(o(small).current_might, 5);
    EXPECT_EQ(o(big).current_might, 5);
}

TEST_F(Batch6Test, R2_DragonForm_OtherModifiersApplyOnTopOfTheNewBase) {
    auto u = addUnit(P1, kInvalidId, 2, 0);
    o(u).buff_count = 1;
    o(u).recomputeMight();
    auto df = addToHand(P1, kDragonForm);
    EnginePathAgent agent(EnginePathAgent::script({raw(u)}));
    playThroughEngine(df, &agent);
    EXPECT_EQ(o(u).current_might, 6) << "base 5, then the buff (arithmetic layer, CR 479.4)";
}

TEST_F(Batch6Test, R2_DragonForm_EndsWithTheTurnAndWhenTheUnitLeaves) {
    auto u = addUnit(P1, kInvalidId, 2, 0);
    auto df = addToHand(P1, kDragonForm);
    EnginePathAgent agent(EnginePathAgent::script({raw(u)}));
    playThroughEngine(df, &agent);
    ASSERT_EQ(o(u).current_might, 5);
    withEngine([&](GameEngine& e) { e.testHook_expirationStep(); e.testHook_cleanup(); });
    EXPECT_EQ(o(u).current_might, 2) << "\"this turn\" (CR 317)";
    EXPECT_EQ(o(u).baseMight(), 2);
}

// ═══ R3: Safety Inspector's kills are simultaneous (CR 370.1.a.2) ═══════════

// Nasus, Guardian of Knowledge: "when an enemy unit here dies". If it dies
// with that unit it does not see it die (CR 383.2.c.2).
TEST_F(Batch6Test, R3_SafetyInspector_TheTwoUnitsDieTogether) {
    auto inspector = addUnit(P1, kSafetyInspector, 0, -1);
    auto mine = addUnit(P1, kShipyardSkulker, 3, 0);
    auto nasus = addUnit(P2, kNasusGuardian, 0, 0);
    {   // a rune for its channel
        auto& rd = state.player(P2).rune_deck;
        auto r = addRune(P2, Domain::Order);
        o(r).location = std::nullopt;
        o(r).zone = ZoneType::RuneDeck;
        rd.push_back(r);
    }
    const int runes_before = static_cast<int>(state.player(P2).rune_deck.size());
    EnginePathAgent me(EnginePathAgent::script({raw(mine)}));
    EnginePathAgent them(EnginePathAgent::script({raw(nasus)}));
    fireTriggerThroughEngine(inspector, TriggerType::WhenYouPlayMe, &me, kInvalidId, &them);
    EXPECT_FALSE(o(mine).location.has_value());
    EXPECT_FALSE(o(nasus).location.has_value());
    EXPECT_EQ(static_cast<int>(state.player(P2).rune_deck.size()), runes_before)
        << "Nasus died with the enemy unit: its ability does not trigger";
}

// ═══ R4: Acceptable Losses: each player chooses, then both gear die ════════

namespace {
/// P2: answers its gear prompt with `pick`, noting whether `watched` (P1's
/// gear) is still on the board when it is asked; passes otherwise.
class ChoosesItsGear : public EnginePathAgent {
public:
    GameObjectId pick = kInvalidId, watched = kInvalidId;
    std::optional<bool> watched_there;
    Intent selectAction(const GameState& st, const std::vector<Intent>& legal) override {
        for (const auto& i : legal)
            if (i.type == IntentType::MakeChoice && !i.chosen_objects.empty() &&
                i.chosen_objects.front() == pick) {
                watched_there = st.getObject(watched).location.has_value();
                return i;
            }
        return EnginePathAgent::selectAction(st, legal);
    }
};
}  // namespace

TEST_F(Batch6Test, R4_AcceptableLosses_TheOpponentChoosesWithMyGearStillThere) {
    auto spell = addToHand(P1, kAcceptableLosses);
    auto my_gear = addGear(P1, kVanguardArmory);
    auto their_gear = addGear(P2, kVanguardArmory);
    auto their_other = addGear(P2, kVanguardArmory);   // so that they are asked
    ChoosesItsGear them;
    them.pick = their_gear;
    them.watched = my_gear;
    EnginePathAgent me(EnginePathAgent::script({raw(my_gear)}));
    playThroughEngine(spell, &me, std::nullopt, &them);
    ASSERT_TRUE(them.watched_there.has_value()) << "Player 2 was asked";
    EXPECT_TRUE(*them.watched_there) << "the kills come after both choices (CR 370.1.a.2)";
    EXPECT_FALSE(o(my_gear).location.has_value());
    EXPECT_FALSE(o(their_gear).location.has_value());
    EXPECT_TRUE(o(their_other).location.has_value());
}

// ═══ R5: token locations ════════════════════════════════════════════════════

namespace {
/// Zilean at battlefield 0 (P1 controls it): "you may play that token and an
/// additional copy of it" — the copy gets the same choice of place.
}  // namespace

TEST_F(Batch6Test, R5_IterativeDesign_AnAdditionalCopyGetsTheSameChoice) {
    addUnit(P1, kZilean, 5, 0);
    state.battlefields[0].controller = P1;
    auto spell = addToHand(P1, kIterativeDesign);
    // where: base; the copy: yes; its place: the battlefield (option 1).
    EnginePathAgent me(EnginePathAgent::script({0, 1, 1}));
    playThroughEngine(spell, &me);
    const auto mechs = tokensNamed(P1, "Mech");
    ASSERT_EQ(mechs.size(), 2u);
    EXPECT_TRUE(o(mechs[0]).isAtBase() != o(mechs[1]).isAtBase())
        << "one in the base, the copy where its controller chose";
}

TEST_F(Batch6Test, R5_IterativeDesign_ABattlefieldUnitsCantBePlayedToIsNotOffered) {
    addUnit(P1, kShipyardSkulker, 3, 0);
    state.battlefields[0].controller = P1;
    state.battlefields[0].blocks_unit_play = true;   // "Units can't be played here."
    auto spell = addToHand(P1, kIterativeDesign);
    EnginePathAgent me(EnginePathAgent::script({}));
    playThroughEngine(spell, &me);
    EXPECT_TRUE(me.prompts.empty()) << "the base is the only place";
    const auto mechs = tokensNamed(P1, "Mech");
    ASSERT_EQ(mechs.size(), 1u);
    EXPECT_TRUE(o(mechs[0]).isAtBase());
}

TEST_F(Batch6Test, R5_VanguardArmory_ABattlefieldUnitsCantBePlayedToIsNotOffered) {
    addUnit(P1, kShipyardSkulker, 3, 0);
    state.battlefields[0].controller = P1;
    state.battlefields[0].blocks_unit_play = true;
    auto armory = addGear(P1, kVanguardArmory);
    Them them;
    Me me(EnginePathAgent::script({}));
    activate(armory, me, them);
    EXPECT_TRUE(me.prompts.empty());
    EXPECT_EQ(tokensNamed(P1, "Recruit").size(), 3u);
}

TEST_F(Batch6Test, R5_VanguardArmory_AnAdditionalCopyGetsTheSameChoice) {
    addUnit(P1, kZilean, 5, 0);
    state.battlefields[0].controller = P1;
    auto armory = addGear(P1, kVanguardArmory);
    Them them;
    // three places (base), the copy: yes, its place: the battlefield.
    Me me(EnginePathAgent::script({0, 0, 0, 1, 1}));
    activate(armory, me, them);
    const auto recruits = tokensNamed(P1, "Recruit");
    ASSERT_EQ(recruits.size(), 4u);
    EXPECT_EQ(std::count_if(recruits.begin(), recruits.end(),
                            [&](GameObjectId r) { return !o(r).isAtBase(); }), 1);
}

TEST_F(Batch6Test, R5_RecruitTheVanguard_AnAdditionalCopyGetsTheSameChoice) {
    addUnit(P1, kZilean, 5, 0);
    state.battlefields[0].controller = P1;
    auto spell = addToHand(P1, kRecruitTheVanguard);
    EnginePathAgent me(EnginePathAgent::script({0, 0, 0, 0, 1, 1}));
    playThroughEngine(spell, &me);
    const auto recruits = tokensNamed(P1, "Recruit");
    ASSERT_EQ(recruits.size(), 5u);
    EXPECT_EQ(std::count_if(recruits.begin(), recruits.end(),
                            [&](GameObjectId r) { return !o(r).isAtBase(); }), 1);
}

TEST_F(Batch6Test, R5_Guards_AnAdditionalCopyGetsTheSameChoice) {
    addUnit(P1, kZilean, 5, 0);
    state.battlefields[0].controller = P1;
    auto spell = addToHand(P1, kGuards);
    EnginePathAgent me(EnginePathAgent::script({0, 1, 1, kRest}));
    playThroughEngine(spell, &me);
    const auto soldiers = tokensNamed(P1, "Sand Soldier");
    ASSERT_EQ(soldiers.size(), 2u);
    EXPECT_TRUE(o(soldiers[0]).isAtBase() != o(soldiers[1]).isAtBase());
}

// ═══ R6: banish / draw one, then the rest ═══════════════════════════════════

namespace {
/// The order of two events: the chosen card entering the board and the rest
/// going to the bottom of the deck ("recycled_main").
struct Order {
    std::vector<std::string> seen;
    boost::signals2::scoped_connection a, b, c;
    void watch(EventBus& bus, GameObjectId played) {
        a = bus.on_entered_board.connect([this, played](const EnteredBoardEvent& e) {
            if (e.object == played) seen.push_back("played");
        });
        b = bus.on_object_state_changed.connect([this](const ObjectStateChangedEvent& e) {
            if (e.what_changed == "recycled_main") seen.push_back("recycled");
        });
    }
};
}  // namespace

TEST_F(Batch6Test, R6_WildClaw_TheRestIsRecycledAfterTheCardIsPlayed) {
    for (int i = 0; i < 3; ++i) addToDeck(P1, kIncinerate);
    auto unit = addToDeck(P1, kShipyardSkulker);   // the top card
    auto spell = addToHand(P1, kWildClaw);
    Order order;
    order.watch(events, unit);
    EnginePathAgent me(EnginePathAgent::script({raw(unit), kRest}));
    playThroughEngine(spell, &me);
    ASSERT_TRUE(o(unit).isAtBase());
    EXPECT_EQ(order.seen, (std::vector<std::string>{"played", "recycled"}));
}

TEST_F(Batch6Test, R6_Reinforce_TheRestIsRecycledAfterTheUnitIsPlayed) {
    for (int i = 0; i < 3; ++i) addToDeck(P1, kIncinerate);
    auto unit = addToDeck(P1, kShipyardSkulker);
    auto spell = addToHand(P1, kReinforce);
    Order order;
    order.watch(events, unit);
    EnginePathAgent me(EnginePathAgent::script({raw(unit), kRest}));
    playThroughEngine(spell, &me);
    ASSERT_TRUE(o(unit).isAtBase());
    EXPECT_EQ(order.seen, (std::vector<std::string>{"played", "recycled"}));
}

TEST_F(Batch6Test, R6_DazzlingAurora_TheRestIsRecycledAfterTheUnitIsPlayed) {
    auto unit = addToDeck(P1, kShipyardSkulker);
    for (int i = 0; i < 2; ++i) addToDeck(P1, kIncinerate);   // revealed first
    auto aurora = addGear(P1, kDazzlingAurora);
    Order order;
    order.watch(events, unit);
    EnginePathAgent me(EnginePathAgent::script({kRest}));
    fireTriggerThroughEngine(aurora, TriggerType::AtEndOfTurn, &me);
    ASSERT_TRUE(o(unit).location.has_value());
    EXPECT_EQ(order.seen, (std::vector<std::string>{"played", "recycled"}));
}

TEST_F(Batch6Test, R6_LightningRush_TheCardIsDrawnBeforeTheRestGoToTheTrash) {
    addToDeck(P1, kIncinerate);
    addToDeck(P1, kIncinerate);
    auto pick = addToDeck(P1, kShipyardSkulker);   // the top card
    auto spell = addToHand(P1, kLightningRush);
    int trash_when_drawn = -1;
    const GameEngine* engine = nullptr;
    boost::signals2::scoped_connection c = events.on_cards_drawn.connect(
        [&](const CardsDrawnEvent&) {
            if (engine) trash_when_drawn = static_cast<int>(engine->state().player(P1).trash.size());
        });
    const int draws_before = state.player(P1).draws_this_turn;
    EnginePathAgent me(EnginePathAgent::script({raw(pick)}));
    fund(P1, card_db.get(kLightningRush).energy_cost, 0);
    withEngineDefaultTurn([&](GameEngine& e) {
        engine = &e;
        Intent i;
        i.type = IntentType::PlayCard;
        i.player = P1;
        i.card = spell;
        e.testHook_executeIntent(i);
        settleLikeTheEngine(e);
        engine = nullptr;
    }, &me, nullptr, P1);
    EXPECT_TRUE(inHand(P1, pick));
    EXPECT_EQ(state.player(P1).draws_this_turn, draws_before + 1) << "a draw";
    EXPECT_EQ(trash_when_drawn, 0) << "draw it, then put the rest into the trash";
    EXPECT_EQ(state.player(P1).trash.size(), 3u) << "the two others and Lightning Rush";
}

// The unit's own permission (CR 355.2.b, 420.3.b) for a card Wild Claw plays.
TEST_F(Batch6Test, R6_WildClaw_OffersWhereTheUnitItselfMayBePlayed) {
    addUnit(P2, kShipyardSkulker, 3, 1);
    state.battlefields[1].controller = P2;
    auto vanguard = addToDeck(P1, kDauntlessVanguard);
    auto spell = addToHand(P1, kWildClaw);
    fund(P1, 0, 1);   // Dauntless Vanguard's [1] after the [5] off
    EnginePathAgent me(EnginePathAgent::script({raw(vanguard), 2, kRest}));
    playThroughEngine(spell, &me);
    EXPECT_EQ(bfOf(vanguard), bf(1));
}

TEST_F(Batch6Test, R6_Reinforce_OffersWhereTheUnitItselfMayBePlayed) {
    addUnit(P2, kShipyardSkulker, 3, 1);
    state.battlefields[1].controller = P2;
    auto vanguard = addToDeck(P1, kDauntlessVanguard);
    auto spell = addToHand(P1, kReinforce);
    fund(P1, 0, 1);
    EnginePathAgent me(EnginePathAgent::script({raw(vanguard), 2, kRest}));
    playThroughEngine(spell, &me);
    EXPECT_EQ(bfOf(vanguard), bf(1));
}

TEST_F(Batch6Test, R6_PromisingFuture_OffersWhereTheUnitItselfMayBePlayed) {
    addUnit(P2, kShipyardSkulker, 3, 1);
    state.battlefields[1].controller = P2;
    auto vanguard = addToDeck(P1, kDauntlessVanguard);
    auto theirs = addToDeck(P2, kIncinerate);
    auto spell = addToHand(P1, kPromisingFuture);
    fund(P1, 0, 1);
    EnginePathAgent me(EnginePathAgent::script({raw(vanguard), 2, kRest}));
    EnginePathAgent them(EnginePathAgent::script({raw(theirs), kRest}));
    playThroughEngine(spell, &me, std::nullopt, &them);
    EXPECT_EQ(bfOf(vanguard), bf(1));
}

// ═══ R7: Cursed Sarcophagus: its own banished units ═════════════════════════

TEST_F(Batch6Test, R7_CursedSarcophagus_ACopyWithNothingBanishedWithItIsNotOffered) {
    auto with_one = addGear(P1, kCursedSarcophagus);
    auto empty = addGear(P1, kCursedSarcophagus);
    auto banished = addToTrash(P1, kShipyardSkulker);
    {
        auto& t = state.player(P1).trash;
        t.erase(std::remove(t.begin(), t.end(), banished), t.end());
        o(banished).zone = ZoneType::Banishment;
        state.player(P1).banishment.push_back(banished);
    }
    o(with_one).tracked_objects.push_back(banished);
    state.turn.phase = TurnPhase::MainPhase;
    state.turn.turn_player = P1;
    std::vector<Intent> legal;
    withEngine([&](GameEngine& e) { legal = e.generateLegalActions(); });
    auto offered = [&](GameObjectId src) {
        return std::any_of(legal.begin(), legal.end(), [&](const Intent& i) {
            return i.type == IntentType::ActivateAbility && i.ability_source == src;
        });
    };
    EXPECT_TRUE(offered(with_one));
    EXPECT_FALSE(offered(empty)) << "nothing was banished with this one";
}

TEST_F(Batch6Test, R7_CursedSarcophagus_PlayedAgainItIsANewObject) {
    auto card = addToHand(P1, kCursedSarcophagus);
    auto banished = addToTrash(P1, kShipyardSkulker);
    {
        auto& t = state.player(P1).trash;
        t.erase(std::remove(t.begin(), t.end(), banished), t.end());
        o(banished).zone = ZoneType::Banishment;
        state.player(P1).banishment.push_back(banished);
    }
    o(card).tracked_objects.push_back(banished);   // from its earlier life (CR 124.1)
    EnginePathAgent me;
    playThroughEngine(card, &me);
    ASSERT_TRUE(o(card).location.has_value());
    EXPECT_TRUE(o(card).tracked_objects.empty());
}

// ═══ R8: the battlefield is the target (CR 355.10.b, 355.10.d) ══════════════

TEST_F(Batch6Test, R8_SiphonPower_TheBattlefieldIsChosenAndStaysChosen) {
    auto mine = addUnit(P1, kShipyardSkulker, 3, 0);
    auto theirs = addUnit(P2, kShipyardSkulker, 3, 0);
    auto spell = addToHand(P1, kSiphonPower);
    Them them;
    // In response my unit goes home: the battlefield is still the one chosen.
    them.respond = [mine](GameEngine& e) {
        e.mutableState().getObject(mine).location = BaseLocation{PlayerId::Player1};
    };
    Me me(EnginePathAgent::script({0}));    // battlefield #0
    play(spell, me, them);
    ASSERT_EQ(me.prompts.size(), 1u);
    EXPECT_EQ(o(theirs).current_might, 2) << "-1 to the enemy unit there";
    EXPECT_EQ(o(mine).current_might, 3) << "no longer there";
}

TEST_F(Batch6Test, R8_SiphonPower_ABattlefieldWithNoUnitCanBeChosen) {
    auto spell = addToHand(P1, kSiphonPower);
    state.turn.phase = TurnPhase::MainPhase;
    state.turn.turn_player = P1;
    fund(P1, 2, 1);
    std::vector<Intent> legal;
    withEngine([&](GameEngine& e) { legal = e.generateLegalActions(); });
    EXPECT_TRUE(std::any_of(legal.begin(), legal.end(), [&](const Intent& i) {
        return i.card == spell; })) << "a battlefield is always there to choose";
}

TEST_F(Batch6Test, R8_Firestorm_NoDeflectIsPaidAndEveryEnemyUnitThereIsHit) {
    auto deflect = addUnit(P2, kPoutyPoro, 2, 0);
    auto other = addUnit(P2, kShipyardSkulker, 3, 0);
    auto spell = addToHand(P1, kFirestorm);
    Them them;
    them.respond = [other](GameEngine& e) {   // one of them leaves: the others still burn
        e.mutableState().getObject(other).location = BaseLocation{PlayerId::Player2};
    };
    Me me(EnginePathAgent::script({0}));     // battlefield #0
    play(spell, me, them);
    EXPECT_FALSE(o(deflect).location.has_value()) << "3 to the [Deflect] unit, no [A] owed";
    EXPECT_TRUE(o(other).location.has_value());
    EXPECT_EQ(o(other).damage_marked, 0);
}

// ═══ R9: Whirlwind's picks are not targets (CR 355.10.e) ════════════════════

TEST_F(Batch6Test, R9_Whirlwind_AUnitWithDeflectIsOfferedAndCostsNothing) {
    auto poro = addUnit(P2, kPoutyPoro, 2, 0);
    auto spell = addToHand(P1, kWhirlwind);
    EnginePathAgent them(EnginePathAgent::script({0}));   // declines
    EnginePathAgent me(EnginePathAgent::script({raw(poro)}));
    playThroughEngine(spell, &me, std::nullopt, &them);
    EXPECT_TRUE(inHand(P2, poro));
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 0);
}

// ═══ R10: Back Off: "If you played this from your hand, draw 1." ═══════════

TEST_F(Batch6Test, R10_BackOff_PlayedByAnEffectFromTheTrashDrawsNothing) {
    for (int i = 0; i < 3; ++i) addToDeck(P1, kIncinerate);
    auto unit = addUnit(P2, kShipyardSkulker, 3, 0);
    auto spell = addToTrash(P1, kBackOff);
    const size_t hand_before = state.player(P1).hand.size();
    EnginePathAgent me(EnginePathAgent::script({raw(unit), kRest}));
    withEngineDefaultTurn([&](GameEngine& e) {
        e.testHook_executor().playIgnoringCost(P1, spell);
        settleLikeTheEngine(e);
    }, &me, nullptr, P1);
    EXPECT_TRUE(o(unit).is_stunned);
    EXPECT_EQ(state.player(P1).hand.size(), hand_before) << "not played from a hand";
}

TEST_F(Batch6Test, R10_BackOff_PlayedFromTheHandDraws) {
    for (int i = 0; i < 3; ++i) addToDeck(P1, kIncinerate);
    auto unit = addUnit(P2, kShipyardSkulker, 3, 0);
    auto spell = addToHand(P1, kBackOff);
    const size_t hand_before = state.player(P1).hand.size();
    EnginePathAgent me(EnginePathAgent::script({raw(unit), kRest}));
    playThroughEngine(spell, &me);
    EXPECT_EQ(state.player(P1).hand.size(), hand_before) << "-1 played, +1 drawn";
}

// ═══ R11: Arcane Shift: "then its owner plays it" — the owner chooses where ═
// The player who plays a unit chooses its location (CR 355.2); for a unit its
// controller does not own that is the owner (as batch 5d's M4 for Thrill of
// the Hunt).

namespace {
/// P2: answers a value prompt with `value`, recording that it was asked.
class AnswersAValue : public EnginePathAgent {
public:
    int value = 0;
    int asked = 0;
    Intent selectAction(const GameState& st, const std::vector<Intent>& legal) override {
        for (const auto& i : legal)
            if (i.type == IntentType::MakeChoice && i.chosen_value.has_value() &&
                i.chosen_objects.empty()) {
                ++asked;
                for (const auto& j : legal)
                    if (j.chosen_value == value) return j;
                return i;
            }
        return EnginePathAgent::selectAction(st, legal);
    }
};
}  // namespace

TEST_F(Batch6Test, R11_ArcaneShift_TheOwnerChoosesWhereTheirUnitIsPlayed) {
    auto borrowed = addUnit(P2, kShipyardSkulker, 3, -1);
    o(borrowed).controller = P1;
    o(borrowed).location = BaseLocation{P1};
    auto enemy = addUnit(P2, kShipyardSkulker, 3, 0);
    addUnit(P2, kShipyardSkulker, 3, 1);
    state.battlefields[1].controller = P2;
    auto spell = addToHand(P1, kArcaneShift);
    EnginePathAgent me(EnginePathAgent::script({raw(borrowed), raw(enemy), kRest}));
    AnswersAValue them;
    them.value = 1;   // the owner's second place: battlefield #1
    playThroughEngine(spell, &me, std::nullopt, &them);
    EXPECT_EQ(them.asked, 1) << "the owner is asked";
    EXPECT_EQ(o(borrowed).controller, P2);
    EXPECT_EQ(bfOf(borrowed), bf(1));
}

// ═══ R13: a card picked from a hidden zone is not a target (CR 355.10.a) ════

namespace {
/// EnginePathAgent that notes the kind of every prompt offering `watch`.
class NotesKinds : public EnginePathAgent {
public:
    GameObjectId watch = kInvalidId;
    std::vector<PromptKind> kinds;
    using EnginePathAgent::EnginePathAgent;
    Intent decide(const GameState& st, const std::vector<Intent>& legal,
                  const Prompt& prompt) override {
        for (const auto& i : legal)
            if (!i.chosen_objects.empty() && i.chosen_objects.front() == watch) {
                kinds.push_back(prompt.kind);
                break;
            }
        return selectAction(st, legal);
    }
};
}  // namespace

TEST_F(Batch6Test, R13_DoubleTrouble_TheUnitFromTheDeckIsAPickNotATarget) {
    addToDeck(P1, kIncinerate);
    auto unit = addToDeck(P1, kShipyardSkulker);
    auto spell = addToHand(P1, kDoubleTrouble);
    NotesKinds me(EnginePathAgent::script({1, raw(unit), kRest}));
    me.watch = unit;
    playThroughEngine(spell, &me);
    ASSERT_EQ(me.kinds.size(), 1u);
    EXPECT_EQ(me.kinds[0], PromptKind::ChooseObject);
    EXPECT_TRUE(inHand(P1, unit));
}

TEST_F(Batch6Test, R13_HereToHelp_TheUnitFromTheHandIsAPickNotATarget) {
    addUnit(P1, kShipyardSkulker, 3, 0);
    state.battlefields[0].controller = P1;
    auto unit = addToHand(P1, kShipyardSkulker);
    auto spell = addToHand(P1, kHereToHelp);
    fund(P1, 3, 0);
    NotesKinds me(EnginePathAgent::script({1, raw(unit), kRest}));
    me.watch = unit;
    playThroughEngine(spell, &me);
    ASSERT_EQ(me.kinds.size(), 1u);
    EXPECT_EQ(me.kinds[0], PromptKind::ChooseObject);
    EXPECT_EQ(bfOf(unit), bf(0));
}

// ═══ R12: card data ═════════════════════════════════════════════════════════

// The gallery prints a modal or typed list as items; each item is a line of
// its own, never run into the one before ("them.Draw 1.").
TEST_F(Batch6Test, R12_NoCardTextRunsTwoSentencesTogether) {
    const std::regex run(R"([a-z0-9\]\)]\.[A-Z\[]|\)\[)");
    int checked = 0;
    for (CardDefId id : card_registry.classDefIds()) {
        const CardDef& def = card_db.get(id);
        ++checked;
        std::smatch m;
        EXPECT_FALSE(std::regex_search(def.ability_text, m, run))
            << id << " " << def.name << ": \"" << m.str() << "\" in " << def.ability_text;
    }
    EXPECT_GT(checked, 900);
}

// Predict is a game action (CR 437), not a keyword: no card has it.
TEST_F(Batch6Test, R12_NoCardCarriesPredictAsAKeyword) {
    for (CardDefId id : card_registry.classDefIds()) {
        const CardDef& def = card_db.get(id);
        EXPECT_FALSE(def.keywords.has(Keyword::Predict)) << id << " " << def.name;
    }
}

// ═══ R14: Maduli the Gatekeeper — judge question (pinned) ══════════════════
// "Move me to an occupied enemy battlefield ...": read as a battlefield with
// enemy units, whoever controls it. Dauntless Vanguard's "an occupied enemy
// battlefield" is read as one the opponent controls with their units there.

TEST_F(Batch6Test, R14_Maduli_ABattlefieldWithEnemyUnitsQualifiesWhoeverControlsIt) {
    auto maduli = addUnit(P1, kMaduli, 9, -1);
    addUnit(P2, kShipyardSkulker, 3, 0);
    addUnit(P1, kShipyardSkulker, 3, 0);
    state.battlefields[0].controller = P1;   // mine, with an enemy unit
    state.player(P1).rune_pool.power[static_cast<int>(Domain::Chaos)] = 1;
    Them them;
    Me me(EnginePathAgent::script({0}));   // battlefield #0
    activate(maduli, me, them);
    EXPECT_EQ(bfOf(maduli), bf(0));
}

TEST_F(Batch6Test, R14_DauntlessVanguard_AnEnemyBattlefieldIsOneTheOpponentControls) {
    auto vanguard = addToHand(P1, kDauntlessVanguard);
    addUnit(P2, kShipyardSkulker, 3, 0);        // battlefield #0: enemy units, no controller
    addUnit(P2, kShipyardSkulker, 3, 1);
    state.battlefields[1].controller = P2;      // battlefield #1: the opponent's
    state.turn.phase = TurnPhase::MainPhase;
    state.turn.turn_player = P1;
    fund(P1, 4, 1);
    std::vector<Intent> legal;
    withEngine([&](GameEngine& e) { legal = e.generateLegalActions(); });
    auto to = [&](int b) {
        return std::any_of(legal.begin(), legal.end(), [&](const Intent& i) {
            return i.card == vanguard && i.play_location.has_value() &&
                   *i.play_location == LocationId{BattlefieldLocation{BattlefieldId::fromRaw(b)}};
        });
    };
    EXPECT_FALSE(to(0)) << "enemy units, but nobody's battlefield";
    EXPECT_TRUE(to(1)) << "the opponent's, with their unit there";
}

}  // namespace riftbound::test::batch_6
