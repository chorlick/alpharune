/// @file test_human_agent.cpp
/// Tests for HumanAgent — the AgentInterface that blocks on an
/// external decider (Beast WS handler, terminal driver, etc.).
///
/// Covers:
///   - selectAction blocks until setChosenAction is called
///   - Out-of-range indices fall back to 0 (defensive)
///   - Empty legal_actions returns Intent::concede without blocking
///   - decisionId monotonically increases per decision
///   - waitForDecision unblocks when engine reaches a pause point

#include "agents/human_agent.h"
#include "core/intent.h"

#include <gtest/gtest.h>

#include <chrono>
#include <thread>

using namespace riftbound;
using namespace std::chrono_literals;

namespace {

// Minimal GameState fixture — only fields HumanAgent actually reads.
GameState makeState(PlayerId p) {
    GameState s{};
    s.turn.turn_player = p;
    return s;
}

std::vector<Intent> makeLegal(PlayerId p, int n) {
    std::vector<Intent> out;
    out.reserve(n);
    for (int i = 0; i < n; ++i) {
        Intent it{};
        it.type   = IntentType::EndTurn;
        it.player = p;
        out.push_back(it);
    }
    return out;
}

} // namespace

TEST(HumanAgent, EmptyLegalReturnsConcedeWithoutBlocking) {
    HumanAgent agent;
    auto state = makeState(PlayerId::Player1);
    std::vector<Intent> empty;

    // No setChosenAction call — must not block.
    auto chosen = agent.selectAction(state, empty);
    EXPECT_EQ(chosen.type, IntentType::Concede);
    EXPECT_EQ(chosen.player, PlayerId::Player1);
    EXPECT_FALSE(agent.atDecision());
}

TEST(HumanAgent, SelectActionBlocksUntilSetChosenAction) {
    HumanAgent agent;
    auto state = makeState(PlayerId::Player2);
    auto legal = makeLegal(PlayerId::Player2, 3);

    // Engine thread blocks in selectAction.
    std::atomic<bool> returned{false};
    std::thread engine([&] {
        auto chosen = agent.selectAction(state, legal);
        EXPECT_EQ(chosen.player, PlayerId::Player2);
        returned.store(true);
    });

    // UI thread: wait for the decision to be published.
    ASSERT_TRUE(agent.waitForDecision(2s));
    EXPECT_TRUE(agent.atDecision());
    EXPECT_EQ(agent.pendingPlayer(), PlayerId::Player2);
    EXPECT_EQ(agent.pendingLegal().size(), 3u);
    EXPECT_FALSE(returned.load());   // engine still blocked

    // Provide the choice. Engine should wake.
    agent.setChosenAction(1);
    engine.join();
    EXPECT_TRUE(returned.load());
    EXPECT_FALSE(agent.atDecision());
}

TEST(HumanAgent, OutOfRangeIndexFallsBackToZero) {
    HumanAgent agent;
    auto state = makeState(PlayerId::Player1);
    auto legal = makeLegal(PlayerId::Player1, 2);

    std::thread engine([&] {
        auto chosen = agent.selectAction(state, legal);
        // Index 99 is OOR → falls back to legal_actions[0].
        EXPECT_EQ(chosen.type, legal[0].type);
    });
    ASSERT_TRUE(agent.waitForDecision(2s));
    agent.setChosenAction(99);
    engine.join();
}

TEST(HumanAgent, DecisionIdIncreasesPerDecision) {
    HumanAgent agent;
    auto state = makeState(PlayerId::Player1);
    auto legal = makeLegal(PlayerId::Player1, 2);

    EXPECT_EQ(agent.decisionId(), 0u);

    // First decision.
    std::thread engine1([&] { agent.selectAction(state, legal); });
    ASSERT_TRUE(agent.waitForDecision(2s));
    EXPECT_EQ(agent.decisionId(), 1u);
    agent.setChosenAction(0);
    engine1.join();

    // Second decision: same agent, decisionId bumps.
    std::thread engine2([&] { agent.selectAction(state, legal); });
    ASSERT_TRUE(agent.waitForDecision(2s));
    EXPECT_EQ(agent.decisionId(), 2u);
    agent.setChosenAction(0);
    engine2.join();
}

TEST(HumanAgent, WaitForDecisionTimesOutBeforeEngineArrives) {
    HumanAgent agent;
    // No engine thread → wait returns false on timeout.
    EXPECT_FALSE(agent.waitForDecision(50ms));
}
