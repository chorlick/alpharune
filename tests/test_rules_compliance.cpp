/// @file test_rules_compliance.cpp
/// Engine-level tests pinning behaviour to specific Core Rules, written while
/// auditing the engine against the 2026-10-09 rules (docs/rules-audit-2026-10.md).
/// Each test names the rule it enforces.

#include "cards/card_test_fixture.h"
#include "cards/card_helpers.h"
#include "engine/chain_manager.h"
#include "engine/effect_executor.h"
#include "engine/game_engine.h"

#include <gtest/gtest.h>

namespace riftbound::test {
namespace {

class RulesComplianceTest : public CardTestFixture {
protected:
    std::unique_ptr<GameEngine> engine_;
    FirstChoiceAgent a1_, a2_;
    GameState& s() { return engine_->mutableState(); }

    void makeEngine(int n_bfs = 2) {
        engine_ = std::make_unique<GameEngine>(card_db, events, card_registry);
        auto& st = s();
        st.mode = ModeOfPlay{};
        st.players[0].id = P1;
        st.players[1].id = P2;
        for (int i = 0; i < n_bfs; ++i) {
            BattlefieldState bf;
            bf.id = BattlefieldId::fromRaw(st.battlefields.size());
            st.battlefields.push_back(bf);
        }
        engine_->testHook_setAgents(&a1_, &a2_);
        st.turn.turn_player = P1;
        st.turn.phase = TurnPhase::MainPhase;
    }

