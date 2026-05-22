#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace riftbound::training {

// Per-thread decision-level ring buffer. One writer (the owning worker
// thread), one reader (the watchdog, only on hang detection). Reads
// take a dump mutex; writes are lock-free relaxed atomics — under
// extreme contention a dump-during-write can produce one garbled
// entry, which we accept for debug-only output.
struct RingEntry {
    int64_t  ns_since_epoch = 0;
    int      iter           = -1;
    int      game           = -1;
    int      thread_id      = -1;
    int      decision       = -1;
    char     event[32]      = {};
    char     ctx[128]       = {};
};

class RingLogger {
public:
    static constexpr int kSize = 256;

    explicit RingLogger(int thread_id);
    ~RingLogger();

    // Lock-free record. Owning thread only.
    void record(int iter, int game, int decision,
                const char* event, const char* ctx);

    // Mark progress without writing an entry. Cheaper than record().
    void touch();

    int64_t lastEventNs() const { return last_event_ns_.load(std::memory_order_relaxed); }
    int threadId() const { return thread_id_; }

    // Dump ring contents to `path` (oldest-first). Takes dump_mu_.
    void dump(const std::string& path) const;

    // Global registry — watchdog iterates this.
    static void registerInstance(RingLogger* l);
    static void unregisterInstance(RingLogger* l);
    static std::vector<RingLogger*> allInstances();

private:
    int thread_id_;
    std::array<RingEntry, kSize> ring_;
    std::atomic<int64_t> write_count_{0};
    std::atomic<int64_t> last_event_ns_{0};
    mutable std::mutex dump_mu_;
};

// Thread-local helper: returns the logger for the current thread,
// lazily creating one (and assigning a fresh thread_id) on first call.
RingLogger& threadRingLogger();

} // namespace riftbound::training
