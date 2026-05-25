/// @file main.cpp
/// Unified Riftbound binary. One entry point covers every mode:
///   • Random/MCTS/ISMCTS vs same     — batch via --games N --threads T
///   • Human (1 or 2 seats) vs AI/AI  — interactive web UI via --web (default
///                                       ON when any seat is human)
///   • Spectator (AI vs AI single)    — web UI watch-mode, no human input
///
/// Agents are wired via `buildAgent(spec, ...)`:
///   random            — uniform random over legal actions
///   human             — block on browser input through HumanAgent
///   mcts:sims=N       — OpenSpiel MCTSBot wrapped as AgentInterface
///   ismcts:sims=N     — OpenSpiel ISMCTSBot wrapped as AgentInterface
///
/// When any seat is human:
///   • `god_mode` edits are accepted by the WebSocket API.
///   • An HTML replay is written to ./replays/<timestamp>/replay.html.
///   • Trace + debug logging stream to the UI by default.
///
/// See docs/play-api.md for the WS wire protocol.

#include "agents/human_agent.h"
#include "agents/mcts_agent.h"
#include "agents/random_agent.h"
#include "cards/card_registry.h"
#include "core/card_db.h"
#include "core/events.h"
#include "core/version.h"
#include "engine/batch_runner.h"
#include "engine/game_engine.h"
#include "io/play_server.h"
#include "io/replay_writer.h"
#include "io/state_renderer.h"
#include "rules/deck_validator.h"

#include <boost/program_options.hpp>
#include <atomic>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <random>
#include <sstream>
#include <stdexcept>
#include <thread>

namespace po = boost::program_options;
namespace fs = std::filesystem;
using namespace riftbound;

namespace {

struct AgentSpec {
    std::string raw;       // original spec string
    std::string kind;      // "random", "human", "mcts", "ismcts"
    int sims = 0;          // populated for mcts/ismcts
};

AgentSpec parseAgentSpec(const std::string& s) {
    AgentSpec out;
    out.raw = s;
    auto colon = s.find(':');
    out.kind = (colon == std::string::npos) ? s : s.substr(0, colon);
    if (out.kind != "random" && out.kind != "human" &&
        out.kind != "mcts" && out.kind != "ismcts") {
        throw std::runtime_error(
            "Unknown agent kind '" + out.kind + "'. Expected one of: "
            "random, human, mcts, ismcts.");
    }
    if (colon != std::string::npos) {
        auto rest = s.substr(colon + 1);
        // key=value pairs separated by commas; only "sims=N" recognised.
        size_t i = 0;
        while (i < rest.size()) {
            auto eq    = rest.find('=', i);
            auto comma = rest.find(',', i);
            if (comma == std::string::npos) comma = rest.size();
            if (eq != std::string::npos && eq < comma) {
                auto key = rest.substr(i, eq - i);
                auto val = rest.substr(eq + 1, comma - eq - 1);
                if (key == "sims") out.sims = std::stoi(val);
            }
            i = comma + 1;
        }
        if ((out.kind == "mcts" || out.kind == "ismcts") && out.sims <= 0) {
            throw std::runtime_error(
                "Agent '" + out.kind + "' requires sims=N (e.g. " + out.kind + ":sims=50).");
        }
    }
    return out;
}

/// `game_seed` MUST be the engine's runGame seed — MctsAgent and IsMctsAgent
/// forward it into the internal OpenSpiel state so the replayed
/// action_history reproduces the same state as the live engine. If you
/// pass a derived/per-seat seed here, the cloned state diverges (different
/// shuffle / draws) and MCTS plans against a phantom position; decodeAction
/// then fails to map the chosen action back into the real legal-action
/// list and falls back to legal_actions[0] every turn — much worse than
/// random play.
///
/// `derived_seed` is per-seat, used for non-stateful internal RNG
/// (RandomAgent action picks, MCTS exploration tiebreaks).
std::unique_ptr<AgentInterface> buildAgent(const AgentSpec& spec,
                                           uint64_t game_seed,
                                           uint64_t derived_seed,
                                           const std::string& deck1,
                                           const std::string& deck2,
                                           const std::string& registry,
                                           HumanAgent** human_out) {
    if (spec.kind == "random") return std::make_unique<RandomAgent>(derived_seed);
    if (spec.kind == "human") {
        auto a = std::make_unique<HumanAgent>();
        if (human_out) *human_out = a.get();
        return a;
    }
    if (spec.kind == "mcts") {
        return std::make_unique<MctsAgent>(deck1, deck2, registry,
                                            game_seed, derived_seed, spec.sims);
    }
    if (spec.kind == "ismcts") {
        return std::make_unique<IsMctsAgent>(deck1, deck2, registry,
                                              game_seed, derived_seed, spec.sims);
    }
    throw std::runtime_error("Unsupported agent spec '" + spec.raw + "'");
}

std::string timestampDirName() {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_r(&t, &tm);
    std::ostringstream s;
    s << std::put_time(&tm, "%Y%m%d-%H%M%S");
    return s.str();
}

} // namespace

