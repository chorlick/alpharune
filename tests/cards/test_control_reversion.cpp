/// @file test_control_reversion.cpp
/// Tests for the "you control it until I leave the board" control-reversion
/// machinery (Akshan, Mischievous 431).
///
///   - EffectExecutor::takeControlUntilSourceLeaves flips control and records
///     the source + the controller to restore.
///   - GameEngine::revertLapsedControl returns control to the original owner
///     once the controlling source leaves the board, and is a no-op while the
///     source remains in play.

#include "tests/cards/card_test_fixture.h"

#include "core/events.h"
#include "core/game_state.h"
#include "engine/effect_executor.h"
#include "engine/game_engine.h"

#include <gtest/gtest.h>

namespace riftbound::test {
namespace {

// Build a controlling source (a unit on a BF) and an enemy gear, then have the
// source's controller take the gear "until source leaves". Returns {source,
// gear}. Operates on the given engine's state.
struct Scene { GameObjectId source; GameObjectId gear; };

Scene buildScene(GameEngine& engine) {
    auto& s = engine.mutableState();
    s.players[0].id = PlayerId::Player1;
    s.players[1].id = PlayerId::Player2;
    s.battlefields.push_back(BattlefieldState{});
    s.battlefields.back().id = 0;

    // P1's controlling source (Akshan stand-in) at the battlefield.
    auto source = s.createObject();
    auto& so = s.getObject(source);
    so.owner = PlayerId::Player1; so.controller = PlayerId::Player1;
    so.card_type = CardType::Unit;
    so.name = "Source";
    so.zone = ZoneType::BattlefieldZone;
    so.location = BattlefieldLocation{0};

    // P2's gear.
    auto gear = s.createObject();
    auto& go = s.getObject(gear);
    go.owner = PlayerId::Player2; go.controller = PlayerId::Player2;
    go.card_type = CardType::Gear;
    go.name = "Stolen Gear";
    go.zone = ZoneType::Base;
    go.location = BaseLocation{PlayerId::Player2};
    return {source, gear};
}

TEST_F(CardTestFixture, TakeControlUntilSourceLeaves_FlipsAndRecords) {
    GameEngine engine(card_db, events, card_registry);
    auto [source, gear] = buildScene(engine);
    auto& s = engine.mutableState();
    EffectExecutor exec(s, events, card_db, &card_registry);

    exec.takeControlUntilSourceLeaves(gear, PlayerId::Player1, source);

    auto& g = s.getObject(gear);
    EXPECT_EQ(g.controller, PlayerId::Player1) << "control taken";
    EXPECT_TRUE(g.control_reverts_on_source_leave);
    EXPECT_EQ(g.control_source_obj, source);
    EXPECT_EQ(g.control_revert_to, PlayerId::Player2) << "original owner recorded";
}

TEST_F(CardTestFixture, RevertLapsedControl_NoOpWhileSourceOnBoard) {
    GameEngine engine(card_db, events, card_registry);
    auto [source, gear] = buildScene(engine);
    auto& s = engine.mutableState();
    EffectExecutor exec(s, events, card_db, &card_registry);
    exec.takeControlUntilSourceLeaves(gear, PlayerId::Player1, source);

    engine.revertLapsedControl();  // source still at its battlefield

    auto& g = s.getObject(gear);
    EXPECT_EQ(g.controller, PlayerId::Player1) << "control retained while source in play";
    EXPECT_TRUE(g.control_reverts_on_source_leave) << "link still armed";
}

TEST_F(CardTestFixture, RevertLapsedControl_RevertsWhenSourceLeavesBoard) {
    GameEngine engine(card_db, events, card_registry);
    auto [source, gear] = buildScene(engine);
    auto& s = engine.mutableState();
    EffectExecutor exec(s, events, card_db, &card_registry);
    exec.takeControlUntilSourceLeaves(gear, PlayerId::Player1, source);

    // Source leaves the board (e.g. killed → trash, no location).
    auto& so = s.getObject(source);
    so.zone = ZoneType::Trash;
    so.location = std::nullopt;

    engine.revertLapsedControl();

    auto& g = s.getObject(gear);
    EXPECT_EQ(g.controller, PlayerId::Player2) << "control returns to original owner";
    EXPECT_FALSE(g.control_reverts_on_source_leave) << "link cleared (one-shot)";
    EXPECT_EQ(g.control_source_obj, kInvalidId);
}

TEST_F(CardTestFixture, RevertLapsedControl_RevertsWhenSourceCeasesToExist) {
    GameEngine engine(card_db, events, card_registry);
    auto [source, gear] = buildScene(engine);
    auto& s = engine.mutableState();
    EffectExecutor exec(s, events, card_db, &card_registry);
    exec.takeControlUntilSourceLeaves(gear, PlayerId::Player1, source);

    // Source object removed entirely (e.g. a token that ceased to exist).
    s.objects.erase(source);

    engine.revertLapsedControl();

    auto& g = s.getObject(gear);
    EXPECT_EQ(g.controller, PlayerId::Player2);
    EXPECT_FALSE(g.control_reverts_on_source_leave);
}

}  // namespace
}  // namespace riftbound::test
