#pragma once

#include <atomic>
#include <chrono>
#include <string>
#include <thread>

namespace riftbound::training {

// Polls all registered RingLogger instances every ~10s. If any one's
// last_event_ns exceeds `timeout`, dumps ALL rings to `dump_dir` then
// calls std::quick_exit(2) so the wrapper restarts the runner.
class Watchdog {
public:
    explicit Watchdog(std::string dump_dir,
                      std::chrono::seconds timeout = std::chrono::seconds(120),
                      std::chrono::seconds poll_interval = std::chrono::seconds(10));
    ~Watchdog();

    void start();
    void stop();

    bool tripped() const { return tripped_.load(); }

private:
    void run();

    std::string dump_dir_;
    std::chrono::seconds timeout_;
    std::chrono::seconds poll_interval_;
    std::atomic<bool> running_{false};
    std::atomic<bool> tripped_{false};
    std::thread t_;
};

} // namespace riftbound::training
