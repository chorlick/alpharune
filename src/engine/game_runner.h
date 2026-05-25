#pragma once
/// @file game_runner.h
/// Per-game execution unit. Each GameRunner is constructed on a worker thread
/// and runs one complete game with its own EventBus, GameEngine, agents, and I/O.
/// Shared singletons (CardDB, CardRegistry) are passed by const reference.

#include "agents/agent_interface.h"
#include "core/card_db.h"
#include "cards/card_registry.h"
#include "core/events.h"
#include "engine/game_engine.h"
#include "rules/deck_validator.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

namespace riftbound {

struct GameConfig {
    int game_index = 0;           // 0-based game number
    uint64_t base_seed = 0;       // base seed (0 = random per game)
    bool do_render = false;
    bool show_hand = false;
    bool step_mode = false;
    bool debug_mode = false;
    bool trace_mode = false;
    int total_games = 1;          // for progress display
    std::string agent1_spec = "random";
    std::string agent2_spec = "random";

    /// Optional agent factory — when set, replaces GameRunner's built-in
    /// "random-only" factory. The seat index is 0 for P1 and 1 for P2.
    /// `game_seed` is the engine RNG seed for THIS game; the factory
    /// must pass it verbatim into any MCTS-style agent that constructs
    /// an internal OpenSpiel state, otherwise replaying action_history
    /// in the cloned state will diverge from the real engine's state
    /// (different initial shuffle, different draws) and MCTS will plan
    /// against a phantom position. The factory derives whatever per-seat
    /// RNG it wants for the agent's internal use (e.g., RandomAgent's
    /// action picks, MCTS's exploration tiebreaks) from game_seed + seat
    /// inside the closure. Thread-safe because each call constructs a
    /// fresh agent instance.
    std::function<std::unique_ptr<AgentInterface>(int seat_idx, uint64_t game_seed)>
        agent_factory;
};

struct AggregateResults {
    std::atomic<int> p1_wins{0};
    std::atomic<int> p2_wins{0};
    std::atomic<int> draws{0};
    std::atomic<int> total_turns{0};
    std::atomic<int> total_decisions{0};
    std::atomic<int> games_completed{0};
    int total_games = 0;
    std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();
    std::mutex console_mutex;
    // Last wall-clock time we printed a progress line (under console_mutex).
    std::chrono::steady_clock::time_point last_progress_time = start_time;
};

class GameRunner {
public:
    GameRunner(const CardDB& card_db,
               const CardRegistry& card_registry,
               const DeckSubmission& deck1,
               const DeckSubmission& deck2,
               const GameConfig& config,
               AggregateResults& results);

    /// Run one game. Thread-safe — all per-game state is local.
    GameResult run();

private:
    const CardDB& card_db_;
    const CardRegistry& card_registry_;
    const DeckSubmission& deck1_;
    const DeckSubmission& deck2_;
    GameConfig config_;
    AggregateResults& results_;
};

} // namespace riftbound
