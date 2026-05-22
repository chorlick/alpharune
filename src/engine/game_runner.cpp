#include "game_runner.h"
#include "agents/random_agent.h"
#include "io/replay_writer.h"
#include "io/state_renderer.h"

#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>

namespace riftbound {

GameRunner::GameRunner(const CardDB& card_db,
                       const CardRegistry& card_registry,
                       const DeckSubmission& deck1,
                       const DeckSubmission& deck2,
                       const GameConfig& config,
                       AggregateResults& results)
    : card_db_(card_db), card_registry_(card_registry),
      deck1_(deck1), deck2_(deck2), config_(config), results_(results) {}

GameResult GameRunner::run() {
    // Per-game resources (all stack/heap local — thread-safe)
    EventBus events;
    GameEngine engine(card_db_, events, card_registry_);
    StateRenderer renderer(card_db_);
    renderer.show_hand = config_.show_hand;

    // Compute seed
    uint64_t game_seed = config_.base_seed == 0
        ? std::random_device{}()
        : config_.base_seed + config_.game_index;

    // Generate UUID
    std::string game_uuid;
    {
        std::mt19937_64 uuid_rng(std::random_device{}());
        std::uniform_int_distribution<uint64_t> dist;
        uint64_t a = dist(uuid_rng), b = dist(uuid_rng);
        std::ostringstream uu;
        uu << std::hex << std::setfill('0');
        uu << std::setw(8) << (a >> 32) << "-";
        uu << std::setw(4) << ((a >> 16) & 0xFFFF) << "-";
        uu << std::setw(4) << (0x4000 | ((a) & 0x0FFF)) << "-";
        uu << std::setw(4) << (0x8000 | ((b >> 48) & 0x3FFF)) << "-";
        uu << std::setw(12) << (b & 0xFFFFFFFFFFFFULL);
        game_uuid = uu.str();
    }

    // Set up debug/trace logging
    std::ostringstream log_buffer; // thread-local buffer
    if (config_.debug_mode) {
        events.on_log.connect([&log_buffer, trace = config_.trace_mode](const LogEvent& e) {
            if (e.level == LogLevel::Trace && !trace) return;
            const char* prefix = "[TRC]";
            if (e.level == LogLevel::Debug) prefix = "[DBG]";
            if (e.level == LogLevel::Info) prefix = "[INF]";
            if (e.level == LogLevel::Warning) prefix = "[WRN]";
            log_buffer << "  " << prefix << " " << e.message << "\n";
        });
    }

    // Set up HTML replay writer
    std::unique_ptr<ReplayWriter> replay;
    if (config_.do_render) {
        std::string replay_path = "replay_game" + std::to_string(config_.game_index + 1) + ".html";
        replay = std::make_unique<ReplayWriter>(card_db_, replay_path);
        replay->setGameHeader("Game " + std::to_string(config_.game_index + 1) +
            " | UUID: " + game_uuid +
            " | Seed: " + std::to_string(game_seed));

        events.on_log.connect([&replay](const LogEvent& e) {
            if (!replay) return;
            const char* prefix = "[TRC]";
            if (e.level == LogLevel::Debug) prefix = "[DBG]";
            if (e.level == LogLevel::Info) prefix = "[INF]";
            if (e.level == LogLevel::Warning) prefix = "[WRN]";
            replay->addTraceLine(std::string(prefix) + " " + e.message);
        });
        // Inject phase-seam snapshots at every PhaseChangedEvent so the
        // replay has discrete visual stops at each phase boundary.
        events.on_phase_changed.connect(
            [&replay, &renderer, &engine](const PhaseChangedEvent& e) {
                if (!replay) return;
                replay->recordPhaseSeam(engine.state(),
                                         e.old_phase, e.new_phase, renderer);
            });
    }

    // Agent factory. Only "random" is supported here — the legacy ML
    // (ONNX) inference path was removed when the Python training pipeline
    // was retired. Future LibTorch-based agents (when C++ AlphaZero training
    // lands) will be wired in alongside RandomAgent. Until then, anyone
    // wanting MCTS / neural agents should use the riftbound_openspiel
    // binary which offers MCTSBot via OpenSpiel.
    auto makeAgent = [&](const std::string& spec, uint64_t seed)
        -> std::unique_ptr<AgentInterface> {
        if (spec == "random") {
            return std::make_unique<RandomAgent>(seed);
        }
        throw std::runtime_error(
            "GameRunner: unsupported agent spec '" + spec +
            "'. Only 'random' is available in this build. For MCTS, use "
            "riftbound_openspiel.");
    };

    auto agent1 = makeAgent(config_.agent1_spec, game_seed * 2);
    auto agent2 = makeAgent(config_.agent2_spec, game_seed * 2 + 1);

    // Decision callback feeds the HTML replay writer when --render is on.
    engine.on_decision = [&](const GameState& state,
                              const std::vector<Intent>& actions,
                              const Intent& chosen) {
        if (replay) {
            replay->recordDecision(state, actions, chosen, renderer);
        }
    };

    auto result = engine.runGame(deck1_, deck2_, *agent1, *agent2, game_seed);

    // Finalize outputs
    if (replay) {
        replay->addTraceLine("[TRC] GAME_OVER: " + result.termination_reason);
        replay->writeHtml();
    }

    // Update aggregate results (atomic)
    if (result.winner == PlayerId::Player1)
        results_.p1_wins.fetch_add(1);
    else if (result.winner == PlayerId::Player2)
        results_.p2_wins.fetch_add(1);
    else
        results_.draws.fetch_add(1);
    results_.total_turns.fetch_add(result.total_turns);
    results_.total_decisions.fetch_add(result.total_decisions);
    int completed = results_.games_completed.fetch_add(1) + 1;

    // Console output (under mutex)
    {
        std::lock_guard<std::mutex> lock(results_.console_mutex);

        // Per-game result (only for small batches)
        if (config_.total_games <= 20) {
            std::cout << "Game " << (config_.game_index + 1) << ": "
                      << toString(result.winner) << " wins"
                      << " (" << result.final_scores[0] << "-"
                      << result.final_scores[1] << ")"
                      << " in " << result.total_turns << " turns"
                      << " (" << result.total_decisions << " decisions)"
                      << " — " << result.termination_reason << "\n";
        }

        // Progress for batches > 20 games. Tick on whichever fires first:
        //   - every 100 games (good signal at very high gps)
        //   - at least once per second of wall time
        //   - at completion
        if (config_.total_games > 20) {
            auto now = std::chrono::steady_clock::now();
            double since_last = std::chrono::duration<double>(
                now - results_.last_progress_time).count();
            bool fire = (completed % 100 == 0)
                     || (completed == results_.total_games)
                     || (since_last >= 1.0);
            if (fire) {
                results_.last_progress_time = now;
                double elapsed = std::chrono::duration<double>(
                    now - results_.start_time).count();
                double gps = elapsed > 0 ? completed / elapsed : 0;
                int eta_sec = gps > 0
                    ? static_cast<int>((results_.total_games - completed) / gps)
                    : 0;
                std::cout << "\r  Progress: " << completed << "/"
                          << results_.total_games << " games"
                          << "  (" << std::fixed << std::setprecision(1)
                          << gps << " games/sec"
                          << ", ETA " << eta_sec / 60 << "m"
                          << eta_sec % 60 << "s)"
                          << "    " << std::flush;
            }
        }
    }

    return result;
}

} // namespace riftbound
