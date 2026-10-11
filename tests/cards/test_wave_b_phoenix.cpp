/// @file test_wave_b_phoenix.cpp
/// Immortal Phoenix (037): "When you kill a unit with a spell, you may pay [1][R]
/// to play me from your trash."

#include "tests/cards/card_test_fixture.h"
#include "engine/trigger_manager.h"
#include "engine/chain_manager.h"
#include <algorithm>
#include <gtest/gtest.h>

namespace riftbound::test {
namespace {

using PhoenixTest = CardTestFixture;
constexpr CardDefId kPhoenix = 37;

static GameObjectId phoenixInTrash(GameState& state, CardDB& db, PlayerId p) {
    auto id = state.createObject();
    auto& o = state.getObject(id);
    o.owner = p; o.controller = p; o.card_def_id = kPhoenix;
    o.card_type = CardType::Unit; o.zone = ZoneType::Trash;
    o.name = db.get(kPhoenix).name;
    state.player(p).trash.push_back(id);
    return id;
}

TEST_F(PhoenixTest, EnqueuesWhenSpellKillsUnitWhilePhoenixInTrash) {
    auto phoenix = phoenixInTrash(state, card_db, P1);
    auto victim = addUnit(P2, 1, 3, /*at_bf=*/0);
    // "When you kill a unit with a spell": a P1 spell is resolving when the
    // unit is killed (the kill records who killed it and with what — 9.M21).
    size_t queued = 0;
    withEngine([&](GameEngine& e) {
        auto& live = e.mutableState();
        ChainItem ri; ri.id = live.chain.allocateId(); ri.controller = P1; ri.is_spell = true;
        live.chain.resuming = ri;
        e.testHook_executor().killObject(victim);
        live.chain.resuming.reset();
        for (const auto& ci : live.chain.items) if (ci.source == phoenix) ++queued;
        live.chain.items.clear();
    });
    EXPECT_EQ(queued, 1u) << "a spell-kill should enqueue Immortal Phoenix from trash";
}

TEST_F(PhoenixTest, PaysAndPlaysFromTrash) {
    auto phoenix = phoenixInTrash(state, card_db, P1);
    addRune(P1, Domain::Fury);   // for [R] (recycle) + counts toward energy
    addRune(P1, Domain::Fury);   // second ready rune for [1] energy
    EffectExecutor exec(state, events, card_db);
    driveResumableTrigger(kPhoenix, P1, phoenix,
        [](const std::vector<Intent>& l) { return l.back(); },  // "yes"
        exec, TriggerType::WhenYouKillAUnitWithASpell);
    EXPECT_NE(state.getObject(phoenix).zone, ZoneType::Trash)
        << "Phoenix leaves the trash (played) after paying [1][R]";
    auto& trash = state.player(P1).trash;
    EXPECT_EQ(std::find(trash.begin(), trash.end(), phoenix), trash.end());
}

TEST_F(PhoenixTest, StaysInTrashWhenDeclined) {
    auto phoenix = phoenixInTrash(state, card_db, P1);
    addRune(P1, Domain::Fury);
    addRune(P1, Domain::Fury);
    EffectExecutor exec(state, events, card_db);
    driveResumableTrigger(kPhoenix, P1, phoenix,
        [](const std::vector<Intent>& l) { return l.front(); },  // "no"
        exec, TriggerType::WhenYouKillAUnitWithASpell);
    EXPECT_EQ(state.getObject(phoenix).zone, ZoneType::Trash);
}

}  // namespace
}  // namespace riftbound::test
