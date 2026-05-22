/// @file test_karthus_deathknell.cpp
/// Tests for Karthus, Eternal (236)'s passive aura: "Your Deathknell
/// effects trigger an additional time." Implemented via
/// Card::applyPassiveAura → bumps PlayerState::deathknell_double_count
/// → TriggerManager::onUnitDied reads the count and enqueues (1+count)
/// chain items per Deathknell death.
///
/// Scenarios covered:
///   1. Karthus alone: count = 1; Deathknell fires 2x.
///   2. Two Karthus on board: count = 2; Deathknell fires 3x.
///   3. No Karthus: count = 0; Deathknell fires 1x (baseline).
///   4. Karthus belongs to one player only: doesn't double the OPPONENT's
///      Deathknells (cross-player isolation).
///   5. Karthus off-board (in trash): doesn't contribute to count.
///   6. Sequential scenario: Karthus alive, Deathknell #1 fires 2x;
///      Karthus dies; recalculateAuras resets count to 0; Deathknell #2
///      fires only 1x.
///
/// Architecture verification: the engine itself doesn't know that
/// card_def_id 236 is Karthus. The hook is `Card::applyPassiveAura`,
/// and Karthus's Card subclass overrides it to bump the counter. Other
/// cards can plug in to the same hook without any engine changes.

#include "card_test_fixture.h"

#include "cards/card_registry.h"
#include "effects/effect_types.h"
#include "engine/trigger_manager.h"

#include <gtest/gtest.h>

namespace riftbound::test {

namespace {

// Constants for clarity
constexpr CardDefId kKarthus = 236;
constexpr CardDefId kRuinedRex = 629;     // has Deathknell trigger

} // namespace

class KarthusDeathknellTest : public CardTestFixture {
protected:
    // Walk recalculateAuras's per-card pass manually (the fixture
    // doesn't own a GameEngine). For each on-board card, call its
    // applyPassiveAura. Reset counters first, mirroring the real
    // engine's Step 1a.
    void runPassiveAuraPass() {
        state.player(PlayerId::Player1).deathknell_double_count = 0;
        state.player(PlayerId::Player2).deathknell_double_count = 0;

        for (auto& [id, obj] : state.objects) {
            if (!obj.location.has_value() && obj.zone != ZoneType::LegendZone) continue;
            if (obj.card_def_id == kInvalidId) continue;
            const Card* card = card_registry.get(obj.card_def_id);
            if (card) card->applyPassiveAura(state, obj.controller);
        }
    }

