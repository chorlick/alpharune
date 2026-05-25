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

    // Compute seed. Truncated to 31 bits so any MCTS-style agent that
    // builds an OpenSpiel state internally (whose `seed` GameParameter
    // is int-typed) sees the same value the engine uses here. Without
    // the mask, a 64-bit nondeterministic seed would be silently
    // truncated only on the agent side and the cloned state's engine
    // would diverge from the real one. See src/main.cpp for full notes.
    uint64_t game_seed = (config_.base_seed == 0
        ? std::random_device{}()
        : config_.base_seed + config_.game_index) & 0x7FFFFFFFu;

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

    // Agent factory. If config_.agent_factory is supplied, defer to it —
    // that's how the riftbound binary plugs in MCTS / IsMCTS (their
    // constructors live in the executable's TU because they pull in
    // OpenSpiel, which riftbound_core itself does not link). Otherwise
    // fall back to the built-in random-only factory so older callers
    // (tests, batch_runner with default config) keep working.
    auto makeAgent = [&](int seat_idx, const std::string& spec)
        -> std::unique_ptr<AgentInterface> {
        if (config_.agent_factory) {
            // Factory receives game_seed verbatim — it picks how to derive
            // any per-seat sub-seeds for the constructed agent.
            return config_.agent_factory(seat_idx, game_seed);
        }
        // Fallback: per-seat derived seed so the two RandomAgents don't
        // draw identical action sequences.
        uint64_t derived = game_seed * 2 + static_cast<uint64_t>(seat_idx);
        if (spec == "random") {
            return std::make_unique<RandomAgent>(derived);
        }
        throw std::runtime_error(
            "GameRunner: agent spec '" + spec +
            "' requires an agent_factory in GameConfig (riftbound_core only "
            "ships RandomAgent; mcts/ismcts/human live in the binary's TU).");
    };

    auto agent1 = makeAgent(0, config_.agent1_spec);
    auto agent2 = makeAgent(1, config_.agent2_spec);

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
            std::cout << "Game " << (config_.game_index + 1)
                      << " [seed=" << game_seed << "]: "
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
