#pragma once
/// @file step_driver.h
/// Internal halting agent used by GameEngine to expose its push-driven
/// runGame() loop through a pull-driven step-machine API
/// (beginGame / applyChoice / currentStep — see game_engine.h).
///
/// Strategy. The engine still runs on a worker thread internally; this
/// agent suspends the engine at each decision point and yields control to
/// the caller. Same mechanism as the legacy
/// `riftbound::openspiel::HaltingAgent`, just lifted INTO the engine so
/// the OpenSpiel wrapper (and any other consumer) doesn't have to own a
/// thread+condvar of its own.
///
/// Phase 11 C-1 (in progress) replaces the internal threading with a
/// native resumable step machine. When that lands, this class is deleted
/// — the beginGame/applyChoice surface stays stable for callers.

#include "agents/agent_interface.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <vector>

namespace riftbound {

class StepDriver : public AgentInterface {
public:
    Intent selectAction(
        const GameState& state,
        const std::vector<Intent>& legal_actions
    ) override;

    /// Block until the engine reaches the next decision OR terminates.
    void waitForDecision();

    /// Wake the engine with the chosen legal-action index.
    void provideChoice(int legal_index);

    /// Mark the engine thread as terminated. Called from the worker
    /// thread once runGame() returns; unblocks any waitForDecision() so
    /// the caller sees isDone() == true.
    void markDone();

    /// Snapshot the engine's pause-point state. Returned under-lock
    /// copies avoid all data races with the engine thread. Valid call
    /// window: between waitForDecision() returning and the next
    /// provideChoice() call.
    std::vector<Intent> legalActionsSnapshot() const;
    PlayerId currentPlayerSnapshot() const;

    /// Lock-free check — done_ is atomic.
    bool isDone() const { return done_.load(std::memory_order_acquire); }

private:
    mutable std::mutex mu_;
    std::condition_variable cv_engine_;  // engine thread waits here
    std::condition_variable cv_main_;    // caller waits here
    std::optional<int> pending_choice_;
    bool at_decision_ = false;
    std::atomic<bool> done_{false};

    std::vector<Intent> paused_legal_;
    PlayerId paused_player_ = PlayerId::None;
};

} // namespace riftbound
