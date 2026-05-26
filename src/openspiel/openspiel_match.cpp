/// @file openspiel_match.cpp
/// Unified Riftbound OpenSpiel match runner.
///
/// Runs N games between two agents through the OpenSpiel wrapper. Each
/// agent is specified by a "spec string":
///   random              uniform random over LegalActions
///   mcts:sims=N         OpenSpiel MCTSBot with RandomRolloutEvaluator
///                       (N is the simulation count per move)
///   ismcts:sims=N       OpenSpiel ISMCTSBot for imperfect-info search
///                       (currently behaves like MCTS — determinizing
///                       resampler is a follow-up).
///
/// Model-based agents (model:/rebel:) live in the riftbound-trainer
/// sibling repo's binary. This binary stays libtorch-free.
///
/// Build:  cmake --build build --target riftbound_openspiel
/// Run:    ./build/src/openspiel/riftbound_openspiel \
///           --agent1 random --agent2 mcts:sims=5 \
///           --games 100 --seed 42

#include "open_spiel/spiel.h"
#include "open_spiel/algorithms/mcts.h"
#include "open_spiel/algorithms/is_mcts.h"

#include <boost/signals2/connection.hpp>

#include "riftbound_state.h"
#include "riftbound_game.h"
#include "action_vocab.h"
#include "core/version.h"
#include "io/progress_bar.h"
#include "io/replay_writer.h"
#include "io/state_renderer.h"

#include <boost/program_options.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <map>

// libtorch-dependent model: / rebel: agents live in the riftbound-trainer
// sibling repo's binary, not here. This binary supports only random /
// mcts / ismcts.
#include <iostream>
#include <memory>
#include <mutex>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace po = boost::program_options;

namespace {

struct AgentSpec {
    // Random  — uniform legal-action pick.
    // Mcts    — OpenSpiel MCTSBot + RandomRolloutEvaluator (perfect-info MCTS).
    // Ismcts  — OpenSpiel ISMCTSBot + RandomRolloutEvaluator (information-set
    //           MCTS — theoretically correct for imperfect-info games).
    //           Requires a resampler; we install one that currently Clone()s
    //           the state (no determinization yet — behaves like MCTS
    //           for now; real hidden-info determinization is a follow-up).
    enum class Kind { Random, Mcts, Ismcts };
    Kind kind = Kind::Random;
    int mcts_sims = 5;
    bool mcts_verbose = false;  // pass-through to MCTSBot's verbose flag
};

// Parses key=val,key=val... into a flat map. Trims whitespace.
static std::map<std::string, std::string> kvSplit(const std::string& s) {
    std::map<std::string, std::string> out;
    size_t i = 0;
    while (i < s.size()) {
        const auto comma = s.find(',', i);
        const auto pair_end = (comma == std::string::npos) ? s.size() : comma;
        const auto eq = s.find('=', i);
        if (eq == std::string::npos || eq > pair_end) {
            throw std::runtime_error("agent spec: expected key=val in '" + s + "'");
        }
        out[s.substr(i, eq - i)] = s.substr(eq + 1, pair_end - eq - 1);
        i = pair_end + 1;
    }
    return out;
}

AgentSpec parseAgentSpec(const std::string& s) {
    AgentSpec a;
    if (s == "random") {
        a.kind = AgentSpec::Kind::Random;
        return a;
    }
    if (s.rfind("mcts:", 0) == 0) {
        a.kind = AgentSpec::Kind::Mcts;
        auto kv = kvSplit(s.substr(5));
        auto it = kv.find("sims");
        if (it == kv.end()) {
            throw std::runtime_error("mcts: spec needs 'sims=N' (got: '" + s + "')");
        }
        a.mcts_sims = std::max(1, std::stoi(it->second));
        return a;
    }
    if (s.rfind("ismcts:", 0) == 0) {
        a.kind = AgentSpec::Kind::Ismcts;
        auto kv = kvSplit(s.substr(7));
        auto it = kv.find("sims");
        if (it == kv.end()) {
            throw std::runtime_error("ismcts: spec needs 'sims=N' (got: '" + s + "')");
        }
        a.mcts_sims = std::max(1, std::stoi(it->second));
        return a;
    }
    throw std::runtime_error("Unknown agent spec: '" + s +
                             "'. Use 'random', 'mcts:sims=N', or "
                             "'ismcts:sims=N'. Model-based agents "
                             "(model:/rebel:) live in the riftbound-"
                             "trainer sibling repo.");
}

// True for any spec that drives a tree-search bot. Used to gate the
// "thinking" indicator and switch from random-pick fast path to the
// search slow path.
bool isMctsLike(const AgentSpec& a) {
    return a.kind == AgentSpec::Kind::Mcts ||
           a.kind == AgentSpec::Kind::Ismcts;
}

std::string agentSpecName(const AgentSpec& a) {
    switch (a.kind) {
        case AgentSpec::Kind::Random: return "random";
        case AgentSpec::Kind::Mcts:
            return "mcts:sims=" + std::to_string(a.mcts_sims);
        case AgentSpec::Kind::Ismcts:
            return "ismcts:sims=" + std::to_string(a.mcts_sims);
    }
    return "?";
}

// Wraps another Evaluator so we can count how many sims have happened
// during the current MCTSearch call. Each MCTS leaf evaluation invokes
// Evaluate() once (n_rollouts=1 in our config), so the counter is a
// live view of "sim X / max_sims" while MCTSBot is blocked. Atomic so
// the progress bar's ticker thread can read it without locking.
class CountingEvaluator : public ::open_spiel::algorithms::Evaluator {
public:
    explicit CountingEvaluator(
        std::shared_ptr<::open_spiel::algorithms::Evaluator> inner)
        : inner_(std::move(inner)) {}

