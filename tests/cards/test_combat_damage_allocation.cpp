/// @file test_combat_damage_allocation.cpp
/// Tests for `GameEngine::advanceCombatDamage` — the function that emits
/// legal damage-assignment Intents for the agent to pick from.
///
/// CRs exercised:
///   - 460.2.c.3 — strict lethality cascade (full lethal each before moving on)
///   - 460.2.c.4 — no over-assign except on the final assigned target
///   - 460.2.c.6 — any order within tied-priority bucket
///   - 815      — Tank must be assigned lethal first
///   - 826      — Backline must be assigned lethal last
///
/// Each test inspects every emitted option against these invariants. If
/// `advanceCombatDamage` ever emits an illegal allocation the tests below
/// catch it.

#include "card_test_fixture.h"
#include "engine/game_engine.h"

#include <gtest/gtest.h>
#include <set>

namespace riftbound::test {
namespace {

class DamageAllocationTest : public CardTestFixture {
protected:
    std::unique_ptr<GameEngine> engine_;
    GameState* eng_state() { return &engine_->mutableState(); }

    void makeEngine() {
        engine_ = std::make_unique<GameEngine>(card_db, events, card_registry);
        auto& s = *eng_state();
        s.mode = ModeOfPlay{};
        s.players[0].id = P1;
        s.players[1].id = P2;
        BattlefieldState bf; bf.id = 0;
        s.battlefields.push_back(bf);
    }

    // Place a vanilla unit with the given might at BF#0, controlled by P2.
    // Optional keyword set (Tank / Backline).
    GameObjectId placeDefender(int might, KeywordSet keywords = {}) {
        auto& s = *eng_state();
        auto id = s.createObject();
        auto& obj = s.getObject(id);
        obj.owner = P2;
        obj.controller = P2;
        obj.card_type = CardType::Unit;
        obj.base_might = might;
        obj.current_might = might;
        obj.keywords = keywords;
        obj.zone = ZoneType::BattlefieldZone;
        obj.location = BattlefieldLocation{0};
        return id;
    }

    // ── CR-compliance checkers run against every emitted Intent ──

    // CR 460.2.c.3 + c.4: every assignment except the last must be exactly
    // lethal for its target; only the last may be sub-lethal or over-lethal,
    // and over-lethal is allowed only if there were no other targets to
    // receive damage (i.e., the cascade covered every initial target).
    void verifyCascadeInvariants(const GameState& s,
                                  int total_damage,
                                  const std::vector<GameObjectId>& original_targets,
                                  const std::vector<DamageAssignment>& da) {
        ASSERT_FALSE(da.empty()) << "an option with non-zero pool produced no assignments";
        int sum = 0;
        for (auto& a : da) {
            ASSERT_GT(a.damage, 0) << "zero-damage entries should not appear in the option";
            sum += a.damage;
        }
        EXPECT_EQ(sum, total_damage) << "cascade total must equal the damage pool";

        // Each non-final assignment must equal exactly the target's lethal
        // threshold at the moment it was assigned (= current_might since
        // damage_marked is 0 at this stage of the test setup).
        for (size_t i = 0; i + 1 < da.size(); ++i) {
            const auto& obj = s.getObject(da[i].target_unit);
            int lethal_at_assign = std::max(1, obj.current_might - obj.damage_marked);
            EXPECT_EQ(da[i].damage, lethal_at_assign)
                << "non-final assignment to " << obj.name
                << " must be exactly lethal (got " << da[i].damage
                << ", lethal=" << lethal_at_assign << ")";
        }
        // The final assignment may be sub-lethal (pool ran out) or
        // over-lethal (only if every original target was assigned).
        if (!da.empty()) {
            const auto& last_obj = s.getObject(da.back().target_unit);
            int last_lethal = std::max(1, last_obj.current_might - last_obj.damage_marked);
            if (da.back().damage > last_lethal) {
                EXPECT_EQ(da.size(), original_targets.size())
                    << "over-assignment to last target only legal once all "
                    << "original targets have been assigned to";
            }
        }
    }

