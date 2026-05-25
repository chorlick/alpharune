#pragma once
/// @file step_driver.h
/// Internal halting agent used by GameEngine to expose its push-driven
/// runGame() loop through a pull-driven step-machine API
/// (beginGame / applyChoice / currentStep — see game_engine.h).
///
/// Phase 6w refactor — was `std::thread`-based with mutex+condvar; is
/// now a `boost::context::fiber` (userspace coroutine). Same public
/// API, but no OS thread is spawned. Context switches are ~50ns
/// (vs ~10μs for thread create/destroy), and there's no kernel
/// thread limit to exhaust.
///
/// Why this matters: branching CFR algorithms (ReBeL, External-sampling
/// MCCFR, vanilla CFR) clone the state and call ApplyAction on each
/// branch. Each Clone+ApplyAction previously spawned a thread; ~600
/// clones per decision × 300 decisions = 180K threads → system
/// exhaustion. With fibers, the same workload uses one ~64KB stack
/// per live fiber and switches instantly.
///
/// The fiber runs runGame() in the caller's thread. When the engine
/// reaches a decision (calls selectAction), it yields back to the
/// caller via fiber switch. When the caller calls provideChoice, the
/// fiber resumes and selectAction returns the chosen action.

#include "agents/agent_interface.h"

#include <boost/context/fiber.hpp>

#include <atomic>
#include <optional>
#include <vector>

namespace riftbound {

class StepDriver : public AgentInterface {
public:
    StepDriver() = default;
    ~StepDriver();

    // Move-only — fibers can't be copied.
    StepDriver(const StepDriver&) = delete;
    StepDriver& operator=(const StepDriver&) = delete;
    StepDriver(StepDriver&&) = delete;
    StepDriver& operator=(StepDriver&&) = delete;

    Intent selectAction(
        const GameState& state,
        const std::vector<Intent>& legal_actions
    ) override;

    /// Launch the engine fiber running `game_fn`. After this returns,
    /// the engine has either reached the first decision (waitForDecision
    /// will return immediately) or completed (isDone will be true).
    void runInFiber(std::function<void()> game_fn);

    /// Synchronous "wait" — for the fiber backend, this is a no-op
    /// because selectAction's yield already returned us here. Kept
    /// for API compat with the old thread-based driver.
    void waitForDecision();

    /// Wake the engine with the chosen legal-action index. Returns
    /// when the engine has either reached the next decision or
    /// terminated.
    void provideChoice(int legal_index);

    /// Mark the engine as terminated. Called from the fiber once
    /// runGame() returns.
    void markDone();

    /// Snapshot of the engine's pause-point state. Valid between a
    /// fiber yield (selectAction) and the next provideChoice().
    std::vector<Intent> legalActionsSnapshot() const { return paused_legal_; }
    PlayerId currentPlayerSnapshot() const { return paused_player_; }

    /// Lock-free check.
    bool isDone() const { return done_.load(std::memory_order_acquire); }

private:
    // The engine fiber runs runGame(). engine_fiber_ holds the
    // suspended engine context when control is in the caller's flow.
    // yield_target_ holds the caller's suspended context when control
    // is inside the engine fiber. Both are moved-into and moved-out
    // by every fiber switch because boost::context::fiber::resume()
    // is consume-and-return.
    boost::context::fiber engine_fiber_;
    boost::context::fiber yield_target_;

    // Pause-point snapshot (set by selectAction before yielding).
    std::vector<Intent> paused_legal_;
    PlayerId            paused_player_ = PlayerId::None;

    // Caller's choice for the next selectAction return.
    std::optional<int>  pending_choice_;

    // True when the engine has yielded at a decision; false when
    // the engine is running.
    bool at_decision_ = false;

    // True when the engine fiber has run to completion (runGame
    // returned). Atomic because external code may poll while the
    // fiber is suspended.
    std::atomic<bool>   done_{false};
};

} // namespace riftbound
