#pragma once
/// @file human_agent.h
/// Agent that blocks on an external decider — used to plug a human
/// player into the engine via whatever UI layer the binary provides
/// (Boost.Beast HTTP+WS server, terminal stdin loop, test fixture).
///
/// The engine thread calls `selectAction()` which blocks on a condvar.
/// Some other thread (the WS handler, the CLI input thread, a unit
/// test) calls `setChosenAction(int)` with the legal-action index the
/// human picked. The engine thread then wakes up and returns that
/// action.
///
/// Snapshot accessors (`pendingLegal`, `pendingPlayer`, `decisionId`)
/// let the UI layer poll for "is the engine waiting on a decision
/// right now, and if so what are the options?" without taking any
/// locks the engine thread holds.
///
/// This is the SAME pattern as the engine's internal StepDriver
/// (which yields a fiber instead of blocking a thread), but exposed
/// at the AgentInterface boundary so any UI driver can sit on top
/// of it without knowing engine internals.
///
/// Thread safety: `setChosenAction` and the snapshot accessors are
/// thread-safe with `selectAction`. Only one engine thread should
/// call `selectAction`; multiple UI threads can call setters, but
/// only one choice is meaningful per decision (last writer wins
/// before the engine wakes).

#include "agent_interface.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <vector>

namespace riftbound {

class HumanAgent : public AgentInterface {
public:
    HumanAgent() = default;

    /// Engine-side entry point. Publishes the pause-point snapshot
    /// for the UI to read, then blocks on `pending_choice_`. When the
    /// UI calls `setChosenAction(idx)`, this wakes and returns the
    /// `legal_actions[idx]` Intent.
    ///
    /// Defensive fallback: if the engine reaches a decision with an
    /// empty legal-action list (engine bug), returns Intent::concede
    /// for the current turn player without blocking — otherwise the
    /// UI would have nothing to choose from and the engine would
    /// hang forever.
    Intent selectAction(
        const GameState& state,
        const std::vector<Intent>& legal_actions
    ) override {
        if (legal_actions.empty()) {
            return Intent::concede(state.turn.turn_player);
        }
        int chosen;
        {
            std::unique_lock<std::mutex> lock(mu_);
            paused_legal_  = legal_actions;
            paused_player_ = state.turn.turn_player;
            decision_id_.fetch_add(1, std::memory_order_release);
            at_decision_   = true;
            cv_ui_.notify_all();

            cv_engine_.wait(lock, [&] { return pending_choice_.has_value(); });
            chosen = *pending_choice_;
            pending_choice_.reset();
            at_decision_ = false;
        }
        if (chosen < 0 || static_cast<size_t>(chosen) >= legal_actions.size()) {
            chosen = 0;   // out-of-range → safe fallback
        }
        return legal_actions[chosen];
    }

    /// UI-side: wake the engine with the chosen legal-action index.
    /// Idempotent up until the engine consumes it; subsequent calls
    /// before the next `selectAction` overwrite the pending choice.
    void setChosenAction(int legal_index) {
        {
            std::lock_guard<std::mutex> lock(mu_);
            pending_choice_ = legal_index;
        }
        cv_engine_.notify_one();
    }

    /// UI-side: block until the engine is paused at a decision. Useful
    /// for the WS handler to wait for the next state to push to the
    /// browser. Returns false if `timeout` elapsed without a decision.
    template <class Rep, class Period>
    bool waitForDecision(std::chrono::duration<Rep, Period> timeout) const {
        std::unique_lock<std::mutex> lock(mu_);
        return cv_ui_.wait_for(lock, timeout, [&] { return at_decision_; });
    }

    /// UI-side snapshot: legal actions at the current pause point.
    /// Valid between `waitForDecision` returning true and the next
    /// `setChosenAction`. Copy returned under the lock.
    std::vector<Intent> pendingLegal() const {
        std::lock_guard<std::mutex> lock(mu_);
        return paused_legal_;
    }

    /// UI-side snapshot: player to act at the current pause point.
    PlayerId pendingPlayer() const {
        std::lock_guard<std::mutex> lock(mu_);
        return paused_player_;
    }

    /// UI-side: monotonically increasing counter. Bumps each time
    /// `selectAction` reaches a new decision. Lets the UI detect
    /// "did the engine move past my last snapshot?" without race.
    uint64_t decisionId() const {
        return decision_id_.load(std::memory_order_acquire);
    }

    /// UI-side: lock-free check.
    bool atDecision() const {
        return at_decision_;
    }

private:
    mutable std::mutex      mu_;
    std::condition_variable cv_engine_;   // engine thread waits here
    mutable std::condition_variable cv_ui_; // UI threads wait here

    std::vector<Intent>     paused_legal_;
    PlayerId                paused_player_ = PlayerId::None;
    std::optional<int>      pending_choice_;
    bool                    at_decision_   = false;
    std::atomic<uint64_t>   decision_id_{0};
};

} // namespace riftbound
