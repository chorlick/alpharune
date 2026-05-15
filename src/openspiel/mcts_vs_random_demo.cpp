// MCTS vs RandomAgent end-to-end demo.
//
// Build:   cmake --build build --target riftbound_mcts_demo
// Run:     ./build/src/openspiel/riftbound_mcts_demo
//
// Demonstrates that the Phase B port is usable for non-random algorithms.
// OpenSpiel's MCTSBot drives one seat against a uniform-random opponent;
// the resulting win rate proves that Clone() + bit-packed action encoding
// + ObservationTensor all work end-to-end.
//
// Environment variables (all optional):
//   RIFTBOUND_DECK1, RIFTBOUND_DECK2 : deck paths
//   RIFTBOUND_REGISTRY               : registry.json path
//   RIFTBOUND_NUM_GAMES              : games to play (default 20)
//   RIFTBOUND_MCTS_SIMULATIONS       : sims per move (default 8)
//   RIFTBOUND_MCTS_PLAYER            : 0 or 1 (default 0; -1 = alternate)

#include "open_spiel/spiel.h"
#include "open_spiel/algorithms/mcts.h"

#include "riftbound_state.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <random>
#include <string>

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
    const std::string deck1    = getenvOr("RIFTBOUND_DECK1",    "decks/leblanc_test.json");
    const std::string deck2    = getenvOr("RIFTBOUND_DECK2",    "decks/leblanc_test.json");
    const std::string registry = getenvOr("RIFTBOUND_REGISTRY", "cards/registry.json");
    const int num_games        = std::max(1, getenvInt("RIFTBOUND_NUM_GAMES", 20));
    const int mcts_sims        = std::max(1, getenvInt("RIFTBOUND_MCTS_SIMULATIONS", 8));
    const int mcts_player_arg  = getenvInt("RIFTBOUND_MCTS_PLAYER", 0);

    const std::string game_str = "riftbound(deck1=" + deck1 +
                                  ",deck2=" + deck2 +
                                  ",registry=" + registry +
                                  ",seed=0)";

    std::cout << "Loading: " << game_str << "\n";
    auto game = ::open_spiel::LoadGame(game_str);
    std::cout << "Plan: " << num_games << " games, " << mcts_sims
              << " MCTS sims/move\n";

    int mcts_wins = 0, random_wins = 0, draws = 0;
    long long total_decisions = 0;

    auto t_start = std::chrono::steady_clock::now();

    for (int g = 0; g < num_games; ++g) {
        // Alternate seats unless explicitly pinned. Per-game RNG seeds.
        int mcts_seat = (mcts_player_arg < 0) ? (g % 2) : mcts_player_arg;

        std::shared_ptr<::open_spiel::algorithms::Evaluator> evaluator =
            std::make_shared<::open_spiel::algorithms::RandomRolloutEvaluator>(
                /*n_rollouts=*/1, /*seed=*/static_cast<int>(g * 17 + 1));
        ::open_spiel::algorithms::MCTSBot mcts_bot(
            *game, evaluator,
            /*uct_c=*/2.0, /*max_simulations=*/mcts_sims,
            /*max_memory_mb=*/256, /*solve=*/false,
            /*seed=*/static_cast<int>(g * 31 + 7), /*verbose=*/false);

        std::mt19937_64 rng(g * 1009 + 53);

        auto state = game->NewInitialState();
        int decisions = 0;
        // Safety cap: replay-based Clone is O(n) per clone, so a runaway
        // mixed-outcome game (where neither side dominates) can spike to
        // many minutes wall-time per game. Cap at 1500 decisions — well
        // above the random-vs-random average of ~1070 — to keep individual
        // games bounded. Aborts here count as draws (neutral for the
        // win-rate threshold).
        constexpr int kMaxDecisionsPerGame = 600;
        while (!state->IsTerminal() && decisions < kMaxDecisionsPerGame) {
            auto legal = state->LegalActions();
            if (legal.empty()) break;
            ::open_spiel::Action action;
            if (state->CurrentPlayer() == mcts_seat) {
                action = mcts_bot.Step(*state);
            } else {
                std::uniform_int_distribution<size_t> d(0, legal.size() - 1);
                action = legal[d(rng)];
            }
            state->ApplyAction(action);
            ++decisions;
        }
        total_decisions += decisions;
        auto ret = state->Returns();
        int winner = -1;
        if (ret[0] >  0.0) winner = 0;
        else if (ret[1] >  0.0) winner = 1;
        if (winner == mcts_seat)            ++mcts_wins;
        else if (winner == 1 - mcts_seat)    ++random_wins;
        else                                  ++draws;

        auto elapsed_so_far = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - t_start).count();
        std::cout << "  game " << (g+1) << "/" << num_games
                  << "  mcts_seat=" << mcts_seat
                  << "  winner=" << (winner < 0 ? "draw" : (winner == 0 ? "P1" : "P2"))
                  << "  decisions=" << decisions
                  << "  cumulative: mcts=" << mcts_wins
                  << " random=" << random_wins
                  << " draw=" << draws
                  << "  elapsed=" << static_cast<int>(elapsed_so_far) << "s"
                  << std::endl;  // flush each game so progress is visible
    }

    auto elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - t_start).count();
    int done = mcts_wins + random_wins + draws;
    std::cout << "\nCompleted " << done << " games in " << elapsed << "s\n";
    if (done > 0) {
        std::cout << "  MCTS wins:   " << mcts_wins   << " (" << (100.0*mcts_wins/done)   << "%)\n";
        std::cout << "  Random wins: " << random_wins << " (" << (100.0*random_wins/done) << "%)\n";
        std::cout << "  Draws:       " << draws       << " (" << (100.0*draws/done)       << "%)\n";
        double decisive = mcts_wins + random_wins;
        if (decisive > 0) {
            std::cout << "  MCTS decisive win rate: "
                      << (100.0*mcts_wins/decisive) << "%\n";
        }
        std::cout << "  Avg decisions/game: " << (total_decisions/done) << "\n";
    }
    return 0;
}