    // Read the chain size to verify how many trigger items got enqueued.
    int chainSize() const {
        return static_cast<int>(state.chain.items.size());
    }
};

TEST_F(KarthusDeathknellTest, BaselineNoKarthus_DeathknellFiresOnce) {
    // No Karthus; a Deathknell unit dying should fire its trigger once.
    auto rex = addUnit(PlayerId::Player1, kRuinedRex, /*might=*/3, /*at_bf=*/0);

    runPassiveAuraPass();
    EXPECT_EQ(state.player(PlayerId::Player1).deathknell_double_count, 0)
        << "no Karthus => count stays 0";

    // Move rex to trash and fire onUnitDied to simulate the death.
    state.getObject(rex).zone = ZoneType::Trash;
    state.getObject(rex).location = std::nullopt;
    // Construct ChainManager + TriggerManager. TriggerManager
    // subscribes to events.on_unit_died at construction time; emitting
    // the event drives onUnitDied through the private code path.
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    events.emit(UnitDiedEvent{rex, PlayerId::Player1});

    EXPECT_EQ(chainSize(), 1)
        << "Deathknell fires once when no Karthus is on board";
}

TEST_F(KarthusDeathknellTest, OneKarthus_DeathknellFiresTwice) {
    addUnit(PlayerId::Player1, kKarthus, /*might=*/5, /*at_bf=*/0);
    auto rex = addUnit(PlayerId::Player1, kRuinedRex, /*might=*/3, /*at_bf=*/1);

    runPassiveAuraPass();
    EXPECT_EQ(state.player(PlayerId::Player1).deathknell_double_count, 1)
        << "one Karthus on board => count = 1";

    state.getObject(rex).zone = ZoneType::Trash;
    state.getObject(rex).location = std::nullopt;
    // Construct ChainManager + TriggerManager. TriggerManager
    // subscribes to events.on_unit_died at construction time; emitting
    // the event drives onUnitDied through the private code path.
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    events.emit(UnitDiedEvent{rex, PlayerId::Player1});

    EXPECT_EQ(chainSize(), 2)
        << "Deathknell fires twice (1 base + 1 extra from Karthus)";
}

TEST_F(KarthusDeathknellTest, TwoKarthus_DeathknellFiresThrice) {
    addUnit(PlayerId::Player1, kKarthus, /*might=*/5, /*at_bf=*/0);
    addUnit(PlayerId::Player1, kKarthus, /*might=*/5, /*at_bf=*/1);
    auto rex = addUnit(PlayerId::Player1, kRuinedRex, /*might=*/3, /*at_bf=*/0);

    runPassiveAuraPass();
    EXPECT_EQ(state.player(PlayerId::Player1).deathknell_double_count, 2)
        << "two Karthus on board => count = 2";

    state.getObject(rex).zone = ZoneType::Trash;
    state.getObject(rex).location = std::nullopt;
    // Construct ChainManager + TriggerManager. TriggerManager
    // subscribes to events.on_unit_died at construction time; emitting
    // the event drives onUnitDied through the private code path.
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    events.emit(UnitDiedEvent{rex, PlayerId::Player1});

    EXPECT_EQ(chainSize(), 3)
        << "Deathknell fires 3 times (1 base + 2 extras)";
}

TEST_F(KarthusDeathknellTest, OpponentsKarthus_DoesNotDoubleMyDeathknell) {
    // Karthus belongs to P2; rex belongs to P1.
    addUnit(PlayerId::Player2, kKarthus, /*might=*/5, /*at_bf=*/0);
    auto rex = addUnit(PlayerId::Player1, kRuinedRex, /*might=*/3, /*at_bf=*/1);

    runPassiveAuraPass();
    EXPECT_EQ(state.player(PlayerId::Player1).deathknell_double_count, 0)
        << "P1 has no Karthus";
    EXPECT_EQ(state.player(PlayerId::Player2).deathknell_double_count, 1)
        << "P2 has Karthus but it doesn't affect P1's Deathknells";

    state.getObject(rex).zone = ZoneType::Trash;
    state.getObject(rex).location = std::nullopt;
    // Construct ChainManager + TriggerManager. TriggerManager
    // subscribes to events.on_unit_died at construction time; emitting
    // the event drives onUnitDied through the private code path.
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    events.emit(UnitDiedEvent{rex, PlayerId::Player1});

    EXPECT_EQ(chainSize(), 1)
        << "P1's Deathknell fires once — P2's Karthus does NOT double it";
}

TEST_F(KarthusDeathknellTest, KarthusInTrash_DoesNotContributeToCount) {
    // Karthus has died (in trash); should not contribute to count.
    auto karthus = addUnit(PlayerId::Player1, kKarthus, /*might=*/5, /*at_bf=*/0);
    state.getObject(karthus).zone = ZoneType::Trash;
    state.getObject(karthus).location = std::nullopt;

    auto rex = addUnit(PlayerId::Player1, kRuinedRex, /*might=*/3, /*at_bf=*/0);

    runPassiveAuraPass();
    EXPECT_EQ(state.player(PlayerId::Player1).deathknell_double_count, 0)
        << "Karthus in trash doesn't count";

    state.getObject(rex).zone = ZoneType::Trash;
    state.getObject(rex).location = std::nullopt;
    // Construct ChainManager + TriggerManager. TriggerManager
    // subscribes to events.on_unit_died at construction time; emitting
    // the event drives onUnitDied through the private code path.
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    events.emit(UnitDiedEvent{rex, PlayerId::Player1});

    EXPECT_EQ(chainSize(), 1) << "no living Karthus, fires once";
}

TEST_F(KarthusDeathknellTest, KarthusDiesBetweenTwoDeaths_SecondFiresOnce) {
    // The user's "sequential" scenario:
    //   1. Karthus alive → recalc sets count = 1
    //   2. Unit A (rex) dies → onUnitDied reads count=1 → enqueues 2 fires
    //   3. Karthus dies (moved to trash + recalculateAuras re-runs)
    //   4. Unit B (another rex) dies → onUnitDied reads count=0 → 1 fire
    auto karthus = addUnit(PlayerId::Player1, kKarthus, /*might=*/5, /*at_bf=*/0);
    auto rex_a = addUnit(PlayerId::Player1, kRuinedRex, /*might=*/3, /*at_bf=*/0);
    auto rex_b = addUnit(PlayerId::Player1, kRuinedRex, /*might=*/3, /*at_bf=*/1);

    // Step 1: Karthus alive; recalc sets count = 1.
    runPassiveAuraPass();
    EXPECT_EQ(state.player(PlayerId::Player1).deathknell_double_count, 1);

    // Construct ChainManager + TriggerManager. TriggerManager
    // subscribes to events.on_unit_died at construction time; emitting
    // the event drives onUnitDied through the private code path.
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();

    // Step 2: rex_a dies. onUnitDied reads count=1 → 2 fires.
    state.getObject(rex_a).zone = ZoneType::Trash;
    state.getObject(rex_a).location = std::nullopt;
    events.emit(UnitDiedEvent{rex_a, PlayerId::Player1});
    EXPECT_EQ(chainSize(), 2)
        << "rex_a's Deathknell fires twice (Karthus alive)";

    // Step 3: Karthus dies. recalculateAuras (simulated) sees Karthus
    // off-board and resets count to 0.
    state.getObject(karthus).zone = ZoneType::Trash;
    state.getObject(karthus).location = std::nullopt;
    runPassiveAuraPass();
    EXPECT_EQ(state.player(PlayerId::Player1).deathknell_double_count, 0)
        << "after Karthus dies, count should be 0";

    // Step 4: rex_b dies. onUnitDied reads count=0 → only 1 fire.
    state.chain.items.clear();  // clear residue from step 2 to isolate
    state.getObject(rex_b).zone = ZoneType::Trash;
    state.getObject(rex_b).location = std::nullopt;
    events.emit(UnitDiedEvent{rex_b, PlayerId::Player1});
    EXPECT_EQ(chainSize(), 1)
        << "rex_b's Deathknell fires ONCE — Karthus is gone, aura dropped";
}

TEST_F(KarthusDeathknellTest, SimultaneousDeath_CR_323_4_3a_AuraSnapshotAtQueueTime) {
    // CR 323.4.3a (Cleanup Step 2d):
    //   "All Units that have non-zero Damage marked on them equalling or
    //    exceeding their Might that have Deathknell abilities will trigger
    //    their Deathknell ability now, MAKING NOTE OF THEIR CURRENT
    //    LOCATION, ATTRIBUTES, AND OTHER INFORMATION RELEVANT to add the
    //    trigger as a Pending Item."
    //
    // Cross-reference: CR 303.2 (game actions are sequenced, never
    // truly simultaneous) + CR 383.3.d (simultaneous triggers ordered
    // by controller).
    //
    // Interpretation: simultaneous deaths in a single cleanup-step kill
    // batch are queued with state SNAPSHOTTED at trigger-queue time.
    // Karthus's aura is part of that snapshot; Unit A's Deathknell
    // fires 2x regardless of kill order within the batch.
    //
    // Our implementation honors this because deathknell_double_count
    // is refreshed only by recalculateAuras, which runs AFTER cleanup's
    // kill batch via the next cleanup pass. All onUnitDied events
    // within a single batch read the same count.
    //
    // Per the user's design preference ("if ambiguous, assume defender
    // dies first since the attack initiates combat"), kill order doesn't
    // matter in our model — the result is the same either way. This
    // matches the CR's "simultaneous" semantics.
    auto karthus = addUnit(PlayerId::Player1, kKarthus, /*might=*/5, /*at_bf=*/0);
    auto rex_a = addUnit(PlayerId::Player1, kRuinedRex, /*might=*/3, /*at_bf=*/0);

    runPassiveAuraPass();
    EXPECT_EQ(state.player(PlayerId::Player1).deathknell_double_count, 1);

    // Construct ChainManager + TriggerManager. TriggerManager
    // subscribes to events.on_unit_died at construction time; emitting
    // the event drives onUnitDied through the private code path.
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();

    // Kill Karthus first (no Deathknell on him, but onUnitDied still fires).
    state.getObject(karthus).zone = ZoneType::Trash;
    state.getObject(karthus).location = std::nullopt;
    events.emit(UnitDiedEvent{karthus, PlayerId::Player1});
    int chain_after_karthus = chainSize();

    // Kill rex_a; count is still 1 (recalc hasn't run between these
    // events). Behavior matches "the aura was in effect at the moment
    // of A's death" — a common TCG resolution for simultaneous deaths.
    state.getObject(rex_a).zone = ZoneType::Trash;
    state.getObject(rex_a).location = std::nullopt;
    events.emit(UnitDiedEvent{rex_a, PlayerId::Player1});
    int chain_after_rex = chainSize();
    int rex_added = chain_after_rex - chain_after_karthus;

    EXPECT_EQ(rex_added, 2)
        << "CR 323.4.3a: simultaneous deaths queue triggers with current "
           "attributes snapshotted. Karthus's aura is active at A's "
           "queue time, so A's Deathknell fires 2x regardless of order.";
}

} // namespace riftbound::test
