#pragma once
/// @file progress_bar.h
/// Minimal ANSI-escape progress bar for batch runs. tqdm-style format:
///
///   [=========>            ] 17/100 (17%) | 0.42 g/s | ETA 3m12s | <suffix>
///                                                                  └─ live stats
///
/// Thread-safe via an internal mutex — multiple worker threads can call
/// update() / setSuffix() / markDecisionStart() concurrently. Output
/// goes to stderr so it doesn't pollute stdout (which carries per-game
/// results / final summary). Carriage return rewrites the same line; no
/// newline until finish().
///
/// Live updates: a background ticker thread re-renders the bar every
/// kTickMs even when no update() is called. Useful when a single
/// MCTS-think takes seconds — the elapsed-on-current-decision counter
/// keeps moving so the user can see the binary isn't hung.
///
/// Suppression:
///   - construct with show_bar=false to disable all output (and the ticker)
///   - never output anything if std::cerr isn't a TTY (auto-detect via
///     isatty(STDERR_FILENO))

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

#include <unistd.h>

namespace riftbound {

class ProgressBar {
public:
    ProgressBar(int total, bool show_bar, std::string suffix = "")
        : total_(total),
          show_bar_(show_bar && isatty(STDERR_FILENO) && total > 0),
          suffix_(std::move(suffix)),
          start_(std::chrono::steady_clock::now()),
          last_render_(start_) {
        if (show_bar_) {
            // Background ticker: repaints the bar every kTickMs so the
            // "thinking N.Ns" counter advances during long MCTS thinks
            // even though no game has completed.
            ticker_ = std::thread([this] { tickerLoop(); });
        }
    }

    /// Bump the completion counter + maybe re-render the bar (rate-limited
    /// to once per 100ms to keep terminal output reasonable under high
    /// throughput).
    void update(int delta = 1) {
        int done = (done_.fetch_add(delta) + delta);
        if (!show_bar_) return;
        std::lock_guard<std::mutex> lk(mu_);
        auto now = std::chrono::steady_clock::now();
        double since = std::chrono::duration<double>(now - last_render_).count();
        if (since < 0.1 && done < total_) return;
        last_render_ = now;
        renderUnderLock(done, now);
    }

    /// Replace the right-hand "stats" string. Thread-safe; next render
    /// picks it up. Callers typically rebuild this after each completed
    /// game with running W/L/D + avg decisions/game.
    void setSuffix(std::string suffix) {
        if (!show_bar_) return;
        std::lock_guard<std::mutex> lk(mu_);
        suffix_ = std::move(suffix);
    }

    /// Install a callback that builds a live-stats sub-string evaluated
    /// on every render (ticker repaint AND update()). Useful for "game
    /// is in progress, here's where we are" feedback that can't wait
    /// for game-completion (e.g. live decision counter when num_games=1).
    /// The callback runs under the bar's mutex — keep it cheap.
    void setLiveStatsFn(std::function<std::string()> fn) {
        if (!show_bar_) return;
        std::lock_guard<std::mutex> lk(mu_);
        live_stats_fn_ = std::move(fn);
    }

    /// Mark "agent is thinking" so the bar can show elapsed-on-decision.
    /// `label_fn` is re-invoked on each render — return a fresh suffix
    /// string (e.g. "P2:mcts sim 42/200") so the bar reflects live MCTS
    /// progress as sims complete. Pass a lambda that closes over an
    /// atomic counter; the bar's tick thread will call it under lock.
    /// Pair with markDecisionEnd() right after the agent's choice call
    /// returns. If `label_fn` is empty the indicator only shows elapsed
    /// time (no sub-label).
    void markDecisionStart(std::function<std::string()> label_fn) {
        if (!show_bar_) return;
        std::lock_guard<std::mutex> lk(mu_);
        active_decision_ = true;
        decision_label_fn_ = std::move(label_fn);
        decision_start_ = std::chrono::steady_clock::now();
    }

    /// Backward-compatible overload: static label, no live updates.
    void markDecisionStart(std::string label) {
        markDecisionStart([s = std::move(label)] { return s; });
    }

