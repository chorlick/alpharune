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
        it.chosen_value = i;   // makes each option distinguishable
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

// 1.10: an out-of-range index used to be applied as action 0. It is not an
// answer to this decision, so it is refused and the engine keeps waiting.
TEST(HumanAgent, OutOfRangeIndexIsRefused) {
    HumanAgent agent;
    auto state = makeState(PlayerId::Player1);
    auto legal = makeLegal(PlayerId::Player1, 2);

    std::atomic<bool> returned{false};
    Intent chosen;
    std::thread engine([&] {
        chosen = agent.selectAction(state, legal);
        returned.store(true);
    });
    ASSERT_TRUE(agent.waitForDecision(2s));
    EXPECT_FALSE(agent.setChosenAction(99));
    EXPECT_FALSE(agent.setChosenAction(-1));
    std::this_thread::sleep_for(50ms);
    EXPECT_FALSE(returned.load());
    EXPECT_TRUE(agent.atDecision());

    EXPECT_TRUE(agent.setChosenAction(1));
    engine.join();
    EXPECT_EQ(chosen, legal[1]);
}

// 1.10 / 4.3: a click made while the engine is NOT waiting (a double click
// that lands after the first was consumed) must not answer the next decision.
TEST(HumanAgent, ChoiceSentBetweenDecisionsIsDropped) {
    HumanAgent agent;
    auto state = makeState(PlayerId::Player1);
    auto legal = makeLegal(PlayerId::Player1, 3);

    std::thread engine1([&] { agent.selectAction(state, legal); });
    ASSERT_TRUE(agent.waitForDecision(2s));
    EXPECT_TRUE(agent.setChosenAction(0));
    // Second click for the same decision: it already has its answer.
    EXPECT_FALSE(agent.setChosenAction(2));
    engine1.join();

    // Nothing is pending now; this click belongs to no decision.
    EXPECT_FALSE(agent.atDecision());
    EXPECT_FALSE(agent.setChosenAction(2));

    std::atomic<bool> returned{false};
    Intent chosen;
    std::thread engine2([&] {
        chosen = agent.selectAction(state, legal);
        returned.store(true);
    });
    ASSERT_TRUE(agent.waitForDecision(2s));
    std::this_thread::sleep_for(50ms);
    EXPECT_FALSE(returned.load()) << "a stale click answered the next decision";
    EXPECT_TRUE(agent.setChosenAction(1));
    engine2.join();
    EXPECT_EQ(chosen, legal[1]);
}

// 1.10 / 4.3: a choice tagged with another decision's id is refused.
TEST(HumanAgent, ChoiceForAnotherDecisionIdIsRefused) {
    HumanAgent agent;
    auto state = makeState(PlayerId::Player1);
    auto legal = makeLegal(PlayerId::Player1, 3);

    std::thread engine1([&] { agent.selectAction(state, legal); });
    ASSERT_TRUE(agent.waitForDecision(2s));
    const uint64_t first_id = agent.decisionId();
    EXPECT_TRUE(agent.setChosenAction(0, first_id));
    engine1.join();

    Intent chosen;
    std::thread engine2([&] { chosen = agent.selectAction(state, legal); });
    ASSERT_TRUE(agent.waitForDecision(2s));
    EXPECT_NE(agent.decisionId(), first_id);
    EXPECT_FALSE(agent.setChosenAction(2, first_id));
    EXPECT_TRUE(agent.atDecision());
    EXPECT_TRUE(agent.setChosenAction(1, agent.decisionId()));
    engine2.join();
    EXPECT_EQ(chosen, legal[1]);
}

// 1.10: the decision belongs to the player the engine is asking, who is not
// the turn player on chain priority / showdown focus / combat damage. Both
// pendingPlayer() and an abort's concede must name that player.
TEST(HumanAgent, PendingPlayerAndAbortUseTheDecidingSeat) {
    HumanAgent agent;
    auto state = makeState(PlayerId::Player1);           // P1's turn
    auto legal = makeLegal(PlayerId::Player2, 2);        // P2 is asked

    Intent chosen;
    std::thread engine([&] { chosen = agent.selectAction(state, legal); });
    ASSERT_TRUE(agent.waitForDecision(2s));
    EXPECT_EQ(agent.pendingPlayer(), PlayerId::Player2);
    agent.abort();
    engine.join();
    EXPECT_EQ(chosen.type, IntentType::Concede);
    EXPECT_EQ(chosen.player, PlayerId::Player2);

    // Still aborted: later decisions concede for whoever is asked.
    auto again = agent.selectAction(state, legal);
    EXPECT_EQ(again.type, IntentType::Concede);
    EXPECT_EQ(again.player, PlayerId::Player2);
}

// A seat given at construction wins over the Intents: the engine can offer a
// seat options that carry another player's name.
TEST(HumanAgent, ConfiguredSeatIsThePendingPlayerAndTheConceder) {
    HumanAgent agent(PlayerId::Player2);
    auto state = makeState(PlayerId::Player2);
    auto legal = makeLegal(PlayerId::Player1, 2);

    Intent chosen;
    std::thread engine([&] { chosen = agent.selectAction(state, legal); });
    ASSERT_TRUE(agent.waitForDecision(2s));
    EXPECT_EQ(agent.pendingPlayer(), PlayerId::Player2);
    agent.abort();
    engine.join();
    EXPECT_EQ(chosen.type, IntentType::Concede);
    EXPECT_EQ(chosen.player, PlayerId::Player2);
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
