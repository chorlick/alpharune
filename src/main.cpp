/// @file main.cpp
/// Riftbound Engine CLI entry point.
///
/// Usage: riftbound <deck1.json> <deck2.json> [--registry path] [--seed N]
///
/// Takes two deck files (output of deck_import.py) and runs a game
/// between two random agents.

#include "agents/random_agent.h"
#include "cards/card_registry.h"
#include "core/card_db.h"
#include "core/events.h"
#include "engine/batch_runner.h"
#include "engine/game_engine.h"
#include "engine/game_runner.h"
#include "io/data_serializer.h"
#include "io/replay_writer.h"
#include "io/state_renderer.h"
#include "rules/deck_validator.h"

#include <boost/program_options.hpp>
#include <thread>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>

namespace po = boost::program_options;
namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
    po::options_description desc("Riftbound Engine");
    desc.add_options()
        ("help,h", "Show help")
        ("deck1", po::value<std::string>()->required(), "Player 1 deck JSON")
        ("deck2", po::value<std::string>()->required(), "Player 2 deck JSON")
        ("registry,r", po::value<std::string>()->default_value("cards/registry.json"),
         "Path to card registry JSON")
        ("seed,s", po::value<uint64_t>()->default_value(0),
         "RNG seed (0 = random)")
        ("games,n", po::value<int>()->default_value(1),
         "Number of games to run")
        ("render", po::bool_switch()->default_value(false),
         "Print ASCII board state each turn")
        ("output,o", po::value<std::string>()->default_value(""),
         "Output path for JSON-lines training data")
        ("show-hand", po::bool_switch()->default_value(false),
         "Show hand contents in render (debug)")
        ("step", po::bool_switch()->default_value(false),
         "Step through game turn by turn (press Enter to advance)")
        ("debug", po::bool_switch()->default_value(false),
         "Show debug log (triggered abilities, chain events, effect execution)")
        ("trace", po::bool_switch()->default_value(false),
         "Show trace log (every game action — phases, intents, targets, costs, combat, scoring)")
        ("threads,t", po::value<int>()->default_value(1),
         "Number of threads for parallel game execution (0 = hardware concurrency)")
    ;

    po::positional_options_description pos;
    pos.add("deck1", 1);
    pos.add("deck2", 1);

    po::variables_map vm;
    try {
        po::store(po::command_line_parser(argc, argv)
                      .options(desc).positional(pos).run(), vm);
        if (vm.count("help")) {
            std::cout << desc << std::endl;
            return 0;
        }
        po::notify(vm);
    } catch (const po::error& e) {
        std::cerr << "Error: " << e.what() << "\n\n" << desc << std::endl;
        return 1;
    }

    auto registry_path = vm["registry"].as<std::string>();
    auto deck1_path = vm["deck1"].as<std::string>();
    auto deck2_path = vm["deck2"].as<std::string>();
    auto seed = vm["seed"].as<uint64_t>();
    auto num_games = vm["games"].as<int>();
    auto do_render = vm["render"].as<bool>();
    auto output_path = vm["output"].as<std::string>();
    auto show_hand = vm["show-hand"].as<bool>();
    auto step_mode = vm["step"].as<bool>();
    auto debug_mode = vm["debug"].as<bool>();
    auto trace_mode = vm["trace"].as<bool>();
    auto num_threads_opt = vm["threads"].as<int>();
    int num_threads = num_threads_opt <= 0
        ? static_cast<int>(std::thread::hardware_concurrency())
        : num_threads_opt;
    if (trace_mode) debug_mode = true; // trace implies debug
    if (step_mode) do_render = true; // step implies render
    // Force single-threaded for interactive modes
    if (step_mode || do_render) num_threads = 1;

    // Load card database and card registry (shared singletons)
    riftbound::CardDB card_db;
    riftbound::CardRegistry card_registry;
    try {
        card_db.loadFromRegistry(registry_path);
        card_registry.loadAll();
        std::cout << "Loaded " << card_db.size() << " cards from registry ("
                  << card_registry.size() << " card objects)\n";
    } catch (const std::exception& e) {
        std::cerr << "Failed to load registry: " << e.what() << "\n";
        return 1;
    }

    // Load and validate decks
    riftbound::DeckValidator validator(card_db);

    // Load ban list if available
    {
        std::string ban_path = registry_path;
        auto slash = ban_path.rfind('/');
        if (slash != std::string::npos)
            ban_path = ban_path.substr(0, slash + 1) + "ban-list.csv";
        else
            ban_path = "cards/ban-list.csv";
        validator.loadBanList(ban_path);
    }

    riftbound::DeckSubmission deck1, deck2;
    try {
        deck1 = riftbound::DeckValidator::loadFromJson(deck1_path, card_db);
        deck2 = riftbound::DeckValidator::loadFromJson(deck2_path, card_db);
    } catch (const std::exception& e) {
        std::cerr << "Failed to load deck: " << e.what() << "\n";
        return 1;
    }

    auto result1 = validator.validate(deck1);
    auto result2 = validator.validate(deck2);
    if (!result1.is_legal) {
        std::cerr << "Deck 1 validation failed:\n";
        for (auto& e : result1.errors) std::cerr << "  - " << e << "\n";
        return 1;
    }
    if (!result2.is_legal) {
        std::cerr << "Deck 2 validation failed:\n";
        for (auto& e : result2.errors) std::cerr << "  - " << e << "\n";
        return 1;
    }

    auto& legend1 = card_db.get(deck1.legend);
    auto& champ1 = card_db.get(deck1.chosen_champion);
    auto& legend2 = card_db.get(deck2.legend);
    auto& champ2 = card_db.get(deck2.chosen_champion);

    std::cout << "P1: " << legend1.name << " / " << champ1.name << "\n";
    std::cout << "P2: " << legend2.name << " / " << champ2.name << "\n";

    // Build game config
    riftbound::GameConfig config;
    config.base_seed = seed;
    config.do_render = do_render;
    config.show_hand = show_hand;
    config.step_mode = step_mode;
    config.debug_mode = debug_mode;
    config.trace_mode = trace_mode;
    config.output_path = output_path;

    // Run games via BatchRunner
    riftbound::AggregateResults results;
    riftbound::BatchRunner batch(card_db, card_registry, num_threads);
    batch.runBatch(deck1, deck2, num_games, config, results);

    std::cout << "\nResults (" << num_games << " games):\n";
    std::cout << "  P1 wins: " << results.p1_wins.load() << "\n";
    std::cout << "  P2 wins: " << results.p2_wins.load() << "\n";
    std::cout << "  Draws:   " << results.draws.load() << "\n";
    if (num_games > 1) {
        std::cout << "  Avg turns: " << results.total_turns.load() / num_games << "\n";
        std::cout << "  Avg decisions: " << results.total_decisions.load() / num_games << "\n";
        if (num_threads > 1) {
            std::cout << "  Threads: " << num_threads << "\n";
        }
    }

    return 0;
}