    std::vector<double> Evaluate(const ::open_spiel::State& state) override {
        sim_count_.fetch_add(1, std::memory_order_relaxed);
        return inner_->Evaluate(state);
    }
    ::open_spiel::ActionsAndProbs Prior(const ::open_spiel::State& state) override {
        return inner_->Prior(state);
    }

    void reset() { sim_count_.store(0, std::memory_order_relaxed); }
    int count() const { return sim_count_.load(std::memory_order_relaxed); }

private:
    std::shared_ptr<::open_spiel::algorithms::Evaluator> inner_;
    std::atomic<int> sim_count_{0};
};

// One per-seat decision source. MCTSBot keeps a tree across moves on the
// same state and is not safely shared across game threads, so each
// Decider owns its own bot.
struct Decider {
    AgentSpec spec;
    std::unique_ptr<::open_spiel::algorithms::MCTSBot> mcts_bot;
    std::unique_ptr<::open_spiel::algorithms::ISMCTSBot> ismcts_bot;
    // Held separately from the bot (which stores it as a base-class
    // pointer) so we can call counting accessors during search.
    std::shared_ptr<CountingEvaluator> counter;
    std::mt19937_64 rng;

    void prepareForGame(const ::open_spiel::Game& game, int game_index,
                        const ::riftbound::openspiel::RiftboundGame& /*rb_game*/) {
        rng.seed(static_cast<uint64_t>(game_index) * 1009 + 53);
        std::shared_ptr<::open_spiel::algorithms::Evaluator> base_eval;
        if (spec.kind == AgentSpec::Kind::Mcts ||
            spec.kind == AgentSpec::Kind::Ismcts) {
            base_eval =
                std::make_shared<::open_spiel::algorithms::RandomRolloutEvaluator>(
                    /*n_rollouts=*/1, /*seed=*/static_cast<int>(game_index * 17 + 1));
        }
        if (base_eval) {
            counter = std::make_shared<CountingEvaluator>(std::move(base_eval));
            if (spec.kind == AgentSpec::Kind::Ismcts) {
                // Full constructor — needs use_observation_string=true
                // because RiftboundState implements ObservationString
                // (FNV-1a hash) but not InformationStateString. The
                // default constructor uses information-state-string and
                // would fault at search time.
                ismcts_bot = std::make_unique<::open_spiel::algorithms::ISMCTSBot>(
                    /*seed=*/static_cast<int>(game_index * 31 + 7),
                    counter,
                    /*uct_c=*/2.0,
                    /*max_simulations=*/spec.mcts_sims,
                    /*max_world_samples=*/
                        ::open_spiel::algorithms::kUnlimitedNumWorldSamples,
                    /*final_policy_type=*/
                        ::open_spiel::algorithms::ISMCTSFinalPolicyType::kNormalizedVisitCount,
                    /*use_observation_string=*/true,
                    /*allow_inconsistent_action_sets=*/false);
                // ISMCTSBot's default resampler calls
                // State::ResampleFromInfostate(player, rng), which our
                // RiftboundState does not currently override. Without a
                // custom resampler, the bot would throw at SampleRootState
                // time. We install a Clone-only resampler as the minimum
                // viable wiring — no actual hidden-info determinization yet
                // (so the bot behaves like standard MCTS for now). Proper
                // determinization (shuffle opp hand from unseen pool,
                // randomize facedown identities) is queued in CLAUDE.md
                // as a Phase 6b follow-up — pulls from the `observed_cards`
                // memory bank already maintained on PlayerState.
                ismcts_bot->SetResampler(
                    [](const ::open_spiel::State& state,
                       ::open_spiel::Player /*pl*/,
                       std::function<double()> /*rng*/)
                       -> std::unique_ptr<::open_spiel::State> {
                        return state.Clone();
                    });
            } else {
                mcts_bot = std::make_unique<::open_spiel::algorithms::MCTSBot>(
                    game, counter,
                    /*uct_c=*/2.0, /*max_simulations=*/spec.mcts_sims,
                    /*max_memory_mb=*/256, /*solve=*/false,
                    /*seed=*/static_cast<int>(game_index * 31 + 7),
                    /*verbose=*/spec.mcts_verbose);
            }
        }
    }

