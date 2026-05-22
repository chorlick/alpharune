// Phase C-2 AlphaZero training driver.
//
// One iteration:
//   1. Generate N self-play games using the CURRENT model.
//   2. Run K optimizer steps sampling minibatches from the replay buffer.
//   3. Save a checkpoint.
//   4. (Future) Eval new vs previous checkpoint and promote on win-rate.
//
// Starting point — minimal, single-threaded, CPU/GPU agnostic via
// --device. Tuned for "do the loop without crashing and losses
// trend down" rather than throughput. Multi-thread + GPU
// optimization comes after we see the training signal.

#include "ml/v2_model.h"
#include "action_vocab.h"   // src/openspiel/action_vocab.h — kVocabSize
                            // (auto-sync the model's softmax dim)
#include "training/replay_buffer.h"
#include "training/self_play.h"
#include "training/trainer.h"

#include "open_spiel/spiel.h"

#include <boost/program_options.hpp>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>

namespace po = boost::program_options;

int main(int argc, char* argv[]) {
    // Line-buffered stdout so log files update per-line under
    // redirection. See riftbound_runner.cpp for full rationale.
    std::setvbuf(stdout, nullptr, _IOLBF, 0);
    std::setvbuf(stderr, nullptr, _IOLBF, 0);

    po::options_description desc("Riftbound AlphaZero trainer");
    desc.add_options()
        ("help,h", "Show help")
        ("deck1", po::value<std::string>()
            ->default_value("decks/miss_fortune_test.json"),
            "Player 1 deck JSON")
        ("deck2", po::value<std::string>()
            ->default_value("decks/miss_fortune_test.json"),
            "Player 2 deck JSON")
        ("registry,r", po::value<std::string>()
            ->default_value("cards/registry.json"),
            "Path to card registry JSON")
        ("checkpoint-dir", po::value<std::string>()
            ->default_value("checkpoints"),
            "Directory to write iter_<n>.pt checkpoints")
        ("resume", po::value<std::string>()->default_value(""),
            "Path to a checkpoint to load before training (.pt)")
        ("iterations", po::value<int>()->default_value(10),
            "Number of (self-play + train) cycles to run")
        ("games-per-iter", po::value<int>()->default_value(20),
            "Self-play games generated per iteration")
        ("mcts-sims", po::value<int>()->default_value(50),
            "MCTS simulations per move during self-play")
        ("max-decisions", po::value<int>()->default_value(600),
            "Max decisions per game (hard cap to keep iterations short)")
        ("train-steps", po::value<int>()->default_value(50),
            "Optimizer steps per iteration")
        ("batch-size", po::value<int>()->default_value(64),
            "Training minibatch size")
        ("replay-capacity", po::value<int>()->default_value(50000),
            "Replay buffer capacity (rolling window)")
        ("lr", po::value<double>()->default_value(1e-3),
            "Optimizer learning rate")
        ("weight-decay", po::value<double>()->default_value(1e-4),
            "AdamW weight decay")
        ("grad-clip", po::value<double>()->default_value(1.0),
            "Gradient norm clip (0 to disable)")
        ("device", po::value<std::string>()->default_value("cpu"),
            "Torch device: cpu or cuda")
        ("seed", po::value<uint64_t>()->default_value(42),
            "Random seed for self-play + buffer sampling")
        ("verbose,v", po::bool_switch()->default_value(false),
            "Per-game self-play progress to stderr");

    po::variables_map vm;
    try {
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);
    } catch (const std::exception& e) {
        std::cerr << "Argument error: " << e.what() << "\n" << desc << "\n";
        return 2;
    }
    if (vm.count("help")) { std::cout << desc << "\n"; return 0; }

    const auto deck1            = vm["deck1"].as<std::string>();
    const auto deck2            = vm["deck2"].as<std::string>();
    const auto registry         = vm["registry"].as<std::string>();
    const auto checkpoint_dir   = vm["checkpoint-dir"].as<std::string>();
    const auto resume_path      = vm["resume"].as<std::string>();
    const auto iterations       = vm["iterations"].as<int>();
    const auto games_per_iter   = vm["games-per-iter"].as<int>();
    const auto mcts_sims        = vm["mcts-sims"].as<int>();
    const auto max_decisions    = vm["max-decisions"].as<int>();
    const auto train_steps      = vm["train-steps"].as<int>();
    const auto batch_size       = vm["batch-size"].as<int>();
    const auto replay_capacity  = vm["replay-capacity"].as<int>();
    const auto lr               = vm["lr"].as<double>();
    const auto weight_decay     = vm["weight-decay"].as<double>();
    const auto grad_clip        = vm["grad-clip"].as<double>();
    const auto device_str       = vm["device"].as<std::string>();
    const auto seed             = vm["seed"].as<uint64_t>();
    const auto verbose          = vm["verbose"].as<bool>();

    torch::Device device = torch::kCPU;
    if (device_str == "cuda") {
        if (!torch::cuda::is_available()) {
            std::cerr << "ERROR: --device=cuda requested but torch::cuda is "
                         "not available in this build. Rebuild with "
                         "-DRIFTBOUND_LIBTORCH_VARIANT=cu121 and ensure the "
                         "Linux CUDA toolkit is installed.\n";
            return 2;
        }
        device = torch::kCUDA;
    } else if (device_str != "cpu") {
        std::cerr << "ERROR: unknown --device='" << device_str
                  << "' (expected 'cpu' or 'cuda')\n";
        return 2;
    }

    std::filesystem::create_directories(checkpoint_dir);

    // ── Game ────────────────────────────────────────────────────────
    const std::string game_str =
        "riftbound(deck1=" + deck1 + ",deck2=" + deck2 +
        ",registry=" + registry + ",seed=0)";
    std::cout << "Loading: " << game_str << "\n";
    auto game = ::open_spiel::LoadGame(game_str);

    // ── Model + Trainer ─────────────────────────────────────────────
    // Sync the flat-policy head softmax dim with the live action vocab.
    // Keeping this explicit (rather than relying on the V2ModelConfig
    // default) means a vocab change in action_vocab.h is picked up by
    // freshly-trained checkpoints without a v2_model.h edit. Existing
    // checkpoints loaded via --resume must still match shape — a shape
    // mismatch fails at torch::load with a clear error.
    ::riftbound::ml::V2ModelConfig model_cfg{};
    model_cfg.num_action_slots = ::riftbound::openspiel::kVocabSize;
    ::riftbound::ml::V2Model model(model_cfg);
    if (!resume_path.empty()) {
        std::cout << "Resuming from " << resume_path << "\n";
        torch::load(model, resume_path);
    }

    ::riftbound::training::TrainerConfig train_cfg{};
    train_cfg.learning_rate = lr;
    train_cfg.weight_decay  = weight_decay;
    train_cfg.grad_clip     = grad_clip;
    ::riftbound::training::Trainer trainer(model, train_cfg, device);

    ::riftbound::training::ReplayBuffer buffer(
        static_cast<size_t>(replay_capacity));

    std::mt19937 rng(seed);

    // ── Iteration loop ──────────────────────────────────────────────
    std::cout << std::fixed << std::setprecision(4);
    auto t_total_start = std::chrono::steady_clock::now();
    for (int iter = 0; iter < iterations; ++iter) {
        std::cout << "\n=== iter " << iter << " / " << iterations
                  << " ===\n";

        // Self-play
        ::riftbound::training::SelfPlayConfig sp_cfg{};
        sp_cfg.num_games     = games_per_iter;
        sp_cfg.mcts_sims     = mcts_sims;
        sp_cfg.max_decisions = max_decisions;
        sp_cfg.verbose       = verbose;

        auto t_sp_start = std::chrono::steady_clock::now();
        auto sp_stats =
            ::riftbound::training::runSelfPlay(*game, model, buffer, sp_cfg, rng);
        auto sp_secs = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - t_sp_start).count();
        std::cout << "  self-play: " << sp_stats.games_played << " games, "
                  << sp_stats.tuples_added << " tuples, W/L/D="
                  << sp_stats.p1_wins << "/" << sp_stats.p2_wins << "/"
                  << sp_stats.draws
                  << "  avg_dec=" << (sp_stats.games_played > 0
                        ? sp_stats.total_decisions / sp_stats.games_played : 0)
                  << "  (" << sp_secs << "s)\n";

        if (buffer.size() < static_cast<size_t>(batch_size)) {
            std::cout << "  buffer too small (" << buffer.size()
                      << " < batch_size " << batch_size
                      << ") — skipping train\n";
            continue;
        }

        // Train
        auto t_tr_start = std::chrono::steady_clock::now();
        double sum_pl = 0, sum_vl = 0, sum_tl = 0;
        for (int s = 0; s < train_steps; ++s) {
            auto batch = buffer.sample(
                static_cast<size_t>(batch_size), rng);
            auto [pl, vl, tl] = trainer.step(batch);
            sum_pl += pl; sum_vl += vl; sum_tl += tl;
        }
        auto tr_secs = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - t_tr_start).count();
        std::cout << "  train:     " << train_steps << " steps, "
                  << "loss policy=" << (sum_pl / train_steps)
                  << " value=" << (sum_vl / train_steps)
                  << " total=" << (sum_tl / train_steps)
                  << "  (" << tr_secs << "s)\n";

        // Checkpoint
        auto ckpt = checkpoint_dir + "/iter_" + std::to_string(iter) + ".pt";
        trainer.save(ckpt);
        std::cout << "  saved " << ckpt << "\n";
    }

    auto total_secs = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - t_total_start).count();
    std::cout << "\nDone. " << iterations << " iterations in "
              << total_secs << "s\n";
    return 0;
}
