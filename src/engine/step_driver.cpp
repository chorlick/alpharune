#include "step_driver.h"

namespace riftbound {

Intent StepDriver::selectAction(
    const GameState& state,
    const std::vector<Intent>& legal_actions) {

    // Always present a non-empty action list to the caller. The engine
    // *should* guard against empty legal-action lists at each call site,
    // but in practice a small fraction of decisions slip through.
    // Returning concede without going through the wait cycle would leave
    // the caller's waitForDecision() blocked forever.
    std::vector<Intent> normalized;
    const std::vector<Intent>* effective = &legal_actions;
    if (legal_actions.empty()) {
        normalized.push_back(Intent::concede(state.turn.turn_player));
        effective = &normalized;
    }

    int chosen;
    {
        std::unique_lock<std::mutex> lock(mu_);
        paused_legal_ = *effective;
        paused_player_ = state.turn.turn_player;
        at_decision_ = true;
        cv_main_.notify_one();

        cv_engine_.wait(lock, [&] { return pending_choice_.has_value(); });
        chosen = *pending_choice_;
        pending_choice_.reset();
        at_decision_ = false;
        // Intentionally NOT clearing paused_legal_. A racing caller may
        // inspect LegalActions/CurrentPlayer between this decision
        // returning and the next selectAction populating again; leaving
        // the previous decision's data stale-but-non-empty is harmless.
    }
    if (chosen < 0 || static_cast<size_t>(chosen) >= effective->size()) {
        chosen = 0;
    }
    return (*effective)[chosen];
}

void StepDriver::waitForDecision() {
    std::unique_lock<std::mutex> lock(mu_);
    cv_main_.wait(lock, [&] {
        return at_decision_ || done_.load(std::memory_order_acquire);
    });
}

void StepDriver::provideChoice(int legal_index) {
    {
        std::lock_guard<std::mutex> lock(mu_);
        pending_choice_ = legal_index;
        // Reset at_decision_ under the same lock that sets pending_choice_,
        // so the next waitForDecision blocks correctly until the engine
        // genuinely reaches the next decision. Without this, the next
        // waitForDecision returns immediately on the still-set at_decision_,
        // and the caller spins one extra cycle overwriting pending_choice_
        // before the engine reads it.
        at_decision_ = false;
    }
    cv_engine_.notify_one();
}

void StepDriver::markDone() {
    {
        std::lock_guard<std::mutex> lock(mu_);
        done_.store(true, std::memory_order_release);
    }
    cv_main_.notify_one();
}

std::vector<Intent> StepDriver::legalActionsSnapshot() const {
    std::lock_guard<std::mutex> lock(mu_);
    return paused_legal_;
}

PlayerId StepDriver::currentPlayerSnapshot() const {
    std::lock_guard<std::mutex> lock(mu_);
    return paused_player_;
}

} // namespace riftbound
