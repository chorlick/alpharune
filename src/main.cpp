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
///   mcts:sims=N       — OpenSpiel MCTSBot over a mirror of the live game,
///                       leaves scored by RiftboundHeuristicEvaluator (no rollout)
///   ismcts:sims=N     — information-set MCTS: our IsMctsSearch (the ISMCTSBot
///                       algorithm, reproducible for a seed), same mirror
///                       and evaluator
/// (agents/mcts_agent.{h,cpp}; keep this and the --help text below in step
/// with that file.)
///
/// When any seat is human:
///   • `god_mode` edits are accepted by the WebSocket API (--god-mode off
///     turns them off, and with them the view of the opponent's hand).
///   • An HTML replay is written to ./replays/<timestamp>/replay.html.
///   • Trace + debug logging stream to the UI by default.
///
/// See docs/play-api.md for the WS wire protocol.

#include "agents/human_agent.h"
#include "agents/mcts_agent.h"
#include "agents/random_agent.h"
#include "cards/card_registry.h"
#include "core/card_db.h"
#include "openspiel/riftbound_game.h"
#include "core/events.h"
#include "core/version.h"
#include "engine/batch_runner.h"
#include "engine/game_engine.h"
#include "io/batch_report.h"
#include "io/play_server.h"
#include "io/replay_writer.h"
#include "io/state_renderer.h"
#include "rules/deck_validator.h"

#include <boost/asio/ip/address.hpp>
#include <boost/program_options.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <atomic>
#include <fstream>
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

