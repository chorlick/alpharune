// Clone microbench. Drives a state to a checkpoint, then clones N times
// in a tight loop without advancing any clone (simulating MCTS leaves
// that get cloned but never descended into). Reports min / median /
// p95 / max wall time per Clone() call.
//
// Build:  cmake --build build-release --target riftbound_clone_microbench
// Run:    ./build-release/src/openspiel/riftbound_clone_microbench
//
// Environment variables:
//   RIFTBOUND_DECK1, RIFTBOUND_DECK2 : deck paths
//   RIFTBOUND_REGISTRY               : registry.json path
//   RIFTBOUND_CHECKPOINT             : decisions before cloning (default 100)
//   RIFTBOUND_NUM_CLONES             : clones to make (default 100)
//   RIFTBOUND_SEED                   : game seed (default 42)

#include "open_spiel/spiel.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace {

std::string getenvOr(const char* name, std::string fallback) {
    if (const char* v = std::getenv(name)) return v;
    return fallback;
}

int getenvInt(const char* name, int fallback) {
    if (const char* v = std::getenv(name)) {
        try { return std::stoi(v); } catch (...) {}
    }
    return fallback;
}

} // namespace

int main() {
    const std::string deck1    = getenvOr("RIFTBOUND_DECK1",    "decks/leblanc_test.txt");
    const std::string deck2    = getenvOr("RIFTBOUND_DECK2",    "decks/leblanc_test.txt");
    const std::string registry = getenvOr("RIFTBOUND_REGISTRY", "cards/ban-list.csv");
    const int checkpoint       = std::max(1, getenvInt("RIFTBOUND_CHECKPOINT", 100));
    const int num_clones       = std::max(1, getenvInt("RIFTBOUND_NUM_CLONES", 100));
    const int seed             = getenvInt("RIFTBOUND_SEED", 42);

    const std::string game_str = "riftbound(deck1=" + deck1 +
                                  ",deck2=" + deck2 +
                                  ",registry=" + registry +
                                  ",seed=" + std::to_string(seed) + ")";

    std::cout << "Loading: " << game_str << "\n";
    auto game = ::open_spiel::LoadGame(game_str);

    // Walk to the checkpoint.
    auto state = game->NewInitialState();
    std::mt19937_64 rng(static_cast<uint64_t>(seed));
    int walked = 0;
    while (!state->IsTerminal() && walked < checkpoint) {
        auto legal = state->LegalActions();
        if (legal.empty()) break;
        std::uniform_int_distribution<size_t> d(0, legal.size() - 1);
        state->ApplyAction(legal[d(rng)]);
        ++walked;
    }
    if (state->IsTerminal()) {
        std::cerr << "ERROR: game ended pre-checkpoint at decision " << walked << "\n";
        return 1;
    }
    std::cout << "Walked " << walked << " decisions to checkpoint.\n";

    // Time N clones — every clone is fresh from the snapshot, none advanced.
    std::vector<double> us(num_clones);
    auto t_all_start = std::chrono::steady_clock::now();
    for (int i = 0; i < num_clones; ++i) {
        auto t0 = std::chrono::steady_clock::now();
        auto clone = state->Clone();
        auto t1 = std::chrono::steady_clock::now();
        us[i] = std::chrono::duration<double, std::micro>(t1 - t0).count();
        // Touch the clone so the optimizer can't elide it. Reading
        // CurrentPlayer is a snapshot-mode field read in the lazy path —
        // no engine spawn.
        if (clone->CurrentPlayer() == ::open_spiel::kInvalidPlayer) {
            std::cerr << "unreachable: terminal clone\n";
        }
    }
    auto t_all_end = std::chrono::steady_clock::now();
    double total_ms = std::chrono::duration<double, std::milli>(
        t_all_end - t_all_start).count();

    std::sort(us.begin(), us.end());
    double min    = us.front();
    double median = us[us.size() / 2];
    double p95    = us[static_cast<size_t>(us.size() * 0.95)];
    double max    = us.back();
    double mean   = std::accumulate(us.begin(), us.end(), 0.0) / us.size();

    std::cout << "\n=== " << num_clones << " Clone() calls at decision "
              << walked << " ===\n";
    std::cout << "Total wall time : " << total_ms << " ms\n";
    std::cout << "Per-clone (us):  min=" << min
              << " median=" << median
              << " mean=" << mean
              << " p95=" << p95
              << " max=" << max << "\n";

    // DoD: <100ms total for 100 clones at decision 100.
    if (num_clones == 100 && walked >= 100 && total_ms >= 100.0) {
        std::cerr << "FAIL: " << total_ms << " ms exceeds 100 ms budget\n";
        return 1;
    }
    std::cout << "OK\n";
    return 0;
}
