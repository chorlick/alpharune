#include "training/ring_logger.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <memory>
#include <mutex>

namespace riftbound::training {

namespace {

std::mutex g_registry_mu;
std::vector<RingLogger*> g_registry;

int64_t nowNs() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

} // namespace

RingLogger::RingLogger(int thread_id) : thread_id_(thread_id) {
    registerInstance(this);
}

RingLogger::~RingLogger() {
    unregisterInstance(this);
}

void RingLogger::record(int iter, int game, int decision,
                        const char* event, const char* ctx) {
    int64_t idx = write_count_.fetch_add(1, std::memory_order_relaxed);
    auto& e = ring_[idx % kSize];
    int64_t ns = nowNs();
    e.ns_since_epoch = ns;
    e.iter      = iter;
    e.game      = game;
    e.thread_id = thread_id_;
    e.decision  = decision;
    std::strncpy(e.event, event ? event : "", sizeof(e.event) - 1);
    e.event[sizeof(e.event) - 1] = 0;
    std::strncpy(e.ctx, ctx ? ctx : "", sizeof(e.ctx) - 1);
    e.ctx[sizeof(e.ctx) - 1] = 0;
    last_event_ns_.store(ns, std::memory_order_relaxed);
}

void RingLogger::touch() {
    last_event_ns_.store(nowNs(), std::memory_order_relaxed);
}

void RingLogger::dump(const std::string& path) const {
    std::lock_guard<std::mutex> g(dump_mu_);
    std::ofstream out(path);
    if (!out) return;

    int64_t total = write_count_.load(std::memory_order_relaxed);
    int64_t first = total > kSize ? total - kSize : 0;
    out << "# ring dump thread_id=" << thread_id_
        << " total_writes=" << total
        << " entries=" << (total - first) << "\n";
    for (int64_t i = first; i < total; ++i) {
        const auto& e = ring_[i % kSize];
        out << e.ns_since_epoch
            << " iter=" << e.iter
            << " game=" << e.game
            << " thr=" << e.thread_id
            << " dec=" << e.decision
            << " " << e.event
            << " | " << e.ctx << "\n";
    }
}

void RingLogger::registerInstance(RingLogger* l) {
    std::lock_guard<std::mutex> g(g_registry_mu);
    g_registry.push_back(l);
}

void RingLogger::unregisterInstance(RingLogger* l) {
    std::lock_guard<std::mutex> g(g_registry_mu);
    g_registry.erase(std::remove(g_registry.begin(), g_registry.end(), l),
                     g_registry.end());
}

std::vector<RingLogger*> RingLogger::allInstances() {
    std::lock_guard<std::mutex> g(g_registry_mu);
    return g_registry;
}

namespace {
std::atomic<int> g_next_thread_id{0};
}

RingLogger& threadRingLogger() {
    thread_local std::unique_ptr<RingLogger> tl;
    if (!tl) {
        tl = std::make_unique<RingLogger>(
            g_next_thread_id.fetch_add(1, std::memory_order_relaxed));
    }
    return *tl;
}

} // namespace riftbound::training
