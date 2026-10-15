/// Testing phase, batch M1 (plans/remaining/10-testing-phase.md, item M1):
/// spells whose targets, modes and move destinations were chosen as they
/// resolved, after both players had priority, and are now chosen as they are
/// played (CR 355.3-355.5, 355.11-355.13). Each test plays the card through
/// the engine with P2 passing (and counting its priority): the choice must
/// come before P2's first priority, and be what the spell does as it
/// resolves. A target that becomes illegal in between is a mistarget.
/// The per-card verdicts are in plans/reports/batch-m1-list.md.

#include <gtest/gtest.h>

#include "batch_m1_agents.h"
#include "card_test_fixture.h"
#include "cards/card_helpers.h"

#include <algorithm>
#include <optional>
#include <vector>

namespace riftbound::test::batch_m1_spells {

using batch_m1::Me;
using batch_m1::Them;
using batch_m1::raw;

constexpr CardDefId kConvergentMutation = 108;
constexpr CardDefId kMorbidReturn = 170;
constexpr CardDefId kTheHarrowing = 198;
constexpr CardDefId kIcathianRain = 247;
constexpr CardDefId kZenithBlade = 258;
constexpr CardDefId kArcaneShift = 515;
constexpr CardDefId kTricksyTentacles = 616;
constexpr CardDefId kCallToBattle = 663;
constexpr CardDefId kDisposalOrder = 665;
constexpr CardDefId kStareDown = 669;
constexpr CardDefId kBoneSkewer = 701;
constexpr CardDefId kShadowsOfThePast = 890;
constexpr CardDefId kTwilightStep = 892;
constexpr CardDefId kDecreeOfDiscord = 894;
constexpr CardDefId kShadowDash = 935;
constexpr CardDefId kAccelerationGate = 937;
constexpr CardDefId kPitRookie = 136;       // a 2-Might unit, [2], no Power
constexpr CardDefId kDredgeUp = 836;        // a spell

constexpr int kRest = EnginePathAgent::kRest;

class BatchM1Spells : public CardTestFixture {
protected:
    GameObject& o(GameObjectId id) { return state.getObject(id); }
    void fund(PlayerId p, int energy, int power) {
        state.player(p).rune_pool.energy += energy;
        state.player(p).rune_pool.universal_power += power;
    }
    GameObjectId addToTrash(PlayerId owner, CardDefId def_id) {
        auto id = addToHand(owner, def_id);
        auto& h = state.player(owner).hand;
        h.erase(std::remove(h.begin(), h.end(), id), h.end());
        o(id).zone = ZoneType::Trash;
        state.player(owner).trash.push_back(id);
        return id;
    }
    std::optional<BattlefieldId> bfOf(GameObjectId id) { return o(id).battlefieldId(); }
    static std::optional<BattlefieldId> bf(int i) { return BattlefieldId::fromRaw(i); }
    /// Play `card` from P1's hand the way a game does (funded), with `targets`
    /// in the intent, P1 answering with `me` and P2 with `them`.
    void play(GameObjectId card, Me& me, Them& them, std::vector<GameObjectId> targets = {}) {
        const auto& def = card_db.get(o(card).card_def_id);
        fund(P1, def.energy_cost + 4, def.power_cost + 4);
        me.other = &them;
        Intent i;
        i.type = IntentType::PlayCard;
        i.player = P1;
        i.card = card;
        i.targets = std::move(targets);
        withEngine([&](GameEngine& e) {
            them.engine = &e;
            e.testHook_executeIntent(i);
            settleLikeTheEngine(e);
            them.engine = nullptr;
        }, &me, &them, P1);
    }
    /// Whether P1 is offered a play of `card` now.
    bool offered(GameObjectId card) {
        fund(P1, 10, 10);
        bool out = false;
        withEngine([&](GameEngine& e) {
            e.testHook_cleanup();
            for (const auto& i : e.generateLegalActions())
                if (i.card == card) out = true;
        });
        return out;
    }
};

// ─── 108 Convergent Mutation: "another friendly unit" is a second target ────

TEST_F(BatchM1Spells, ConvergentMutation_TheOtherUnitIsChosenAsItIsPlayed) {
    auto card = addToHand(P1, kConvergentMutation);
    auto a = addUnit(P1, kInvalidId, 2, 0);
    auto b = addUnit(P1, kInvalidId, 5, 0);
    addUnit(P1, kInvalidId, 1, 1);
    Them them;
    Me me(EnginePathAgent::script({raw(b), kRest}));
    play(card, me, them, {a});
    const int k = me.promptOffering(b);
    ASSERT_GE(k, 0) << "the other friendly unit is asked";
    EXPECT_EQ(me.other_priority_at[k], 0) << "before the opponent can respond (CR 355.5)";
    EXPECT_EQ(me.promptsAfterPriority(), 0) << "nothing is asked as it resolves";
    EXPECT_EQ(o(a).current_might, 5);
}

TEST_F(BatchM1Spells, ConvergentMutation_TheOtherUnitBouncedInResponseIsAMistarget) {
    auto card = addToHand(P1, kConvergentMutation);
    auto a = addUnit(P1, kInvalidId, 2, 0);
    auto b = addUnit(P1, kInvalidId, 5, 0);
    addUnit(P1, kInvalidId, 4, 1);
    Them them;
    them.respond = [&](GameEngine& e) { e.testHook_executor().bounceToHand(b); };
    Me me(EnginePathAgent::script({raw(b), kRest}));
    play(card, me, them, {a});
    ASSERT_TRUE(them.responded);
    EXPECT_EQ(me.promptsAfterPriority(), 0) << "no other unit is chosen instead";
    EXPECT_EQ(o(a).current_might, 2);
}

TEST_F(BatchM1Spells, ConvergentMutation_IsNotOfferedWithOneFriendlyUnit) {
    auto card = addToHand(P1, kConvergentMutation);
    addUnit(P1, kInvalidId, 2, 0);
    EXPECT_FALSE(offered(card)) << "CR 355.8: no valid choice for the second target";
    addUnit(P1, kInvalidId, 3, 1);
    EXPECT_TRUE(offered(card));
}

// ─── 170 Morbid Return / 198 The Harrowing: a unit in your trash ────────────

TEST_F(BatchM1Spells, MorbidReturn_TheUnitIsChosenAsItIsPlayed) {
    auto card = addToHand(P1, kMorbidReturn);
    addToTrash(P1, kPitRookie);
    auto u2 = addToTrash(P1, kPitRookie);
    Them them;
    Me me(EnginePathAgent::script({raw(u2), kRest}));
    play(card, me, them);
    const int k = me.promptOffering(u2);
    ASSERT_GE(k, 0);
    EXPECT_EQ(me.other_priority_at[k], 0) << "the trash is public: a target (CR 355.10.a)";
    EXPECT_TRUE(inHand(P1, u2));
}

TEST_F(BatchM1Spells, MorbidReturn_AUnitThatLeftTheTrashInResponseIsAMistarget) {
    auto card = addToHand(P1, kMorbidReturn);
    auto u1 = addToTrash(P1, kPitRookie);
    auto u2 = addToTrash(P1, kPitRookie);
    Them them;
    them.respond = [&](GameEngine& e) { e.testHook_executor().banishObject(u2); };
    Me me(EnginePathAgent::script({raw(u2), kRest}));
    play(card, me, them);
    ASSERT_TRUE(them.responded);
    EXPECT_FALSE(inHand(P1, u2));
    EXPECT_FALSE(inHand(P1, u1)) << "no other unit is returned instead";
    EXPECT_EQ(me.promptsAfterPriority(), 0);
}

TEST_F(BatchM1Spells, MorbidReturn_IsNotOfferedWithNoUnitInTheTrash) {
    auto card = addToHand(P1, kMorbidReturn);
    addToTrash(P1, kDredgeUp);
    EXPECT_FALSE(offered(card)) << "CR 355.8";
    addToTrash(P1, kPitRookie);
    EXPECT_TRUE(offered(card));
}

TEST_F(BatchM1Spells, TheHarrowing_TheUnitIsChosenAsItIsPlayed) {
    auto card = addToHand(P1, kTheHarrowing);
    addToTrash(P1, kPitRookie);
    auto u2 = addToTrash(P1, kPitRookie);
    Them them;
    Me me(EnginePathAgent::script({raw(u2), kRest}));
    play(card, me, them);
    const int k = me.promptOffering(u2);
    ASSERT_GE(k, 0);
    EXPECT_EQ(me.other_priority_at[k], 0);
    EXPECT_TRUE(o(u2).location.has_value()) << "played from the trash";
}

TEST_F(BatchM1Spells, TheHarrowing_IsNotOfferedWithNoUnitInTheTrash) {
    auto card = addToHand(P1, kTheHarrowing);
    EXPECT_FALSE(offered(card));
    addToTrash(P1, kPitRookie);
    EXPECT_TRUE(offered(card));
}

// ─── 247 Icathian Rain: six "Deal 2 to a unit", each a target ───────────────

TEST_F(BatchM1Spells, IcathianRain_AllSixUnitsAreChosenAsItIsPlayed) {
    auto card = addToHand(P1, kIcathianRain);
    auto e1 = addUnit(P2, kInvalidId, 30, 0);
    auto e2 = addUnit(P2, kInvalidId, 30, 1);
    Them them;
    Me me(EnginePathAgent::script({raw(e1), raw(e2), raw(e2), raw(e2), raw(e2), raw(e2)}));
    play(card, me, them);
    ASSERT_EQ(me.prompts.size(), 6u);
    EXPECT_EQ(me.promptsAfterPriority(), 0) << "every one before the opponent can respond";
    EXPECT_EQ(o(e1).damage_marked, 2);
    EXPECT_EQ(o(e2).damage_marked, 10);
}

TEST_F(BatchM1Spells, IcathianRain_AUnitBouncedInResponseTakesNothing) {
    auto card = addToHand(P1, kIcathianRain);
    auto e1 = addUnit(P2, kInvalidId, 30, 0);
    auto e2 = addUnit(P2, kInvalidId, 30, 1);
    Them them;
    them.respond = [&](GameEngine& e) { e.testHook_executor().bounceToHand(e2); };
    Me me(EnginePathAgent::script({raw(e1), raw(e2), raw(e2), raw(e2), raw(e2), raw(e1)}));
    play(card, me, them);
    ASSERT_TRUE(them.responded);
    EXPECT_EQ(o(e1).damage_marked, 4) << "its two instances; e2's four are mistargets";
    EXPECT_EQ(me.promptsAfterPriority(), 0);
}

// ─── 258 Zenith Blade: the friendly unit of "you may move" (CR 355.12) ──────

TEST_F(BatchM1Spells, ZenithBlade_TheFriendlyUnitIsChosenAsItIsPlayedAndTheMayAsItResolves) {
    auto card = addToHand(P1, kZenithBlade);
    auto enemy = addUnit(P2, kInvalidId, 3, 0);
    auto f1 = addUnit(P1, kInvalidId, 2);         // base
    addUnit(P1, kInvalidId, 2, 1);
    Them them;
    Me me(EnginePathAgent::script({raw(enemy), raw(f1), 1}));
    play(card, me, them);
    const int k = me.promptOffering(f1);
    ASSERT_GE(k, 0);
    EXPECT_EQ(me.other_priority_at[k], 0) << "the unit is a target, chosen at play (CR 355.12)";
    const int may = me.promptWith("Yes");
    ASSERT_GE(may, 0);
    EXPECT_GT(me.other_priority_at[may], 0) << "a spell's \"may\" is decided as it resolves";
    EXPECT_EQ(bfOf(f1), bf(0));
    EXPECT_TRUE(o(enemy).is_stunned);
}

TEST_F(BatchM1Spells, ZenithBlade_AFriendlyUnitThatLeftIsNotMoved) {
    auto card = addToHand(P1, kZenithBlade);
    auto enemy = addUnit(P2, kInvalidId, 3, 0);
    auto f1 = addUnit(P1, kInvalidId, 2);
    auto f2 = addUnit(P1, kInvalidId, 2, 1);
    Them them;
    them.respond = [&](GameEngine& e) { e.testHook_executor().bounceToHand(f1); };
    Me me(EnginePathAgent::script({raw(enemy), raw(f1), kRest}));
    play(card, me, them);
    ASSERT_TRUE(them.responded);
    EXPECT_EQ(me.promptsAfterPriority(), 0) << "nothing to move: no \"may\", no other unit";
    EXPECT_EQ(bfOf(f2), bf(1));
    EXPECT_TRUE(o(enemy).is_stunned);
}

// ─── 515 Arcane Shift: the friendly unit and the enemy unit, a pair ─────────

TEST_F(BatchM1Spells, ArcaneShift_TheEnemyUnitIsChosenAsItIsPlayed) {
    auto card = addToHand(P1, kArcaneShift);
    auto mine = addUnit(P1, kPitRookie, 0, 0);
    addUnit(P2, kInvalidId, 9, 0);
    auto e2 = addUnit(P2, kInvalidId, 9, 1);
    Them them;
    Me me(EnginePathAgent::script({raw(mine), raw(e2), kRest}));
    play(card, me, them);
    const int k = me.promptOffering(e2);
    ASSERT_GE(k, 0);
    EXPECT_EQ(me.other_priority_at[k], 0);
    EXPECT_EQ(o(e2).damage_marked, 3);
}

TEST_F(BatchM1Spells, ArcaneShift_IsNotOfferedWhenTheOnlyEnemyUnitsDeflectCannotBePaid) {
    // A fuzz finding of this batch: the enemy unit, chosen in the finalize
    // pass, was offered nothing when its Deflect could not be paid with the
    // cost, and the play threw. As a pair it is offered only with a payable
    // pair (CR 355.8, 809).
    constexpr CardDefId kPoutyPoro = 13;   // [Deflect]
    auto card = addToHand(P1, kArcaneShift);
    addUnit(P1, kPitRookie, 0, 0);
    addUnit(P2, kPoutyPoro, 0, 1);
    const auto& def = card_db.get(kArcaneShift);
    state.player(P1).rune_pool.energy = def.energy_cost;
    state.player(P1).rune_pool.universal_power = def.power_cost;
    bool out = false;
    withEngine([&](GameEngine& e) {
        e.testHook_cleanup();
        for (const auto& i : e.generateLegalActions())
            if (i.card == card) out = true;
    });
    EXPECT_FALSE(out) << "the Deflect's [A] is not there beside the cost";
    state.player(P1).rune_pool.universal_power += 1;
    withEngine([&](GameEngine& e) {
        e.testHook_cleanup();
        for (const auto& i : e.generateLegalActions())
            if (i.card == card) out = true;
    });
    EXPECT_TRUE(out);
}

TEST_F(BatchM1Spells, ArcaneShift_IsNotOfferedWithNoEnemyUnitAtABattlefield) {
    auto card = addToHand(P1, kArcaneShift);
    addUnit(P1, kPitRookie, 0, 0);
    addUnit(P2, kInvalidId, 9);                  // in base
    EXPECT_FALSE(offered(card)) << "CR 355.8: its second target";
    addUnit(P2, kInvalidId, 9, 1);
    EXPECT_TRUE(offered(card));
}

// ─── 616 Tricksy Tentacles: the units (CR 355.11) and the location (355.4) ──

TEST_F(BatchM1Spells, TricksyTentacles_TheUnitsAndTheLocationAreChosenAsItIsPlayed) {
    auto card = addToHand(P1, kTricksyTentacles);
    auto e1 = addUnit(P2, kInvalidId, 3, 0);
    auto e2 = addUnit(P2, kInvalidId, 4, 0);
    auto e3 = addUnit(P2, kInvalidId, 5, 0);
    Them them;
    Me me(EnginePathAgent::script({raw(e1), raw(e2), 2}));   // 2 = battlefield 1
    play(card, me, them);
    ASSERT_GE(me.prompts.size(), 3u);
    EXPECT_EQ(me.promptsAfterPriority(), 0);
    EXPECT_EQ(bfOf(e1), bf(1));
    EXPECT_EQ(bfOf(e2), bf(1));
    EXPECT_EQ(bfOf(e3), bf(0));
}

TEST_F(BatchM1Spells, TricksyTentacles_UnitsThatNoLongerFitAreASubsetChosenAsItResolves) {
    // CR 355.11.b: the group no longer has total Might 8 or less; the
    // controller chooses a legal subset of the original targets.
    auto card = addToHand(P1, kTricksyTentacles);
    auto e1 = addUnit(P2, kInvalidId, 3, 0);
    auto e2 = addUnit(P2, kInvalidId, 4, 0);
    Them them;
    them.respond = [&](GameEngine& e) {
        e.testHook_executor().giveTemporaryMight(e1, 3);
        e.testHook_cleanup();
    };
    Me me(EnginePathAgent::script({raw(e1), raw(e2), 0, raw(e2)}));   // 0 = their base
    play(card, me, them);
    ASSERT_TRUE(them.responded);
    EXPECT_EQ(me.promptsAfterPriority(), 1) << "only the subset is asked as it resolves";
    EXPECT_TRUE(o(e2).isAtBase());
    EXPECT_EQ(bfOf(e1), bf(0)) << "6 + 4 is more than 8: only e2 moves";
}

// ─── 663 Call to Battle: "to a battlefield you control" (CR 355.4) ──────────

TEST_F(BatchM1Spells, CallToBattle_TheBattlefieldIsChosenAsItIsPlayed) {
    auto card = addToHand(P1, kCallToBattle);
    auto mover = addUnit(P1, kInvalidId, 2);      // base
    addUnit(P1, kInvalidId, 2, 0);
    addUnit(P1, kInvalidId, 2, 1);
    state.battlefields[0].controller = P1;
    state.battlefields[1].controller = P1;
    auto theirs = addUnit(P2, kInvalidId, 2);
    Them them;
    Me me(EnginePathAgent::script({raw(mover), 1}));   // battlefield 1
    play(card, me, them);
    const int k = me.promptWith("Battlefield");
    ASSERT_GE(k, 0);
    EXPECT_EQ(me.other_priority_at[k], 0);
    EXPECT_EQ(bfOf(mover), bf(1));
    EXPECT_EQ(bfOf(theirs), bf(1)) << "the opponent moves theirs to the same battlefield";
}

// ─── 665 Disposal Order: the mode (CR 355.3) and the cards (355.13) ─────────

TEST_F(BatchM1Spells, DisposalOrder_TheModeAndTheCardsAreChosenAsItIsPlayed) {
    auto card = addToHand(P1, kDisposalOrder);
    auto c1 = addToTrash(P2, kDredgeUp);
    auto c2 = addToTrash(P2, kDredgeUp);
    auto c3 = addToTrash(P2, kDredgeUp);
    Them them;
    Me me(EnginePathAgent::script({0, raw(c1), raw(c3), 0}));   // mode 0, two cards, Done
    play(card, me, them);
    ASSERT_GE(me.prompts.size(), 3u);
    EXPECT_EQ(me.promptsAfterPriority(), 0);
    EXPECT_TRUE(inDeck(P2, c1));
    EXPECT_TRUE(inDeck(P2, c3));
    EXPECT_TRUE(inTrash(P2, c2));
}

TEST_F(BatchM1Spells, DisposalOrder_ACardThatLeftTheTrashIsAMistarget) {
    auto card = addToHand(P1, kDisposalOrder);
    auto c1 = addToTrash(P2, kDredgeUp);
    auto c2 = addToTrash(P2, kDredgeUp);
    auto c3 = addToTrash(P2, kDredgeUp);
    Them them;
    them.respond = [&](GameEngine& e) { e.testHook_executor().banishObject(c1); };
    Me me(EnginePathAgent::script({0, raw(c1), raw(c3), 0}));
    play(card, me, them);
    ASSERT_TRUE(them.responded);
    EXPECT_FALSE(inDeck(P2, c1));
    EXPECT_TRUE(inDeck(P2, c3));
    EXPECT_TRUE(inTrash(P2, c2)) << "nothing else is taken instead";
}

// ─── 669 Stare Down: "and a battlefield" (CR 355.5) ─────────────────────────

TEST_F(BatchM1Spells, StareDown_TheBattlefieldIsChosenAsItIsPlayed) {
    auto card = addToHand(P1, kStareDown);
    auto mine = addUnit(P1, kInvalidId, 4);
    auto weak = addUnit(P2, kInvalidId, 2, 1);
    auto strong = addUnit(P2, kInvalidId, 6, 1);
    Them them;
    Me me(EnginePathAgent::script({raw(mine), 1}));
    play(card, me, them);
    ASSERT_EQ(me.prompts.size(), 2u);
    EXPECT_EQ(me.promptsAfterPriority(), 0);
    EXPECT_TRUE(o(weak).isAtBase());
    EXPECT_EQ(bfOf(strong), bf(1));
}

// ─── 701 Bone Skewer: "Choose a battlefield" (CR 355.5) ─────────────────────

TEST_F(BatchM1Spells, BoneSkewer_TheBattlefieldIsChosenAsItIsPlayed) {
    auto card = addToHand(P1, kBoneSkewer);
    auto theirs = addToHand(P2, kPitRookie);
    Them them;
    Me me(EnginePathAgent::script({1, 1, raw(theirs)}));   // battlefield 1, yes, the unit
    play(card, me, them);
    ASSERT_GE(me.prompts.size(), 1u);
    EXPECT_EQ(me.other_priority_at[0], 0) << "the battlefield, before anyone responds";
    const int k = me.promptOffering(theirs);
    ASSERT_GE(k, 0);
    EXPECT_GT(me.other_priority_at[k], 0) << "the unit from the revealed hand, as it resolves";
    EXPECT_EQ(bfOf(theirs), bf(1));
}

// ─── 890 Shadows of the Past: up to 2 units from trashes (CR 355.13) ────────

TEST_F(BatchM1Spells, ShadowsOfThePast_TheUnitsAreChosenAsItIsPlayed) {
    auto card = addToHand(P1, kShadowsOfThePast);
    auto mine = addToTrash(P1, kPitRookie);
    auto theirs = addToTrash(P2, kPitRookie);
    addToTrash(P2, kPitRookie);
    Them them;
    Me me(EnginePathAgent::script({raw(mine), raw(theirs)}));
    play(card, me, them);
    ASSERT_EQ(me.prompts.size(), 2u);
    EXPECT_EQ(me.promptsAfterPriority(), 0);
    EXPECT_TRUE(inHand(P1, mine));
    EXPECT_TRUE(inHand(P2, theirs));
}

TEST_F(BatchM1Spells, ShadowsOfThePast_AUnitThatLeftTheTrashIsAMistarget) {
    auto card = addToHand(P1, kShadowsOfThePast);
    auto mine = addToTrash(P1, kPitRookie);
    auto theirs = addToTrash(P2, kPitRookie);
    auto other = addToTrash(P2, kPitRookie);
    Them them;
    them.respond = [&](GameEngine& e) { e.testHook_executor().banishObject(theirs); };
    Me me(EnginePathAgent::script({raw(mine), raw(theirs)}));
    play(card, me, them);
    ASSERT_TRUE(them.responded);
    EXPECT_TRUE(inHand(P1, mine));
    EXPECT_FALSE(inHand(P2, other)) << "nothing else is taken instead";
    EXPECT_EQ(me.promptsAfterPriority(), 0);
}

// ─── 892 Twilight Step / 935 Shadow Dash: where the unit moves (CR 355.4) ───

TEST_F(BatchM1Spells, TwilightStep_TheDestinationIsChosenAsItIsPlayed) {
    auto card = addToHand(P1, kTwilightStep);
    auto u = addUnit(P2, kInvalidId, 2, 0);
    Them them;
    Me me(EnginePathAgent::script({raw(u), 2}));   // 2 = battlefield 1
    play(card, me, them);
    ASSERT_EQ(me.prompts.size(), 2u);
    EXPECT_EQ(me.promptsAfterPriority(), 0);
    EXPECT_EQ(bfOf(u), bf(1));
}

TEST_F(BatchM1Spells, ShadowDash_TheBattlefieldIsChosenAsItIsPlayed) {
    auto card = addToHand(P1, kShadowDash);
    auto enemy = addUnit(P2, kInvalidId, 2, 0);
    auto a = addUnit(P1, kInvalidId, 2, 1);
    auto b = addUnit(P1, kInvalidId, 2, 1);
    addUnit(P1, kInvalidId, 2, 0);
    Them them;
    Me me(EnginePathAgent::script({raw(enemy), 2}));   // 2 = battlefield 1
    play(card, me, them);
    ASSERT_EQ(me.prompts.size(), 2u);
    EXPECT_EQ(me.promptsAfterPriority(), 0);
    EXPECT_EQ(bfOf(enemy), bf(1));
    EXPECT_EQ(o(a).current_might, 3) << "exactly two of mine there";
    EXPECT_EQ(o(b).current_might, 3);
}

// ─── 894 Decree of Discord: enemy Order units, total Might 5 (CR 355.11) ────

TEST_F(BatchM1Spells, DecreeOfDiscord_TheUnitsAreChosenAsItIsPlayed) {
    auto card = addToHand(P1, kDecreeOfDiscord);
    auto e1 = addUnit(P2, kInvalidId, 2, 0);
    auto e2 = addUnit(P2, kInvalidId, 3, 1);
    auto e3 = addUnit(P2, kInvalidId, 1, 1);
    for (auto e : {e1, e2, e3}) o(e).domains = std::vector<Domain>{Domain::Order};
    Them them;
    Me me(EnginePathAgent::script({raw(e1), raw(e2)}));
    play(card, me, them);
    EXPECT_GE(me.prompts.size(), 2u);
    EXPECT_EQ(me.promptsAfterPriority(), 0);
    EXPECT_TRUE(inHand(P2, e1));
    EXPECT_TRUE(inHand(P2, e2));
    EXPECT_EQ(bfOf(e3), bf(1));
}

// ─── 937 Acceleration Gate: up to 4 units, gear and/or runes (CR 355.13) ────

TEST_F(BatchM1Spells, AccelerationGate_TheObjectsAreChosenAsItIsPlayed) {
    auto card = addToHand(P1, kAccelerationGate);
    auto u = addUnit(P1, kInvalidId, 2, 0);
    o(u).is_exhausted = true;
    auto r = addRune(P1, Domain::Fury);
    o(r).is_exhausted = true;
    Them them;
    auto ready = addUnit(P2, kInvalidId, 2, 1);   // offered too, not chosen
    Me me(EnginePathAgent::script({raw(u), raw(r), 0}));   // two, then "None"
    play(card, me, them);
    ASSERT_EQ(me.prompts.size(), 3u);
    EXPECT_TRUE(me.wasOffered(ready)) << "any unit, gear or rune: not only exhausted ones";
    EXPECT_EQ(me.promptsAfterPriority(), 0);
    EXPECT_FALSE(o(u).is_exhausted);
    EXPECT_FALSE(o(r).is_exhausted);
}

TEST_F(BatchM1Spells, AccelerationGate_AReadyObjectCanBeChosen) {
    // At play nothing says the object is still ready as the spell resolves:
    // "Ready up to 4 units" has no "exhausted" restriction (CR 355.9).
    auto card = addToHand(P1, kAccelerationGate);
    auto u = addUnit(P1, kInvalidId, 2, 0);
    Them them;
    them.respond = [&](GameEngine& e) { e.testHook_executor().exhaustObject(u); };
    Me me(EnginePathAgent::script({raw(u), 0}));
    play(card, me, them);
    ASSERT_TRUE(them.responded);
    EXPECT_TRUE(me.wasOffered(u));
    EXPECT_FALSE(o(u).is_exhausted) << "exhausted in response, readied as it resolves";
}

}  // namespace riftbound::test::batch_m1_spells