    // CR 815/826: in every option, Tanks must appear (assigned to) before
    // any non-Tank; non-Backlines before any Backline.
    void verifyTankBacklinePriority(const GameState& s,
                                     const std::vector<DamageAssignment>& da) {
        int bucket = 0;  // 0=Tank, 1=Mid, 2=Backline; must be monotone non-decreasing
        for (auto& a : da) {
            const auto& obj = s.getObject(a.target_unit);
            int b;
            if (obj.hasKeyword(Keyword::Tank)) b = 0;
            else if (obj.hasKeyword(Keyword::Backline)) b = 2;
            else b = 1;
            EXPECT_GE(b, bucket)
                << "option violates Tank-first / Backline-last priority";
            bucket = b;
        }
    }
};

// ─── Smoke: 0-damage and empty-targets produce Skip ─────────────────────────

TEST_F(DamageAllocationTest, ZeroDamage_ReturnsSkip) {
    makeEngine();
    auto t = placeDefender(3);
    auto adv = engine_->testHook_advanceCombatDamage(P1, 0, {t});
    EXPECT_EQ(adv.kind, GameEngine::CombatDamageAdvance::Kind::Skip);
    EXPECT_TRUE(adv.legal.empty());
}

TEST_F(DamageAllocationTest, EmptyTargets_ReturnsSkip) {
    makeEngine();
    auto adv = engine_->testHook_advanceCombatDamage(P1, 5, {});
    EXPECT_EQ(adv.kind, GameEngine::CombatDamageAdvance::Kind::Skip);
}

// ─── Single target: one option, all pool on it ─────────────────────────────

TEST_F(DamageAllocationTest, SingleTarget_OneOption_AllOnIt) {
    makeEngine();
    auto t = placeDefender(3);
    auto adv = engine_->testHook_advanceCombatDamage(P1, 5, {t});
    ASSERT_EQ(adv.legal.size(), 1u);
    const auto& da = adv.legal[0].damage_assignments;
    ASSERT_EQ(da.size(), 1u);
    EXPECT_EQ(da[0].target_unit, t);
    EXPECT_EQ(da[0].damage, 5);  // over-assignment allowed when sole target
}

// ─── CR 460.2.c.3 worked example: 5 damage among four 3-Might units ────────

TEST_F(DamageAllocationTest, CRExample_5DamageAmongFour3MightUnits_AllOptionsCRLegal) {
    // The exact case the CR uses in its 460.2.c.3 example. The illegal
    // 2-1-1-1 split must NEVER appear in the legal set; only "3 to one,
    // 2 to another" shapes are valid.
    makeEngine();
    auto a = placeDefender(3);
    auto b = placeDefender(3);
    auto c = placeDefender(3);
    auto d = placeDefender(3);
    std::vector<GameObjectId> targets = {a, b, c, d};

    auto adv = engine_->testHook_advanceCombatDamage(P1, 5, targets);
    ASSERT_EQ(adv.kind, GameEngine::CombatDamageAdvance::Kind::NeedDecision);
    ASSERT_GT(adv.legal.size(), 0u);

    for (size_t i = 0; i < adv.legal.size(); ++i) {
        SCOPED_TRACE("option " + std::to_string(i));
        const auto& da = adv.legal[i].damage_assignments;
        verifyCascadeInvariants(*eng_state(), 5, targets, da);
        verifyTankBacklinePriority(*eng_state(), da);
        // Shape: exactly one full-lethal + one sub-lethal leftover.
        ASSERT_EQ(da.size(), 2u) << "5/4×3M case must produce {lethal, leftover}";
        EXPECT_EQ(da[0].damage, 3);
        EXPECT_EQ(da[1].damage, 2);
    }

    // Every legal allocation = (which dies, which catches leftover).
    // 4 picks of who-dies × 3 picks of who-catches = 12 legal allocations.
    EXPECT_EQ(adv.legal.size(), 12u);
}

// ─── Tank priority (CR 815) ─────────────────────────────────────────────────

TEST_F(DamageAllocationTest, Tank_AlwaysAssignedFirstAcrossEveryOption) {
    makeEngine();
    KeywordSet tank_kw; tank_kw.set(Keyword::Tank);
    auto tank = placeDefender(2, tank_kw);
    auto normal_a = placeDefender(2);
    auto normal_b = placeDefender(2);
    std::vector<GameObjectId> targets = {tank, normal_a, normal_b};

    auto adv = engine_->testHook_advanceCombatDamage(P1, 6, targets);
    ASSERT_GT(adv.legal.size(), 0u);
    for (size_t i = 0; i < adv.legal.size(); ++i) {
        SCOPED_TRACE("option " + std::to_string(i));
        const auto& da = adv.legal[i].damage_assignments;
        verifyTankBacklinePriority(*eng_state(), da);
        // First assignment must be the Tank.
        ASSERT_FALSE(da.empty());
        EXPECT_EQ(da[0].target_unit, tank);
    }
}

// ─── Backline priority (CR 826) ─────────────────────────────────────────────

TEST_F(DamageAllocationTest, Backline_AlwaysAssignedLastAcrossEveryOption) {
    makeEngine();
    KeywordSet bl_kw; bl_kw.set(Keyword::Backline);
    auto backline = placeDefender(2, bl_kw);
    auto normal_a = placeDefender(2);
    auto normal_b = placeDefender(2);
    std::vector<GameObjectId> targets = {normal_a, normal_b, backline};

    // Pool large enough that the cascade reaches the Backline.
    auto adv = engine_->testHook_advanceCombatDamage(P1, 6, targets);
    ASSERT_GT(adv.legal.size(), 0u);
    for (size_t i = 0; i < adv.legal.size(); ++i) {
        const auto& da = adv.legal[i].damage_assignments;
        verifyTankBacklinePriority(*eng_state(), da);
        // If Backline appears at all, it must be the last assignment.
        for (size_t j = 0; j < da.size(); ++j) {
            if (da[j].target_unit == backline) {
                EXPECT_EQ(j + 1, da.size())
                    << "Backline must only be assigned last";
            }
        }
    }
}

TEST_F(DamageAllocationTest, Backline_NeverAssignedWhilePoolStillNotConsumingNonBacklines) {
    // If pool is smaller than the non-Backline total, Backline must never
    // receive any damage. With pool=3 vs two 2-Might non-Backlines + one
    // 2-Might Backline, total 6, the cascade should kill one non-Backline
    // (2) and leave 1 leftover on the OTHER non-Backline — never on the
    // Backline.
    makeEngine();
    KeywordSet bl_kw; bl_kw.set(Keyword::Backline);
    auto backline = placeDefender(2, bl_kw);
    auto normal_a = placeDefender(2);
    auto normal_b = placeDefender(2);
    std::vector<GameObjectId> targets = {normal_a, normal_b, backline};

    auto adv = engine_->testHook_advanceCombatDamage(P1, 3, targets);
    ASSERT_GT(adv.legal.size(), 0u);
    for (size_t i = 0; i < adv.legal.size(); ++i) {
        const auto& da = adv.legal[i].damage_assignments;
        for (auto& a : da) {
            EXPECT_NE(a.target_unit, backline)
                << "Backline cannot receive damage while non-Backlines have "
                << "not yet had lethal assigned (CR 826.4.b)";
        }
    }
}

// ─── Tank + Backline composite ──────────────────────────────────────────────

TEST_F(DamageAllocationTest, TankPlusBackline_PriorityOrderStrict) {
    makeEngine();
    KeywordSet tank_kw; tank_kw.set(Keyword::Tank);
    KeywordSet bl_kw;   bl_kw.set(Keyword::Backline);
    auto tank = placeDefender(2, tank_kw);
    auto mid = placeDefender(2);
    auto bl = placeDefender(2, bl_kw);
    std::vector<GameObjectId> targets = {tank, mid, bl};

    // Pool=6 kills all three; verify every option visits in T→M→B order.
    auto adv = engine_->testHook_advanceCombatDamage(P1, 6, targets);
    ASSERT_GT(adv.legal.size(), 0u);
    for (size_t i = 0; i < adv.legal.size(); ++i) {
        const auto& da = adv.legal[i].damage_assignments;
        ASSERT_EQ(da.size(), 3u);
        EXPECT_EQ(da[0].target_unit, tank);
        EXPECT_EQ(da[1].target_unit, mid);
        EXPECT_EQ(da[2].target_unit, bl);
    }
}

// ─── Pool exceeds sum of lethals (the over-assign-on-last branch) ──────────

TEST_F(DamageAllocationTest, PoolExceedsSumOfLethals_LastTargetOverAssigned) {
    // Three 3-Might units, pool=10 → 3+3+4 (last over-assigned by 1).
    // CR 460.2.c.4 says this is legal only because no further targets
    // remain. The verifier checks that condition.
    makeEngine();
    auto a = placeDefender(3);
    auto b = placeDefender(3);
    auto c = placeDefender(3);
    std::vector<GameObjectId> targets = {a, b, c};

    auto adv = engine_->testHook_advanceCombatDamage(P1, 10, targets);
    ASSERT_GT(adv.legal.size(), 0u);
    for (size_t i = 0; i < adv.legal.size(); ++i) {
        SCOPED_TRACE("option " + std::to_string(i));
        const auto& da = adv.legal[i].damage_assignments;
        verifyCascadeInvariants(*eng_state(), 10, targets, da);
        // All three assigned to.
        std::set<GameObjectId> hit;
        for (auto& x : da) hit.insert(x.target_unit);
        EXPECT_EQ(hit.size(), 3u);
    }
}

// ─── Dedup: identical-outcome cascades collapse to one option ──────────────

TEST_F(DamageAllocationTest, DistinctOptionsHaveDistinctOutcomes) {
    // The bucket-permuted builder enumerates orderings, but dedups on
    // final marked-damage pattern. Verify no two emitted options resolve
    // to the same per-target damage map.
    makeEngine();
    auto a = placeDefender(3);
    auto b = placeDefender(3);
    auto c = placeDefender(3);
    auto adv = engine_->testHook_advanceCombatDamage(P1, 6, {a, b, c});

    std::set<std::string> seen;
    for (const auto& intent : adv.legal) {
        std::vector<std::pair<GameObjectId,int>> kv;
        for (auto& da : intent.damage_assignments)
            kv.emplace_back(da.target_unit, da.damage);
        std::sort(kv.begin(), kv.end());
        std::string key;
        for (auto& [t, d] : kv) {
            key += std::to_string(t); key += ':'; key += std::to_string(d); key += ',';
        }
        EXPECT_TRUE(seen.insert(key).second) << "duplicate outcome: " << key;
    }
}

}  // namespace
}  // namespace riftbound::test
