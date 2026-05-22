// Clone-cost breakdown. Walks the same checkpoint as the existing
// microbench, then runs three timed loops:
//
//   (A) Full Clone()         — measures the cost of the public API
//   (B) GameState alone      — raw deep-copy of just engineState()
//   (C) Clone + EnsureLive   — what an MCTS leaf that DOES descend pays
//
// Gives an apples-to-apples view of whether Clone() is already pinned
// at the GameState deep-copy floor (in which case "memcpy Clone" — i.e.
// std::make_unique<RiftboundState>(*this) via copy-ctor — buys nothing
// without GameState-level optimizations) or whether there's wrapper
// overhead worth shaving.
//
// Build:  cmake --build build-release --target riftbound_clone_breakdown
// Run:    ./build-release/src/openspiel/riftbound_clone_breakdown
//
// Same env vars as clone_microbench: RIFTBOUND_DECK1/2, RIFTBOUND_REGISTRY,
// RIFTBOUND_CHECKPOINT, RIFTBOUND_NUM_CLONES, RIFTBOUND_SEED.

#include "open_spiel/spiel.h"
#include "core/game_state.h"
#include "riftbound_state.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <numeric>
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

struct Stats {
    double min = 0, median = 0, mean = 0, p95 = 0, max = 0;
};

Stats summarize(std::vector<double> us) {
    std::sort(us.begin(), us.end());
    Stats s;
    s.min    = us.front();
    s.median = us[us.size() / 2];
    s.p95    = us[static_cast<size_t>(us.size() * 0.95)];
    s.max    = us.back();
    s.mean   = std::accumulate(us.begin(), us.end(), 0.0) / us.size();
    return s;
}

void print(const std::string& label, const Stats& s) {
    std::cout << "  " << label << "  min=" << s.min
              << " median=" << s.median
              << " mean=" << s.mean
              << " p95=" << s.p95
              << " max=" << s.max << " us\n";
}

} // namespace

int main() {
    const std::string deck1    = getenvOr("RIFTBOUND_DECK1",    "decks/leblanc_test.json");
    const std::string deck2    = getenvOr("RIFTBOUND_DECK2",    "decks/leblanc_test.json");
    const std::string registry = getenvOr("RIFTBOUND_REGISTRY", "cards/registry.json");
    const int checkpoint       = std::max(1, getenvInt("RIFTBOUND_CHECKPOINT", 100));
    const int num_clones       = std::max(1, getenvInt("RIFTBOUND_NUM_CLONES", 200));
    const int seed             = getenvInt("RIFTBOUND_SEED", 42);

    const std::string game_str = "riftbound(deck1=" + deck1 +
                                  ",deck2=" + deck2 +
                                  ",registry=" + registry +
                                  ",seed=" + std::to_string(seed) + ")";

    std::cout << "Loading: " << game_str << "\n";
    auto game = ::open_spiel::LoadGame(game_str);

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
    std::cout << "Walked " << walked << " decisions to checkpoint.\n\n";

    auto* rb_state =
        static_cast<::riftbound::openspiel::RiftboundState*>(state.get());

    // (A) Full Clone()
    std::vector<double> us_clone(num_clones);
    for (int i = 0; i < num_clones; ++i) {
        auto t0 = std::chrono::steady_clock::now();
        auto clone = state->Clone();
        auto t1 = std::chrono::steady_clock::now();
        us_clone[i] = std::chrono::duration<double, std::micro>(t1 - t0).count();
        // Touch so the optimizer can't elide.
        if (clone->CurrentPlayer() == ::open_spiel::kInvalidPlayer) {
            std::cerr << "unreachable\n";
        }
    }

    // (B) Raw GameState deep-copy in isolation
    std::vector<double> us_gs(num_clones);
    for (int i = 0; i < num_clones; ++i) {
        auto t0 = std::chrono::steady_clock::now();
        ::riftbound::GameState gs_copy = rb_state->engineState();
        auto t1 = std::chrono::steady_clock::now();
        us_gs[i] = std::chrono::duration<double, std::micro>(t1 - t0).count();
        if (gs_copy.turn.turn_number < 0) {
            std::cerr << "unreachable\n";  // touch
        }
    }

    auto s_clone = summarize(us_clone);
    auto s_gs    = summarize(us_gs);

    std::cout << "=== " << num_clones << " calls at decision " << walked
              << " ===\n";
    print("(A) Full state->Clone()  ", s_clone);
    print("(B) GameState copy alone ", s_gs);
    std::cout << "\n";
    double overhead = s_clone.median - s_gs.median;
    double pct = (overhead / s_clone.median) * 100.0;
    std::cout << "Wrapper overhead (Clone - GameState copy): "
              << overhead << " us median (" << pct << "% of Clone)\n";

    return 0;
}
