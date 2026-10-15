/// Testing phase, batch M1 (plans/remaining/10-testing-phase.md, item M1):
/// triggered and activated abilities of units, gear and legends whose leading
/// "may", cost, targets or move destination were chosen as they resolved, and
/// are now chosen as they are finalized (CR 383.3, 403.1-403.2, 405, 355.4-
/// 355.5). The controller's prompt must come before P2's first priority; a
/// target that becomes illegal in between is a mistarget. Also the pins of
/// judge questions 14 (Card Sharp) and 15 (Dr. Mundo). Verdicts:
/// plans/reports/batch-m1-list.md.

#include <gtest/gtest.h>

#include "batch_m1_agents.h"
#include "card_test_fixture.h"
#include "cards/card_helpers.h"

#include <algorithm>
#include <optional>
#include <vector>

namespace riftbound::test::batch_m1_abilities {

using batch_m1::Me;
using batch_m1::Them;
using batch_m1::raw;

constexpr CardDefId kDrMundoExpert = 109;
constexpr CardDefId kUnforgiven = 256;
constexpr CardDefId kCardSharp = 403;
constexpr CardDefId kFaeDragon = 423;
constexpr CardDefId kIascylla = 612;
constexpr CardDefId kMaduli = 706;
constexpr CardDefId kBottledConstellation = 854;
constexpr CardDefId kCorruptedDragon = 878;
constexpr CardDefId kGlowstone = 920;
constexpr CardDefId kZhonyasHourglass = 77;
constexpr CardDefId kDredgeUp = 836;

constexpr int kRest = EnginePathAgent::kRest;

class BatchM1Abilities : public CardTestFixture {
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
    /// Queue `source`'s ability for `trigger` and settle, P1 answering with
    /// `me`, P2 with `them`.
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
    /// Activate ability `index` of `source` the way a game does.
    void activate(GameObjectId source, Me& me, Them& them, int index = 0,
                  std::vector<GameObjectId> targets = {}) {
        me.other = &them;
        Intent i;
        i.type = IntentType::ActivateAbility;
        i.player = P1;
        i.ability_source = source;
        i.ability_index = index;
        i.targets = std::move(targets);
        withEngine([&](GameEngine& e) {
            them.engine = &e;
            e.testHook_executeIntent(i);
            settleLikeTheEngine(e);
            them.engine = nullptr;
        }, &me, &them, P1);
    }
};

// ─── 854 Bottled Constellation: the "may" and its cost (CR 383.3.a-b) ──────

TEST_F(BatchM1Abilities, BottledConstellation_TheMayAndTheKillsAreMadeAsItIsFinalized) {
    auto bc = addGear(P1, kBottledConstellation);
    auto u1 = addUnit(P1, kInvalidId, 1);
    auto u2 = addUnit(P1, kInvalidId, 1);
    auto u3 = addUnit(P1, kInvalidId, 1);
    auto keep = addUnit(P1, kInvalidId, 1);
    Them them;
    bool paid_by_priority = false;
    them.look = [&](const GameState& st) {
        paid_by_priority = !st.getObject(u1).location.has_value() &&
                           !st.getObject(u2).location.has_value() &&
                           !st.getObject(u3).location.has_value();
    };
    Me me(EnginePathAgent::script({1, raw(u1), raw(u2), raw(u3)}));
    fire(bc, TriggerType::AtStartOfMain, me, them);
    ASSERT_EQ(me.prompts.size(), 4u);
    EXPECT_EQ(me.promptsAfterPriority(), 0) << "the may and the three picks (CR 383.3)";
    EXPECT_TRUE(paid_by_priority) << "the kill is the ability's cost, paid as it is finalized (CR 405)";
    EXPECT_TRUE(o(keep).location.has_value());
    EXPECT_EQ(state.player(P1).score, 1);
}

TEST_F(BatchM1Abilities, BottledConstellation_AReplacedDeathPaysNothingAndScoresNothing) {
    auto bc = addGear(P1, kBottledConstellation);
    addGear(P1, kZhonyasHourglass);   // "If a friendly unit would die, kill this instead."
    auto u1 = addUnit(P1, kInvalidId, 1);
    auto u2 = addUnit(P1, kInvalidId, 1);
    auto u3 = addUnit(P1, kInvalidId, 1);
    Them them;
    Me me(EnginePathAgent::script({1, raw(u1), raw(u2), raw(u3), kRest}));
    fire(bc, TriggerType::AtStartOfMain, me, them);
    EXPECT_EQ(state.player(P1).score, 0) << "a death replaced is not a kill: the cost is unpaid";
}

// ─── 920 Glowstone: "Choose a player" (CR 403.2) ────────────────────────────

TEST_F(BatchM1Abilities, Glowstone_ThePlayerIsChosenAsItIsActivated) {
    auto g = addGear(P1, kGlowstone);
    o(g).empowered = 1;
    Them them;
    Me me(EnginePathAgent::script({1}));   // the opponent
    activate(g, me, them);
    ASSERT_EQ(me.prompts.size(), 1u);
    EXPECT_EQ(me.other_priority_at[0], 0);
    EXPECT_EQ(o(g).controller, P2);
    EXPECT_TRUE(o(g).isAtBase());
}

// ─── 256 Unforgiven: where a unit in base moves (CR 355.4) ──────────────────

TEST_F(BatchM1Abilities, Unforgiven_TheBattlefieldIsChosenAsItIsActivated) {
    auto legend = legendOf(P1, kUnforgiven);
    auto u = addUnit(P1, kInvalidId, 2);   // base
    fund(P1, 2, 0);
    Them them;
    Me me(EnginePathAgent::script({2}));   // 2 = battlefield 1
    activate(legend, me, them, 0, {u});
    ASSERT_EQ(me.prompts.size(), 1u);
    EXPECT_EQ(me.other_priority_at[0], 0);
    EXPECT_EQ(bfOf(u), bf(1));
}

TEST_F(BatchM1Abilities, Unforgiven_AUnitAtABattlefieldGoesToBaseWithNothingToChoose) {
    auto legend = legendOf(P1, kUnforgiven);
    auto u = addUnit(P1, kInvalidId, 2, 0);
    fund(P1, 2, 0);
    Them them;
    Me me(EnginePathAgent::script({}));
    activate(legend, me, them, 0, {u});
    EXPECT_TRUE(me.prompts.empty());
    EXPECT_TRUE(o(u).isAtBase());
}

// ─── 109 Dr. Mundo: "recycle 3 from your trash" — targets (CR 355.10.a) ─────

TEST_F(BatchM1Abilities, DrMundo_TheThreeCardsAreChosenAsItIsFinalized) {
    auto mundo = addUnit(P1, kDrMundoExpert, 0, 0);
    std::vector<GameObjectId> t;
    for (int i = 0; i < 5; ++i) t.push_back(addToTrash(P1, kDredgeUp));
    Them them;
    Me me(EnginePathAgent::script({raw(t[0]), raw(t[2]), raw(t[4])}));
    fire(mundo, TriggerType::AtStartOfBeginning, me, them);
    ASSERT_EQ(me.prompts.size(), 3u);
    EXPECT_EQ(me.promptsAfterPriority(), 0);
    EXPECT_TRUE(inDeck(P1, t[0]));
    EXPECT_TRUE(inDeck(P1, t[2]));
    EXPECT_TRUE(inDeck(P1, t[4]));
    EXPECT_TRUE(inTrash(P1, t[1]));
    EXPECT_TRUE(inTrash(P1, t[3]));
}

TEST_F(BatchM1Abilities, DrMundo_ACardThatLeftTheTrashIsAMistargetAndTheOthersAreRecycled) {
    auto mundo = addUnit(P1, kDrMundoExpert, 0, 0);
    std::vector<GameObjectId> t;
    for (int i = 0; i < 4; ++i) t.push_back(addToTrash(P1, kDredgeUp));
    Them them;
    them.respond = [&](GameEngine& e) { e.testHook_executor().banishObject(t[0]); };
    Me me(EnginePathAgent::script({raw(t[0]), raw(t[1]), raw(t[2])}));
    fire(mundo, TriggerType::AtStartOfBeginning, me, them);
    ASSERT_TRUE(them.responded);
    EXPECT_TRUE(inDeck(P1, t[1]));
    EXPECT_TRUE(inDeck(P1, t[2]));
    EXPECT_TRUE(inTrash(P1, t[3])) << "no other card is taken instead";
}

TEST_F(BatchM1Abilities, DrMundo_Pin_WithFewerThanThreeCardsTheAbilityIsRemoved) {
    // Judge question 15. "Recycle 3 from your trash" names three targets; with
    // two cards there the ability has no legal choice for its third and is
    // removed as it would be finalized (CR 403.4, read strictly as for judge
    // question 6): nothing is recycled.
    auto mundo = addUnit(P1, kDrMundoExpert, 0, 0);
    auto a = addToTrash(P1, kDredgeUp);
    auto b = addToTrash(P1, kDredgeUp);
    Them them;
    Me me(EnginePathAgent::script({kRest}));
    fire(mundo, TriggerType::AtStartOfBeginning, me, them);
    EXPECT_TRUE(inTrash(P1, a));
    EXPECT_TRUE(inTrash(P1, b));
    EXPECT_EQ(them.priority, 0) << "removed before it is finalized: no priority";
}

// ─── 423 Fae Dragon: "buff up to four friendly units" (CR 355.13, 427.1.c) ──

TEST_F(BatchM1Abilities, FaeDragon_TheUnitsAreChosenAsItIsFinalizedBuffedOnesIncluded) {
    auto fd = addUnit(P1, kFaeDragon, 0);
    auto u1 = addUnit(P1, kInvalidId, 2, 0);
    o(u1).buff_count = 1;
    auto u2 = addUnit(P1, kInvalidId, 2, 0);
    auto u3 = addUnit(P1, kInvalidId, 2, 1);
    Them them;
    Me me(EnginePathAgent::script({raw(u1), raw(u2), 0}));   // two, then "None"
    fire(fd, TriggerType::WhenYouPlayMe, me, them, fd);
    ASSERT_GE(me.prompts.size(), 3u);
    EXPECT_EQ(me.promptsAfterPriority(), 0);
    EXPECT_TRUE(me.wasOffered(u1)) << "a buffed unit can still be chosen (CR 427.1.c)";
    EXPECT_EQ(o(u1).buff_count, 1) << "but is not buffed again";
    EXPECT_EQ(o(u2).buff_count, 1);
    EXPECT_EQ(o(u3).buff_count, 0);
}

// ─── 612 Iascylla: the delayed ability's "may" and unit (CR 383.3.a, 355.5.b)

TEST_F(BatchM1Abilities, Iascylla_TheMayAndTheUnitAreChosenAsTheDelayedAbilityIsFinalized) {
    auto ias = addUnit(P1, kIascylla, 0, 0);
    auto enemy = addUnit(P2, kInvalidId, 3, 1);
    o(ias).card_counters["__iascylla_pull_bf"] = 0;
    Them them;
    Me me(EnginePathAgent::script({1, raw(enemy)}));
    fire(ias, TriggerType::AtStartOfMain, me, them);
    ASSERT_EQ(me.prompts.size(), 2u);
    EXPECT_EQ(me.promptsAfterPriority(), 0);
    EXPECT_EQ(bfOf(enemy), bf(0));
}

TEST_F(BatchM1Abilities, Iascylla_AUnitBouncedInResponseIsAMistarget) {
    auto ias = addUnit(P1, kIascylla, 0, 0);
    auto enemy = addUnit(P2, kInvalidId, 3, 1);
    auto other = addUnit(P2, kInvalidId, 3, 1);
    o(ias).card_counters["__iascylla_pull_bf"] = 0;
    Them them;
    them.respond = [&](GameEngine& e) { e.testHook_executor().bounceToHand(enemy); };
    Me me(EnginePathAgent::script({1, raw(enemy)}));
    fire(ias, TriggerType::AtStartOfMain, me, them);
    ASSERT_TRUE(them.responded);
    EXPECT_EQ(me.promptsAfterPriority(), 0);
    EXPECT_EQ(bfOf(other), bf(1)) << "no other unit is moved instead";
}

TEST_F(BatchM1Abilities, Iascylla_TheHoldThroughTheEngineSchedulesOneDelayedAbility) {
    auto ias = addUnit(P1, kIascylla, 0, 0);
    Them them;
    Me me(EnginePathAgent::script({}));
    fire(ias, TriggerType::WhenIHold, me, them);
    EXPECT_EQ(state.delayed_abilities.size(), 1u);
    EXPECT_TRUE(me.prompts.empty());
}

// ─── 706 Maduli the Gatekeeper: the enemy battlefield (CR 355.4) ────────────

TEST_F(BatchM1Abilities, Maduli_TheBattlefieldIsChosenAsItIsActivated) {
    auto m = addUnit(P1, kMaduli, 0);
    addUnit(P2, kInvalidId, 2, 0);
    addUnit(P2, kInvalidId, 3, 1);
    fund(P1, 0, 1);
    Them them;
    Me me(EnginePathAgent::script({1}));   // battlefield 1
    activate(m, me, them);
    ASSERT_EQ(me.prompts.size(), 1u);
    EXPECT_EQ(me.other_priority_at[0], 0);
    EXPECT_EQ(bfOf(m), bf(1));
}

// ─── 878 Corrupted Dragon: the "may" and the units (CR 383.3.a, 355.13) ─────

TEST_F(BatchM1Abilities, CorruptedDragon_TheMayAndTheUnitsAreChosenAsItIsFinalized) {
    auto cd = addUnit(P1, kCorruptedDragon, 0, 0);
    auto e1 = addUnit(P2, kInvalidId, 3, 0);
    auto e2 = addUnit(P2, kInvalidId, 4, 0);
    auto e3 = addUnit(P2, kInvalidId, 6, 0);
    combatInProgress(0, P1);
    Them them;
    Me me(EnginePathAgent::script({1, raw(e1), raw(e2), kRest}));
    fire(cd, TriggerType::WhenIAttack, me, them);
    ASSERT_GE(me.prompts.size(), 3u);
    EXPECT_EQ(me.promptsAfterPriority(), 0);
    EXPECT_TRUE(o(e1).isAtBase());
    EXPECT_TRUE(o(e2).isAtBase());
    EXPECT_EQ(bfOf(e3), bf(0)) << "more than 5 Might";
}

TEST_F(BatchM1Abilities, CorruptedDragon_AUnitThatLeftIsAMistarget) {
    auto cd = addUnit(P1, kCorruptedDragon, 0, 0);
    auto e1 = addUnit(P2, kInvalidId, 3, 0);
    auto e2 = addUnit(P2, kInvalidId, 4, 0);
    combatInProgress(0, P1);
    Them them;
    them.respond = [&](GameEngine& e) { e.testHook_executor().bounceToHand(e1); };
    Me me(EnginePathAgent::script({1, raw(e1), 0}));   // e1 only, then "None"
    fire(cd, TriggerType::WhenIAttack, me, them);
    ASSERT_TRUE(them.responded);
    EXPECT_EQ(bfOf(e2), bf(0)) << "no other unit is moved instead";
    EXPECT_EQ(me.promptsAfterPriority(), 0);
}

// ─── 403 Card Sharp: judge question 14, pinned at resolution ────────────────

TEST_F(BatchM1Abilities, CardSharp_Pin_EachPlayersMayIsDecidedAsItResolves) {
    // "When you play me, you and each opponent may play a Gold gear token
    // exhausted. For each opponent who did, you play a Gold gear token
    // exhausted." Each player's "may" is their own; declining mine does not
    // decide whether the ability is performed (CR 383.3.a.1), so it is asked
    // as the ability resolves (CR 383.3.a.3, 355.10.e) and the opponent is
    // still asked.
    auto cs = addUnit(P1, kCardSharp, 0);
    Them them;
    Me me(EnginePathAgent::script({0}));   // I decline
    fire(cs, TriggerType::WhenYouPlayMe, me, them, cs);
    ASSERT_EQ(me.prompts.size(), 1u);
    EXPECT_GT(me.other_priority_at[0], 0);
    EXPECT_EQ(them.prompts.size(), 1u) << "the opponent decides for themselves";
}

}  // namespace riftbound::test::batch_m1_abilities