    /// A legend in `p`'s Legend Zone (def_id may be kInvalidId for a blank one).
    GameObjectId setLegend(PlayerId p, CardDefId def_id = kInvalidId) {
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

    GameObjectId engineUnit(PlayerId p, int might, int at_bf = -1,
                            CardDefId def_id = kInvalidId) {
        auto id = s().createObject();
        auto& u = s().getObject(id);
        u.owner = u.controller = p;
        u.card_type = CardType::Unit;
        u.card_def_id = def_id;
        u.name = def_id != kInvalidId ? card_db.get(def_id).name : "TestUnit";
        u.base_might = u.current_might = might;
        if (at_bf >= 0) {
            u.zone = ZoneType::BattlefieldZone;
            u.location = BattlefieldLocation{BattlefieldId::fromRaw(at_bf)};
        } else {
            u.zone = ZoneType::Base;
            u.location = BaseLocation{p};
        }
        return id;
    }

    bool onBoard(GameObjectId id) { return s().getObject(id).location.has_value(); }
};

// ─── Death replacement effects ("would die ... instead") ────────────────────
// A replacement applies only under its own printed condition. The Boss (264):
// "When a buffed unit you control would die, you may pay ... exhaust me, and
// spend its buff to ... recall it instead."

constexpr CardDefId kTheBoss = 264;

TEST_F(RulesComplianceTest, DeathReplacement_TheBoss_DoesNotSaveAnUnbuffedUnit) {
    makeEngine();
    setLegend(P1, kTheBoss);
    setLegend(P2);
    auto unit = engineUnit(P1, 2, 0);
    s().getObject(unit).damage_marked = 2;      // lethal

    engine_->testHook_processLethalDamage();

    EXPECT_FALSE(onBoard(unit)) << "The Boss only saves BUFFED units";
    EXPECT_EQ(s().getObject(unit).zone, ZoneType::Trash);
}

TEST_F(RulesComplianceTest, DeathReplacement_TheBoss_SavesABuffedUnit) {
    makeEngine();
    auto boss = setLegend(P1, kTheBoss);
    setLegend(P2);
    auto unit = engineUnit(P1, 2, 0);
    s().getObject(unit).buff_count = 1;
    s().getObject(unit).recomputeMight();
    s().getObject(unit).damage_marked = 5;      // lethal
    // "you may pay [C], exhaust me, and spend its buff": a real "may" (the
    // controller says yes here) and a real payment — [C] is one Power of the
    // legend's Domains, Body or Order.
    EnginePathAgent yes(EnginePathAgent::script({1}));
    engine_->testHook_setAgents(&yes, &a2_);
    s().player(P1).rune_pool.power[static_cast<int>(Domain::Order)] = 1;

    engine_->testHook_processLethalDamage();

    EXPECT_TRUE(onBoard(unit));
    EXPECT_TRUE(s().getObject(unit).isAtBase()) << "recalled to base";
    EXPECT_EQ(s().getObject(unit).buff_count, 0) << "its buff is spent";
    EXPECT_TRUE(s().getObject(boss).is_exhausted) << "The Boss exhausts to pay";
    EXPECT_EQ(s().player(P1).rune_pool.power[static_cast<int>(Domain::Order)], 0)
        << "[C] was paid";
}

TEST_F(RulesComplianceTest, DeathReplacement_AppliesToEffectKillsToo) {
    // A unit killed by a spell or ability "would die" just the same, so the
    // replacement must be consulted on that path as well.
    makeEngine();
    setLegend(P1, kTheBoss);
    setLegend(P2);
    auto unit = engineUnit(P1, 2, 0);
    s().getObject(unit).buff_count = 1;
    s().getObject(unit).recomputeMight();
    // The Boss's replacement is a paid "may" (card text): P1 says yes and has
    // the [C]. The engine's executor is the one that can ask.
    EnginePathAgent yes(EnginePathAgent::script({1}));
    engine_->testHook_setAgents(&yes, &a2_);
    s().player(P1).rune_pool.power[static_cast<int>(Domain::Body)] = 1;

    engine_->testHook_executor().killObject(unit);

    EXPECT_TRUE(onBoard(unit)) << "saved by The Boss";
    EXPECT_TRUE(s().getObject(unit).isAtBase());
}

TEST_F(RulesComplianceTest, DeathReplacement_ExhaustedBossCannotSaveASecondUnit) {
    makeEngine();
    auto boss = setLegend(P1, kTheBoss);
    setLegend(P2);
    s().getObject(boss).is_exhausted = true;
    auto unit = engineUnit(P1, 2, 0);
    s().getObject(unit).buff_count = 1;
    s().getObject(unit).recomputeMight();
    s().getObject(unit).damage_marked = 5;

    engine_->testHook_processLethalDamage();

    EXPECT_FALSE(onBoard(unit)) << "the cost (exhaust me) can't be paid";
}

// ─── CR 467.2.c.5 — replacement effects apply to combat damage ASSIGNMENT ───

namespace {
int assignedTo(const std::vector<DamageAssignment>& da, GameObjectId id) {
    for (const auto& a : da) if (a.target_unit == id) return a.damage;
    return 0;
}
}  // namespace

TEST_F(RulesComplianceTest, CombatDamage_PreventionRaisesTheLethalAssignment) {
    // "Prevent the next 7 damage" (Ki Barrier): 467.2.c.4.a — the assigner must
    // assign enough that the unit actually takes lethal damage.
    makeEngine();
    setLegend(P1); setLegend(P2);
    auto shielded = engineUnit(P2, 3, 0);
    auto other    = engineUnit(P2, 2, 0);
    s().getObject(shielded).prevent_damage_remaining = 7;

    auto adv = engine_->testHook_advanceCombatDamage(P1, 12, {shielded, other});
    ASSERT_FALSE(adv.legal.empty());
    const auto& da = adv.legal.front().damage_assignments;
    EXPECT_EQ(assignedTo(da, shielded), 10) << "3 lethal + 7 prevented";
    EXPECT_EQ(assignedTo(da, other), 2);

    engine_->testHook_executeIntent(adv.legal.front());
    EXPECT_EQ(s().getObject(shielded).damage_marked, 3) << "7 of the 10 was prevented";
    EXPECT_EQ(s().getObject(shielded).prevent_damage_remaining, 0);
    EXPECT_EQ(s().getObject(other).damage_marked, 2);
}

TEST_F(RulesComplianceTest, CombatDamage_PreventionAbsorbsAnInsufficientAssignment) {
    makeEngine();
    setLegend(P1); setLegend(P2);
    auto shielded = engineUnit(P2, 3, 0);
    s().getObject(shielded).prevent_damage_remaining = 7;

    auto adv = engine_->testHook_advanceCombatDamage(P1, 5, {shielded});
    ASSERT_FALSE(adv.legal.empty());
    engine_->testHook_executeIntent(adv.legal.front());

    EXPECT_EQ(s().getObject(shielded).damage_marked, 0) << "all 5 prevented";
    EXPECT_EQ(s().getObject(shielded).prevent_damage_remaining, 2);
}

TEST_F(RulesComplianceTest, CombatDamage_UnitThatCannotBeDamagedIsExempt) {
    // CR 467.2.c.10: no amount is lethal for it, so it is skipped and the
    // damage goes to the units that can take it.
    makeEngine();
    setLegend(P1); setLegend(P2);
    auto immune = engineUnit(P2, 3, 0);
    auto other  = engineUnit(P2, 2, 0);
    s().getObject(immune).immune_to_damage = true;

    auto adv = engine_->testHook_advanceCombatDamage(P1, 3, {immune, other});
    ASSERT_FALSE(adv.legal.empty());
    const auto& da = adv.legal.front().damage_assignments;
    EXPECT_EQ(assignedTo(da, immune), 0);
    EXPECT_EQ(assignedTo(da, other), 3);

    engine_->testHook_executeIntent(adv.legal.front());
    EXPECT_EQ(s().getObject(immune).damage_marked, 0);
    EXPECT_EQ(s().getObject(other).damage_marked, 3);
}

TEST_F(RulesComplianceTest, CombatDamage_NextDamagePreventedIsNotDealtInCombat) {
    // "The next time that unit would be dealt damage this turn, prevent it."
    makeEngine();
    setLegend(P1); setLegend(P2);
    auto guarded = engineUnit(P2, 3, 0);
    s().getObject(guarded).prevent_next_damage_this_turn = true;

    Intent assign;
    assign.type = IntentType::AssignCombatDamage;
    assign.player = P1;
    assign.damage_assignments = {{guarded, 4}};
    engine_->testHook_executeIntent(assign);

    EXPECT_EQ(s().getObject(guarded).damage_marked, 0);
    EXPECT_FALSE(s().getObject(guarded).prevent_next_damage_this_turn) << "consumed";
}

TEST_F(RulesComplianceTest, CombatDamage_DoubledDamageNeedsHalfTheAssignment) {
    // Damage to this unit is doubled this turn: 467.2.c.4.a — assign the
    // minimum that results in lethal damage.
    makeEngine();
    setLegend(P1); setLegend(P2);
    auto marked = engineUnit(P2, 4, 0);
    auto other  = engineUnit(P2, 2, 0);
    s().getObject(marked).damage_doubled_this_turn = true;

    auto adv = engine_->testHook_advanceCombatDamage(P1, 4, {marked, other});
    ASSERT_FALSE(adv.legal.empty());
    const auto& da = adv.legal.front().damage_assignments;
    EXPECT_EQ(assignedTo(da, marked), 2) << "2 assigned is 4 dealt";
    EXPECT_EQ(assignedTo(da, other), 2);

    engine_->testHook_executeIntent(adv.legal.front());
    EXPECT_EQ(s().getObject(marked).damage_marked, 4);
}

TEST_F(RulesComplianceTest, CombatDamage_ManyDefendersEnumeratesQuicklyAndBounded) {
    // 16 one-Might defenders and 3 damage: C(16,3) = 560 distinct outcomes.
    // Enumeration must not permute all 16 units (16! orderings). Testing
    // phase B7 (by design): the list used to be capped at 64 of the 560, the
    // rest withheld; past the cap the assignment is now made one unit at a
    // time (GameEngine::assignCombatDamage), so the list is not built at all.
    makeEngine();
    setLegend(P1); setLegend(P2);
    std::vector<GameObjectId> defenders;
    for (int i = 0; i < 16; ++i) defenders.push_back(engineUnit(P2, 1, 0));

    auto adv = engine_->testHook_advanceCombatDamage(P1, 3, defenders);

    EXPECT_EQ(adv.kind, GameEngine::CombatDamageAdvance::Kind::StepByStep);
    EXPECT_TRUE(adv.legal.empty());
}

TEST_F(RulesComplianceTest, CombatDamage_OffersEachDistinctChoiceOfWhoDies) {
    // 3 defenders with 2 Might each and 4 damage: two die. The three
    // different pairs are three different outcomes.
    makeEngine();
    setLegend(P1); setLegend(P2);
    auto a = engineUnit(P2, 2, 0), b = engineUnit(P2, 2, 0), c = engineUnit(P2, 2, 0);

    auto adv = engine_->testHook_advanceCombatDamage(P1, 4, {a, b, c});

    EXPECT_EQ(adv.legal.size(), 3u);
}

// ─── CR 190.3 / 323.7-323.10 — Contested status ────────────────────────────
// These run a Cleanup in a Closed State so the staged Showdown / Combat is
// recorded but not started (323.11 / 323.12 need a Neutral Open State).

TEST_F(RulesComplianceTest, Contested_RemovedWhenContesterHasNoUnitsThere) {
    // P1 moved in and contested P2's battlefield, then P1's unit left (died /
    // bounced) before the showdown opened. CR 323.10: Contested is removed.
    makeEngine();
    setLegend(P1); setLegend(P2);
    s().turn.oc_state = OpenClosedState::Closed;
    engineUnit(P2, 3, 0);
    auto& bf = s().battlefields[0];
    bf.controller = P2;
    bf.is_contested = true;
    bf.contested_by = P1;
    bf.showdown_staged = true;

    engine_->testHook_cleanup();

    EXPECT_FALSE(bf.is_contested);
    EXPECT_FALSE(bf.showdown_staged) << "no showdown for a contest that no longer exists";
    EXPECT_FALSE(bf.combat_staged);
    ASSERT_TRUE(bf.controller.has_value());
    EXPECT_EQ(*bf.controller, P2);
}

TEST_F(RulesComplianceTest, Contested_AppliedWhenAUnitArrivesByAnEffect) {
    // CR 190.3.a: a Unit that "otherwise becomes present" at a battlefield its
    // controller doesn't control applies Contested — not only a standard move.
    makeEngine();
    setLegend(P1); setLegend(P2);
    s().turn.oc_state = OpenClosedState::Closed;
    engineUnit(P2, 3, 0);
    auto& bf = s().battlefields[0];
    bf.controller = P2;
    engineUnit(P1, 2, 0);            // put there by an effect: no move intent ran

    engine_->testHook_cleanup();

    EXPECT_TRUE(bf.is_contested);
    EXPECT_EQ(bf.contested_by, P1);
    EXPECT_TRUE(bf.combat_staged) << "CR 323.8: opposing units -> Combat staged";
}

TEST_F(RulesComplianceTest, Contested_NotAppliedByTheControllersOwnUnits) {
    makeEngine();
    setLegend(P1); setLegend(P2);
    s().turn.oc_state = OpenClosedState::Closed;
    engineUnit(P2, 3, 0);
    auto& bf = s().battlefields[0];
    bf.controller = P2;

    engine_->testHook_cleanup();

    EXPECT_FALSE(bf.is_contested);
    EXPECT_FALSE(bf.showdown_staged);
}

TEST_F(RulesComplianceTest, Contested_ReappliedByRemainingNonControllerUnits) {
    // CR 323.10.a: Contested (by P1) is removed because P1 has no units, but
    // P2 has units at a battlefield P2 does not control -> P2 applies it.
    makeEngine();
    setLegend(P1); setLegend(P2);
    s().turn.oc_state = OpenClosedState::Closed;
    engineUnit(P2, 3, 0);
    auto& bf = s().battlefields[0];
    bf.controller = std::nullopt;     // uncontrolled
    bf.is_contested = true;
    bf.contested_by = P1;

    engine_->testHook_cleanup();

    EXPECT_TRUE(bf.is_contested);
    EXPECT_EQ(bf.contested_by, P2);
    EXPECT_TRUE(bf.showdown_staged) << "CR 323.7";
    EXPECT_FALSE(bf.combat_staged);
}

// ─── CR 164.2 — paying Energy and Power with runes ─────────────────────────
// A Basic Rune has "[E]: Add [1]" and "Recycle this: Add [C]". Recycling does
// not need the rune to be exhausted, and a rune exhausted for Energy can still
// be recycled for Power.

namespace {
constexpr CardDefId kPunchFirst = 419;   // spell, [1] + 2 Body power
}  // namespace

class CostPaymentRulesTest : public RulesComplianceTest {
protected:
    GameObjectId rune(PlayerId p, Domain d, bool exhausted = false) {
        auto id = s().createObject();
        auto& r = s().getObject(id);
        r.owner = r.controller = p;
        r.card_type = CardType::Rune;
        r.name = "Rune";
        r.domains = {d};
        r.zone = ZoneType::Base;
        r.location = BaseLocation{p};
        r.is_exhausted = exhausted;
        return id;
    }
    GameObjectId cardInHand(PlayerId p, CardDefId def_id) {
        auto id = s().createObject();
        auto& c = s().getObject(id);
        const auto& def = card_db.get(def_id);
        c.owner = c.controller = p;
        c.card_def_id = def_id;
        c.card_type = def.card_type;
        c.name = def.name;
        c.domains = def.domains;
        c.keywords = def.keywords;
        c.zone = ZoneType::Hand;
        s().player(p).hand.push_back(id);
        return id;
    }
    int runesInBase(PlayerId p) {
        int n = 0;
        for (auto& [id, o] : s().objects)
            if (o.isRune() && o.controller == p && o.location.has_value()) ++n;
        return n;
    }
};

TEST_F(CostPaymentRulesTest, BaseCost_TwoReadyRunesPayOneEnergyAndTwoPower) {
    // [1] + [O][O] with exactly two ready Body runes: exhaust one for the
    // Energy, then recycle both for the Power.
    makeEngine();
    setLegend(P1); setLegend(P2);
    rune(P1, Domain::Body);
    rune(P1, Domain::Body);
    auto spell = cardInHand(P1, kPunchFirst);

    EXPECT_TRUE(engine_->testHook_canAfford(P1, spell));
    EXPECT_TRUE(engine_->testHook_payCardCost(P1, spell));
    EXPECT_EQ(runesInBase(P1), 0) << "both runes recycled for Power";
}

TEST_F(CostPaymentRulesTest, BaseCost_NotAffordableWithoutEnoughMatchingRunes) {
    makeEngine();
    setLegend(P1); setLegend(P2);
    rune(P1, Domain::Body);
    rune(P1, Domain::Fury);          // wrong domain for the second Power
    auto spell = cardInHand(P1, kPunchFirst);

    EXPECT_FALSE(engine_->testHook_canAfford(P1, spell));
}

TEST_F(CostPaymentRulesTest, BaseCost_SpendsFloatingPowerBeforeRecyclingRunes) {
    makeEngine();
    setLegend(P1); setLegend(P2);
    rune(P1, Domain::Fury);          // pays the Energy only
    s().player(P1).rune_pool.power[static_cast<int>(Domain::Body)] = 2;
    auto spell = cardInHand(P1, kPunchFirst);

    ASSERT_TRUE(engine_->testHook_canAfford(P1, spell));
    EXPECT_TRUE(engine_->testHook_payCardCost(P1, spell));
    EXPECT_EQ(s().player(P1).rune_pool.power[static_cast<int>(Domain::Body)], 0);
    EXPECT_EQ(runesInBase(P1), 1) << "no rune recycled: the floating Power paid";
}

TEST_F(CostPaymentRulesTest, AdditionalCost_AnyDomainPowerMayMixDomains) {
    // [A][A] is two Power of any Domain; they need not match each other.
    makeEngine();
    setLegend(P1); setLegend(P2);
    rune(P1, Domain::Fury);
    rune(P1, Domain::Calm);

    EXPECT_TRUE(engine_->testHook_canPayAdditionalCost(P1, 0, 2, Domain::Count));
    EXPECT_TRUE(engine_->testHook_payAdditionalCost(P1, 0, 2, Domain::Count));
    EXPECT_EQ(runesInBase(P1), 0);
}

TEST_F(CostPaymentRulesTest, AdditionalCost_OneRunePaysEnergyAndPower) {
    makeEngine();
    setLegend(P1); setLegend(P2);
    rune(P1, Domain::Fury);

    EXPECT_TRUE(engine_->testHook_canPayAdditionalCost(P1, 1, 1, Domain::Fury));
    EXPECT_TRUE(engine_->testHook_payAdditionalCost(P1, 1, 1, Domain::Fury));
    EXPECT_EQ(runesInBase(P1), 0);
}

TEST_F(CostPaymentRulesTest, AdditionalCost_ExhaustedRuneCannotPayEnergy) {
    makeEngine();
    setLegend(P1); setLegend(P2);
    rune(P1, Domain::Fury, /*exhausted=*/true);

    EXPECT_FALSE(engine_->testHook_canPayAdditionalCost(P1, 1, 0, Domain::Fury));
    EXPECT_TRUE(engine_->testHook_canPayAdditionalCost(P1, 0, 1, Domain::Fury))
        << "but it can still be recycled for Power";
}

TEST_F(CostPaymentRulesTest, AdditionalCost_FailedPaymentChangesNothing) {
    makeEngine();
    setLegend(P1); setLegend(P2);
    auto r = rune(P1, Domain::Fury);

    EXPECT_FALSE(engine_->testHook_payAdditionalCost(P1, 2, 0, Domain::Fury));
    EXPECT_FALSE(s().getObject(r).is_exhausted) << "nothing is paid when it can't be paid in full";
}

// ─── CR 809 — Deflect ──────────────────────────────────────────────────────
// "Spells and abilities an opponent controls that target [me] cost an amount
// of Power equal to [Deflect Value] more ... The Power used to pay this cost
// may always be of any Domain."

class DeflectRulesTest : public CostPaymentRulesTest {
protected:
    GameObjectId deflector(PlayerId p, int value, int at_bf = 0) {
        auto id = engineUnit(p, 3, at_bf);
        auto& u = s().getObject(id);
        u.keywords.set(Keyword::Deflect);
        u.deflect_value = value;
        return id;
    }
};

TEST_F(DeflectRulesTest, CostIsTheDeflectValueForOpponentsOnly) {
    makeEngine();
    setLegend(P1); setLegend(P2);
    auto unit = deflector(P2, 2);
    EXPECT_EQ(deflectCostFor(s(), P1, unit), 2);
    EXPECT_EQ(deflectCostFor(s(), P2, unit), 0) << "its controller pays nothing";
}

TEST_F(DeflectRulesTest, OmittedValueIsOne) {
    // CR 809.1.b.3
    makeEngine();
    setLegend(P1); setLegend(P2);
    auto unit = deflector(P2, 0);
    EXPECT_EQ(deflectCostFor(s(), P1, unit), 1);
}

TEST_F(DeflectRulesTest, GrantedDeflectSumsWithPrinted) {
    // CR 809.2
    makeEngine();
    setLegend(P1); setLegend(P2);
    auto unit = deflector(P2, 1);
    GameObject::AuraEffect ae;
    ae.source = unit;
    ae.keyword = Keyword::Deflect;
    ae.keyword_value = 2;
    s().getObject(unit).aura_effects.push_back(ae);
    EXPECT_EQ(deflectCostFor(s(), P1, unit), 3);
}

TEST_F(DeflectRulesTest, ChoosingAtPlayTimePaysAnyDomainPower) {
    // A spell with a play-time target going on the chain pays the cost with a
    // rune of an unrelated Domain (CR 809.1.c.1).
    makeEngine();
    setLegend(P1); setLegend(P2);
    auto unit = deflector(P2, 1);
    rune(P1, Domain::Order);
    auto spell = cardInHand(P1, kPunchFirst);
    s().player(P1).hand.clear();

    ChainManager cm(s(), events, card_db);
    EffectExecutor exec(s(), events, card_db, &card_registry);
    cm.setEffectExecutor(&exec);
    cm.addSpell(spell, P1, {unit});

    EXPECT_EQ(runesInBase(P1), 0) << "the rune was recycled for the Deflect cost";
    ASSERT_FALSE(s().chain.items.empty());
    EXPECT_EQ(s().chain.items.back().chosen_objects, std::vector<GameObjectId>{unit});
}

TEST_F(DeflectRulesTest, UnaffordableTargetIsNotALegalTarget) {
    // No Power available: the default target enumeration leaves it out.
    makeEngine();
    setLegend(P1); setLegend(P2);
    auto guarded = deflector(P2, 1);
    auto plain   = engineUnit(P2, 3, 0);
    const Card* punch = card_registry.get(kPunchFirst);

    auto legal = punch->enumerateLegalTargets(s(), P1);
    EXPECT_EQ(std::count(legal.begin(), legal.end(), plain), 1);
    EXPECT_EQ(std::count(legal.begin(), legal.end(), guarded), 0);

    rune(P1, Domain::Fury);
    legal = punch->enumerateLegalTargets(s(), P1);
    EXPECT_EQ(std::count(legal.begin(), legal.end(), guarded), 1) << "payable now";
}

// ─── Scoring by an effect ──────────────────────────────────────────────────

TEST_F(RulesComplianceTest, EffectScoring_RespectsCantGainPoints) {
    makeEngine();
    setLegend(P1); setLegend(P2);
    EffectExecutor exec(s(), events, card_db, &card_registry);
    s().player(P1).cannot_gain_points = true;
    EXPECT_EQ(exec.scorePoints(P1, 1), 0);
    EXPECT_EQ(s().player(P1).score, 0);
}

TEST_F(RulesComplianceTest, EffectScoring_WinsImmediatelyAtVictoryScore) {
    // CR 466.1.a.1: points from effects have no Winning Point restriction.
    makeEngine();
    setLegend(P1); setLegend(P2);
    EffectExecutor exec(s(), events, card_db, &card_registry);
    s().player(P1).score = s().mode.victory_score - 1;
    EXPECT_EQ(exec.scorePoints(P1, 1), 1);
    EXPECT_TRUE(s().game_over);
    EXPECT_EQ(s().winner, P1);
}

// ─── CR 185.1 — tokens are an intrinsic category ───────────────────────────

TEST_F(RulesComplianceTest, Token_CopyOfAChampionIsAChampionAndStillAToken) {
    constexpr CardDefId kAhriAlluring = 66;
    makeEngine();
    setLegend(P1); setLegend(P2);
    EffectExecutor exec(s(), events, card_db, &card_registry);
    auto champion = engineUnit(P1, 4, 0, kAhriAlluring);
    s().getObject(champion).super_type = card_db.get(kAhriAlluring).super_type;
    ASSERT_EQ(s().getObject(champion).super_type, SuperType::Champion);

    auto token = exec.createToken(P1, CardType::Unit, "Reflection", 0, {}, KeywordSet{},
                                  LocationId{BaseLocation{P1}}, false);
    ASSERT_TRUE(s().getObject(token).isToken());
    exec.copyUnit(token, champion);

    EXPECT_TRUE(s().getObject(token).isToken()) << "a token never stops being a token";
    EXPECT_EQ(s().getObject(token).super_type, SuperType::Champion)
        << "the copy has the copied supertype";
    EXPECT_FALSE(s().getObject(champion).isToken()) << "a card can never become a token";
}

// ─── CR 383.2.c.3.b — "When I die" uses information from before the death ──

TEST_F(RulesComplianceTest, LastKnown_CapturedBeforeTheUnitDies) {
    makeEngine();
    setLegend(P1); setLegend(P2);
    auto unit = engineUnit(P1, 2, 0);
    {
        auto& u = s().getObject(unit);
        u.buff_count = 1;
        u.empowered = 1;
        u.combat_designation = CombatDesignation::Attacker;
        u.recomputeMight();
        u.damage_marked = 9;
    }
    s().battlefields[0].combat_in_progress = true;

    engine_->testHook_processLethalDamage();

    const auto& lk = s().getObject(unit).last_known;
    ASSERT_TRUE(lk.valid);
    EXPECT_EQ(lk.empowered, 1);
    EXPECT_EQ(lk.buff_count, 1);
    EXPECT_EQ(lk.combat_designation, CombatDesignation::Attacker);
    EXPECT_TRUE(lk.in_combat);
    EXPECT_EQ(lk.might, 3);
}

// ─── CR 426 / 391.3.a — countering ─────────────────────────────────────────

TEST_F(RulesComplianceTest, Counter_FlowSpellIsBanishedNotTrashed) {
    // A spell played for its Flow cost carries "then banish it": if it would
    // leave the chain other than by resolving, it is banished instead.
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto flow_spell = pushSpellOnChain(P2);
    state.getObject(flow_spell).banish_on_leave_chain = true;
    auto counter = addToHand(P1, kInvalidId);
    CardContext ctx{state, events, exec, P1, counter};

    counterChosenItem(ctx, state.chain.items.back().id);

    EXPECT_TRUE(state.chain.items.empty());
    EXPECT_EQ(state.getObject(flow_spell).zone, ZoneType::Banishment);
    EXPECT_FALSE(inTrash(P2, flow_spell));
}

TEST_F(RulesComplianceTest, Counter_OrdinarySpellGoesToTrash) {
    // CR 426.1.a.1
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto spell = pushSpellOnChain(P2);
    auto counter = addToHand(P1, kInvalidId);
    CardContext ctx{state, events, exec, P1, counter};

    counterChosenItem(ctx, state.chain.items.back().id);

    EXPECT_TRUE(inTrash(P2, spell));
}

TEST_F(RulesComplianceTest, Counter_DoesNotUncountThePlayedCard) {
    // CR 420.4.b: non-triggered checks count FINALIZED cards; a countered
    // spell was finalized, so "cards played this turn" keeps it.
    EffectExecutor exec(state, events, card_db, &card_registry);
    pushSpellOnChain(P2);
    state.player(P2).cards_played_this_turn = 2;
    auto counter = addToHand(P1, kInvalidId);
    CardContext ctx{state, events, exec, P1, counter};

    counterChosenItem(ctx, state.chain.items.back().id);

    EXPECT_EQ(state.player(P2).cards_played_this_turn, 2);
}

// ─── CR 420.4.a — "when you play a spell" triggers on resolution ───────────

TEST_F(RulesComplianceTest, PlayTrigger_SpellTriggersFireOnResolutionNotOnPlay) {
    constexpr CardDefId kViktorInnovator = 117;   // "When you play a card on an opponent's turn ..."
    constexpr CardDefId kHextechRay = 9;
    // (WhenYouPlayACard since finding F1: for a spell it fires as the spell
    // resolves, like WhenYouPlayASpell — and not for a token.)
    ASSERT_TRUE(card_registry.get(kViktorInnovator)->firesOn(TriggerType::WhenYouPlayACard));
    makeEngine();
    setLegend(P1); setLegend(P2);
    auto watcher = engineUnit(P1, 2, -1, kViktorInnovator);
    auto spell = s().createObject();
    {
        auto& sp = s().getObject(spell);
        sp.owner = sp.controller = P1;
        sp.card_type = CardType::Spell;
        sp.card_def_id = kHextechRay;
        sp.name = "Hextech Ray";
        sp.zone = ZoneType::Chain;
    }
    auto triggersFrom = [&](GameObjectId src) {
        int n = 0;
        for (const auto& it : s().chain.items) if (it.source == src && it.is_ability) ++n;
        return n;
    };

    s().turn.turn_player = P2;   // "... on an opponent's turn": part of the condition (CR 383.2.a.1)
    events.emit(CardPlayedEvent{spell, P1, CardType::Spell, 1, 1});
    EXPECT_EQ(triggersFrom(watcher), 0) << "not yet: the spell has only been put on the chain";

    events.emit(SpellResolvedEvent{spell, P1});
    EXPECT_EQ(triggersFrom(watcher), 1) << "fires once the spell has resolved";
}

TEST_F(RulesComplianceTest, PlayTrigger_CounteredSpellNeverTriggers) {
    // CR 420.4.a.1 / 426.1.b: a countered spell produces no SpellResolved.
    constexpr CardDefId kViktorInnovator = 117;
    makeEngine();
    setLegend(P1); setLegend(P2);
    auto watcher = engineUnit(P1, 2, -1, kViktorInnovator);
    auto spell = s().createObject();
    {
        auto& sp = s().getObject(spell);
        sp.owner = sp.controller = P1;
        sp.card_type = CardType::Spell;
        sp.name = "Some Spell";
        sp.zone = ZoneType::Chain;
    }
    events.emit(CardPlayedEvent{spell, P1, CardType::Spell, 1, 1});
    // ... it is countered: nothing else is emitted for it.
    int n = 0;
    for (const auto& it : s().chain.items) if (it.source == watcher) ++n;
    EXPECT_EQ(n, 0);
}

// ─── CR 356.1.b.2 / 445.2 — effect-driven plays that still owe a cost ──────

TEST_F(CostPaymentRulesTest, EffectPlay_FullCostIsChargedThroughTheNormalPath) {
    // "Play a unit ... (You must pay its costs.)": the engine's cost path runs.
    makeEngine();
    setLegend(P1); setLegend(P2);
    rune(P1, Domain::Body);
    rune(P1, Domain::Body);
    auto spell = cardInHand(P1, kPunchFirst);      // [1] + 2 Body power
    s().player(P1).hand.clear();
    engineUnit(P1, 2, 0);   // its target (CR 355.8: no target, no play — batch 3)
    auto& exec = engine_->testHook_executor();

    ASSERT_TRUE(exec.canPlayPayingCost(P1, spell));
    EXPECT_TRUE(exec.playPayingCost(P1, spell));
    EXPECT_EQ(runesInBase(P1), 0) << "both runes recycled for the Power";
}

TEST_F(CostPaymentRulesTest, EffectPlay_IgnoringEnergyStillChargesPower) {
    makeEngine();
    setLegend(P1); setLegend(P2);
    rune(P1, Domain::Body, /*exhausted=*/true);
    rune(P1, Domain::Body, /*exhausted=*/true);    // no Energy available at all
    auto spell = cardInHand(P1, kPunchFirst);
    s().player(P1).hand.clear();
    engineUnit(P1, 2, 0);   // its target (CR 355.8: no target, no play — batch 3)
    auto& exec = engine_->testHook_executor();

    EXPECT_FALSE(exec.canPlayPayingCost(P1, spell)) << "the [1] can't be paid";
    const EffectPlayCost power_only{.ignore_energy = true};
    ASSERT_TRUE(exec.canPlayPayingCost(P1, spell, power_only));
    EXPECT_TRUE(exec.playPayingCost(P1, spell, std::nullopt, power_only));
    EXPECT_EQ(runesInBase(P1), 0);
}

TEST_F(CostPaymentRulesTest, EffectPlay_NotPlayedWhenTheCostCannotBePaid) {
    makeEngine();
    setLegend(P1); setLegend(P2);
    auto spell = cardInHand(P1, kPunchFirst);
    s().player(P1).hand.clear();
    auto& exec = engine_->testHook_executor();

    EXPECT_FALSE(exec.canPlayPayingCost(P1, spell));
    EXPECT_FALSE(exec.playPayingCost(P1, spell));
    EXPECT_TRUE(s().chain.items.empty()) << "nothing was played";
}

TEST_F(CostPaymentRulesTest, EffectPlay_ASpellGoesOnTheChainToResolve) {
    makeEngine();
    setLegend(P1); setLegend(P2);
    auto spell = cardInHand(P1, kPunchFirst);
    s().player(P1).hand.clear();
    auto& exec = engine_->testHook_executor();

    exec.playIgnoringCost(P1, spell);

    ASSERT_EQ(s().chain.items.size(), 1u) << "a spell played by an effect is still a spell";
    EXPECT_TRUE(s().chain.items.back().is_spell);
    EXPECT_EQ(s().chain.items.back().source, spell);
}

// ─── "[A] more" costs are any-domain and come on top of the card's own ─────

TEST_F(CostPaymentRulesTest, SpellTax_AnyDomainPowerIsOwedOnTop) {
    // Helm of Suppression style: this player's spells cost [A] more.
    makeEngine();
    setLegend(P1); setLegend(P2);
    rune(P1, Domain::Body);
    rune(P1, Domain::Body);
    auto spell = cardInHand(P1, kPunchFirst);      // needs both Body runes
    s().player(P1).spell_tax_power = 1;

    EXPECT_FALSE(engine_->testHook_canAfford(P1, spell)) << "no rune left for the [A]";

    rune(P1, Domain::Order);                        // any domain will do
    ASSERT_TRUE(engine_->testHook_canAfford(P1, spell));
    EXPECT_TRUE(engine_->testHook_payCardCost(P1, spell));
    EXPECT_EQ(runesInBase(P1), 0) << "the Order rune paid the [A]";
}

// ─── CR 355 — targets are chosen when the spell is finalized ───────────────

namespace {
// Records the order of decisions and picks a given object when offered.
class TargetingAgent : public AgentInterface {
public:
    GameObjectId want = kInvalidId;
    std::vector<std::string> log;
    Intent selectAction(const GameState&, const std::vector<Intent>& legal) override {
        if (legal.empty()) return Intent{};
        bool object_choice = false;
        for (const auto& i : legal)
            if (i.type == IntentType::MakeChoice && !i.chosen_objects.empty()) object_choice = true;
        if (object_choice) {
            log.push_back("target");
            for (const auto& i : legal)
                if (!i.chosen_objects.empty() && i.chosen_objects.front() == want) return i;
            return legal.front();
        }
        for (const auto& i : legal)
            if (i.type == IntentType::PassPriority) { log.push_back("pass"); return i; }
        log.push_back("other");
        return legal.front();
    }
};
}  // namespace

TEST_F(CostPaymentRulesTest, Targeting_ChosenAtFinalizationBeforeAnyoneGetsPriority) {
    makeEngine();
    setLegend(P1); setLegend(P2);
    TargetingAgent p1, p2;
    engine_->testHook_setAgents(&p1, &p2);
    s().turn.ns_state = NeutralShowdownState::Neutral;
    s().turn.oc_state = OpenClosedState::Open;
    auto first  = engineUnit(P1, 2);
    auto second = engineUnit(P1, 2);
    p1.want = second;
    // Floating resources so no rune-choice decisions get in the way.
    s().player(P1).rune_pool.energy = 1;
    s().player(P1).rune_pool.power[static_cast<int>(Domain::Body)] = 2;
    auto spell = cardInHand(P1, kPunchFirst);     // "Give a unit +5 [M] this turn."

    Intent play;
    play.type = IntentType::PlayCard;
    play.player = P1;
    play.card = spell;
    engine_->testHook_executeIntent(play); settleLikeTheEngine(*engine_);

    ASSERT_FALSE(p1.log.empty());
    EXPECT_EQ(p1.log.front(), "target") << "the target is chosen before priority passes";
    EXPECT_EQ(std::count(p1.log.begin(), p1.log.end(), "target"), 1)
        << "and it is not asked for again as the spell resolves";
    EXPECT_EQ(s().getObject(second).temp_might_bonus, 5);
    EXPECT_EQ(s().getObject(first).temp_might_bonus, 0);
}

TEST_F(CostPaymentRulesTest, Targeting_MistargetedSpellDoesNothing) {
    // The target chosen at finalization leaves the board before resolution:
    // the instruction is ignored (CR 359.3.e.6) — no new target is picked.
    makeEngine();
    setLegend(P1); setLegend(P2);
    TargetingAgent p1;
    // P2 "responds" by doing nothing; the test removes the target mid-chain.
    class RemoveTargetAgent : public AgentInterface {
    public:
        GameState* st = nullptr; GameObjectId victim = kInvalidId; bool done = false;
        Intent selectAction(const GameState&, const std::vector<Intent>& legal) override {
            if (!done && st) {            // first time P2 has priority
                done = true;
                auto& u = st->getObject(victim);
                u.location = std::nullopt;
                u.zone = ZoneType::Trash;
            }
            for (const auto& i : legal) if (i.type == IntentType::PassPriority) return i;
            return legal.empty() ? Intent{} : legal.front();
        }
    } p2;
    engine_->testHook_setAgents(&p1, &p2);
    s().turn.ns_state = NeutralShowdownState::Neutral;
    s().turn.oc_state = OpenClosedState::Open;
    auto target = engineUnit(P1, 2);
    auto other  = engineUnit(P1, 2);
    p1.want = target;
    p2.st = &s();
    p2.victim = target;
    s().player(P1).rune_pool.energy = 1;
    s().player(P1).rune_pool.power[static_cast<int>(Domain::Body)] = 2;
    auto spell = cardInHand(P1, kPunchFirst);

    Intent play;
    play.type = IntentType::PlayCard;
    play.player = P1;
    play.card = spell;
    engine_->testHook_executeIntent(play); settleLikeTheEngine(*engine_);

    EXPECT_EQ(std::count(p1.log.begin(), p1.log.end(), "target"), 1) << "no re-targeting";
    EXPECT_EQ(s().getObject(other).temp_might_bonus, 0) << "the buff does not jump to another unit";
}

// ─── Triggered abilities decide at finalization (CR 383.3) ──────────────────
// A triggered ability's "you may", its "[cost] to [effect]" payment and its
// targets are settled when it is put on the chain — before any player gets
// priority to respond — and are not asked again as it resolves.

namespace {
constexpr CardDefId kKennenKeeper   = 922;  // "When you play me, you may pay [2] to stun a unit."
constexpr CardDefId kFieldMusicians = 813;  // "When you play me, give a unit +3 [M] this turn."

// Answers "you may" with `say_yes`, picks `want` when offered objects, passes otherwise.
class TriggerChoiceAgent : public AgentInterface {
public:
    bool say_yes = true;
    GameObjectId want = kInvalidId;
    int may_asked = 0, target_asked = 0;
    Intent selectAction(const GameState&, const std::vector<Intent>& legal) override {
        if (legal.empty()) return Intent{};
        const bool choice = legal.front().type == IntentType::MakeChoice;
        if (choice && legal.front().chosen_value.has_value()) {
            ++may_asked;
            for (const auto& i : legal)
                if (i.chosen_value == (say_yes ? 1 : 0)) return i;
        }
        if (choice && !legal.front().chosen_objects.empty()) {
            ++target_asked;
            for (const auto& i : legal)
                if (i.chosen_objects.front() == want) return i;
        }
        for (const auto& i : legal) if (i.type == IntentType::PassPriority) return i;
        return legal.front();
    }
};

// Runs `on_first_priority` the first time it is asked for anything, then passes.
class ObservingAgent : public AgentInterface {
public:
    std::function<void()> on_first_priority;
    bool seen = false;
    Intent selectAction(const GameState&, const std::vector<Intent>& legal) override {
        if (!seen) { seen = true; if (on_first_priority) on_first_priority(); }
        for (const auto& i : legal) if (i.type == IntentType::PassPriority) return i;
        return legal.empty() ? Intent{} : legal.front();
    }
};
}  // namespace

class TriggerFinalizationTest : public CostPaymentRulesTest {
protected:
    TriggerChoiceAgent p1_;
    ObservingAgent p2_;
    void setUpBoard() {
        makeEngine();
        setLegend(P1); setLegend(P2);
        engine_->testHook_setAgents(&p1_, &p2_);
        s().turn.ns_state = NeutralShowdownState::Neutral;
        s().turn.oc_state = OpenClosedState::Open;
    }
    void playUnitFromHand(CardDefId def, int energy) {
        s().player(P1).rune_pool.energy = energy;
        auto card = cardInHand(P1, def);
        Intent play;
        play.type = IntentType::PlayCard;
        play.player = P1;
        play.card = card;
        play.play_location = LocationId{BaseLocation{P1}};
        engine_->testHook_executeIntent(play); settleLikeTheEngine(*engine_);
    }
    int readyRunes(PlayerId p) {
        int n = 0;
        for (auto& [id, o] : s().objects)
            if (o.isRune() && o.controller == p && o.location.has_value() && !o.is_exhausted) ++n;
        return n;
    }
};

TEST_F(TriggerFinalizationTest, MayCostAndTargetAreSettledBeforeTheOpponentCanRespond) {
    setUpBoard();
    auto victim = engineUnit(P2, 3);
    p1_.want = victim;
    rune(P1, Domain::Order); rune(P1, Domain::Order);
    int ready_at_response = -1; bool stunned_at_response = true; int may_at_response = -1;
    p2_.on_first_priority = [&] {
        ready_at_response = readyRunes(P1);
        stunned_at_response = s().getObject(victim).is_stunned;
        may_at_response = p1_.may_asked;
    };
    playUnitFromHand(kKennenKeeper, 3);

    ASSERT_TRUE(p2_.seen);
    EXPECT_EQ(may_at_response, 1) << "the \"you may\" is answered before the opponent has priority";
    EXPECT_EQ(ready_at_response, 0) << "and the [2] is already paid";
    EXPECT_FALSE(stunned_at_response) << "but the effect waits for resolution";
    EXPECT_TRUE(s().getObject(victim).is_stunned);
    EXPECT_EQ(p1_.may_asked, 1);
    EXPECT_EQ(p1_.target_asked, 1) << "nothing is asked again as the ability resolves";
    EXPECT_EQ(readyRunes(P1), 0) << "and the cost is charged once";
}

TEST_F(TriggerFinalizationTest, DecliningCostsNothingAndDoesNothing) {
    setUpBoard();
    auto victim = engineUnit(P2, 3);
    p1_.say_yes = false;
    rune(P1, Domain::Order); rune(P1, Domain::Order);
    playUnitFromHand(kKennenKeeper, 3);

    EXPECT_EQ(p1_.may_asked, 1);
    EXPECT_EQ(p1_.target_asked, 0);
    EXPECT_EQ(readyRunes(P1), 2);
    EXPECT_FALSE(s().getObject(victim).is_stunned);
}

TEST_F(TriggerFinalizationTest, TargetThatLeavesBeforeResolutionIsAMistarget) {
    // The unit chosen at finalization is gone when the ability resolves: the
    // ability does nothing. It does not pick another unit (CR 359.3.e.6).
    setUpBoard();
    auto chosen = engineUnit(P1, 2);
    auto other  = engineUnit(P1, 2);
    p1_.want = chosen;
    int asked_at_response = -1;
    p2_.on_first_priority = [&] {
        asked_at_response = p1_.target_asked;
        auto& u = s().getObject(chosen);
        u.location = std::nullopt;
        u.zone = ZoneType::Trash;
    };
    playUnitFromHand(kFieldMusicians, 4);

    ASSERT_TRUE(p2_.seen);
    EXPECT_EQ(asked_at_response, 1) << "the target is chosen before the opponent has priority";
    EXPECT_EQ(p1_.target_asked, 1) << "no re-targeting at resolution";
    EXPECT_EQ(s().getObject(other).temp_might_bonus, 0);
    EXPECT_EQ(s().getObject(chosen).temp_might_bonus, 0);
}

// ─── "At the end of this turn" belongs to the turn, not the turn player ─────
TEST_F(RulesComplianceTest, DelayedEndOfTurnAbilityFiresForTheNonTurnPlayer) {
    // Targon's Peak (284): "When you conquer here, ready up to 2 runes at the
    // end of this turn." P2 sets it up during P1's turn.
    constexpr CardDefId kTargonsPeak = 284;
    makeEngine();
    setLegend(P1); setLegend(P2);
    auto bf_card = s().createObject();
    {
        auto& b = s().getObject(bf_card);
        b.card_type = CardType::Battlefield;
        b.card_def_id = kTargonsPeak;
        b.name = "Targon's Peak";
        b.owner = b.controller = P2;
    }
    s().battlefields[0].card_object_id = bf_card;
    DelayedAbility da;
    da.source = bf_card;
    da.card_def_id = kTargonsPeak;
    da.controller = P2;
    da.trigger = TriggerType::AtEndOfTurn;
    da.expires_on_turn = s().turn.turn_number;
    s().delayed_abilities.push_back(da);
    ASSERT_EQ(s().turn.turn_player, P1);

    events.emit(PhaseChangedEvent{TurnPhase::MainPhase, TurnPhase::EndingStep, P1});

    ASSERT_EQ(s().chain.items.size(), 1u) << "P2's delayed ability goes on the chain";
    EXPECT_EQ(s().chain.items.back().controller, P2);
    EXPECT_TRUE(s().delayed_abilities.empty());
}

// ─── Battlefield abilities (CR 190.6) ───────────────────────────────────────
class BattlefieldAbilityTest : public RulesComplianceTest {
protected:
    GameObjectId setBattlefieldCard(int bf, CardDefId def, std::optional<PlayerId> controller) {
        auto id = s().createObject();
        auto& b = s().getObject(id);
        b.card_type = CardType::Battlefield;
        b.card_def_id = def;
        b.name = card_db.get(def).name;
        s().battlefields[bf].card_object_id = id;
        s().battlefields[bf].controller = controller;
        return id;
    }
    GameObjectId resolvedSpell(PlayerId p) {
        auto id = s().createObject();
        auto& sp = s().getObject(id);
        sp.owner = sp.controller = p;
        sp.card_type = CardType::Spell;
        sp.name = "TestSpell";
        sp.zone = ZoneType::Trash;
        sp.card_counters["__energy_spent_on_play"] = 4;
        return id;
    }
    int abilitiesOnChainFrom(GameObjectId source, PlayerId controller) {
        int n = 0;
        for (const auto& ci : s().chain.items)
            if (ci.source == source && ci.controller == controller) ++n;
        return n;
    }
};

TEST_F(BattlefieldAbilityTest, WhenYouDefendHereTriggersForTheDefender) {
    constexpr CardDefId kRavenbloom = 529;   // "When you defend here, reveal the top card ..."
    makeEngine();
    setLegend(P1); setLegend(P2);
    auto bf = setBattlefieldCard(0, kRavenbloom, P2);
    events.emit(CombatStartedEvent{BattlefieldId::fromRaw(0), /*attacker=*/P1, /*defender=*/P2});
    EXPECT_EQ(abilitiesOnChainFrom(bf, P2), 1);
    EXPECT_EQ(abilitiesOnChainFrom(bf, P1), 0);
}

TEST_F(BattlefieldAbilityTest, DefendHereDoesNotTriggerAtAnotherBattlefield) {
    constexpr CardDefId kRavenbloom = 529;
    makeEngine();
    setLegend(P1); setLegend(P2);
    auto bf = setBattlefieldCard(0, kRavenbloom, P2);
    events.emit(CombatStartedEvent{BattlefieldId::fromRaw(1), P1, P2});
    EXPECT_EQ(abilitiesOnChainFrom(bf, P2), 0);
}

TEST_F(BattlefieldAbilityTest, APlayerPlaysASpell_ThatPlayerControlsTheAbility) {
    constexpr CardDefId kAbandonedHall = 761;  // "When a player plays a spell, they may ..."
    makeEngine();
    setLegend(P1); setLegend(P2);
    auto bf = setBattlefieldCard(0, kAbandonedHall, P1);
    engineUnit(P2, 2, /*at_bf=*/0);
    // P2 — not the battlefield's controller — plays the spell (CR 190.6.c).
    auto spell = resolvedSpell(P2);
    events.emit(SpellResolvedEvent{spell, P2});
    EXPECT_EQ(abilitiesOnChainFrom(bf, P2), 1);
}

TEST_F(BattlefieldAbilityTest, WhileYouControlThisBattlefield_OnlyTheControllersSpellsCount) {
    constexpr CardDefId kForgottenLibrary = 767;  // "While you control this battlefield, when you
                                                  // play a spell, if you spent [4] or more, [Predict]."
    makeEngine();
    setLegend(P1); setLegend(P2);
    auto bf = setBattlefieldCard(0, kForgottenLibrary, P1);
    for (PlayerId p : {P1, P2})
        for (int i = 0; i < 3; ++i) {
            auto c = s().createObject();
            auto& o = s().getObject(c);
            o.owner = o.controller = p;
            o.card_type = CardType::Unit;
            o.zone = ZoneType::MainDeck;
            s().player(p).main_deck.push_back(c);
        }
    // Runs the battlefield's ability for `who` and reports whether it Predicted
    // (the first-choice agent recycles the card it looks at, so the top changes).
    auto predicts = [&](PlayerId who) {
        auto spell = resolvedSpell(who);
        events.emit(SpellResolvedEvent{spell, who});
        // CR 383.2.a.1: "While you control this battlefield" is part of the
        // trigger condition, so for the other player nothing is put on the
        // chain at all (Card::triggerCondition) — there is no item to resolve.
        EXPECT_EQ(abilitiesOnChainFrom(bf, who), who == P1 ? 1 : 0);
        if (abilitiesOnChainFrom(bf, who) == 0) return false;
        const auto item = s().chain.items.back();
        s().chain.items.clear();
        s().chain.resuming = item;
        const auto top_before = s().player(who).main_deck.back();
        CardContext ctx{s(), events, engine_->testHook_executor(), who, bf};
        card_registry.get(kForgottenLibrary)->onTrigger(ctx, {});
        s().chain.resuming.reset();
        return s().player(who).main_deck.back() != top_before;
    };
    EXPECT_FALSE(predicts(P2)) << "no Predict for a player who doesn't control the battlefield";
    EXPECT_TRUE(predicts(P1));
}

// ─── Excess combat damage ("if you assigned N or more excess damage") ───────
TEST_F(RulesComplianceTest, ExcessCombatDamage_IsWhatWasAssignedBeyondLethal) {
    makeEngine();
    setLegend(P1); setLegend(P2);
    auto small = engineUnit(P2, 2, /*at_bf=*/0);
    auto big   = engineUnit(P2, 4, /*at_bf=*/0);
    s().player(P1).last_combat_bf = BattlefieldId::fromRaw(0);

    Intent assign;
    assign.type = IntentType::AssignCombatDamage;
    assign.player = P1;
    assign.damage_assignments = {{small, 2}, {big, 9}};   // lethal, then 5 over lethal
    engine_->testHook_executeIntent(assign);

    EXPECT_EQ(s().player(P1).excess_combat_damage, 5);
    EXPECT_EQ(excessCombatDamageAt(s(), P1, BattlefieldId::fromRaw(0)), 5);
    EXPECT_EQ(excessCombatDamageAt(s(), P1, BattlefieldId::fromRaw(1)), 0) << "a combat elsewhere doesn't count";
    EXPECT_EQ(s().player(P2).excess_combat_damage, 0);
}

TEST_F(RulesComplianceTest, ExcessCombatDamage_TryndamereNeedsFiveAfterAnAttackHere) {
    constexpr CardDefId kTryndamere = 34;
    makeEngine();
    auto tryn = engineUnit(P1, 8, /*at_bf=*/0, kTryndamere);
    const Card* card = card_registry.get(kTryndamere);
    auto fires = [&] {
        return card->triggerCondition(s(), tryn, P1, TriggerType::WhenIConquer, kInvalidId);
    };
    EXPECT_FALSE(fires()) << "no combat this turn";
    auto& ps = s().player(P1);
    ps.last_combat_bf = BattlefieldId::fromRaw(0); ps.last_combat_attacker = true; ps.excess_combat_damage = 4;
    EXPECT_FALSE(fires()) << "4 excess is not enough";
    ps.excess_combat_damage = 5;
    EXPECT_TRUE(fires());
    ps.last_combat_attacker = false;
    EXPECT_FALSE(fires()) << "\"after an attack\": not when defending";
    ps.last_combat_attacker = true; ps.last_combat_bf = BattlefieldId::fromRaw(1);
    EXPECT_FALSE(fires()) << "the combat was at another battlefield";
}

// ─── Trigger conditions are checked when the event happens ──────────────────
TEST_F(RulesComplianceTest, TriggerCondition_VanguardHelmOnlyForABuffedUnit) {
    constexpr CardDefId kVanguardHelm = 228;   // "When a buffed friendly unit dies, ..."
    makeEngine();
    setLegend(P1); setLegend(P2);
    auto helm = s().createObject();
    {
        auto& g = s().getObject(helm);
        g.owner = g.controller = P1;
        g.card_type = CardType::Gear;
        g.card_def_id = kVanguardHelm;
        g.name = "Vanguard Helm";
        g.zone = ZoneType::Base;
        g.location = BaseLocation{P1};
    }
    engineUnit(P1, 2);                         // someone to buff
    auto plain  = engineUnit(P1, 2);
    auto buffed = engineUnit(P1, 2);
    s().getObject(buffed).buff_count = 1;

    engine_->testHook_executor().killObject(plain);
    EXPECT_TRUE(s().chain.items.empty()) << "an unbuffed unit's death doesn't trigger it";
    engine_->testHook_executor().killObject(buffed);
    ASSERT_EQ(s().chain.items.size(), 1u);
    EXPECT_EQ(s().chain.items.back().source, helm);
}

// ─── Energy that only pays for units and their abilities ────────────────────
TEST_F(CostPaymentRulesTest, UnitOnlyEnergy_PaysForAUnitButNothingElse) {
    makeEngine();
    setLegend(P1); setLegend(P2);
    s().player(P1).rune_pool.unit_energy = 4;
    auto unit = cardInHand(P1, 813);            // Field Musicians: [4]
    EXPECT_TRUE(engine_->testHook_canAfford(P1, unit));
    EXPECT_FALSE(engine_->testHook_canPayAdditionalCost(P1, 1, 0, Domain::Fury))
        << "an ordinary cost can't use it";
    ASSERT_TRUE(engine_->testHook_payCardCost(P1, unit));
    EXPECT_EQ(s().player(P1).rune_pool.unit_energy, 0);
}

TEST_F(CostPaymentRulesTest, UnitOnlyEnergy_PaysForAUnitsActivatedAbility) {
    constexpr CardDefId kRenektonBrute = 879;   // "[1]: Give me +1 [M] this turn."
    makeEngine();
    setLegend(P1); setLegend(P2);
    s().turn.ns_state = NeutralShowdownState::Neutral;
    s().turn.oc_state = OpenClosedState::Open;
    auto ren = engineUnit(P1, 3, -1, kRenektonBrute);
    s().player(P1).rune_pool.unit_energy = 1;
    Intent act;
    act.type = IntentType::ActivateActionAbility;
    act.player = P1;
    act.ability_source = ren;
    engine_->testHook_executeIntent(act); settleLikeTheEngine(*engine_);
    EXPECT_EQ(s().player(P1).rune_pool.unit_energy, 0);
    EXPECT_EQ(s().getObject(ren).temp_might_bonus, 1);
}

TEST_F(CostPaymentRulesTest, AbilityEnergyCostSpendsFloatingEnergy) {
    // An Energy-only ability cost is paid from floating Energy first — it used
    // to exhaust runes directly and leave the pool untouched.
    constexpr CardDefId kRenektonBrute = 879;
    makeEngine();
    setLegend(P1); setLegend(P2);
    s().turn.ns_state = NeutralShowdownState::Neutral;
    s().turn.oc_state = OpenClosedState::Open;
    auto ren = engineUnit(P1, 3, -1, kRenektonBrute);
    auto r = rune(P1, Domain::Fury);
    s().player(P1).rune_pool.energy = 1;
    Intent act;
    act.type = IntentType::ActivateActionAbility;
    act.player = P1;
    act.ability_source = ren;
    engine_->testHook_executeIntent(act); settleLikeTheEngine(*engine_);
    EXPECT_EQ(s().player(P1).rune_pool.energy, 0);
    EXPECT_FALSE(s().getObject(r).is_exhausted) << "the rune is not needed";
    EXPECT_EQ(s().getObject(ren).temp_might_bonus, 1);
}

// ─── Hooks given the object they are about ──────────────────────────────────
TEST_F(RulesComplianceTest, Shadow_EntersReadyOnlyAtABattlefield) {
    constexpr CardDefId kShadow = 752;
    makeEngine();
    const Card* card = card_registry.get(kShadow);
    auto at_bf   = engineUnit(P1, 2, /*at_bf=*/0, kShadow);
    auto at_base = engineUnit(P1, 2, -1, kShadow);
    EXPECT_TRUE(card->entersReadyOnPlay(s(), P1, s().getObject(at_bf)));
    EXPECT_FALSE(card->entersReadyOnPlay(s(), P1, s().getObject(at_base)));
}

TEST_F(RulesComplianceTest, Shadow_OnlyAttackersAtItsBattlefieldAreLegalTargets) {
    constexpr CardDefId kShadow = 752;
    makeEngine();
    const Card* card = card_registry.get(kShadow);
    auto shadow = engineUnit(P1, 2, /*at_bf=*/0, kShadow);
    auto here      = engineUnit(P2, 2, /*at_bf=*/0);
    auto elsewhere = engineUnit(P2, 2, /*at_bf=*/1);
    s().getObject(here).combat_designation = CombatDesignation::Attacker;
    s().getObject(elsewhere).combat_designation = CombatDesignation::Attacker;
    auto legal = card->enumerateLegalTargets(s(), P1, 0, shadow);
    EXPECT_EQ(legal, std::vector<GameObjectId>{here});
}

// ─── "When you play ... an activated ability with Energy cost [7] or more" ──
TEST_F(RulesComplianceTest, AbilityActivationTriggersCuratorOfTheSands) {
    constexpr CardDefId kCurator = 932;
    makeEngine();
    auto legend = setLegend(P1, kCurator);
    setLegend(P2);
    auto src = engineUnit(P1, 3);
    s().getObject(src).card_counters["__ability_energy_cost"] = 7;
    events.emit(ObjectStateChangedEvent{src, "ability_activated"});
    ASSERT_EQ(s().chain.items.size(), 1u);
    EXPECT_EQ(s().chain.items.back().source, legend);
    EXPECT_EQ(s().chain.items.back().triggering_spell_energy_spent, 7);
    EXPECT_EQ(s().chain.items.back().fired_trigger, TriggerType::WhenYouActivateAnAbility);
}

// ─── Card behaviour the engine asks the card for ────────────────────────────
TEST_F(BattlefieldAbilityTest, AspirantsClimbRaisesThePointsNeededToWin) {
    constexpr CardDefId kAspirantsClimb = 271;
    makeEngine();
    setLegend(P1); setLegend(P2);
    const int base = s().mode.victory_score;
    EXPECT_EQ(engine_->testHook_executor().effectiveVictoryScore(), base);
    setBattlefieldCard(0, kAspirantsClimb, std::nullopt);
    EXPECT_EQ(engine_->testHook_executor().effectiveVictoryScore(), base + 1);
}

TEST_F(BattlefieldAbilityTest, BrushTokenBattlefieldIsTheBrushCard) {
    constexpr CardDefId kBrush = 561;   // "Bird, Cat, Dog, Poro, and Ivern units here have +1 [M]."
    makeEngine();
    setLegend(P1); setLegend(P2);
    setBattlefieldCard(0, 271, P1);
    auto poro  = engineUnit(P1, 2, /*at_bf=*/0);
    auto other = engineUnit(P1, 2, /*at_bf=*/0);
    auto away  = engineUnit(P1, 2, /*at_bf=*/1);
    s().getObject(poro).tags = {"Poro"};
    s().getObject(away).tags = {"Poro"};

    engine_->testHook_executor().replaceBattlefieldWithToken(BattlefieldId::fromRaw(0), "Brush", P1, kBrush);
    EXPECT_EQ(s().getObject(s().battlefields[0].card_object_id).card_def_id, kBrush);
    engine_->testHook_cleanup();
    EXPECT_EQ(s().getObject(poro).current_might, 3);
    EXPECT_EQ(s().getObject(other).current_might, 2) << "not one of the named tags";
    EXPECT_EQ(s().getObject(away).current_might, 2) << "not here";
    engine_->testHook_cleanup();
    EXPECT_EQ(s().getObject(poro).current_might, 3) << "recalculated, not accumulated";
}

TEST_F(RulesComplianceTest, RekSaiGrantsAccelerateOnlyToFriendlyUnitsNotPlayedFromHand) {
    constexpr CardDefId kRekSai = 352;
    makeEngine();
    const Card* card = card_registry.get(kRekSai);
    auto reksai = engineUnit(P1, 4, -1, kRekSai);
    auto mine   = engineUnit(P1, 2);
    auto theirs = engineUnit(P2, 2);
    EXPECT_TRUE(card->grantsAccelerateTo(s(), reksai, s().getObject(mine), /*from_hand=*/false));
    EXPECT_FALSE(card->grantsAccelerateTo(s(), reksai, s().getObject(mine), /*from_hand=*/true));
    EXPECT_FALSE(card->grantsAccelerateTo(s(), reksai, s().getObject(theirs), /*from_hand=*/false));
}

// ─── A unit's gear when it dies ─────────────────────────────────────────────
class GearOnDeathTest : public RulesComplianceTest {
protected:
    GameObjectId equip(GameObjectId unit, CardDefId gear_def) {
        auto id = s().createObject();
        auto& g = s().getObject(id);
        const auto& u = s().getObject(unit);
        g.owner = g.controller = u.controller;
        g.card_type = CardType::Gear;
        g.card_def_id = gear_def;
        g.name = card_db.get(gear_def).name;
        g.zone = u.zone;
        g.location = u.location;
        g.attached_to = unit;
        s().getObject(unit).attachments.push_back(id);
        return id;
    }
    void giveDeck(PlayerId p, int n) {
        for (int i = 0; i < n; ++i) {
            auto c = s().createObject();
            auto& o = s().getObject(c);
            o.owner = o.controller = p;
            o.card_type = CardType::Unit;
            o.zone = ZoneType::MainDeck;
            s().player(p).main_deck.push_back(c);
        }
    }
};

TEST_F(GearOnDeathTest, EquipmentGrantedDeathknellFiresOnAnEffectKill) {
    constexpr CardDefId kSacredShears = 493;   // "[Deathknell] — Draw 1."
    makeEngine();
    setLegend(P1); setLegend(P2);
    giveDeck(P1, 3);
    auto unit = engineUnit(P1, 2, /*at_bf=*/0);
    auto shears = equip(unit, kSacredShears);
    const auto hand_before = s().player(P1).hand.size();

    engine_->testHook_executor().killObject(unit);
    engine_->testHook_endingStep();   // CR 383.3: the Deathknell resolves from the chain

    EXPECT_EQ(s().player(P1).hand.size(), hand_before + 1);
    EXPECT_FALSE(s().getObject(shears).attached_to.has_value()) << "the gear is shed";
    EXPECT_TRUE(s().getObject(shears).location.has_value()) << "and stays on the board (CR 719.5)";
    EXPECT_TRUE(s().turn.any_unit_died_this_turn) << "an effect kill is a death this turn";
}

TEST_F(GearOnDeathTest, EquipmentGrantedDeathknellFiresOnACombatDeath) {
    constexpr CardDefId kSacredShears = 493;
    makeEngine();
    setLegend(P1); setLegend(P2);
    giveDeck(P1, 3);
    auto unit = engineUnit(P1, 2, /*at_bf=*/0);
    auto shears = equip(unit, kSacredShears);
    const auto hand_before = s().player(P1).hand.size();

    s().getObject(unit).damage_marked = 5;
    engine_->testHook_processLethalDamage();
    engine_->testHook_endingStep();   // CR 383.3: the Deathknell resolves from the chain

    EXPECT_EQ(s().getObject(unit).zone, ZoneType::Trash);
    EXPECT_EQ(s().player(P1).hand.size(), hand_before + 1);
    EXPECT_FALSE(s().getObject(shears).attached_to.has_value());
}

// ─── Discard triggers ───────────────────────────────────────────────────────
TEST_F(RulesComplianceTest, DiscardingTriggersWhenYouDiscardOncePerDiscard) {
    constexpr CardDefId kJinxRebel = 202;   // "When you discard one or more cards, ready me ..."
    makeEngine();
    setLegend(P1); setLegend(P2);
    auto jinx = engineUnit(P1, 3, -1, kJinxRebel);
    engineUnit(P2, 3, -1, kJinxRebel);          // the opponent's doesn't see it
    std::vector<GameObjectId> cards;
    for (int i = 0; i < 2; ++i) {
        auto c = s().createObject();
        auto& o = s().getObject(c);
        o.owner = o.controller = P1;
        o.card_type = CardType::Spell;
        o.zone = ZoneType::Hand;
        s().player(P1).hand.push_back(c);
        cards.push_back(c);
    }
    engine_->testHook_executor().applyDiscard(P1, cards[0]);
    engine_->testHook_executor().applyDiscard(P1, cards[1]);
    ASSERT_EQ(s().chain.items.size(), 1u) << "one or more cards: one ability";
    EXPECT_EQ(s().chain.items.back().source, jinx);
}

TEST_F(RulesComplianceTest, FlameChompersTriggersWhenItIsDiscarded) {
    constexpr CardDefId kFlameChompers = 6;   // "When you discard me, you may pay [R] to play me."
    makeEngine();
    setLegend(P1); setLegend(P2);
    auto c = s().createObject();
    {
        auto& o = s().getObject(c);
        o.owner = o.controller = P1;
        o.card_type = CardType::Unit;
        o.card_def_id = kFlameChompers;
        o.name = "Flame Chompers";
        o.zone = ZoneType::Hand;
        s().player(P1).hand.push_back(c);
    }
    engine_->testHook_executor().applyDiscard(P1, c);
    ASSERT_EQ(s().chain.items.size(), 1u);
    EXPECT_EQ(s().chain.items.back().source, c);
    EXPECT_EQ(s().chain.items.back().fired_trigger, TriggerType::WhenIAmDiscarded);
}

// ─── The last Vendetta cards ────────────────────────────────────────────────
class VendettaLastCardsTest : public TriggerFinalizationTest {
protected:
    GameObjectId setBattlefieldCard(int bf, CardDefId def, std::optional<PlayerId> controller) {
        auto id = s().createObject();
        auto& b = s().getObject(id);
        b.card_type = CardType::Battlefield;
        b.card_def_id = def;
        b.name = card_db.get(def).name;
        s().battlefields[bf].card_object_id = id;
        s().battlefields[bf].controller = controller;
        return id;
    }
    static const Intent* paidPlay(const std::vector<Intent>& actions, GameObjectId card) {
        for (const auto& a : actions)
            if (a.type == IntentType::PlayCard && a.card == card && a.pay_location_cost) return &a;
        return nullptr;
    }
};

TEST_F(VendettaLastCardsTest, DragonRoost_ADragonMayPayToBePlayedThere) {
    constexpr CardDefId kDragonRoost = 944;   // "[A][A] ... to play a Dragon ... to this battlefield."
    setUpBoard();
    setBattlefieldCard(0, kDragonRoost, std::nullopt);       // nobody controls it
    engine_->testHook_cleanup();
    auto dragon = cardInHand(P1, kFieldMusicians);           // any [4] unit ...
    s().getObject(dragon).tags = {"Dragon"};                 // ... that is a Dragon
    auto plain = cardInHand(P1, kFieldMusicians);
    s().player(P1).rune_pool.energy = 4;

    EXPECT_EQ(paidPlay(engine_->generateLegalActions(), dragon), nullptr)
        << "not without the [A][A]";
    rune(P1, Domain::Fury); rune(P1, Domain::Calm);
    const auto actions = engine_->generateLegalActions();
    const Intent* offer = paidPlay(actions, dragon);
    ASSERT_NE(offer, nullptr);
    EXPECT_EQ(paidPlay(actions, plain), nullptr) << "only Dragons";
    ASSERT_TRUE(offer->play_location.has_value());
    EXPECT_EQ(*offer->play_location, LocationId{BattlefieldLocation{0}});

    engine_->testHook_executeIntent(*offer); settleLikeTheEngine(*engine_);
    EXPECT_EQ(s().getObject(dragon).battlefieldId().value_or(kInvalidId), BattlefieldId::fromRaw(0));
    EXPECT_EQ(runesInBase(P1), 0) << "both runes recycled for the [A][A]";
    EXPECT_EQ(s().player(P1).rune_pool.energy, 0);
}

TEST_F(VendettaLastCardsTest, SandsweptTomb_DiscountsASpellThatChoosesAFriendlyUnitThere) {
    constexpr CardDefId kSandsweptTomb = 951;   // "... costs [A] less."
    setUpBoard();
    setBattlefieldCard(0, kSandsweptTomb, std::nullopt);
    engine_->testHook_cleanup();
    auto here  = engineUnit(P1, 2, /*at_bf=*/0);
    auto there = engineUnit(P1, 2, /*at_bf=*/1);
    auto cast = [&](GameObjectId target) {
        p1_.want = target;
        s().player(P1).rune_pool.energy = 1;
        s().player(P1).rune_pool.power[static_cast<int>(Domain::Body)] = 2;
        auto spell = cardInHand(P1, kPunchFirst);            // [1] + 2 Body
        Intent play;
        play.type = IntentType::PlayCard;
        play.player = P1;
        play.card = spell;
        engine_->testHook_executeIntent(play); settleLikeTheEngine(*engine_);
        const int left = s().player(P1).rune_pool.power[static_cast<int>(Domain::Body)];
        s().player(P1).rune_pool.clear();
        return left;
    };
    EXPECT_EQ(cast(here), 1) << "the target is known before the cost is paid";
    EXPECT_EQ(s().getObject(here).temp_might_bonus, 5);
    EXPECT_EQ(cast(there), 0) << "no discount for a unit at another battlefield";
}

TEST_F(VendettaLastCardsTest, NewChoices_TheNewControllerReChoosesTheTarget) {
    setUpBoard();
    auto original = engineUnit(P1, 2);
    auto theirs   = engineUnit(P2, 2);
    // P1's Punch First on the chain, target already chosen; then P2 takes it.
    auto spell = cardInHand(P1, kPunchFirst);
    s().player(P1).hand.clear();
    ChainItem item;
    item.id = s().chain.allocateId();
    item.status = ChainItemStatus::Finalized;
    item.source = spell;
    item.card_def_id = kPunchFirst;
    item.controller = P2;
    item.is_spell = true;
    item.targets = {original};
    item.chosen_objects = {original};
    item.resume_data = {0, 0, 0, 0, 0, 1};
    s().chain.items.push_back(item);

    TriggerChoiceAgent p2;
    p2.want = theirs;
    engine_->testHook_setAgents(&p1_, &p2);
    engine_->testHook_executor().makeNewChoices(item.id);

    ASSERT_EQ(s().chain.items.size(), 1u);
    EXPECT_EQ(s().chain.items.back().targets, std::vector<GameObjectId>{theirs});
    EXPECT_EQ(p2.target_asked, 1);
}

TEST_F(VendettaLastCardsTest, FallenFeline_NamesAnySpell) {
    constexpr CardDefId kFallenFeline = 919;   // "When you play me, name a spell."
    setUpBoard();
    class NamingAgent : public AgentInterface {
    public:
        CardDefId name = kInvalidId; size_t offered = 0;
        Intent selectAction(const GameState&, const std::vector<Intent>& legal) override {
            if (!legal.empty() && legal.front().chosen_card_def != kInvalidId) {
                offered = legal.size();
                for (const auto& i : legal) if (i.chosen_card_def == name) return i;
            }
            for (const auto& i : legal) if (i.type == IntentType::PassPriority) return i;
            return legal.empty() ? Intent{} : legal.front();
        }
    } namer;
    namer.name = kPunchFirst;
    engine_->testHook_setAgents(&namer, &p2_);
    s().player(P1).rune_pool.power[static_cast<int>(Domain::Order)] = 1;
    auto card = cardInHand(P1, kFallenFeline);
    s().player(P1).rune_pool.energy = 2;
    Intent play;
    play.type = IntentType::PlayCard;
    play.player = P1;
    play.card = card;
    play.play_location = LocationId{BaseLocation{P1}};
    engine_->testHook_executeIntent(play); settleLikeTheEngine(*engine_);

    EXPECT_GT(namer.offered, 100u) << "every spell name is offered, not only ones seen";
    EXPECT_EQ(s().getObject(card).string_state["named_spell"], card_db.get(kPunchFirst).name)
        << "a spell the opponent has never shown";
}

TEST_F(VendettaLastCardsTest, LegionMarauder_EmpowerMayBePaidEitherWay) {
    constexpr CardDefId kLegionMarauder = 861;   // "[Empower] — [1] or [O]"
    setUpBoard();
    const auto abs = card_registry.get(kLegionMarauder)->activatedAbilities();
    ASSERT_EQ(abs.size(), 2u);
    EXPECT_TRUE(abs[0].is_empower);
    EXPECT_TRUE(abs[1].is_empower);
    EXPECT_EQ(abs[0].cost.energy, 1);
    EXPECT_EQ(abs[1].cost.energy, 0);
    EXPECT_EQ(abs[1].cost.power, 1);

    auto marauder = engineUnit(P1, 2, -1, kLegionMarauder);
    auto body = rune(P1, Domain::Body, /*exhausted=*/true);   // can't give Energy, can give [O]
    Intent act;
    act.type = IntentType::ActivateActionAbility;
    act.player = P1;
    act.ability_source = marauder;
    act.ability_index = 1;
    engine_->testHook_executeIntent(act); settleLikeTheEngine(*engine_);
    EXPECT_TRUE(s().getObject(marauder).isEmpowered());
    EXPECT_FALSE(s().getObject(body).location.has_value()) << "the rune was recycled for [O]";
}

TEST_F(VendettaLastCardsTest, ChooseObjectNowAsksTheAgent) {
    // Shady Spectacles picks the unit to copy as it is attached, outside the chain.
    setUpBoard();
    auto first  = engineUnit(P1, 2);
    auto second = engineUnit(P1, 3);
    p1_.want = second;
    EXPECT_EQ(engine_->testHook_executor().chooseObjectNow(P1, {first, second}), second);
    EXPECT_EQ(engine_->testHook_executor().chooseObjectNow(P1, {}), kInvalidId);
}

}  // namespace
}  // namespace riftbound::test