    void markDecisionEnd() {
        if (!show_bar_) return;
        std::lock_guard<std::mutex> lk(mu_);
        active_decision_ = false;
        decision_label_fn_ = {};
    }

    /// Force one final render + newline. Safe to call multiple times.
    void finish() {
        if (!show_bar_) return;
        bool was_finished;
        {
            std::lock_guard<std::mutex> lk(mu_);
            was_finished = finished_;
            if (!was_finished) {
                finished_ = true;
                renderUnderLock(done_.load(), std::chrono::steady_clock::now());
                std::cerr << "\n" << std::flush;
            }
        }
        cv_.notify_all();
        if (ticker_.joinable()) ticker_.join();
    }

    ~ProgressBar() { finish(); }

private:
    static constexpr int kTickMs = 500;

    void tickerLoop() {
        std::unique_lock<std::mutex> lk(mu_);
        while (!finished_) {
            if (cv_.wait_for(lk, std::chrono::milliseconds(kTickMs),
                             [this] { return finished_; })) {
                break;
            }
            // Repaint without changing done_ — surfaces live thinking-time.
            renderUnderLock(done_.load(), std::chrono::steady_clock::now());
            last_render_ = std::chrono::steady_clock::now();
        }
    }

    void renderUnderLock(int done,
                          std::chrono::steady_clock::time_point now) {
        constexpr int kBarWidth = 30;
        double frac = total_ > 0 ? double(done) / total_ : 0.0;
        if (frac > 1.0) frac = 1.0;
        int filled = static_cast<int>(frac * kBarWidth);
        std::ostringstream bar;
        bar << "\r[";
        for (int i = 0; i < kBarWidth; ++i) {
            if (i < filled)        bar << '=';
            else if (i == filled)  bar << '>';
            else                    bar << ' ';
        }
        bar << "] " << done << "/" << total_
            << " (" << static_cast<int>(frac * 100.0) << "%)";

        double elapsed = std::chrono::duration<double>(now - start_).count();
        if (elapsed > 0 && done > 0) {
            double rate = done / elapsed;
            int eta = rate > 0
                ? static_cast<int>((total_ - done) / rate) : 0;
            bar << " | " << std::fixed;
            bar.precision(2);
            bar << rate << " g/s | ETA " << (eta / 60) << "m"
                << (eta % 60) << "s";
        }
        if (!suffix_.empty()) bar << " | " << suffix_;
        if (live_stats_fn_) {
            std::string live = live_stats_fn_();
            if (!live.empty()) bar << " | " << live;
        }
        if (active_decision_) {
            // Always show the thinking indicator while a decision is in
            // flight — even short MCTS thinks at low sim counts benefit
            // from visible feedback. The 500ms tick rate naturally
            // limits flicker for sub-tick decisions.
            double think_s = std::chrono::duration<double>(
                now - decision_start_).count();
            bar << " | " << std::fixed;
            bar.precision(1);
            bar << "thinking " << think_s << "s";
            if (decision_label_fn_) {
                std::string lbl = decision_label_fn_();
                if (!lbl.empty()) bar << " [" << lbl << "]";
            }
        }
        // Pad to clear any trailing chars from the previous render.
        bar << std::string(8, ' ');
        std::cerr << bar.str() << std::flush;
    }

    int total_;
    bool show_bar_;
    std::string suffix_;
    std::chrono::steady_clock::time_point start_;
    std::chrono::steady_clock::time_point last_render_;
    std::atomic<int> done_{0};
    std::mutex mu_;
    std::condition_variable cv_;
    std::thread ticker_;
    bool finished_ = false;

    // Live in-flight decision tracking. `decision_label_fn_` is invoked
    // on each render so the label can carry live state (e.g. MCTS sim
    // count) — the callback must be thread-safe (typically an atomic
    // read).
    bool active_decision_ = false;
    std::function<std::string()> decision_label_fn_;
    std::chrono::steady_clock::time_point decision_start_;

    // Always-visible live stats — appended between suffix and thinking
    // indicator on every render so progress is visible even during
    // long single-game runs (where the game counter sits at 0/1).
    std::function<std::string()> live_stats_fn_;
};

} // namespace riftbound