    // Wraps the chosen action with metadata MCTS can produce (root value,
    // sim count). Random agent fills the action and leaves the optionals
    // unset; readers treat absence as "no estimate available."
    struct Choice {
        ::open_spiel::Action action;
        std::optional<double> mean_value;   // root value in [-1, +1]
        std::optional<int>    sim_count;    // total sims at root
    };

    Choice choose(::open_spiel::State& state,
                  const std::vector<::open_spiel::Action>& legal) {
        if (spec.kind == AgentSpec::Kind::Random) {
            std::uniform_int_distribution<size_t> d(0, legal.size() - 1);
            return Choice{legal[d(rng)], std::nullopt, std::nullopt};
        }
        if (spec.kind == AgentSpec::Kind::Ismcts) {
            // ISMCTSBot exposes Step(state) (returns best action) and
            // RunSearch(state) (returns policy distribution at root).
            // We use Step directly — Bot::Step is the standard inference
            // interface and matches the action-only return shape used by
            // the Mcts path. No root-value or sim-count surface today
            // (ISMCTSNode internals aren't exposed); fields stay nullopt.
            ::open_spiel::Action act = ismcts_bot->Step(state);
            return Choice{act, std::nullopt, std::nullopt};
        }
        // Mcts goes through MCTSearch.
        auto root = mcts_bot->MCTSearch(state);
        ::open_spiel::Action act = root->BestChild().action;
        std::optional<double> v;
        if (root->explore_count > 0) {
            v = root->total_reward / root->explore_count;
        }
        return Choice{act, v, root->explore_count};
    }
};

} // namespace