// ── --dump-registry: serialize the compiled card DB to registry-style JSON ──
// A validation/export tool. Card data is owned by the C++ classes; this dumps
// it back out so it can be diffed one-to-one against a reference registry.json.
void dumpRegistry(const CardDB& db, const std::string& path) {
    using json = nlohmann::json;
    auto cardTypeStr = [](CardType t) -> std::string {
        switch (t) {
            case CardType::Unit: return "unit";
            case CardType::Spell: return "spell";
            case CardType::Gear: return "gear";
            case CardType::Rune: return "rune";
            case CardType::Battlefield: return "battlefield";
            case CardType::Legend: return "legend";
        }
        return "unit";
    };
    auto superTypeJson = [](SuperType s) -> json {
        switch (s) {
            case SuperType::Champion: return "champion";
            case SuperType::Signature: return "signature";
            case SuperType::Token: return "token";
            case SuperType::None: break;
        }
        return nullptr;
    };
    auto domainStr = [](Domain d) -> std::string {
        switch (d) {
            case Domain::Fury: return "fury";
            case Domain::Calm: return "calm";
            case Domain::Mind: return "mind";
            case Domain::Body: return "body";
            case Domain::Chaos: return "chaos";
            case Domain::Order: return "order";
            default: return "fury";
        }
    };
    auto rarityStr = [](Rarity r) -> std::string {
        switch (r) {
            case Rarity::Common: return "common";
            case Rarity::Uncommon: return "uncommon";
            case Rarity::Rare: return "rare";
            case Rarity::Epic: return "epic";
            case Rarity::Showcase: return "showcase";
        }
        return "common";
    };
    // Keyword bit order — must match the engine's canonical order.
    static const Keyword kKwOrder[] = {
        Keyword::Accelerate, Keyword::Action, Keyword::Ambush, Keyword::Assault,
        Keyword::Backline, Keyword::Deathknell, Keyword::Deflect, Keyword::Equip,
        Keyword::Ganking, Keyword::Hidden, Keyword::Hunt, Keyword::Legion,
        Keyword::Level, Keyword::Predict, Keyword::QuickDraw, Keyword::Reaction,
        Keyword::Repeat, Keyword::Shield, Keyword::Tank, Keyword::Temporary,
        Keyword::Unique, Keyword::Vision, Keyword::Weaponmaster,
    };

    // Sort by card_id for stable, diff-friendly output.
    std::vector<CardDefId> ids;
    for (const auto& [id, def] : db.all()) ids.push_back(id);
    std::sort(ids.begin(), ids.end());

    json cards = json::array();
    for (CardDefId id : ids) {
        const CardDef& d = db.get(id);
        json kw = json::array();
        for (Keyword k : kKwOrder) kw.push_back(d.keywords.has(k) ? 1 : 0);
        json doms = json::array();
        for (Domain dm : d.domains) doms.push_back(domainStr(dm));
        cards.push_back({
            {"card_id", d.id},
            {"card_def_id", d.def_id},
            {"name", d.name},
            {"set", d.set_code},
            {"set_name", d.set_name},
            {"public_code", d.public_code},
            {"collector_number", d.collector_number},
            {"artist", d.artist},
            {"card_type", cardTypeStr(d.card_type)},
            {"super_type", superTypeJson(d.super_type)},
            {"domains", doms},
            {"tags", d.tags},
            {"energy_cost", d.energy_cost},
            {"power_cost", d.power_cost},
            {"might", d.might},
            {"might_bonus", d.might_bonus},
            {"rarity", rarityStr(d.rarity)},
            {"ability_text", d.ability_text},
            {"effect_text", d.effect_text},
            {"image_url", d.image_url},
            {"banned", d.banned},
            {"features", {
                {"assault_value", d.assault_value},
                {"shield_value", d.shield_value},
                {"deflect_value", d.deflect_value},
                {"keywords", kw},
            }},
        });
    }
    json out = {
        {"version", 1},
        {"num_cards", cards.size()},
        {"source", "compiled-from-cpp-classes"},
        {"cards", cards},
    };
    std::ofstream f(path);
    f << out.dump(2) << "\n";
}

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
    const bool search = (out.kind == "mcts" || out.kind == "ismcts");
    if (colon != std::string::npos) {
        auto rest = s.substr(colon + 1);
        // key=value pairs separated by commas; only "sims=N" recognised.
        size_t i = 0;
        while (i < rest.size()) {
            auto eq    = rest.find('=', i);
            auto comma = rest.find(',', i);
            if (comma == std::string::npos) comma = rest.size();
            auto key = rest.substr(i, std::min(eq, comma) - i);
            if (!search || key != "sims" || eq == std::string::npos || eq > comma) {
                throw std::runtime_error(
                    "Agent spec '" + s + "': unrecognised option '" +
                    rest.substr(i, comma - i) + "'." +
                    (search ? " Expected sims=N." : " '" + out.kind + "' takes no options."));
            }
            auto val = rest.substr(eq + 1, comma - eq - 1);
            size_t used = 0;
            try { out.sims = std::stoi(val, &used); } catch (const std::exception&) { used = 0; }
            if (used == 0 || used != val.size()) {
                throw std::runtime_error(
                    "Agent spec '" + s + "': sims must be a whole number, got '" + val + "'.");
            }
            i = comma + 1;
        }
    }
    // Checked whether or not the spec had a ':' — a bare "mcts" used to slip
    // through with sims=0.
    if (search && out.sims <= 0) {
        throw std::runtime_error(
            "Agent '" + out.kind + "' requires sims=N with N > 0 (e.g. " +
            out.kind + ":sims=50).");
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
                                           PlayerId seat,
                                           HumanAgent** human_out) {
    if (spec.kind == "random") return std::make_unique<RandomAgent>(derived_seed);
    if (spec.kind == "human") {
        auto a = std::make_unique<HumanAgent>(seat);
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

/// Parses an auto|on|off flag value. Throws on anything else: a typo such
/// as "--web true" must not silently mean "auto".
bool resolveTriState(const std::string& flag, const std::string& value, bool auto_value) {
    if (value == "on")   return true;
    if (value == "off")  return false;
    if (value == "auto") return auto_value;
    throw std::runtime_error("--" + flag + " must be one of: auto | on | off (got '" + value + "')");
}

std::string randomToken() {
    std::random_device rd;
    std::ostringstream s;
    s << std::hex << std::setfill('0');
    for (int i = 0; i < 4; ++i) s << std::setw(8) << rd();
    return s.str();
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
        "  mcts:sims=N       OpenSpiel MCTSBot. N is the simulation budget\n"
        "                    per decision (try sims=20 for fast tests,\n"
        "                    sims=200+ for stronger play). A leaf is scored\n"
        "                    by a heuristic evaluator of the position, not\n"
        "                    by a random rollout. The agent keeps a mirror\n"
        "                    of the live game and, at each decision, replays\n"
        "                    onto it only the choices made since its last\n"
        "                    one, so the cost per decision does not grow\n"
        "                    with the length of the game.\n"
        "  ismcts:sims=N     Information-set MCTS: the engine's own search\n"
        "                    (the algorithm of OpenSpiel's ISMCTSBot, made\n"
        "                    reproducible for a fixed seed), same N, mirror\n"
        "                    and evaluator. It does not yet sample the\n"
        "                    opponent's hidden cards: each simulation runs\n"
        "                    on a copy of the real state.\n"
        "                    Caveat for both: a god-mode edit bypasses the\n"
        "                    history the mirror is built from. After one the\n"
        "                    agent searches from a copy of the live state at\n"
        "                    main-phase decisions only and answers every\n"
        "                    other decision at random.\n"
        "\n"
        "Common invocations:\n"
        "  ./riftbound --deck1 D1.txt --deck2 D2.txt\n"
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
        ("deck1", po::value<std::string>()->required(), "Player 1 deck list (.txt)")
        ("deck2", po::value<std::string>()->required(), "Player 2 deck list (.txt)")
        ("registry,r", po::value<std::string>()->default_value("cards/registry.json"),
         "Deprecated/ignored — card data and ban status are compiled into the engine")
        ("dump-registry", po::value<std::string>(),
         "Dump the compiled card database to a registry-style JSON file and exit "
         "(validation/export tool; does not require decks)")
        ("agent1", po::value<std::string>()->default_value("random"),
         "Player 1 agent spec (see top of --help for full descriptions). "
         "One of: random | human | mcts:sims=N | ismcts:sims=N")
        ("agent2", po::value<std::string>()->default_value("random"),
         "Player 2 agent spec (see top of --help for full descriptions). "
         "One of: random | human | mcts:sims=N | ismcts:sims=N")
        ("web", po::value<std::string>()->default_value("auto"),
         "Web UI mode: auto (default — ON if any seat is human, else OFF), "
         "on (force ON: with no human seat this is spectator mode), off. "
         "Single game only.")
        ("port,p", po::value<uint16_t>()->default_value(8080),
         "TCP port for the web UI when --web is on")
        ("bind", po::value<std::string>()->default_value("127.0.0.1"),
         "Bind address for the web UI when --web is on. Anything other than "
         "loopback exposes the game to the network: the server then requires "
         "the access token it prints in the URL.")
        ("god-mode", po::value<std::string>()->default_value("auto"),
         "God-mode state-editor in the web UI: auto (default — ON if any "
         "seat is human), on (force ON), off (force OFF, even with a human seat). "
         "While it is on the UI shows every hand and deck; turn it off to play "
         "without seeing the opponent's cards.")
        ("seed,s", po::value<uint64_t>()->default_value(0),
         "RNG seed (0 = random)")
        ("render-html", po::value<std::string>()->default_value("auto"),
         "Write HTML replay: auto (default — ON if any seat is human), "
         "on (force ON), off (force OFF)")
        ("replay-dir", po::value<std::string>()->default_value("replays"),
         "Directory for HTML replay output. Single-game writes "
         "<dir>/<timestamp>/replay.html; batch writes "
         "<dir>/<timestamp>/replay_gameN.html.")
        ("debug", po::bool_switch()->default_value(false),
         "Print debug-level engine events (trigger firings, ability resolution) "
         "to stdout. With a human seat they are implied and go to the web UI.")
        ("trace", po::bool_switch()->default_value(false),
         "Print trace-level engine events (phases, intents, draws, kills, "
         "scoring) to stdout. Implies --debug. With a human seat they are "
         "implied and go to the web UI.")
        ("games,n", po::value<int>()->default_value(1),
         "Number of games (>= 1). > 1 runs via BatchRunner: no human seats, no web UI.")
        ("threads,t", po::value<int>()->default_value(1),
         "Parallel workers for --games > 1 (0 = hardware concurrency).")
        ("show-hand", po::bool_switch()->default_value(false),
         "Name facedown cards in replay renders, and show every hand in the "
         "web UI even with god mode off (debug aid).")
        ("wild", po::bool_switch()->default_value(false),
         "Wild format: allow banned cards in decks (skips the ban-list check).")
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
        if (vm.count("dump-registry")) {
            const CardDB& db = openspiel::sharedCards().db;   // authored data only
            auto out_path = vm["dump-registry"].as<std::string>();
            dumpRegistry(db, out_path);
            std::cout << "Dumped " << db.size() << " cards to " << out_path << "\n";
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

    // Resolve auto-mode flags. Every flag is checked here, before anything
    // is loaded or written to disk.
    bool web_enabled, render_html, god_mode_enabled;
    bool bind_is_loopback = true;
    try {
        web_enabled      = resolveTriState("web", web_mode, any_human);
        render_html      = resolveTriState("render-html", render_mode, any_human);
        god_mode_enabled = resolveTriState("god-mode", god_mode_arg, any_human);
        if (web_enabled) {
            boost::system::error_code ec;
            auto addr = boost::asio::ip::make_address(bind_address, ec);
            if (ec) throw std::runtime_error("--bind '" + bind_address + "' is not an IP address");
            bind_is_loopback = addr.is_loopback();
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    // Flags the user typed print to stdout; the logging a human seat
    // implies goes to the web UI and the replay only.
    const bool log_to_stdout = debug_mode || trace_mode;
    // Human seat implies full logging — user wants to see what the engine
    // is doing. Trace implies debug.
    if (any_human) { debug_mode = true; trace_mode = true; }
    if (trace_mode) debug_mode = true;

    if (num_games < 1) {
        std::cerr << "Error: --games must be at least 1.\n";
        return 1;
    }
    if (num_games > 1 && any_human) {
        std::cerr << "Error: --games > 1 cannot be combined with a 'human' "
                     "seat. Use --games 1 for interactive play.\n";
        return 1;
    }
    if (num_games > 1 && web_mode == "on") {
        std::cerr << "Error: --web on needs --games 1 (the web UI shows a single game).\n";
        return 1;
    }
    if (any_human && !web_enabled) {
        // A human seat with --web off would just hang forever.
        std::cerr << "Error: --web off cannot be combined with a 'human' seat.\n";
        return 1;
    }
    if (num_threads <= 0) {
        num_threads = std::max(1, static_cast<int>(std::thread::hardware_concurrency()));
    }

    std::cout << "Riftbound " << kVersionTag
              << " (built " << kBuildDate << ")\n";

    // ── Load card database (from the C++ classes), decks ──────────────────
    // Card data — including tournament legality (CardDef::banned) — is owned by
    // the Card classes. registry.json and ban-list.csv are retired.
    // One card table per process: the one the search agents' games use
    // (openspiel::sharedCards). This used to build a second registry and
    // database of its own next to that one.
    const openspiel::SharedCards* shared_cards = nullptr;
    try {
        shared_cards = &openspiel::sharedCards();
    } catch (const std::exception& e) {
        std::cerr << "Failed to load cards: " << e.what() << "\n";
        return 1;
    }
    const CardDB& card_db = shared_cards->db;
    const CardRegistry& card_registry = shared_cards->registry;
    std::cout << "Loaded " << card_db.size() << " cards from classes\n";

    DeckValidator validator(card_db);
    const bool wild = vm["wild"].as<bool>();
    if (wild) {
        std::cout << "Wild format: banned cards are allowed (--wild).\n";
        validator.setWild(true);
    }
    // Otherwise: banned cards are rejected via CardDef::banned (DeckValidator::isBanned).

    auto endsWith = [](const std::string& s, const std::string& suf) {
        return s.size() >= suf.size() &&
               s.compare(s.size() - suf.size(), suf.size(), suf) == 0;
    };
    auto loadDeck = [&](const std::string& p) {
        if (!endsWith(p, ".txt")) {
            throw std::runtime_error(
                "Deck '" + p + "' must be a .txt deck list (Piltover Archive "
                "format). The legacy JSON deck loader has been retired.");
        }
        return DeckValidator::loadFromDeckList(p, card_db);
    };
    DeckSubmission deck1, deck2;
    try {
        deck1 = loadDeck(deck1_path);
        deck2 = loadDeck(deck2_path);
    } catch (const std::exception& e) {
        std::cerr << "Failed to load deck: " << e.what() << "\n";
        return 1;
    }

    // Debug aid for beta testers: log exactly which cards (by gallery def_id)
    // each deck resolved to, so they can confirm the parser loaded the intended
    // cards from their Piltover Archive deck list.
    if (debug_mode) {
        auto logDeck = [&](const std::string& label, const std::string& path,
                           const DeckSubmission& d) {
            std::cout << "[DECK] " << label << " (" << path << "):\n";
            auto line = [&](const char* zone, CardDefId id) {
                const auto& c = card_db.get(id);
                std::cout << "    " << zone << "  [" << c.def_id << "] " << c.name
                          << (c.banned ? "  (BANNED)" : "") << "\n";
            };
            line("legend  ", d.legend);
            line("champion", d.chosen_champion);
            for (auto id : d.main_deck)    line("main    ", id);
            for (auto id : d.battlefields) line("bf      ", id);
            for (auto id : d.rune_deck)    line("rune    ", id);
            for (auto id : d.sideboard)    line("side    ", id);
        };
        logDeck("P1", deck1_path, deck1);
        logDeck("P2", deck2_path, deck2);
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
                return buildAgent(s, game_seed, derived, d1, d2, r,
                                  seat == 0 ? PlayerId::Player1 : PlayerId::Player2, nullptr);
            };

        GameConfig cfg;
        if (render_html) {
            // One directory per run, so a second run does not overwrite the first.
            auto dir = fs::path(replay_dir) / timestampDirName();
            std::error_code ec;
            fs::create_directories(dir, ec);
            if (ec) {
                std::cerr << "Error: cannot create replay directory '" << dir.string()
                          << "': " << ec.message() << "\n";
                return 1;
            }
            cfg.replay_dir = dir.string();
            std::cout << "Replays will be written to " << cfg.replay_dir
                      << "/replay_gameN.html\n";
        }
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
        try {
            batch.runBatch(deck1, deck2, num_games, cfg, results);
        } catch (const std::exception& e) {
            // The games that completed are still results: table first, then
            // the error. Exit status stays 1.
            reportFailedBatch(std::cout, std::cerr, results, num_games, num_threads, e.what());
            return 1;
        }

        printBatchResults(std::cout, results, num_games, num_threads);
        if (log_to_stdout)
            std::cout << "Card tables built in this process: " << CardDB::tablesBuilt() << "\n";
        return 0;
    }

    // ── Single-game (interactive) flow ────────────────────────────────────
    HumanAgent* p1_human_ptr = nullptr;
    HumanAgent* p2_human_ptr = nullptr;
    std::unique_ptr<AgentInterface> agent1, agent2;
    try {
        // game_seed flows through verbatim so MCTS-style agents seed their
        // internal OpenSpiel state with the same value runGame uses below.
        agent1 = buildAgent(spec1, game_seed, game_seed * 2,     deck1_path, deck2_path, registry_path, PlayerId::Player1, &p1_human_ptr);
        agent2 = buildAgent(spec2, game_seed, game_seed * 2 + 1, deck1_path, deck2_path, registry_path, PlayerId::Player2, &p2_human_ptr);
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
    // Names facedown cards in renders. On for a human seat: the web UI
    // then decides per frame whose cards that seat may see.
    renderer.show_hand = show_hand || any_human;

    std::unique_ptr<ReplayWriter> replay;
    std::string replay_dir_path;
    std::string replay_path;
    if (render_html) {
        replay_dir_path = (fs::path(replay_dir) / timestampDirName()).string();
        std::error_code mkdir_ec;
        fs::create_directories(replay_dir_path, mkdir_ec);
        if (mkdir_ec) {
            std::cerr << "Error: cannot create replay directory '" << replay_dir_path
                      << "': " << mkdir_ec.message() << "\n";
            return 1;
        }
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

    // --debug / --trace as typed: print the engine's log to stdout.
    if (log_to_stdout) {
        events.on_log.connect([trace_mode](const LogEvent& e) {
            if (e.level == LogLevel::Trace && !trace_mode) return;
            const char* prefix = "[TRC]";
            if (e.level == LogLevel::Debug)   prefix = "[DBG]";
            if (e.level == LogLevel::Info)    prefix = "[INF]";
            if (e.level == LogLevel::Warning) prefix = "[WRN]";
            std::cout << "  " << prefix << " " << e.message << "\n";
        });
    }

    // ── Web server (optional) ─────────────────────────────────────────────
    std::unique_ptr<PlayServer> server;
    // AI seats are wrapped once the server exists (see ServedAiSeat).
    std::unique_ptr<AgentInterface> published1, published2;
    AgentInterface* seat1 = agent1.get();
    AgentInterface* seat2 = agent2.get();
    if (web_enabled) {
        PlayServerConfig server_config;
        server_config.port             = port;
        server_config.bind_address     = bind_address;
        server_config.god_mode_enabled = god_mode_enabled;
        // The god-mode editor lists every zone, and with no human seat
        // there is nobody to hide a hand from. Otherwise a frame carries
        // only what the human seat may see.
        server_config.reveal_hidden    = god_mode_enabled || !any_human || show_hand;
        server_config.p1_human         = p1_human_ptr;
        server_config.p2_human         = p2_human_ptr;
        server_config.spectator        = !any_human;
        server_config.version_tag      = kVersionTag;
        server_config.build_date       = kBuildDate;
        server_config.seed             = game_seed;
        // Off loopback anyone on the network can reach the port; make them
        // need the URL this prints.
        if (!bind_is_loopback) server_config.access_token = randomToken();
        server_config.on_shutdown_signal = [p1_human_ptr, p2_human_ptr] {
            if (p1_human_ptr) p1_human_ptr->abort();
            if (p2_human_ptr) p2_human_ptr->abort();
        };

        server = std::make_unique<PlayServer>(engine, renderer, card_db,
                                              std::move(server_config));
        std::string url;
        try {
            url = server->start();
        } catch (const std::exception& e) {
            std::cerr << "Error: cannot start the web UI on " << bind_address << ":"
                      << port << ": " << e.what() << "\n";
            return 1;
        }
        std::cout << "\n  → Open " << url << " in your browser to play.\n\n";
        if (!bind_is_loopback) {
            std::cout << "  The web UI is reachable from the network. Anyone with the URL "
                         "above can play this game"
                      << (god_mode_enabled ? " and edit its state" : "") << ".\n\n";
        }

        if (!any_human) {
            std::cout << "  Spectator mode: the game is paused at each decision — "
                         "use Step / Play in the browser to advance.\n\n";
        }
        // After every decision: retire the prompt that belonged to it, and
        // in spectator mode hold the engine until the viewer steps (or
        // autoplay advances) in the browser.
        engine.on_decision = [s = server.get(), prev = std::move(engine.on_decision)](
                                 const GameState& st,
                                 const std::vector<Intent>& actions,
                                 const Intent& chosen) {
            if (prev) prev(st, actions, chosen);
            s->awaitSpectatorStep(actions, chosen);
            s->notifyDecisionMade();
        };

        if (!p1_human) {
            published1 = std::make_unique<ServedAiSeat>(*agent1, *server);
            seat1 = published1.get();
        }
        if (!p2_human) {
            published2 = std::make_unique<ServedAiSeat>(*agent2, *server);
            seat2 = published2.get();
        }

        if (any_human && !god_mode_enabled && !show_hand) {
            std::cout << "  Fair play: the opponent's hand, both decks and the engine "
                         "log are not sent to the browser.\n\n";
        }

        // Stream engine log events into the UI log panel. (The server
        // drops them when it is withholding hidden information.)
        if (debug_mode || trace_mode) {
            events.on_log.connect([s = server.get(), trace_mode](const LogEvent& e) {
                if (e.level == LogLevel::Trace && !trace_mode) return;
                const char* level = "TRC";
                if (e.level == LogLevel::Debug)   level = "DBG";
                if (e.level == LogLevel::Info)    level = "INF";
                if (e.level == LogLevel::Warning) level = "WRN";
                s->broadcastLog(level, e.message, e.audience);
            });
        }
    }

    // ── Run the single game in a worker thread; main waits for it ─────────
    std::atomic<bool> game_failed{false};
    std::thread game_thread([&] {
        std::string last_line;
        try {
            auto result = engine.runGame(deck1, deck2, *seat1, *seat2, game_seed);
            std::cout << "\nGame over: winner="
                      << static_cast<int>(result.winner)
                      << " | reason=" << result.termination_reason
                      << " | turns=" << result.total_turns
                      << "\n";
            last_line = "[TRC] GAME_OVER: " + result.termination_reason;
        } catch (const std::exception& e) {
            std::cerr << "[engine] exception: " << e.what() << "\n";
            game_failed.store(true);
            last_line = std::string("[WRN] ENGINE_EXCEPTION: ") + e.what();
        }
        // Written on failure too: the replay up to the exception is what
        // one needs to debug it.
        if (replay) {
            try {
                replay->addTraceLine(last_line);
                replay->writeHtml();
                std::cout << "Replay saved to " << replay_path << "\n";
            } catch (const std::exception& e) {
                std::cerr << "Error: could not write replay: " << e.what() << "\n";
                game_failed.store(true);
            }
        }
    });

    if (game_thread.joinable()) game_thread.join();
    if (server) {
        // After an engine exception GameState may be half-updated: the
        // server then keeps showing its last good snapshot.
        if (!game_failed.load()) server->notifyGameFinished();
        server->stop();
    }
    if (log_to_stdout)
        std::cout << "Card tables built in this process: " << CardDB::tablesBuilt() << "\n";
    return game_failed.load() ? 1 : 0;
}
