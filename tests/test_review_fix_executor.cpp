/// @file test_review_fix_executor.cpp
/// Regression tests for the EffectExecutor findings of the 2026-10 engine
/// review (plans/02-engine-rules.md items 2.14, 2.15, 2.21, 2.22, 2.28, 2.31;
/// plans/08-cards-full-review.md items 8.13, 8.14).
///
/// Every test runs on a real GameEngine, so the executor is the engine's own
/// (TriggerManager subscribed, chain and agents wired). Where a card shows the
/// defect it is played or activated through `executeIntent`.

#include "cards/card_test_fixture.h"
#include "engine/chain_manager.h"
#include "engine/effect_executor.h"
#include "engine/game_engine.h"
#include "engine/knowledge_tracker.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <memory>

namespace riftbound::test {
namespace test_review_fix_executor {  // file-unique: test sources are unity-built
namespace {

constexpr CardDefId kVanillaUnit = 1;        // Blazing Scorcher
constexpr CardDefId kLeeSinAscetic = 78;     // "[E]: Buff me. I can have any number of buffs."
constexpr CardDefId kKaynUnleashed = 189;    // "If I have moved twice this turn, I don't take damage."
constexpr CardDefId kKarmaChanneler = 235;   // "[Vision] ... When you recycle ..., buff a friendly unit."
constexpr CardDefId kBlindMonk = 254;        // legend: "[1], [E]: Buff a friendly unit."
constexpr CardDefId kSimianAncestor = 370;   // "When you buff me, ready me."
constexpr CardDefId kTianna = 383;           // "While I'm at a battlefield, opponents can't gain points."
constexpr CardDefId kFrigidTouch = 389;      // "Give a unit -2 [M] this turn."
constexpr CardDefId kRiftHerald = 741;       // "[Deathknell] Play a unit from your hand to your base ..."
constexpr CardDefId kClairvoyance = 843;     // "[Predict 5]. Draw 2."

class ExecutorReviewFixTest : public CardTestFixture {
protected:
    std::unique_ptr<GameEngine> engine_;
    FirstChoiceAgent a1_, a2_;
    GameState& s() { return engine_->mutableState(); }
    EffectExecutor& exec() { return engine_->testHook_executor(); }

    void SetUp() override {
        CardTestFixture::SetUp();
        engine_ = std::make_unique<GameEngine>(card_db, events, card_registry);
        auto& st = s();
        st.mode = ModeOfPlay{};
        st.players[0].id = P1;
        st.players[1].id = P2;
        for (int i = 0; i < 2; ++i) {
            BattlefieldState bf;
            bf.id = BattlefieldId::fromRaw(st.battlefields.size());
            st.battlefields.push_back(bf);
        }
        engine_->testHook_setAgents(&a1_, &a2_);
        st.turn.turn_player = P1;
        st.turn.turn_number = 1;
        st.turn.phase = TurnPhase::MainPhase;
        st.turn.ns_state = NeutralShowdownState::Neutral;
        st.turn.oc_state = OpenClosedState::Open;
        legend(P1);
        legend(P2);
    }

    GameObjectId legend(PlayerId p, CardDefId def_id = kInvalidId) {
        auto id = s().createObject();
        auto& lo = s().getObject(id);
        lo.owner = lo.controller = p;
        lo.card_type = CardType::Legend;
        lo.card_def_id = def_id;
        if (def_id != kInvalidId) lo.name = card_db.get(def_id).name;
        lo.zone = ZoneType::LegendZone;
        s().player(p).legend_zone = id;
        return id;
    }