int main(int argc, char* argv[]) {
    po::options_description desc("Riftbound OpenSpiel match runner");
    desc.add_options()
        ("help,h", "Show help")
        ("version,v", "Print engine version + git commit (matches the tag "
                       "embedded in rendered HTML replays) and exit")
        ("deck1", po::value<std::string>()->default_value("decks/leblanc_test.txt"),
         "Player 1 deck JSON")
        ("deck2", po::value<std::string>()->default_value("decks/leblanc_test.txt"),
         "Player 2 deck JSON")
        ("registry,r", po::value<std::string>()->default_value("cards/ban-list.csv"),
         "Path used only to locate ban-list.csv (card data is compiled in)")
        ("agent1", po::value<std::string>()->default_value("random"),
         "Player 1 agent spec: random | mcts:sims=N | ismcts:sims=N")
        ("agent2", po::value<std::string>()->default_value("random"),
         "Player 2 agent spec: random | mcts:sims=N | ismcts:sims=N")
        ("games,n", po::value<int>()->default_value(10),
         "Number of games to run")
        ("seed,s", po::value<int>()->default_value(0),
         "Game RNG base seed (0 = nondeterministic; >0 = deterministic, game g uses base+g)")
        ("threads,t", po::value<int>()->default_value(1),
         "Number of worker threads (0 = hardware concurrency). MCTS bots are not "
         "thread-safe across seats, so we shard whole games across threads, not seats.")
        ("max-decisions", po::value<int>()->default_value(600),
         "Per-game decision cap (games hitting this count as draws)")
        ("quiet", po::bool_switch()->default_value(false),
         "Suppress per-game progress lines (summary only)")
        ("no-progress", po::bool_switch()->default_value(false),
         "Disable the tqdm-style progress bar (auto-disabled when stderr "
         "isn't a TTY).")
        ("mcts-verbose", po::bool_switch()->default_value(false),
         "Pass verbose=true to OpenSpiel's MCTSBot — surfaces per-simulation "
         "progress to stderr. Useful when one move is taking a long time and "
         "you want to know the bot is working.")
        ("render-html", po::bool_switch()->default_value(false),
         "Write per-game HTML replay (board + decisions + trace log) to "
         "./replays/replay_gameN.html. Decision capture happens at the match "
         "runner level; trace lines are captured via the engine's EventBus "
         "after the state is constructed (so logs emitted during the engine's "
         "setup / mulligan phase aren't included in the replay).")
        ("show-hand", po::bool_switch()->default_value(false),
         "Reveal hand contents in the HTML replay's board render (debug "
         "convenience; the rendered HTML is for the developer, not a player).")
    ;

    po::variables_map vm;
    try {
        po::store(po::parse_command_line(argc, argv, desc), vm);
        if (vm.count("help")) {
            std::cout << desc << std::endl;
            return 0;
        }
        if (vm.count("version")) {
            // Same tag the HTML replay header prints — use this to verify
            // a replay file was produced by this exact binary.
            std::cout << "Riftbound " << ::riftbound::kVersionTag
                      << " (built " << ::riftbound::kBuildDate << ")"
                      << std::endl;
            return 0;
        }
        po::notify(vm);
    } catch (const po::error& e) {
        std::cerr << "Error: " << e.what() << "\n\n" << desc << std::endl;
        return 1;
    }

    AgentSpec spec1, spec2;
    try {
        spec1 = parseAgentSpec(vm["agent1"].as<std::string>());
        spec2 = parseAgentSpec(vm["agent2"].as<std::string>());
        spec1.mcts_verbose = vm["mcts-verbose"].as<bool>();
        spec2.mcts_verbose = vm["mcts-verbose"].as<bool>();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    const auto deck1 = vm["deck1"].as<std::string>();
    const auto deck2 = vm["deck2"].as<std::string>();
    const auto registry = vm["registry"].as<std::string>();
    const auto num_games = std::max(1, vm["games"].as<int>());
    const auto seed_base = vm["seed"].as<int>();
    int threads = vm["threads"].as<int>();
    if (threads <= 0) threads = static_cast<int>(std::thread::hardware_concurrency());
    const auto max_decisions = std::max(1, vm["max-decisions"].as<int>());
    const auto quiet = vm["quiet"].as<bool>();
    const auto render_html = vm["render-html"].as<bool>();
    const auto show_hand = vm["show-hand"].as<bool>();
    const auto no_progress = vm["no-progress"].as<bool>();
    const auto mcts_verbose = vm["mcts-verbose"].as<bool>();

    // Per-run replay dir setup. One-shot at startup so worker threads can
    // each write into it without racing on directory creation.
    if (render_html) {
        std::error_code mkdir_ec;
        std::filesystem::create_directories("replays", mkdir_ec);
        if (mkdir_ec) {
            std::cerr << "Warning: could not create ./replays directory: "
                      << mkdir_ec.message() << "\n";
        }
    }

    auto buildGameStr = [&](int seed) {
        return "riftbound(deck1=" + deck1 +
               ",deck2=" + deck2 +
               ",registry=" + registry +
               ",seed=" + std::to_string(seed) + ")";
    };

    std::cout << "Riftbound " << ::riftbound::kVersionTag
              << " (built " << ::riftbound::kBuildDate << ")" << std::endl;

    // Load once with the base seed; per-game we re-load if seed_base>0 so each
    // game seeds the engine deterministically as seed_base+g.
    const std::string base_game_str = buildGameStr(seed_base);
    std::cout << "Loading: " << base_game_str << "\n";
    auto base_game = ::open_spiel::LoadGame(base_game_str);

    std::cout << "Plan: " << num_games << " games, threads=" << threads
              << ", agent1=" << agentSpecName(spec1)
              << ", agent2=" << agentSpecName(spec2);
    if (seed_base > 0) {
        std::cout << ", deterministic game seeds (base=" << seed_base << ")";
    } else {
        std::cout << ", nondeterministic game RNG";
    }
    std::cout << std::endl;

    std::atomic<int> p1_wins{0}, p2_wins{0}, draws_n{0};
    std::atomic<long long> total_decisions{0};
    std::atomic<int> games_done{0};
    std::mutex out_mu;
    auto t_start = std::chrono::steady_clock::now();

    // Progress bar — shown when stderr is a TTY, --no-progress isn't set,
    // and num_games > 1. Suffix surfaces the MCTS sim count per move so
    // a reader can scan-multiply (sims × avg_decisions = total sim cost
    // per game). Useful for "is this taking forever because sims=200 is
    // too high?" diagnosis.
    std::ostringstream prog_suffix;
    if (isMctsLike(spec1) || isMctsLike(spec2)) {
        prog_suffix << "MCTS sims/move=";
        if (isMctsLike(spec1)) prog_suffix << spec1.mcts_sims;
        if (isMctsLike(spec1) && isMctsLike(spec2)) prog_suffix << "/";
        if (isMctsLike(spec2)) prog_suffix << spec2.mcts_sims;
    }
    // Bar is useful even at num_games=1 because the "thinking N.Ns"
    // indicator surfaces long MCTS thinks. Only suppress when the user
    // explicitly opts out.
    ::riftbound::ProgressBar progress(num_games, !no_progress,
                                       prog_suffix.str());

    // Always-visible live stats — sums the decision counts across all
    // in-flight games and includes the running W/L/D. This ticks
    // forward on every 500ms repaint even when game counter sits at
    // 0/1 (single-game runs), so the bar visibly progresses.
    // Each thread's `decisions` atomic registers a pointer into this
    // vector at game-start, deregisters at game-end. Pointer access
    // is guarded by `live_stats_mu` so the bar's ticker thread reads
    // a consistent snapshot.
    std::mutex live_stats_mu;
    std::vector<std::atomic<int>*> active_decision_ptrs;
    progress.setLiveStatsFn([&] {
        long long sum_dec = 0;
        {
            std::lock_guard<std::mutex> lk(live_stats_mu);
            for (auto* p : active_decision_ptrs) {
                if (p) sum_dec += p->load(std::memory_order_relaxed);
            }
        }
        std::ostringstream s;
        s << "dec=" << sum_dec
          << " W/L/D=" << p1_wins.load() << "/" << p2_wins.load()
          << "/" << draws_n.load();
        return s.str();
    });

    auto runOneGame = [&](int g) {
        std::shared_ptr<const ::open_spiel::Game> game_this =
            (seed_base > 0) ? ::open_spiel::LoadGame(buildGameStr(seed_base + g))
                             : base_game;

        const auto& rb_game =
            *static_cast<const ::riftbound::openspiel::RiftboundGame*>(game_this.get());
        Decider d1, d2;
        d1.spec = spec1; d1.prepareForGame(*game_this, g * 2,     rb_game);
        d2.spec = spec2; d2.prepareForGame(*game_this, g * 2 + 1, rb_game);

        auto state = game_this->NewInitialState();
        auto* rb_state =
            static_cast<::riftbound::openspiel::RiftboundState*>(state.get());

        // Per-game HTML replay: subscribe to the engine's EventBus for trace
        // lines and snapshot the (pre-decision) board state on each ApplyAction
        // round-trip. The EventBus is created inside RiftboundState's
        // constructor and lives until the state is destroyed; subscribing now
        // means we miss logs emitted during the engine's setup / mulligan
        // phase (the worker thread has already run through them by the time
        // NewInitialState returns), which is acceptable — the HTML replay
        // starts from the first user-visible decision.
        std::unique_ptr<::riftbound::ReplayWriter> replay;
        std::unique_ptr<::riftbound::StateRenderer> renderer;
        // The log subscription must NOT outlive `replay`. When a game hits
        // --max-decisions without reaching terminal, ~GameEngine drains the
        // worker thread by feeding it concede-equivalent choices, which can
        // emit additional log events AFTER the main loop exits. We declare
        // the connection BEFORE replay so it destructs AFTER replay (members
        // destruct in reverse declaration order); we ALSO manually
        // disconnect just before resetting replay below, so the lambda's
        // captured replay pointer never dangles. Using a scoped_connection
        // gives us .disconnect() and automatic cleanup as a belt + braces.
        boost::signals2::scoped_connection log_conn;
        boost::signals2::scoped_connection phase_conn;
        if (render_html) {
            std::string replay_path =
                "replays/replay_game" + std::to_string(g + 1) + ".html";
            replay = std::make_unique<::riftbound::ReplayWriter>(
                rb_game.cardDb(), replay_path);
            // Seed shown in the header is the RESOLVED seed:
            //   - seed_base > 0: deterministic, header shows seed_base+g
            //   - seed_base == 0: nondeterministic from CLI's perspective,
            //     but RiftboundState picks a concrete random seed at
            //     construction (so Clone() is reproducible). Read it back
            //     via engineSeed() and surface it so the user can rerun
            //     a "random" game by passing the resolved seed back in.
            std::ostringstream header;
            header << "Game " << (g + 1) << " | "
                   << "agent1=" << agentSpecName(spec1) << " | "
                   << "agent2=" << agentSpecName(spec2)
                   << " | seed=" << rb_state->engineSeed();
            if (seed_base == 0) {
                header << " (random)";
            }
            replay->setGameHeader(header.str());

            renderer = std::make_unique<::riftbound::StateRenderer>(rb_game.cardDb());
            renderer->show_hand = show_hand;

            log_conn = boost::signals2::scoped_connection(
                rb_state->eventBus().on_log.connect(
                    [replay_ptr = replay.get()](const ::riftbound::LogEvent& e) {
                        if (!replay_ptr) return;
                        const char* prefix = "[TRC]";
                        if (e.level == ::riftbound::LogLevel::Debug) prefix = "[DBG]";
                        else if (e.level == ::riftbound::LogLevel::Info) prefix = "[INF]";
                        else if (e.level == ::riftbound::LogLevel::Warning) prefix = "[WRN]";
                        replay_ptr->addTraceLine(std::string(prefix) + " " + e.message);
                    }));
            // Phase-seam snapshots: noop visual stops at every phase
            // transition so the replay scroll has discrete chapter
            // markers. The seam carries the post-transition state's
            // board + phase name; pre-seam trace events stay with the
            // decision that caused them.
            phase_conn = boost::signals2::scoped_connection(
                rb_state->eventBus().on_phase_changed.connect(
                    [replay_ptr = replay.get(),
                     renderer_ptr = renderer.get(),
                     rb_state](const ::riftbound::PhaseChangedEvent& e) {
                        if (!replay_ptr || !renderer_ptr) return;
                        replay_ptr->recordPhaseSeam(rb_state->engineState(),
                                                     e.old_phase, e.new_phase,
                                                     *renderer_ptr);
                    }));
        }

        // Atomic so the progress bar's ticker thread can read the live
        // count while the main thread is blocked inside MCTSearch.
        std::atomic<int> decisions{0};
        // Register this game's counter so the always-visible live-stats
        // suffix on the progress bar shows decisions ticking up even
        // inside a single long game (when the games counter sits at 0/1).
        {
            std::lock_guard<std::mutex> lk(live_stats_mu);
            active_decision_ptrs.push_back(&decisions);
        }
        // Deregister on scope exit — RAII guard so we don't leak the
        // pointer if the game throws.
        struct LiveStatsDeregister {
            std::mutex& mu;
            std::vector<std::atomic<int>*>& vec;
            std::atomic<int>* ptr;
            ~LiveStatsDeregister() {
                std::lock_guard<std::mutex> lk(mu);
                vec.erase(std::remove(vec.begin(), vec.end(), ptr), vec.end());
            }
        } _dereg{live_stats_mu, active_decision_ptrs, &decisions};
        while (!state->IsTerminal() && decisions.load() < max_decisions) {
            auto legal = state->LegalActions();
            if (legal.empty()) break;
            const auto cp = state->CurrentPlayer();
            // Surface "thinking N.Ns [P2:mcts dec 42 sim X/N]" on the
            // progress bar while a potentially-long MCTSearch runs. The
            // live label re-reads the seat's CountingEvaluator + the
            // game-wide decision counter each tick so users see sims
            // accumulate even when one MCTSearch takes many seconds.
            // Random agents complete in microseconds so the indicator
            // only shows when MCTS is actually thinking (>1s by the
            // bar's threshold).
            const auto& spec_now = (cp == 0) ? spec1 : spec2;
            Decider* d_now = (cp == 0) ? &d1 : &d2;
            const bool mcts_like = isMctsLike(spec_now);
            if (mcts_like) {
                d_now->counter->reset();
                int max_sims = spec_now.mcts_sims;
                int player_num = cp + 1;
                const char* kind_tag =
                    (spec_now.kind == AgentSpec::Kind::Ismcts) ? "ismcts"
                                                                : "mcts";
                auto* counter_ptr = d_now->counter.get();
                auto* dec_ptr = &decisions;
                progress.markDecisionStart(
                    [counter_ptr, dec_ptr, max_sims, player_num, kind_tag] {
                        std::ostringstream s;
                        s << "P" << player_num << ":" << kind_tag << " dec "
                          << dec_ptr->load() << " sim "
                          << counter_ptr->count() << "/" << max_sims;
                        return s.str();
                    });
            }
            Decider::Choice choice =
                (cp == 0) ? d1.choose(*state, legal) : d2.choose(*state, legal);
            if (mcts_like) {
                progress.markDecisionEnd();
            }

            // Snapshot the PRE-decision state before applying. The engine has
            // suspended at this decision; engineState() + legalIntents() reflect
            // what the agent saw. If the agent was MCTS, attach its root value
            // estimate (mean simulated reward → win probability for the
            // current player) so the replay header surfaces it.
            if (replay) {
                if (choice.mean_value.has_value()) {
                    replay->setNextWinProb(*choice.mean_value,
                                            choice.sim_count.value_or(0),
                                            static_cast<int>(cp));
                }
                const auto& engine_state = rb_state->engineState();
                const auto intents = rb_state->legalIntents();
                const ::riftbound::Intent* chosen_intent =
                    ::riftbound::openspiel::decodeAction(
                        static_cast<int64_t>(choice.action), intents, engine_state);
                if (chosen_intent) {
                    replay->recordDecision(engine_state, intents,
                                            *chosen_intent, *renderer);
                }
            }

            state->ApplyAction(choice.action);
            decisions.fetch_add(1, std::memory_order_relaxed);
        }
        total_decisions += decisions.load();
        auto ret = state->Returns();
        int winner = -1;
        if (ret[0] > 0.0)      { winner = 0; ++p1_wins; }
        else if (ret[1] > 0.0) { winner = 1; ++p2_wins; }
        else                    {              ++draws_n; }

        // Finalize HTML replay.
        if (replay) {
            const auto& engine_result = rb_state->engineResult();
            std::ostringstream tail;
            tail << "[TRC] GAME_OVER: ";
            if (winner < 0) tail << "draw";
            else tail << (winner == 0 ? "P1" : "P2") << " wins";
            tail << " (" << engine_result.final_scores[0] << "-"
                 << engine_result.final_scores[1] << ")";
            if (!engine_result.termination_reason.empty()) {
                tail << " — " << engine_result.termination_reason;
            }
            replay->addTraceLine(tail.str());
            replay->writeHtml();
        }

        int done = ++games_done;
        if (!quiet) {
            std::lock_guard<std::mutex> lk(out_mu);
            auto elapsed_now = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - t_start).count();
            // Clear the progress bar's line before writing per-game output
            // (the bar is on stderr, but it shares the terminal). Carriage
            // return + spaces over its width works for both interleaving
            // and re-rendering on next update().
            std::cerr << "\r" << std::string(80, ' ') << "\r" << std::flush;
            std::cout << "  game " << done << "/" << num_games
                      << "  winner=" << (winner < 0 ? "draw" :
                                          (winner == 0 ? "P1" : "P2"))
                      << "  decisions=" << decisions.load()
                      << "  cumulative: P1=" << p1_wins.load()
                      << " P2=" << p2_wins.load()
                      << " draw=" << draws_n.load()
                      << "  elapsed=" << static_cast<int>(elapsed_now) << "s"
                      << std::endl;
        }
        progress.update();
        // Refresh the live-stats suffix with running totals so users can
        // see win distribution and avg game length without scrolling.
        {
            int p1 = p1_wins.load();
            int p2 = p2_wins.load();
            int dr = draws_n.load();
            long long dec = total_decisions.load();
            int games = games_done.load();
            std::ostringstream s;
            // Static MCTS info first (if any), then live stats.
            if (isMctsLike(spec1) || isMctsLike(spec2)) {
                s << "MCTS sims/move=";
                if (isMctsLike(spec1)) s << spec1.mcts_sims;
                if (isMctsLike(spec1) && isMctsLike(spec2)) s << "/";
                if (isMctsLike(spec2)) s << spec2.mcts_sims;
                s << " | ";
            }
            s << "W/L/D=" << p1 << "/" << p2 << "/" << dr;
            if (games > 0) {
                s << " | " << (dec / games) << " dec/game";
            }
            progress.setSuffix(s.str());
        }
    };

    if (threads <= 1) {
        for (int g = 0; g < num_games; ++g) runOneGame(g);
    } else {
        std::vector<std::thread> workers;
        std::atomic<int> next{0};
        for (int t = 0; t < threads; ++t) {
            workers.emplace_back([&]() {
                int g;
                while ((g = next.fetch_add(1)) < num_games) runOneGame(g);
            });
        }
        for (auto& w : workers) w.join();
    }

    progress.finish();
    auto elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - t_start).count();
    const int done = p1_wins + p2_wins + draws_n;
    std::cout << "\nCompleted " << done << " games in " << elapsed << "s"
              << " (" << (done / elapsed) << " games/sec)\n";
    if (done > 0) {
        std::cout << "  P1 wins: " << p1_wins.load() << " ("
                  << (100.0 * p1_wins.load() / done) << "%)\n";
        std::cout << "  P2 wins: " << p2_wins.load() << " ("
                  << (100.0 * p2_wins.load() / done) << "%)\n";
        std::cout << "  Draws:   " << draws_n.load() << " ("
                  << (100.0 * draws_n.load() / done) << "%)\n";
        const int decisive = p1_wins + p2_wins;
        if (decisive > 0 && spec1.kind != spec2.kind) {
            // Asymmetric matchup — report P1 decisive win rate (excludes draws).
            std::cout << "  P1 decisive win rate (excl. draws): "
                      << (100.0 * p1_wins.load() / decisive) << "%\n";
        }
        std::cout << "  Avg decisions/game: " << (total_decisions.load() / done) << "\n";
    }
    return 0;
}
