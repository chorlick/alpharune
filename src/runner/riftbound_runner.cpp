/// @file riftbound_runner.cpp
/// Unified entry binary for Riftbound — Phase 6a.
///
/// The only CLI surface is:
///
///   riftbound --config PATH.json [--dry-run]
///
/// All operational parameters (mode, agents, decks, training knobs,
/// model hyperparameters, deck pools) live in the config file. The
/// binary parses + validates the config (JSON Schema draft 7) and
/// then either:
///
///   --dry-run      validates and exits 0 (or prints error and exits non-zero).
///   (no flag)      validates and dispatches to play mode or training mode.
///
/// In Phase 6a only the validation + dispatch skeleton is wired —
/// play and training are stubbed with TODOs that point at the
/// existing `riftbound_openspiel` and `riftbound_train` binaries.
/// Those bodies migrate in Phases 6b–6g. The contract is the config
/// schema, which is locked NOW so Phases 6b+ have a stable shape to
/// bind against.

#include "config/config.h"

#ifdef RIFTBOUND_HAVE_LIBTORCH
#include "training/config_driver.h"
#endif

#include <boost/program_options.hpp>

#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <string>

namespace po = boost::program_options;

namespace {

void printDryRunSummary(const riftbound::config::Config& cfg) {
    std::cout << "Config validated.\n";
    std::cout << "  mode     : "
              << (cfg.mode == riftbound::config::Mode::Play ? "play" : "train")
              << "\n";
    std::cout << "  seed     : " << cfg.seed << "\n";
    std::cout << "  registry : " << cfg.registry << "\n";
    if (cfg.root_dir.has_value()) {
        std::cout << "  root_dir : " << *cfg.root_dir << "\n";
    } else {
        std::cout << "  root_dir : (unset — all paths absolute)\n";
    }
    if (cfg.mode == riftbound::config::Mode::Play) {
        const auto& pb = *cfg.play_block;
        std::cout << "  play.games       : " << pb.play.games << "\n";
        std::cout << "  play.threads     : " << pb.play.threads << "\n";
        std::cout << "  agents.p1.kind   : " << static_cast<int>(pb.p1_agent.kind) << "\n";
        std::cout << "  agents.p2.kind   : " << static_cast<int>(pb.p2_agent.kind) << "\n";
        std::cout << "  decks.p1         : " << pb.p1_deck << "\n";
        std::cout << "  decks.p2         : " << pb.p2_deck << "\n";
    } else {
        const auto& t = *cfg.training;
        std::cout << "  training.legend             : " << t.legend << "\n";
        std::cout << "  training.iterations         : " << t.iterations << "\n";
        std::cout << "  training.games_per_iter     : " << t.games_per_iter << "\n";
        std::cout << "  training.batch_size         : " << t.batch_size << "\n";
        std::cout << "  training.replay_capacity    : " << t.replay_capacity << "\n";
        std::cout << "  training.checkpoint_dir     : " << t.checkpoint_dir << "\n";
        std::cout << "  training.devices            : ";
        for (size_t i = 0; i < t.devices.size(); ++i) {
            if (i > 0) std::cout << ",";
            std::cout << t.devices[i];
        }
        std::cout << "\n";
        std::cout << "  training.train_device       : " << t.train_device << "\n";
        std::cout << "  training.deck_pools.self    : " << t.deck_pools.self.size()
                  << " deck(s)\n";
        std::cout << "  training.deck_pools.opponent: " << t.deck_pools.opponent.size()
                  << " deck(s)\n";
        std::cout << "  training.model.d_model      : " << t.model.d_model << "\n";
        std::cout << "  training.model.encoder_layers: " << t.model.encoder_layers << "\n";
        std::cout << "  training.optimizer.lr       : " << t.optimizer.lr << "\n";
        std::cout << "  training.mcts.uct_c         : " << t.mcts.uct_c << "\n";
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    // When stdout is redirected to a file (not a tty), libc switches
    // from line-buffered to block-buffered — long training runs would
    // sit with an empty log file for minutes at a time. Force
    // line-buffered output so each newline flushes.
    std::setvbuf(stdout, nullptr, _IOLBF, 0);
    std::setvbuf(stderr, nullptr, _IOLBF, 0);

    po::options_description desc("Riftbound unified runner");
    desc.add_options()
        ("help,h",     "Show help")
        ("config,c",   po::value<std::string>()->required(),
                       "Path to JSON config file (schema in src/config/schema.cpp)")
        ("dry-run,d",  po::bool_switch()->default_value(false),
                       "Parse + validate the config and exit. Does not run play/training.");

    po::variables_map vm;
    try {
        po::store(po::parse_command_line(argc, argv, desc), vm);
        if (vm.count("help")) {
            std::cout << desc << "\n";
            return 0;
        }
        po::notify(vm);
    } catch (const po::error& e) {
        std::cerr << "Argument error: " << e.what() << "\n\n" << desc << "\n";
        return 2;
    }

    const std::string config_path = vm["config"].as<std::string>();
    const bool dry_run = vm["dry-run"].as<bool>();

    riftbound::config::Config cfg;
    try {
        cfg = riftbound::config::parseConfig(config_path);
    } catch (const riftbound::config::ConfigError& e) {
        std::cerr << "Config error: " << e.what() << "\n";
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Unexpected error parsing config: " << e.what() << "\n";
        return 1;
    }

    if (dry_run) {
        printDryRunSummary(cfg);
        return 0;
    }

    if (cfg.mode == riftbound::config::Mode::Play) {
        std::cerr << "Play mode dispatch not yet wired — Phase 6b will\n"
                     "migrate riftbound_openspiel's behavior here. For\n"
                     "now run that binary directly.\n";
        return 3;
    } else {
#ifdef RIFTBOUND_HAVE_LIBTORCH
        try {
            riftbound::training::runConfigTraining(cfg);
            return 0;
        } catch (const std::exception& e) {
            std::cerr << "Training error: " << e.what() << "\n";
            return 1;
        }
#else
        std::cerr << "Training mode requires the binary to be built with\n"
                     "-DRIFTBOUND_BUILD_LIBTORCH=ON. This build was\n"
                     "compiled without LibTorch.\n";
        return 3;
#endif
    }
}
