// Clone-equivalence test for RiftboundState.
//
// DoD #5 (Phase 11 C-1): from any decision point K, cloning + playing
// random actions to terminal reproduces a trajectory deterministically
// identical to running the same action sequence on the original.
//
// Concretely, for each game:
//   1. Walk forward N decisions on the original with a per-game RNG.
//   2. Clone the state.
//   3. Pick the SAME action sequence on original + clone (shared RNG seed
//      for continuation, but actions chosen against each state's own
//      LegalActions list).
//   4. Assert: same legal actions at every step, same terminal state,
//      same Returns().
//
// Returns 0 if all games pass equivalence, 1 if any diverge.
//
// Build:  cmake --build build-release --target riftbound_clone_equiv_test
// Run:    ./build-release/src/openspiel/riftbound_clone_equiv_test
//
// Environment variables:
//   RIFTBOUND_DECK1, RIFTBOUND_DECK2 : deck paths
//   RIFTBOUND_REGISTRY               : registry.json path
//   RIFTBOUND_NUM_GAMES              : games to play (default 10)
//   RIFTBOUND_CHECKPOINT             : decisions before cloning (default 50)
//   RIFTBOUND_MAX_POST_CLONE         : safety cap on continuation steps (default 1500)

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
    const int num_games        = std::max(1, getenvInt("RIFTBOUND_NUM_GAMES", 10));
    const int checkpoint       = std::max(1, getenvInt("RIFTBOUND_CHECKPOINT", 50));
    const int max_post_clone   = std::max(1, getenvInt("RIFTBOUND_MAX_POST_CLONE", 1500));

    const std::string game_str = "riftbound(deck1=" + deck1 +
                                  ",deck2=" + deck2 +
                                  ",registry=" + registry +
                                  ",seed=0)";

    std::cout << "Loading: " << game_str << "\n";
    auto game = ::open_spiel::LoadGame(game_str);

    int passed = 0;
    int skipped = 0;
    int failed = 0;
    auto t_start = std::chrono::steady_clock::now();

    for (int g = 0; g < num_games; ++g) {
        std::mt19937_64 walk_rng(static_cast<uint64_t>(g) * 1009 + 53);

        auto state = game->NewInitialState();

        // Walk forward to the checkpoint.
        int walked = 0;
        while (!state->IsTerminal() && walked < checkpoint) {
            auto legal = state->LegalActions();
            if (legal.empty()) break;
            std::uniform_int_distribution<size_t> d(0, legal.size() - 1);
            state->ApplyAction(legal[d(walk_rng)]);
            ++walked;
        }
        if (state->IsTerminal()) {
            std::cout << "  game " << (g+1) << ": ended pre-checkpoint at decision "
                      << walked << " — skip\n";
            ++skipped;
            continue;
        }

        // Clone at the checkpoint — measure throughput for DoD #1 tracking.
        auto t_clone_start = std::chrono::steady_clock::now();
        auto clone = state->Clone();
        auto clone_us = std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - t_clone_start).count();

        // Drive both forward with the same action choices.
        std::mt19937_64 cont_rng(static_cast<uint64_t>(g) * 7919 + 31);
        int steps = 0;
        bool diverged = false;

        while (!state->IsTerminal() && !clone->IsTerminal()
                && steps < max_post_clone) {
            auto legal_orig  = state->LegalActions();
            auto legal_clone = clone->LegalActions();

            if (legal_orig != legal_clone) {
                std::cerr << "  game " << (g+1) << " step " << steps
                          << ": LegalActions divergence (orig.size="
                          << legal_orig.size() << ", clone.size="
                          << legal_clone.size() << ")\n";
                diverged = true;
                break;
            }
            if (legal_orig.empty()) break;

            std::uniform_int_distribution<size_t> d(0, legal_orig.size() - 1);
            auto choice = legal_orig[d(cont_rng)];

            state->ApplyAction(choice);
            clone->ApplyAction(choice);
            ++steps;
        }

        if (diverged) {
            ++failed;
            continue;
        }
        if (state->IsTerminal() != clone->IsTerminal()) {
            std::cerr << "  game " << (g+1) << ": terminal mismatch ("
                      << state->IsTerminal() << " vs "
                      << clone->IsTerminal() << ")\n";
            ++failed;
            continue;
        }
        auto r_orig  = state->Returns();
        auto r_clone = clone->Returns();
        if (r_orig != r_clone) {
            std::cerr << "  game " << (g+1) << ": Returns mismatch ("
                      << r_orig[0] << "," << r_orig[1] << ") vs ("
                      << r_clone[0] << "," << r_clone[1] << ")\n";
            ++failed;
            continue;
        }
        std::cout << "  game " << (g+1) << ": OK ("
                  << walked << " walked, " << steps << " post-clone, "
                  << (state->IsTerminal() ? "terminal" : "capped")
                  << ", clone=" << clone_us << " us)\n";
        ++passed;
    }

    auto elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - t_start).count();

    std::cout << "\nClone-equivalence: " << passed << "/" << num_games
              << " passed, " << skipped << " skipped, "
              << failed << " failed in " << elapsed << "s\n";

    return failed == 0 ? 0 : 1;
}
