// Demo: load Riftbound via OpenSpiel and play random rollouts.
//
// Build: cmake -B build -DRIFTBOUND_BUILD_OPENSPIEL=ON && cmake --build build
// Run:   ./build/src/openspiel/riftbound_openspiel_demo
//
// Phase A scope: random rollouts via the OpenSpiel pull-driven API,
// confirming game registration, halting-agent control-flow inversion,
// LegalActions/ApplyAction/Returns all work end-to-end against a real
// Riftbound engine. No MCTS, no ONNX, no information sets yet.
//
// Environment variables (all optional):
//   RIFTBOUND_DECK1, RIFTBOUND_DECK2 : deck paths
//   RIFTBOUND_REGISTRY               : registry.json path
//   RIFTBOUND_SEED                   : engine seed (0 = nondeterministic)
//   RIFTBOUND_NUM_GAMES              : number of games to play (default 1)
//   RIFTBOUND_THREADS                : parallel worker threads (default 1)

#include "open_spiel/spiel.h"

#include "riftbound_state.h"  // for engineResult()

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <random>
#include <string>
#include <thread>
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

struct Aggregates {
    std::atomic<int> p1_wins{0};
    std::atomic<int> p2_wins{0};
    std::atomic<int> draws{0};
    std::atomic<long long> total_decisions{0};      // counted by demo loop
    std::atomic<long long> total_engine_decisions{0}; // engine's own counter
    std::atomic<int> completed{0};
};

void runOneGame(const ::open_spiel::Game& game, uint64_t worker_seed,
                Aggregates& agg) {
    auto state = game.NewInitialState();
    // Per-player RNGs, matching BatchRunner's `game_seed*2 / *2+1` pattern.
    // A SHARED RNG (one stream for both players) creates correlated action
    // sequences that drive mirror games into symmetric stalemates: higher
    // draw rate, much longer games. Verified empirically against
    // BatchRunner — distributions only line up with per-player RNGs.
    std::mt19937_64 rng_p0(worker_seed * 2 + 0);
    std::mt19937_64 rng_p1(worker_seed * 2 + 1);
    int decisions = 0;
    while (!state->IsTerminal()) {
        auto legal = state->LegalActions();
        if (legal.empty()) {
            // Benign race: the game terminated between the IsTerminal()
            // check and LegalActions(). Exit cleanly.
            break;
        }
        auto& r = (state->CurrentPlayer() == 0) ? rng_p0 : rng_p1;
        std::uniform_int_distribution<size_t> dist(0, legal.size() - 1);
        state->ApplyAction(legal[dist(r)]);
        ++decisions;
    }
    auto ret = state->Returns();
    agg.total_decisions.fetch_add(decisions);
    // Cross-check against the engine's own decision counter — diverging
    // numbers point at either double-counting in the demo loop or
    // missed decisions in the engine.
    const auto* rb_state = static_cast<const ::riftbound::openspiel::RiftboundState*>(state.get());
    int eng_dec = rb_state->engineResult().total_decisions;
    agg.total_engine_decisions.fetch_add(eng_dec);
    if (decisions > eng_dec + 5 && decisions > 100) {
        std::cerr << "  [game] demo=" << decisions
                  << " engine=" << eng_dec
                  << " win=" << (ret[0]>0?"P1":(ret[1]>0?"P2":"D"))
                  << "\n";
    }
    if (ret[0] > 0)      agg.p1_wins.fetch_add(1);
    else if (ret[1] > 0) agg.p2_wins.fetch_add(1);
    else                 agg.draws.fetch_add(1);
    agg.completed.fetch_add(1);
}

} // namespace

int main(int /*argc*/, char** /*argv*/) {
    const std::string deck1    = getenvOr("RIFTBOUND_DECK1",    "decks/leblanc_test.json");
    const std::string deck2    = getenvOr("RIFTBOUND_DECK2",    "decks/leblanc_test.json");
    const std::string registry = getenvOr("RIFTBOUND_REGISTRY", "cards/registry.json");
    const std::string seed_str = getenvOr("RIFTBOUND_SEED",     "0");
    const int num_games        = std::max(1, getenvInt("RIFTBOUND_NUM_GAMES", 1));
    const int num_threads      = std::max(1, getenvInt("RIFTBOUND_THREADS",   1));

    const std::string game_str = std::string("riftbound(")
        + "deck1=" + deck1 + ","
        + "deck2=" + deck2 + ","
        + "registry=" + registry + ","
        + "seed=" + seed_str + ")";

    std::cout << "Loading: " << game_str << "\n";
    std::cout << "Plan: " << num_games << " games on "
              << num_threads << " threads\n";

    // Each RiftboundGame instance loads its own CardDB + decks (heavy).
    // We share one across all threads — Game is const after construction,
    // and NewInitialState() is the per-thread entry. CardDB / CardRegistry
    // are already designed as immutable-after-load shared singletons in the
    // existing engine, so concurrent reads from N threads are safe.
    auto game = ::open_spiel::LoadGame(game_str);
    std::cout << "Loaded: " << game->GetType().short_name << "\n";

    Aggregates agg;
    auto t_start = std::chrono::steady_clock::now();
    std::mt19937_64 master_seed_rng(std::random_device{}());

    std::vector<std::thread> workers;
    workers.reserve(num_threads);
    std::atomic<int> next_game_index{0};

    auto worker_fn = [&](uint64_t seed_offset) {
        std::mt19937_64 local_seed_rng(seed_offset);
        while (true) {
            int i = next_game_index.fetch_add(1);
            if (i >= num_games) break;
            runOneGame(*game, local_seed_rng(), agg);
        }
    };

    for (int t = 0; t < num_threads; ++t) {
        workers.emplace_back(worker_fn, master_seed_rng());
    }
    for (auto& w : workers) w.join();

    auto elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - t_start).count();
    int p1 = agg.p1_wins.load();
    int p2 = agg.p2_wins.load();
    int dr = agg.draws.load();
    int done = agg.completed.load();
    long long dec = agg.total_decisions.load();
    double gps = elapsed > 0 ? done / elapsed : 0;

    std::cout << "\nCompleted " << done << "/" << num_games << " games "
              << "in " << elapsed << "s "
              << "(" << gps << " games/sec)\n";
    if (done > 0) {
        std::cout << "  P1 wins: " << p1
                  << " (" << (100.0 * p1 / done) << "%)\n";
        std::cout << "  P2 wins: " << p2
                  << " (" << (100.0 * p2 / done) << "%)\n";
        std::cout << "  Draws:   " << dr
                  << " (" << (100.0 * dr / done) << "%)\n";
        std::cout << "  Avg decisions/game (demo loop):   " << (dec / done) << "\n";
        std::cout << "  Avg decisions/game (engine view): "
                  << (agg.total_engine_decisions.load() / done) << "\n";
    }
    return 0;
}