    /// A card object of `def_id` owned by `p`, in no zone yet.
    GameObjectId card(PlayerId p, CardDefId def_id) {
        auto id = s().createObject();
        auto& c = s().getObject(id);
        const auto& def = card_db.get(def_id);
        c.owner = c.controller = p;
        c.card_def_id = def_id;
        c.card_type = def.card_type;
        c.super_type = def.super_type;
        c.name = def.name;
        c.domains = def.domains;
        c.tags = def.tags;
        c.keywords = def.keywords;
        c.base_might = c.current_might = def.might;
        return id;
    }
    GameObjectId unit(PlayerId p, CardDefId def_id, int at_bf = -1) {
        auto id = card(p, def_id);
        auto& u = s().getObject(id);
        if (at_bf >= 0) {
            u.zone = ZoneType::BattlefieldZone;
            u.location = BattlefieldLocation{BattlefieldId::fromRaw(at_bf)};
        } else {
            u.zone = ZoneType::Base;
            u.location = BaseLocation{p};
        }
        return id;
    }
    GameObjectId inHandCard(PlayerId p, CardDefId def_id) {
        auto id = card(p, def_id);
        s().getObject(id).zone = ZoneType::Hand;
        s().player(p).hand.push_back(id);
        return id;
    }
    GameObjectId inTrashCard(PlayerId p, CardDefId def_id) {
        auto id = card(p, def_id);
        s().getObject(id).zone = ZoneType::Trash;
        s().player(p).trash.push_back(id);
        return id;
    }
    GameObjectId onDeck(PlayerId p, CardDefId def_id) {
        auto id = card(p, def_id);
        deck::putOnTop(s(), p, id, deck::Place::Public);
        return id;
    }
    /// A token unit, made the way cards make them.
    GameObjectId token(PlayerId p, int at_bf) {
        return exec().createToken(p, CardType::Unit, "Recruit", 1, {}, KeywordSet{},
                                  LocationId{BattlefieldLocation{BattlefieldId::fromRaw(at_bf)}});
    }
    /// A gear on the board attached to `to`.
    GameObjectId attachedGear(PlayerId p, GameObjectId to) {
        auto id = s().createObject();
        auto& g = s().getObject(id);
        g.owner = g.controller = p;
        g.card_type = CardType::Gear;
        g.name = "TestGear";
        g.zone = s().getObject(to).zone;
        g.location = s().getObject(to).location;
        g.attached_to = to;
        s().getObject(to).attachments.push_back(id);
        return id;
    }

    bool listed(const std::vector<GameObjectId>& v, GameObjectId id) {
        return std::find(v.begin(), v.end(), id) != v.end();
    }
    bool onBoard(GameObjectId id) { return s().getObject(id).location.has_value(); }

    /// Resolve whatever triggers are waiting on the chain (the Ending Step
    /// runs the chain when one exists).
    void resolveChain() { engine_->testHook_endingStep(); }

