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
/// call `selectAction`; multiple UI threads can call setters.
///
/// A choice is only ever applied to the decision it was made for:
/// `setChosenAction` is refused (returns false) when the engine is not
/// waiting, when the index is not one of the pending legal actions, when
/// a choice for this decision has already been accepted, or — for the
/// two-argument form — when `decision_id` is not the pending decision's.
/// A UI that shows the user a decision should send back the `decisionId()`
/// it rendered, so a click that arrives after the engine has moved on is
/// dropped instead of answering a later, different question.

#include "agent_interface.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <vector>

namespace riftbound {

class HumanAgent : public AgentInterface {
public:
    /// `seat` is the player this agent answers for. When given, it is what
    /// `pendingPlayer()` reports and whom an abort concedes for. Without it
    /// the agent goes by the player named on the offered Intents
    /// (`decidingPlayer`), which is the same player except where the engine
    /// offers a seat another player's options.
    explicit HumanAgent(PlayerId seat = PlayerId::None) : seat_(seat) {}

    /// Engine-side entry point. Publishes the pause-point snapshot
    /// for the UI to read, then blocks on `pending_choice_`. When the
    /// UI calls `setChosenAction(idx)`, this wakes and returns the
    /// `legal_actions[idx]` Intent.
    ///
    /// Defensive fallback: if the engine reaches a decision with an
    /// empty legal-action list (engine bug), returns Intent::concede
    /// without blocking — otherwise the UI would have nothing to choose
    /// from and the engine would hang forever. A concede (here, or after
    /// `abort()`) is made for the player being asked, who is not always
    /// the turn player.
    Intent selectAction(
        const GameState& state,
        const std::vector<Intent>& legal_actions
    ) override {
        const PlayerId decider = seat_ != PlayerId::None
            ? seat_ : decidingPlayer(state, legal_actions);
        if (legal_actions.empty() || aborted_.load(std::memory_order_acquire)) {
            return Intent::concede(decider);
        }
        // Every decision — including 1-option "forced" steps like the
        // final rune-exhaust in an exactly-affordable cost payment —
        // surfaces to the UI. Earlier we short-circuited
        // legal_actions.size() == 1 here to spare the player a click,
        // but that left forced cost-payment steps invisible: the user
        // couldn't see which rune the engine "auto-picked" on their
        // behalf. Per the user's spec, treat single-option decisions
        // identically to multi-option ones — the WS frame is still
        // pushed, the UI still renders a button, the player still
        // clicks (or watches the agent click in spectator mode).
        int chosen;
        {
            std::unique_lock<std::mutex> lock(mu_);
            // A click that raced in after the previous decision was answered
            // belongs to that decision, not this one.
            pending_choice_.reset();
            paused_legal_  = legal_actions;
            paused_player_ = decider;
            decision_id_.fetch_add(1, std::memory_order_release);
            at_decision_   = true;
            cv_ui_.notify_all();

            cv_engine_.wait(lock, [&] {
                return pending_choice_.has_value() || aborted_.load(std::memory_order_acquire);
            });
            if (aborted_.load(std::memory_order_acquire)) {
                at_decision_ = false;
                pending_choice_.reset();
                return Intent::concede(decider);
            }
            chosen = *pending_choice_;
            pending_choice_.reset();
            at_decision_ = false;
        }
        // setChosenAction only accepts an index into paused_legal_.
        return legal_actions[static_cast<size_t>(chosen)];
    }

    /// Wake the engine thread with a forced concede. Used by the UI
    /// binary when the user signals shutdown — the engine would
    /// otherwise sit in `cv_engine_.wait()` indefinitely. Idempotent.
    /// After `abort()`, every subsequent `selectAction` call also
    /// concedes, so the engine drains out cleanly.
    void abort() {
        aborted_.store(true, std::memory_order_release);
        cv_engine_.notify_all();
    }

    /// UI-side: answer the pending decision with a legal-action index.
    /// Returns true if the choice was accepted and the engine woken; false
    /// if it was dropped — the engine is not waiting on a decision, the
    /// index is out of range, or this decision already has an answer.
    bool setChosenAction(int legal_index) {
        {
            std::lock_guard<std::mutex> lock(mu_);
            if (!acceptsLocked(legal_index)) return false;
            pending_choice_ = legal_index;
        }
        cv_engine_.notify_one();
        return true;
    }

    /// As above, and additionally refused unless `decision_id` is the
    /// pending decision's `decisionId()` — i.e. unless the choice was made
    /// for the decision the engine is actually waiting on.
    bool setChosenAction(int legal_index, uint64_t decision_id) {
        {
            std::lock_guard<std::mutex> lock(mu_);
            if (decision_id != decision_id_.load(std::memory_order_acquire)) {
                return false;
            }
            if (!acceptsLocked(legal_index)) return false;
            pending_choice_ = legal_index;
        }
        cv_engine_.notify_one();
        return true;
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

    /// UI-side: is the engine parked in `selectAction` right now?
    bool atDecision() const {
        std::lock_guard<std::mutex> lock(mu_);
        return at_decision_;
    }

private:
    /// Caller holds `mu_`.
    bool acceptsLocked(int legal_index) const {
        return at_decision_ && !pending_choice_.has_value() &&
               legal_index >= 0 &&
               static_cast<size_t>(legal_index) < paused_legal_.size();
    }

    mutable std::mutex      mu_;
    std::condition_variable cv_engine_;   // engine thread waits here
    mutable std::condition_variable cv_ui_; // UI threads wait here

    std::vector<Intent>     paused_legal_;
    PlayerId                paused_player_ = PlayerId::None;
    std::optional<int>      pending_choice_;
    bool                    at_decision_   = false;
    std::atomic<uint64_t>   decision_id_{0};
    std::atomic<bool>       aborted_{false};
    const PlayerId          seat_;
};

} // namespace riftbound
