// Parity baseline: runs GameEngine + RandomAgent DIRECTLY (no OpenSpiel
// wrapper) using the same RiftboundGame-loaded resources. If this produces
// distributions matching BatchRunner but differs from the OpenSpiel-driven
// demo, then the OpenSpiel wrapper is introducing the divergence.

#include "riftbound_game.h"

#include "engine/game_engine.h"
#include "core/events.h"
#include "agents/random_agent.h"

#include "open_spiel/spiel.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <random>
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

}

int main() {
    const std::string deck1    = getenvOr("RIFTBOUND_DECK1",    "decks/leblanc_test.json");
    const std::string deck2    = getenvOr("RIFTBOUND_DECK2",    "decks/leblanc_test.json");
    const std::string registry = getenvOr("RIFTBOUND_REGISTRY", "cards/registry.json");
    const int num_games        = std::max(1, getenvInt("RIFTBOUND_NUM_GAMES", 1));
    const int num_threads      = std::max(1, getenvInt("RIFTBOUND_THREADS",   1));

    const std::string game_str = "riftbound(deck1=" + deck1 + ",deck2=" + deck2 +
                                  ",registry=" + registry + ",seed=0)";

    // We use RiftboundGame just as a resource container — CardDB, decks.
    auto game_base = ::open_spiel::LoadGame(game_str);
    const auto* rb_game = static_cast<const ::riftbound::openspiel::RiftboundGame*>(game_base.get());

    std::atomic<int> p1{0}, p2{0}, dr{0};
    std::atomic<long long> total_dec{0};
    std::atomic<int> next{0};

    auto t_start = std::chrono::steady_clock::now();
    std::mt19937_64 master(std::random_device{}());

    std::vector<std::thread> workers;
    for (int t = 0; t < num_threads; ++t) {
        uint64_t seed = master();
        workers.emplace_back([&, seed]() {
            std::mt19937_64 local(seed);
            while (true) {
                int i = next.fetch_add(1);
                if (i >= num_games) break;
                uint64_t game_seed = local();
                ::riftbound::EventBus bus;
                ::riftbound::GameEngine engine(rb_game->cardDb(), bus, rb_game->cardRegistry());
                ::riftbound::RandomAgent a1(game_seed * 2);
                ::riftbound::RandomAgent a2(game_seed * 2 + 1);
                auto result = engine.runGame(rb_game->deck1(), rb_game->deck2(),
                                              a1, a2, game_seed);
                total_dec.fetch_add(result.total_decisions);
                if (result.winner == ::riftbound::PlayerId::Player1)      p1.fetch_add(1);
                else if (result.winner == ::riftbound::PlayerId::Player2) p2.fetch_add(1);
                else                                                       dr.fetch_add(1);
            }
        });
    }
    for (auto& w : workers) w.join();

    double elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - t_start).count();
    int done = p1.load() + p2.load() + dr.load();
    std::cout << "Baseline: " << done << " games in " << elapsed << "s ("
              << done/elapsed << " games/sec)\n";
    std::cout << "  P1 wins: " << p1 << " (" << 100.0*p1/done << "%)\n";
    std::cout << "  P2 wins: " << p2 << " (" << 100.0*p2/done << "%)\n";
    std::cout << "  Draws:   " << dr << " (" << 100.0*dr/done << "%)\n";
    std::cout << "  Avg decisions/game: " << total_dec/done << "\n";
    return 0;
}
