// Clone-and-advance microbench. Distinct from clone_microbench.cpp which
// only measures the snapshot cost. This one measures Clone() + the FIRST
// ApplyAction on the clone — the actual hot path for MCTS / ISMCTS,
// where every expanded tree node pays both costs back-to-back.
//
// The first ApplyAction on a clone builds its engine, and there are two very
// different ways that happens (see riftbound_state.h):
//
//   resumed  — the checkpoint is a top-level main-phase decision; the engine
//              is resumed straight from the snapshot.
//   replayed — any other decision (chain priority, combat, cost payment, a
//              prompt, mulligan); the engine is rebuilt from the nearest
//              resumable ancestor by replaying the choices made since.
//
// It also times dropping the advanced clone (`~State`), which destroys an
// engine whose game is still in progress.
//
// One number for "ApplyAction" would mix the two depending on where the
// checkpoint happened to land, so the bench finds one checkpoint of each
// kind at or after RIFTBOUND_CHECKPOINT and reports them separately. The
// action applied to each clone is drawn at random from the legal set (not
// always the first, which is nearly always EndTurn / Pass).
//
// Build:  cmake --build build --target riftbound_clone_advance_microbench
// Run:    ./build/src/openspiel/riftbound_clone_advance_microbench
//
// Environment variables:
//   RIFTBOUND_DECK1, RIFTBOUND_DECK2 : deck paths
//   RIFTBOUND_CHECKPOINT             : decisions before the first checkpoint (default 100)
//   RIFTBOUND_NUM_CLONES             : clones per checkpoint (default 100)
//   RIFTBOUND_SEED                   : game seed (default 42)

#include "open_spiel/spiel.h"

#include "engine/game_engine.h"
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

bool isResumable(const ::open_spiel::State& state) {
    const auto& rb =
        static_cast<const ::riftbound::openspiel::RiftboundState&>(state);
    return ::riftbound::GameEngine::isResumableDecision(rb.engineState(),
                                                        rb.legalIntents());
}

void report(const char* label, std::vector<double>& us) {
    std::sort(us.begin(), us.end());
    double mean = std::accumulate(us.begin(), us.end(), 0.0) / us.size();
    std::cout << label << "  min=" << us.front()
              << "  median=" << us[us.size() / 2]
              << "  mean=" << mean
              << "  p95=" << us[static_cast<size_t>(us.size() * 0.95)]
              << "  max=" << us.back() << "\n";
}

/// Time `num_clones` Clone() + first-ApplyAction pairs at `state`.
void measure(const ::open_spiel::State& state, const char* path, int decision,
             int num_clones, std::mt19937_64& rng) {
    const auto legal = state.LegalActions();
    std::vector<double> us_clone(num_clones);
    std::vector<double> us_advance(num_clones);
    std::vector<double> us_destroy(num_clones);
    std::uniform_int_distribution<size_t> pick(0, legal.size() - 1);

    for (int i = 0; i < num_clones; ++i) {
        const auto action = legal[pick(rng)];
        auto t0 = std::chrono::steady_clock::now();
        auto clone = state.Clone();
        auto t1 = std::chrono::steady_clock::now();
        clone->ApplyAction(action);
        auto t2 = std::chrono::steady_clock::now();
        // Dropping an advanced clone tears down its engine while the game
        // is still in progress — what MCTS does with every simulation.
        clone.reset();
        auto t3 = std::chrono::steady_clock::now();
        us_clone[i]   = std::chrono::duration<double, std::micro>(t1 - t0).count();
        us_advance[i] = std::chrono::duration<double, std::micro>(t2 - t1).count();
        us_destroy[i] = std::chrono::duration<double, std::micro>(t3 - t2).count();
    }

    std::cout << "\n=== " << num_clones << " Clone()+ApplyAction pairs at decision "
              << decision << " — engine " << path << " ("
              << legal.size() << " legal actions) ===\n";
    report("Clone()       (us):", us_clone);
    report("ApplyAction   (us):", us_advance);
    report("~State        (us):", us_destroy);
}

} // namespace

int main() {
    const std::string deck1    = getenvOr("RIFTBOUND_DECK1",    "decks/yi_rebuild.txt");
    const std::string deck2    = getenvOr("RIFTBOUND_DECK2",    "decks/yi_rebuild.txt");
    const int checkpoint       = std::max(1, getenvInt("RIFTBOUND_CHECKPOINT", 100));
    const int num_clones       = std::max(1, getenvInt("RIFTBOUND_NUM_CLONES", 100));
    const int seed             = getenvInt("RIFTBOUND_SEED", 42);

    const std::string game_str = "riftbound(deck1=" + deck1 +
                                  ",deck2=" + deck2 +
                                  ",seed=" + std::to_string(seed) + ")";
    std::cout << "Loading: " << game_str << "\n";
    auto game = ::open_spiel::LoadGame(game_str);

    auto state = game->NewInitialState();
    std::mt19937_64 rng(static_cast<uint64_t>(seed));
    std::mt19937_64 bench_rng(static_cast<uint64_t>(seed) ^ 0x9E3779B97F4A7C15ull);

    // Walk forward; from the checkpoint on, measure the first decision of
    // each kind we come across.
    bool measured_resumed = false, measured_replayed = false;
    int walked = 0;
    while (!state->IsTerminal() && !(measured_resumed && measured_replayed)) {
        auto legal = state->LegalActions();
        if (legal.empty()) break;
        if (walked >= checkpoint) {
            const bool resumable = isResumable(*state);
            if (resumable && !measured_resumed) {
                measure(*state, "RESUMED from the snapshot", walked,
                        num_clones, bench_rng);
                measured_resumed = true;
            } else if (!resumable && !measured_replayed) {
                measure(*state, "REPLAYED from the nearest resumable ancestor",
                        walked, num_clones, bench_rng);
                measured_replayed = true;
            }
        }
        std::uniform_int_distribution<size_t> d(0, legal.size() - 1);
        state->ApplyAction(legal[d(rng)]);
        ++walked;
    }

    if (walked < checkpoint) {
        std::cerr << "ERROR: game ended pre-checkpoint at decision " << walked << "\n";
        return 1;
    }
    if (!measured_resumed) {
        std::cout << "\n(no resumable decision between the checkpoint and the "
                     "end of the game at decision " << walked << ")\n";
    }
    if (!measured_replayed) {
        std::cout << "\n(no replayed decision between the checkpoint and the "
                     "end of the game at decision " << walked << ")\n";
    }
    return (measured_resumed || measured_replayed) ? 0 : 1;
}
