// Clone-and-advance microbench. Distinct from clone_microbench.cpp which
// only measures the snapshot cost. This one measures Clone() + the FIRST
// ApplyAction on the clone — the actual hot path for MCTS / ISMCTS,
// where every expanded tree node pays both costs back-to-back.
//
// Build:  cmake --build build-release --target riftbound_clone_advance_microbench
// Run:    ./build-release/src/openspiel/riftbound_clone_advance_microbench

#include "open_spiel/spiel.h"

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

    auto legal = state->LegalActions();
    if (legal.empty()) {
        std::cerr << "ERROR: no legal actions at checkpoint\n";
        return 1;
    }

    std::vector<double> us_clone(num_clones);
    std::vector<double> us_advance(num_clones);

    for (int i = 0; i < num_clones; ++i) {
        auto t0 = std::chrono::steady_clock::now();
        auto clone = state->Clone();
        auto t1 = std::chrono::steady_clock::now();
        clone->ApplyAction(legal.front());
        auto t2 = std::chrono::steady_clock::now();
        us_clone[i]   = std::chrono::duration<double, std::micro>(t1 - t0).count();
        us_advance[i] = std::chrono::duration<double, std::micro>(t2 - t1).count();
    }

    auto report = [](const char* label, std::vector<double>& us) {
        std::sort(us.begin(), us.end());
        double mean = std::accumulate(us.begin(), us.end(), 0.0) / us.size();
        std::cout << label << "  min=" << us.front()
                  << "  median=" << us[us.size() / 2]
                  << "  mean=" << mean
                  << "  p95=" << us[static_cast<size_t>(us.size() * 0.95)]
                  << "  max=" << us.back() << "\n";
    };

    std::cout << "\n=== " << num_clones << " Clone()+Advance pairs at decision "
              << walked << " ===\n";
    report("Clone()       (us):", us_clone);
    report("ApplyAction   (us):", us_advance);
    return 0;
}