    void activate(GameObjectId source, const std::vector<GameObjectId>& targets = {}) {
        Intent act;
        act.type = IntentType::ActivateActionAbility;
        act.player = s().getObject(source).controller;
        act.ability_source = source;
        act.targets = targets;
        engine_->testHook_executeIntent(act); settleLikeTheEngine(*engine_);
    }
    void play(PlayerId p, GameObjectId c) {
        Intent i;
        i.type = IntentType::PlayCard;
        i.player = p;
        i.card = c;
        engine_->testHook_executeIntent(i); settleLikeTheEngine(*engine_);
    }
};

// ─── 2.14 A: one Buff per unit (CR 427.1.b, 702.3) ──────────────────────────

TEST_F(ExecutorReviewFixTest, Buff_BlindMonkDoesNotStackASecondBuff) {
    auto monk = legend(P1, kBlindMonk);
    auto u = unit(P1, kVanillaUnit);
    const int printed = s().getObject(u).current_might;

    s().player(P1).rune_pool.energy = 1;
    activate(monk, {u});
    ASSERT_EQ(s().getObject(u).buff_count, 1) << "an unbuffed unit gets its Buff";
    EXPECT_EQ(s().getObject(u).current_might, printed + 1);

    // Next turn: the legend is ready again and the ability is paid for again.
    s().getObject(monk).is_exhausted = false;
    s().player(P1).rune_pool.energy = 1;
    activate(monk, {u});
    EXPECT_EQ(s().getObject(u).buff_count, 1)
        << "CR 427.1.b.1: a unit with a Buff does not get another one";
    EXPECT_EQ(s().getObject(u).current_might, printed + 1);
}

TEST_F(ExecutorReviewFixTest, Buff_LeeSinAsceticCanHaveAnyNumber) {
    auto lee = unit(P1, kLeeSinAscetic);
    activate(lee);
    ASSERT_EQ(s().getObject(lee).buff_count, 1);
    s().getObject(lee).is_exhausted = false;
    activate(lee);
    EXPECT_EQ(s().getObject(lee).buff_count, 2)
        << "CR 427.1.b.2: \"I can have any number of buffs\" lifts the limit";
}

TEST_F(ExecutorReviewFixTest, Buff_ReportsWhetherABuffWasPlaced) {
    auto u = unit(P1, kVanillaUnit);
    EXPECT_TRUE(exec().buffUnit(u));
    EXPECT_FALSE(exec().buffUnit(u)) << "CR 427.1.c: chosen, but not buffed";
}

// CR 427.1.c, second example: "When you buff me, ready me" does not trigger
// when the unit chosen for a Buff already has one.
TEST_F(ExecutorReviewFixTest, Buff_SimianAncestorTriggersOnlyWhenActuallyBuffed) {
    auto monk = legend(P1, kBlindMonk);
    auto simian = unit(P1, kSimianAncestor);
    s().getObject(simian).is_exhausted = true;

    s().player(P1).rune_pool.energy = 1;
    activate(monk, {simian});
    ASSERT_EQ(s().getObject(simian).buff_count, 1);
    EXPECT_FALSE(s().getObject(simian).is_exhausted) << "buffed -> readied";

    s().getObject(simian).is_exhausted = true;
    s().getObject(monk).is_exhausted = false;
    s().player(P1).rune_pool.energy = 1;
    activate(monk, {simian});
    EXPECT_EQ(s().getObject(simian).buff_count, 1);
    EXPECT_TRUE(s().getObject(simian).is_exhausted)
        << "already buffed: not buffed again, so the ability does not trigger";
}

// ─── 2.14 B: +/-[M] and keywords "this turn" are not Buffs ──────────────────

TEST_F(ExecutorReviewFixTest, TemporaryMight_IsNotABuff_FrigidTouchOnSimianAncestor) {
    auto simian = unit(P1, kSimianAncestor);
    s().getObject(simian).is_exhausted = true;
    const auto& def = card_db.get(kFrigidTouch);
    s().player(P1).rune_pool.energy = def.energy_cost;
    s().player(P1).rune_pool.power[static_cast<int>(def.domains.front())] = def.power_cost;
    auto spell = inHandCard(P1, kFrigidTouch);

    play(P1, spell);

    ASSERT_EQ(s().getObject(simian).temp_might_bonus, -2) << "Frigid Touch resolved on it";
    EXPECT_EQ(s().getObject(simian).buff_count, 0);
    EXPECT_TRUE(s().getObject(simian).is_exhausted)
        << "-2 [M] this turn is not a Buff: \"When you buff me\" must not fire";
}

TEST_F(ExecutorReviewFixTest, TemporaryKeyword_IsNotABuff) {
    auto simian = unit(P1, kSimianAncestor);
    s().getObject(simian).is_exhausted = true;
    exec().giveTemporaryKeyword(simian, Keyword::Ganking, 0);
    exec().giveTemporaryMight(simian, 3);
    EXPECT_TRUE(s().chain.items.empty()) << "no \"When you buff me\" ability was queued";
    resolveChain();
    EXPECT_TRUE(s().getObject(simian).is_exhausted);
}

// ─── 2.15: Predict and Vision recycle (CR 437.1, 817.1.b) ───────────────────

TEST_F(ExecutorReviewFixTest, Predict_RecyclingFiresWhenYouRecycle) {
    auto karma = unit(P1, kKarmaChanneler);            // the only friendly unit
    for (int i = 0; i < 8; ++i) onDeck(P1, kVanillaUnit);
    const auto& def = card_db.get(kClairvoyance);
    s().player(P1).rune_pool.energy = def.energy_cost;
    s().player(P1).rune_pool.power[static_cast<int>(def.domains.front())] = def.power_cost;
    auto spell = inHandCard(P1, kClairvoyance);

    // FirstChoiceAgent takes the first option of each Predict choice: recycle.
    play(P1, spell);
    resolveChain();

    EXPECT_EQ(s().getObject(karma).buff_count, 1)
        << "Karma, Channeler: a Predict recycle is a recycle to the Main Deck";
    EXPECT_TRUE(knowledge::check(s()).empty());
}

TEST_F(ExecutorReviewFixTest, Predict_KeepingEveryCardRecyclesNothing) {
    // An agent that keeps each Predicted card on top (the second option).
    PickByPredicateAgent keep([](int, const std::vector<Intent>& legal) {
        return legal.size() > 1 ? legal[1] : legal.front();
    });
    engine_->testHook_setAgents(&keep, &a2_);
    auto karma = unit(P1, kKarmaChanneler);
    for (int i = 0; i < 3; ++i) onDeck(P1, kVanillaUnit);

    exec().predict(P1, 2);

    EXPECT_TRUE(s().chain.items.empty()) << "nothing was recycled, so nothing triggers";
    EXPECT_EQ(s().getObject(karma).buff_count, 0);
}

TEST_F(ExecutorReviewFixTest, RevealAndChoose_RecyclingFiresWhenYouRecycle) {
    // An agent that draws none of the revealed cards (the second option), so
    // they are all recycled; it then takes the first option for Karma's Buff.
    PickByPredicateAgent skip([](int n, const std::vector<Intent>& legal) {
        return (n < 2 && legal.size() > 1) ? legal[1] : legal.front();
    });
    engine_->testHook_setAgents(&skip, &a2_);
    auto karma = unit(P1, kKarmaChanneler);
    for (int i = 0; i < 3; ++i) onDeck(P1, kVanillaUnit);

    auto drawn = exec().revealAndChoose(P1, 2);
    ASSERT_TRUE(drawn.empty());
    resolveChain();

    EXPECT_EQ(s().getObject(karma).buff_count, 1);
}

TEST_F(ExecutorReviewFixTest, Vision_RecyclingFiresWhenYouRecycle) {
    auto karma = unit(P1, kKarmaChanneler);
    onDeck(P1, kVanillaUnit);
    auto top_spell = onDeck(P1, kFrigidTouch);     // Vision recycles a spell on top
    const auto& def = card_db.get(kKarmaChanneler);
    s().player(P1).rune_pool.energy = def.energy_cost;
    s().player(P1).rune_pool.power[static_cast<int>(def.domains.front())] = def.power_cost;
    auto second = inHandCard(P1, kKarmaChanneler); // a second Vision unit, played now

    play(P1, second);
    resolveChain();

    ASSERT_TRUE(onBoard(second));
    ASSERT_EQ(s().player(P1).main_deck.front(), top_spell) << "Vision recycled the spell";
    EXPECT_GE(s().getObject(karma).buff_count + s().getObject(second).buff_count, 1)
        << "the Vision recycle is announced, so a Karma on board buffs a unit";
}

// ─── 2.21: the burn-out point is a point gained ─────────────────────────────

TEST_F(ExecutorReviewFixTest, BurnOut_GivesAPointAndAnnouncesIt) {
    inTrashCard(P1, kVanillaUnit);              // something to recycle and draw
    int announced = 0;
    auto conn = events.on_points_gained.connect([&](const PointsGainedEvent& e) {
        if (e.player == P2) announced += e.amount;
    });

    engine_->testHook_drawCards(P1, 1);
    conn.disconnect();

    EXPECT_EQ(s().player(P2).score, 1);
    EXPECT_EQ(announced, 1) << "PointsGainedEvent is emitted for the burn-out point";
    EXPECT_EQ(s().player(P1).hand.size(), 1u);
}

TEST_F(ExecutorReviewFixTest, BurnOut_RespectsCantGainPoints) {
    unit(P1, kTianna, /*at_bf=*/0);             // "opponents can't gain points"
    engine_->testHook_cleanup();
    ASSERT_TRUE(s().player(P2).cannot_gain_points);
    inTrashCard(P1, kVanillaUnit);

    engine_->testHook_drawCards(P1, 1);

    EXPECT_EQ(s().player(P2).score, 0) << "Tianna Crownguard stops the burn-out point";
    EXPECT_EQ(s().player(P1).hand.size(), 1u) << "the Burn Out itself still happens";
    EXPECT_FALSE(s().game_over);
}

TEST_F(ExecutorReviewFixTest, BurnOut_WithNothingToRecycleAndNoPointStops) {
    unit(P1, kTianna, /*at_bf=*/0);
    engine_->testHook_cleanup();
    s().player(P2).score = 7;

    engine_->testHook_drawCards(P1, 1);         // empty deck, empty trash

    EXPECT_EQ(s().player(P2).score, 7);
    EXPECT_FALSE(s().game_over);
    EXPECT_TRUE(s().player(P1).hand.empty());
}

// ─── 2.22: effect-driven moves count and carry attached gear ────────────────

TEST_F(ExecutorReviewFixTest, EffectMove_CarriesAttachedGear) {
    auto u = unit(P1, kVanillaUnit);
    auto gear = attachedGear(P1, u);

    exec().moveToBattlefield(u, BattlefieldId::fromRaw(1));
    ASSERT_TRUE(s().getObject(gear).location.has_value());
    EXPECT_EQ(*s().getObject(gear).location, LocationId{BattlefieldLocation{1}})
        << "CR 719.3: attached gear is at its unit's location";
    EXPECT_EQ(s().getObject(gear).zone, ZoneType::BattlefieldZone);

    exec().moveToBase(u);
    EXPECT_EQ(*s().getObject(gear).location, LocationId{BaseLocation{P1}});
    EXPECT_EQ(s().getObject(gear).zone, ZoneType::Base);
    EXPECT_EQ(s().getObject(gear).attached_to, std::optional<GameObjectId>(u));
}

TEST_F(ExecutorReviewFixTest, EffectMove_CountsTowardKaynUnleashed) {
    auto kayn = unit(P1, kKaynUnleashed);
    exec().moveToBattlefield(kayn, BattlefieldId::fromRaw(0));
    engine_->testHook_cleanup();
    EXPECT_FALSE(s().getObject(kayn).immune_to_damage) << "one move is not enough";

    exec().moveToBattlefield(kayn, BattlefieldId::fromRaw(1));
    engine_->testHook_cleanup();
    EXPECT_EQ(s().getObject(kayn).moves_this_turn, 2);
    EXPECT_TRUE(s().getObject(kayn).immune_to_damage)
        << "moved twice this turn by effects: Kayn doesn't take damage";

    exec().dealDamage(kayn, 3, kInvalidId);
    EXPECT_EQ(s().getObject(kayn).damage_marked, 0);
}

TEST_F(ExecutorReviewFixTest, EffectMove_ToWhereItAlreadyIsIsNotAMove) {
    auto u = unit(P1, kVanillaUnit, /*at_bf=*/0);
    int moved = 0;
    auto conn = events.on_unit_moved.connect([&](const UnitMovedEvent&) { ++moved; });
    exec().moveToBattlefield(u, BattlefieldId::fromRaw(0));
    conn.disconnect();
    EXPECT_EQ(moved, 0) << "CR 421.1: a move is between two locations";
    EXPECT_EQ(s().getObject(u).moves_this_turn, 0);
}

// ─── 2.28 / 8.14: board actions need an object that is on the board ─────────

TEST_F(ExecutorReviewFixTest, OffBoard_SecondKillOfADeadTokenAnnouncesNothing) {
    auto tok = token(P1, 0);
    int deaths = 0;
    auto conn = events.on_unit_died.connect([&](const UnitDiedEvent&) { ++deaths; });
    exec().killObject(tok);
    exec().killObject(tok);       // Falling Star's second hit on the same token
    conn.disconnect();
    EXPECT_EQ(deaths, 1);
    EXPECT_EQ(s().getObject(tok).zone, ZoneType::Banishment);
}

TEST_F(ExecutorReviewFixTest, OffBoard_CardInHandIsNotKilledOrDamaged) {
    auto c = inHandCard(P1, kVanillaUnit);
    int deaths = 0;
    auto conn = events.on_unit_died.connect([&](const UnitDiedEvent&) { ++deaths; });
    exec().dealDamage(c, 5, kInvalidId);
    exec().killObject(c);
    conn.disconnect();
    EXPECT_EQ(deaths, 0);
    EXPECT_EQ(s().getObject(c).damage_marked, 0);
    EXPECT_EQ(s().getObject(c).zone, ZoneType::Hand);
    EXPECT_TRUE(listed(s().player(P1).hand, c));
    EXPECT_FALSE(listed(s().player(P1).trash, c));
}

TEST_F(ExecutorReviewFixTest, OffBoard_DeadUnitIsNotMovedBouncedBuffedOrStunned) {
    auto u = unit(P1, kVanillaUnit, /*at_bf=*/0);
    exec().killObject(u);
    ASSERT_EQ(s().getObject(u).zone, ZoneType::Trash);

    exec().moveToBattlefield(u, BattlefieldId::fromRaw(1));     // Loyal Pup / Stealthy Pursuer resolving after death
    exec().moveToBase(u);
    exec().bounceToHand(u);             // Vayne, Hunter resolving after death
    EXPECT_FALSE(exec().buffUnit(u));
    exec().stunUnit(u);
    exec().dealDamage(u, 2, kInvalidId);

    const auto& obj = s().getObject(u);
    EXPECT_EQ(obj.zone, ZoneType::Trash);
    EXPECT_FALSE(obj.location.has_value());
    EXPECT_TRUE(listed(s().player(P1).trash, u));
    EXPECT_FALSE(listed(s().player(P1).hand, u)) << "not in trash and hand at once";
    EXPECT_EQ(obj.buff_count, 0);
    EXPECT_FALSE(obj.is_stunned);
    EXPECT_EQ(obj.damage_marked, 0);
    EXPECT_EQ(obj.moves_this_turn, 0);
}

TEST_F(ExecutorReviewFixTest, OffBoard_ReadyStillWorksForALegendButNotACardInTrash) {
    auto monk = legend(P1, kBlindMonk);
    s().getObject(monk).is_exhausted = true;
    exec().readyObject(monk);
    EXPECT_FALSE(s().getObject(monk).is_exhausted) << "a legend is in play without a location";

    auto dead = inTrashCard(P1, kVanillaUnit);
    s().getObject(dead).is_exhausted = true;
    int readied = 0;
    auto conn = events.on_unit_readied.connect([&](const UnitReadiedEvent&) { ++readied; });
    exec().readyObject(dead);
    conn.disconnect();
    EXPECT_EQ(readied, 0);
}

// ─── 2.31: Deflect given "this turn" ends with the turn ─────────────────────

TEST_F(ExecutorReviewFixTest, TemporaryDeflect_ExpiresAndDoesNotAccumulateAcrossTurns) {
    auto u = unit(P1, kVanillaUnit, /*at_bf=*/0);
    ASSERT_EQ(exec().deflectCostFor(P2, u), 0);

    exec().giveTemporaryKeyword(u, Keyword::Deflect, 2);   // Jayce, Hammer in Hand
    EXPECT_EQ(exec().deflectCostFor(P2, u), 2);
    EXPECT_EQ(exec().deflectCostFor(P1, u), 0) << "only opponents pay";

    // The turn ends: the engine's expiration clears the keyword bit, and the
    // next turn begins.
    GameEngine::expireTemporaryKeywords(s().getObject(u), card_db);
    s().turn.turn_number++;
    EXPECT_EQ(exec().deflectCostFor(P2, u), 0) << "\"this turn\" is over";
    EXPECT_EQ(s().getObject(u).deflect_value, 0) << "the printed value was never touched";

    // Given again on a later turn it is 2 again, not 4.
    exec().giveTemporaryKeyword(u, Keyword::Deflect, 2);
    EXPECT_EQ(exec().deflectCostFor(P2, u), 2);
}

// ─── 8.13: a card played by an effect leaves the zone it was in ─────────────

TEST_F(ExecutorReviewFixTest, PlayIgnoringCost_RiftHeraldDeathknellTakesTheUnitOutOfHand) {
    auto herald = unit(P1, kRiftHerald, /*at_bf=*/0);
    auto in_hand = inHandCard(P1, kVanillaUnit);

    exec().killObject(herald);
    resolveChain();                         // Deathknell: play a unit from hand to base

    ASSERT_TRUE(onBoard(in_hand)) << "the Deathknell played it";
    EXPECT_TRUE(s().getObject(in_hand).isAtBase());
    EXPECT_FALSE(listed(s().player(P1).hand, in_hand)) << "on the board, so no longer in hand";
}

TEST_F(ExecutorReviewFixTest, PlayIgnoringCost_LeavesTrashBanishmentAndDeck) {
    auto from_trash = inTrashCard(P1, kVanillaUnit);
    auto from_banish = card(P1, kVanillaUnit);
    s().getObject(from_banish).zone = ZoneType::Banishment;
    s().player(P1).banishment.push_back(from_banish);
    onDeck(P1, kVanillaUnit);
    auto from_deck = onDeck(P1, kVanillaUnit);
    onDeck(P1, kVanillaUnit);

    exec().playIgnoringCost(P1, from_trash);
    exec().playIgnoringCost(P1, from_banish);
    exec().playIgnoringCost(P1, from_deck);

    EXPECT_TRUE(onBoard(from_trash));
    EXPECT_FALSE(listed(s().player(P1).trash, from_trash));
    EXPECT_TRUE(onBoard(from_banish));
    EXPECT_FALSE(listed(s().player(P1).banishment, from_banish));
    EXPECT_TRUE(onBoard(from_deck));
    EXPECT_FALSE(listed(s().player(P1).main_deck, from_deck));
    EXPECT_EQ(s().player(P1).main_deck.size(), 2u);
    EXPECT_TRUE(knowledge::check(s()).empty()) << "the deck edit went through deck_ops";
}

}  // namespace
}  // namespace test_review_fix_executor
}  // namespace riftbound::test
