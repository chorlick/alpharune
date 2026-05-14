#pragma once
/// @file game_runner.h
/// Per-game execution unit. Each GameRunner is constructed on a worker thread
/// and runs one complete game with its own EventBus, GameEngine, agents, and I/O.
/// Shared singletons (CardDB, CardRegistry) are passed by const reference.

#include "core/card_db.h"
#include "cards/card_registry.h"
#include "core/events.h"
#include "engine/game_engine.h"
#include "rules/deck_validator.h"

#include <atomic>
#include <chrono>
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
    std::string output_path;      // empty = no JSONL output
    int total_games = 1;          // for progress display
    std::string agent1_spec = "random"; // "random" or "model:path.onnx"
    std::string agent2_spec = "random";
    double agent1_temperature = 0.0;    // model sampling temperature; 0 = argmax (eval)
    double agent2_temperature = 0.0;    // >0 = softmax sample (self-play data gen)
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
