#include "training/watchdog.h"
#include "training/ring_logger.h"

#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <sstream>

namespace riftbound::training {

Watchdog::Watchdog(std::string dump_dir,
                   std::chrono::seconds timeout,
                   std::chrono::seconds poll_interval)
  : dump_dir_(std::move(dump_dir)),
    timeout_(timeout),
    poll_interval_(poll_interval) {}

Watchdog::~Watchdog() { stop(); }

void Watchdog::start() {
    if (running_.exchange(true)) return;
    t_ = std::thread([this] { run(); });
}

void Watchdog::stop() {
    if (!running_.exchange(false)) return;
    if (t_.joinable()) t_.join();
}

void Watchdog::run() {
    using clock = std::chrono::steady_clock;
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::create_directories(dump_dir_, ec);

    while (running_.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(poll_interval_);
        if (!running_.load(std::memory_order_relaxed)) break;

        int64_t now_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            clock::now().time_since_epoch()).count();
        int64_t timeout_ns = static_cast<int64_t>(timeout_.count()) * 1'000'000'000LL;

        auto instances = RingLogger::allInstances();
        for (auto* logger : instances) {
            int64_t last = logger->lastEventNs();
            if (last == 0) continue;          // never wrote — ignore
            if (now_ns - last <= timeout_ns) continue;

            if (tripped_.exchange(true)) {
                // already tripped — first trigger wins, just exit
                std::quick_exit(2);
            }

            std::time_t t = std::time(nullptr);
            std::ostringstream pfx;
            pfx << dump_dir_ << "/hang_" << t;

            for (auto* l : instances) {
                std::ostringstream path;
                path << pfx.str() << "_thr" << l->threadId() << ".log";
                l->dump(path.str());
            }
            std::cerr << "[WATCHDOG] HANG DETECTED on thread "
                      << logger->threadId() << " (idle "
                      << ((now_ns - last) / 1'000'000'000) << "s, threshold "
                      << timeout_.count() << "s).\n"
                      << "[WATCHDOG] Ring dumps at " << pfx.str()
                      << "_thr*.log\n"
                      << "[WATCHDOG] Aborting with exit=2 — wrapper will restart.\n";
            std::cerr.flush();
            std::quick_exit(2);
        }
    }
}

} // namespace riftbound::training
