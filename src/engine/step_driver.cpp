#include "step_driver.h"

#include <boost/context/detail/exception.hpp>
#include <boost/context/protected_fixedsize_stack.hpp>

#include <cstdlib>
#include <stdexcept>
#include <string>
#include <utility>

namespace riftbound {

namespace {
// Per-fiber stack size. 256KB is plenty for the engine's deepest call
// stacks (chain resolution + triggers + cleanup) while keeping per-
// clone memory low. boost::context::protected_fixedsize_stack adds a
// guard page so stack overflow segfaults cleanly instead of corrupting
// the heap.
constexpr size_t kFiberStackBytes = 256 * 1024;

// Fault injection for checking that an engine failure is reported as a
// failure: RIFTBOUND_STEP_FAULT_AT=<n> makes every step-machine engine throw
// from inside its fiber at its n-th decision. 0 / unset = off.
long stepFaultAt() {
    static const long at = [] {
        const char* v = std::getenv("RIFTBOUND_STEP_FAULT_AT");
        return v ? std::atol(v) : 0L;
    }();
    return at;
}
}

StepDriver::~StepDriver() {
    // A fiber still parked at a decision belongs to a game nobody is going
    // to finish (a discarded search clone, an abandoned session). Destroying
    // a suspended boost.context fiber unwinds its stack: boost throws
    // `forced_unwind` inside the fiber, the engine frames parked there run
    // their destructors, and the stack is freed. Nothing of the game runs.
    //
    // Do it here rather than leaving it to member destruction, so every
    // member the fiber could refer to is still alive while it unwinds.
    //
    // (This used to drain the fiber by answering 0 to every decision until
    // the game ended — a whole rest-of-game, events and triggers included,
    // per abandoned engine. Measured per discarded search clone: ~445 us
    // of teardown then, ~36 us now; plans/06-performance.md item 6.7.)
    //
    // Requirement on engine code: a `catch (...)` that does not rethrow
    // would swallow `forced_unwind` and leave the fiber running with no one
    // to yield to. There is none; keep it that way (catch std::exception).
    boost::context::fiber abandoned = std::move(engine_fiber_);
}

void StepDriver::runInFiber(std::function<void()> game_fn) {
    if (engine_fiber_) {
        throw std::logic_error(
            "StepDriver::runInFiber called twice — fiber already started.");
    }
    // Capture the function by value into the fiber's lambda. The fiber
    // entry point takes the caller's continuation (the "main fiber")
    // as its first arg; calling `.resume()` on that continuation
    // suspends the engine and returns control to the caller.
    boost::context::protected_fixedsize_stack stack_alloc(kFiberStackBytes);
    engine_fiber_ = boost::context::fiber(
        std::allocator_arg, stack_alloc,
        [this, game_fn = std::move(game_fn)](boost::context::fiber&& caller) {
            // We can't store `caller` directly in a member because the
            // fiber's resume() consumes and returns a new continuation.
            // Instead, we route every yield through a thread-local
            // (effectively fiber-local since fibers don't migrate
            // threads). When selectAction wants to yield, it reads
            // this and calls .resume() on it.
            yield_target_ = std::move(caller);
            try {
                game_fn();
            } catch (const boost::context::detail::forced_unwind&) {
                // Boost unwinding a suspended fiber that is being destroyed:
                // not an error, and it must reach the fiber's own entry point.
                throw;
            } catch (...) {
                // The game did not finish. Keep the exception: GameEngine
                // rethrows it to the caller (applyChoice / currentStep) so a
                // crashed game is never mistaken for a completed one.
                failure_ = std::current_exception();
            }
            done_.store(true, std::memory_order_release);
            // Returning the yield_target hands control back to the
            // caller; this fiber is destroyed afterward.
            return std::move(yield_target_);
        });
    // Initial enter — runs until the first selectAction yield (or
    // until runGame returns immediately, in degenerate cases).
    engine_fiber_ = std::move(engine_fiber_).resume();
}

Intent StepDriver::selectAction(
    const GameState& state,
    const std::vector<Intent>& legal_actions) {
    return selectActionFor(decidingPlayer(state, legal_actions), state,
                           legal_actions);
}

Intent StepDriver::selectActionFor(
    PlayerId decider,
    const GameState& state,
    const std::vector<Intent>& legal_actions) {

    ++decisions_served_;
    if (stepFaultAt() > 0 && decisions_served_ == stepFaultAt()) {
        throw std::runtime_error(
            "RIFTBOUND_STEP_FAULT_AT: injected engine fault at decision " +
            std::to_string(decisions_served_));
    }
    (void)state;

    // Always present a non-empty action list to the caller. The engine
    // *should* guard against empty legal-action lists at each call site,
    // but in practice a small fraction of decisions slip through.
    std::vector<Intent> normalized;
    const std::vector<Intent>* effective = &legal_actions;
    if (legal_actions.empty()) {
        normalized.push_back(Intent::concede(decider));
        effective = &normalized;
    }

    // Publish the decision snapshot for the caller, then yield.
    paused_legal_   = effective;
    paused_player_  = decider;
    at_decision_    = true;
    pending_choice_.reset();

    // Yield to caller. When the caller calls provideChoice(), they
    // resume the engine fiber, which switches back here with
    // pending_choice_ populated.
    yield_target_ = std::move(yield_target_).resume();

    at_decision_ = false;
    paused_legal_ = nullptr;
    int chosen = pending_choice_.value_or(0);
    pending_choice_.reset();

    if (chosen < 0 || static_cast<size_t>(chosen) >= effective->size()) {
        chosen = 0;
    }
    return (*effective)[chosen];
}

void StepDriver::waitForDecision() {
    // Fiber backend: selectAction's yield ALREADY returned us here,
    // so there's nothing to wait for. The engine either yielded at a
    // decision (at_decision_ == true) or completed (done_ == true).
    // Keep this method for API compat with the previous thread-based
    // driver.
}

void StepDriver::provideChoice(int legal_index) {
    if (done_.load(std::memory_order_acquire)) {
        // Engine already finished. No-op — there's no fiber to resume.
        return;
    }
    pending_choice_ = legal_index;
    // Resume the engine fiber. It re-enters selectAction's body just
    // past the yield, reads pending_choice_, and runs forward until
    // the next selectAction yield or until runGame returns.
    engine_fiber_ = std::move(engine_fiber_).resume();
}

void StepDriver::markDone() {
    done_.store(true, std::memory_order_release);
}

} // namespace riftbound