int main(int argc, char* argv[]) {
    po::options_description desc(
        "riftbound — Riftbound TCG simulation engine\n"
        "\n"
        "Agent specs (for --agent1 / --agent2):\n"
        "  random            Uniform random over legal actions. Fast,\n"
        "                    no extra deps. Baseline for testing.\n"
        "  human             Block at every decision and wait for a\n"
        "                    browser click via the WebSocket UI. Implies\n"
        "                    --web on, --render-html on, --debug, --trace.\n"
        "  mcts:sims=N       OpenSpiel MCTSBot with a RandomRolloutEvaluator.\n"
        "                    N is the simulation budget per decision (try\n"
        "                    sims=20 for fast tests, sims=200+ for stronger\n"
        "                    play). Each selectAction call rebuilds a fresh\n"
        "                    OpenSpiel state by replaying action_history,\n"
        "                    so interactive use is fine but cost grows with\n"
        "                    decision depth.\n"
        "  ismcts:sims=N     OpenSpiel ISMCTSBot for imperfect-info search.\n"
        "                    Same N semantics as MCTS. Currently behaves\n"
        "                    like MCTS at search time — proper hidden-info\n"
        "                    determinization (sampling from the unseen card\n"
        "                    pool) is queued as a follow-up. Caveat:\n"
        "                    StateEditor god-mode edits do not participate\n"
        "                    in MCTS planning; the bot sees the state\n"
        "                    implied by action_history alone.\n"
        "\n"
        "Common invocations:\n"
        "  ./riftbound --deck1 D1.json --deck2 D2.json\n"
        "      (default: random vs random, one game, stdout only)\n"
        "  ./riftbound --deck1 ... --deck2 ... --agent1 human\n"
        "      (browser UI on http://127.0.0.1:8080; auto-replay)\n"
        "  ./riftbound --deck1 ... --deck2 ... --agent1 random --agent2 mcts:sims=50\n"
        "      (single game, watch MCTS-50 vs random in stdout)\n"
        "  ./riftbound --deck1 ... --deck2 ... --games 100 --threads 8\n"
        "      (batch random-vs-random benchmark)\n"
        "\n"
        "Flags");
    desc.add_options()
        ("help,h", "Show help")
        ("version,v", "Print engine version + git commit")
        ("deck1", po::value<std::string>()->required(), "Player 1 deck JSON")
        ("deck2", po::value<std::string>()->required(), "Player 2 deck JSON")
        ("registry,r", po::value<std::string>()->default_value("cards/registry.json"),
         "Path to card registry JSON")
        ("agent1", po::value<std::string>()->default_value("random"),
         "Player 1 agent spec (see top of --help for full descriptions). "
         "One of: random | human | mcts:sims=N | ismcts:sims=N")
        ("agent2", po::value<std::string>()->default_value("random"),
         "Player 2 agent spec (see top of --help for full descriptions). "
         "One of: random | human | mcts:sims=N | ismcts:sims=N")
        ("web", po::value<std::string>()->default_value("auto"),
         "Web UI mode: auto (default — ON if any seat is human, else OFF), "
         "on (force ON), off (force OFF, even with human seats)")
        ("port,p", po::value<uint16_t>()->default_value(8080),
         "TCP port for the web UI when --web is on")
        ("bind", po::value<std::string>()->default_value("127.0.0.1"),
         "Bind address for the web UI when --web is on "
         "(use 0.0.0.0 to accept connections from any interface)")
        ("god-mode", po::value<std::string>()->default_value("auto"),
         "God-mode state-editor in the web UI: auto (default — ON if any "
         "seat is human), on (force ON), off (force OFF, even with a human seat)")
        ("seed,s", po::value<uint64_t>()->default_value(0),
         "RNG seed (0 = random)")
        ("render-html", po::value<std::string>()->default_value("auto"),
         "Write HTML replay: auto (default — ON if any seat is human), "
         "on (force ON), off (force OFF)")
        ("replay-dir", po::value<std::string>()->default_value("replays"),
         "Directory for HTML replay output. Single-game writes "
         "<dir>/<timestamp>/replay.html; batch writes <dir>/replay_gameN.html.")
        ("debug", po::bool_switch()->default_value(false),
         "Stream debug-level engine events (trigger firings, ability resolution). "
         "Implied when any seat is human.")
        ("trace", po::bool_switch()->default_value(false),
         "Stream trace-level engine events (phases, intents, draws, kills, "
         "scoring). Implies --debug. Implied when any seat is human.")
        ("games,n", po::value<int>()->default_value(1),
         "Number of games. > 1 runs via BatchRunner and requires no human seats.")
        ("threads,t", po::value<int>()->default_value(1),
         "Parallel workers for --games > 1 (0 = hardware concurrency).")
        ("show-hand", po::bool_switch()->default_value(false),
         "Show hand contents in stdout / replay renders (debug aid).")
    ;

    po::variables_map vm;
    try {
        po::store(po::parse_command_line(argc, argv, desc), vm);
        if (vm.count("help"))    { std::cout << desc << "\n"; return 0; }
        if (vm.count("version")) {
            std::cout << "Riftbound " << kVersionTag
                      << " (built " << kBuildDate << ")\n";
            return 0;
        }
        po::notify(vm);
    } catch (const po::error& e) {
        std::cerr << "Error: " << e.what() << "\n\n" << desc << std::endl;
        return 1;
    }

    auto deck1_path    = vm["deck1"].as<std::string>();
    auto deck2_path    = vm["deck2"].as<std::string>();
    auto registry_path = vm["registry"].as<std::string>();
    auto agent1_raw    = vm["agent1"].as<std::string>();
    auto agent2_raw    = vm["agent2"].as<std::string>();
    auto web_mode      = vm["web"].as<std::string>();
    auto port          = vm["port"].as<uint16_t>();
    auto bind_address  = vm["bind"].as<std::string>();
    auto god_mode_arg  = vm["god-mode"].as<std::string>();
    auto seed          = vm["seed"].as<uint64_t>();
    auto render_mode   = vm["render-html"].as<std::string>();
    auto replay_dir    = vm["replay-dir"].as<std::string>();
    bool debug_mode    = vm["debug"].as<bool>();
    bool trace_mode    = vm["trace"].as<bool>();
    int  num_games     = vm["games"].as<int>();
    int  num_threads   = vm["threads"].as<int>();
    bool show_hand     = vm["show-hand"].as<bool>();

    AgentSpec spec1, spec2;
    try {
        spec1 = parseAgentSpec(agent1_raw);
        spec2 = parseAgentSpec(agent2_raw);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    const bool p1_human = (spec1.kind == "human");
    const bool p2_human = (spec2.kind == "human");
    const bool any_human = p1_human || p2_human;

    // Resolve auto-mode flags.
    bool web_enabled =
        (web_mode == "on")  ? true  :
        (web_mode == "off") ? false : any_human;  // auto
    bool render_html =
        (render_mode == "on")  ? true  :
        (render_mode == "off") ? false : any_human;  // auto

    // Human seat implies full logging — user wants to see what the engine
    // is doing. Trace implies debug.
    if (any_human) { debug_mode = true; trace_mode = true; }
    if (trace_mode) debug_mode = true;

    if (num_games > 1 && any_human) {
        std::cerr << "Error: --games > 1 cannot be combined with a 'human' "
                     "seat. Use --games 1 for interactive play.\n";
        return 1;
    }
    if (num_threads <= 0) num_threads = static_cast<int>(std::thread::hardware_concurrency());

    std::cout << "Riftbound " << kVersionTag
              << " (built " << kBuildDate << ")\n";

    // ── Load card database, registry, decks ───────────────────────────────
    CardDB card_db;
    CardRegistry card_registry;
    try {
        card_db.loadFromRegistry(registry_path);
        card_registry.loadAll();
        std::cout << "Loaded " << card_db.size() << " cards from registry\n";
    } catch (const std::exception& e) {
        std::cerr << "Failed to load registry: " << e.what() << "\n";
        return 1;
    }

    DeckValidator validator(card_db);
    {
        std::string ban_path = registry_path;
        auto slash = ban_path.rfind('/');
        if (slash != std::string::npos)
            ban_path = ban_path.substr(0, slash + 1) + "ban-list.csv";
        else
            ban_path = "cards/ban-list.csv";
        validator.loadBanList(ban_path);
    }

    DeckSubmission deck1, deck2;
    try {
        deck1 = DeckValidator::loadFromJson(deck1_path, card_db);
        deck2 = DeckValidator::loadFromJson(deck2_path, card_db);
    } catch (const std::exception& e) {
        std::cerr << "Failed to load deck: " << e.what() << "\n";
        return 1;
    }

    auto v1 = validator.validate(deck1);
    auto v2 = validator.validate(deck2);
    if (!v1.is_legal || !v2.is_legal) {
        std::cerr << "Deck validation failed.\n";
        for (auto& e : v1.errors) std::cerr << "  P1: " << e << "\n";
        for (auto& e : v2.errors) std::cerr << "  P2: " << e << "\n";
        return 1;
    }

    std::cout << "P1: " << card_db.get(deck1.legend).name
              << " / " << card_db.get(deck1.chosen_champion).name
              << "  (" << spec1.raw << ")\n";
    std::cout << "P2: " << card_db.get(deck2.legend).name
              << " / " << card_db.get(deck2.chosen_champion).name
              << "  (" << spec2.raw << ")\n";

    // OpenSpiel's RiftboundGame registers `seed` as int — 31-bit positive
    // range. The engine's runGame accepts uint64_t. If main passes a 64-bit
    // value to runGame but MctsAgent truncates the same value before
    // forwarding to LoadGame, the cloned OpenSpiel state's engine ends up
    // with a different RNG seed than the live engine. After replaying
    // action_history the two diverge (different initial shuffles + draws),
    // MCTS plans against a phantom state, decodeAction can't map its
    // chosen action into the live legal_actions, and the agent silently
    // degrades to legal_actions[0] every turn.
    //
    // Truncating here, ONCE, keeps both paths in lockstep. 2^31 distinct
    // seeds is plenty for batch reproducibility.
    uint64_t game_seed = (seed == 0 ? std::random_device{}() : seed) & 0x7FFFFFFFu;
    if (num_games == 1) {
        std::cout << "Seed: " << game_seed
                  << (seed == 0 ? "  (random)" : "  (--seed)") << "\n";
    } else if (seed != 0) {
        std::cout << "Seed base: " << seed
                  << "  (game N uses seed=base+N)\n";
    } else {
        std::cout << "Seed: random per game\n";
    }

    // ── Batch mode (num_games > 1) ────────────────────────────────────────
    if (num_games > 1) {
        // BatchRunner's GameRunner only knows "random" out of the box; we
        // pipe in our own agent factory so mcts/ismcts work in batch too.
        // Each call constructs an independent agent instance.
        //
        // game_seed flows through unchanged so MCTS-style agents can seed
        // their internal OpenSpiel state with the same engine seed
        // GameRunner uses for runGame; derived_seed is per-seat to keep
        // RandomAgent / MCTS-exploration RNGs distinct between seats.
        auto factory_spec1 = spec1;
        auto factory_spec2 = spec2;
        auto factory =
            [factory_spec1, factory_spec2,
             d1 = deck1_path, d2 = deck2_path, r = registry_path]
            (int seat, uint64_t game_seed) -> std::unique_ptr<AgentInterface> {
                const AgentSpec& s = (seat == 0) ? factory_spec1 : factory_spec2;
                if (s.kind == "human") {
                    throw std::runtime_error("batch mode cannot use human seats");
                }
                uint64_t derived = game_seed * 2 + static_cast<uint64_t>(seat);
                return buildAgent(s, game_seed, derived, d1, d2, r, nullptr);
            };

        GameConfig cfg;
        cfg.base_seed     = seed;
        cfg.do_render     = render_html;
        cfg.show_hand     = show_hand;
        cfg.debug_mode    = debug_mode;
        cfg.trace_mode    = trace_mode;
        cfg.total_games   = num_games;
        cfg.agent1_spec   = spec1.raw;
        cfg.agent2_spec   = spec2.raw;
        cfg.agent_factory = std::move(factory);

        AggregateResults results;
        BatchRunner batch(card_db, card_registry, num_threads);
        batch.runBatch(deck1, deck2, num_games, cfg, results);

        std::cout << "\nResults (" << num_games << " games):\n"
                  << "  P1 wins: " << results.p1_wins.load() << "\n"
                  << "  P2 wins: " << results.p2_wins.load() << "\n"
                  << "  Draws:   " << results.draws.load()   << "\n";
        if (num_games > 1) {
            std::cout << "  Avg turns: "
                      << (results.total_turns.load() / num_games) << "\n"
                      << "  Avg decisions: "
                      << (results.total_decisions.load() / num_games) << "\n"
                      << "  Threads: " << num_threads << "\n";
        }
        return 0;
    }

    // ── Single-game (interactive) flow ────────────────────────────────────
    HumanAgent* p1_human_ptr = nullptr;
    HumanAgent* p2_human_ptr = nullptr;
    std::unique_ptr<AgentInterface> agent1, agent2;
    try {
        // game_seed flows through verbatim so MCTS-style agents seed their
        // internal OpenSpiel state with the same value runGame uses below.
        agent1 = buildAgent(spec1, game_seed, game_seed * 2,     deck1_path, deck2_path, registry_path, &p1_human_ptr);
        agent2 = buildAgent(spec2, game_seed, game_seed * 2 + 1, deck1_path, deck2_path, registry_path, &p2_human_ptr);
    } catch (const std::exception& e) {
        std::cerr << "Failed to build agents: " << e.what() << "\n";
        return 1;
    }
    if (!p1_human) p1_human_ptr = nullptr;
    if (!p2_human) p2_human_ptr = nullptr;

    // ── Wire engine + event bus + optional replay writer ──────────────────
    EventBus events;
    GameEngine engine(card_db, events, card_registry);
    StateRenderer renderer(card_db);
    renderer.show_hand = show_hand || any_human;

    std::unique_ptr<ReplayWriter> replay;
    std::string replay_dir_path;
    std::string replay_path;
    if (render_html) {
        replay_dir_path = (fs::path(replay_dir) / timestampDirName()).string();
        fs::create_directories(replay_dir_path);
        replay_path = replay_dir_path + "/replay.html";
        replay = std::make_unique<ReplayWriter>(card_db, replay_path);

        std::ostringstream header;
        header << "seed " << game_seed
               << " | P1=" << spec1.raw
               << " | P2=" << spec2.raw;
        replay->setGameHeader(header.str());

        events.on_log.connect([rp = replay.get()](const LogEvent& e) {
            const char* prefix = "[TRC]";
            if (e.level == LogLevel::Debug)   prefix = "[DBG]";
            if (e.level == LogLevel::Info)    prefix = "[INF]";
            if (e.level == LogLevel::Warning) prefix = "[WRN]";
            rp->addTraceLine(std::string(prefix) + " " + e.message);
        });
        events.on_phase_changed.connect(
            [rp = replay.get(), &renderer, &engine](const PhaseChangedEvent& e) {
                rp->recordPhaseSeam(engine.state(), e.old_phase, e.new_phase, renderer);
            });
        engine.on_decision = [rp = replay.get(), &renderer](
                                 const GameState& s,
                                 const std::vector<Intent>& actions,
                                 const Intent& chosen) {
            rp->recordDecision(s, actions, chosen, renderer);
        };
        std::cout << "Replay will be written to " << replay_path << "\n";
    }

    // ── Web server (optional) ─────────────────────────────────────────────
    std::unique_ptr<PlayServer> server;
    if (web_enabled) {
        PlayServerConfig server_config;
        server_config.port             = port;
        server_config.bind_address     = bind_address;
        bool god_mode_enabled;
        if      (god_mode_arg == "on")  god_mode_enabled = true;
        else if (god_mode_arg == "off") god_mode_enabled = false;
        else if (god_mode_arg == "auto") god_mode_enabled = any_human;
        else {
            std::cerr << "Error: --god-mode must be one of: auto | on | off\n";
            return 1;
        }
        server_config.god_mode_enabled = god_mode_enabled;
        server_config.p1_human         = p1_human_ptr;
        server_config.p2_human         = p2_human_ptr;
        server_config.version_tag      = kVersionTag;
        server_config.build_date       = kBuildDate;
        server_config.seed             = game_seed;
        server_config.on_shutdown_signal = [p1_human_ptr, p2_human_ptr] {
            if (p1_human_ptr) p1_human_ptr->abort();
            if (p2_human_ptr) p2_human_ptr->abort();
        };

        server = std::make_unique<PlayServer>(engine, renderer, card_db,
                                              std::move(server_config));
        std::string url = server->start();
        std::cout << "\n  → Open " << url << " in your browser to play.\n\n";

        // Stream engine log events into the UI log panel.
        if (debug_mode || trace_mode) {
            events.on_log.connect([s = server.get(), trace_mode](const LogEvent& e) {
                if (e.level == LogLevel::Trace && !trace_mode) return;
                const char* level = "TRC";
                if (e.level == LogLevel::Debug)   level = "DBG";
                if (e.level == LogLevel::Info)    level = "INF";
                if (e.level == LogLevel::Warning) level = "WRN";
                s->broadcastLog(level, e.message);
            });
        }
    } else if (any_human) {
        // Sanity: a human seat with --web off would just hang forever.
        std::cerr << "Error: --web off cannot be combined with a 'human' seat.\n";
        return 1;
    }

    // ── Run the single game in a worker thread; main waits for it ─────────
    std::thread game_thread([&] {
        try {
            auto result = engine.runGame(deck1, deck2, *agent1, *agent2, game_seed);
            std::cout << "\nGame over: winner="
                      << static_cast<int>(result.winner)
                      << " | reason=" << result.termination_reason
                      << " | turns=" << result.total_turns
                      << "\n";
            if (replay) {
                replay->addTraceLine("[TRC] GAME_OVER: " + result.termination_reason);
                replay->writeHtml();
                std::cout << "Replay saved to " << replay_path << "\n";
            }
        } catch (const std::exception& e) {
            std::cerr << "[engine] exception: " << e.what() << "\n";
        }
    });

    if (game_thread.joinable()) game_thread.join();
    if (server) server->stop();
    return 0;
}
