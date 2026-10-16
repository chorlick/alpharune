/// balance: first-player edge and deck balance (testing phase item T5).
///
///   balance --agent random --source lists --games 5928 --start 0 --seed 1
///           --threads 3 --out lists_random_0.csv
///
/// One agent kind plays both seats (random | random-player | ismcts:N, honest
/// sampled worlds). Game g of a run:
///   --source lists:  every ordered pairing (i, j), i != j, of the deck lists
///                    in --deck-dir (sorted by path): pairing g % P, repeat
///                    g / P (P = n * (n - 1)); deck i is seat 1. With
///                    --draw-pairs the pairing is drawn by the game's seed
///                    instead (a short run then covers every list).
///   --source random: two RandomDeckBuilder decks, seeds mix(seed, 2g) and
///                    mix(seed, 2g + 1) (a seed the validator rejects is
///                    stepped until one passes).
/// The engine seed and the agents' seeds are functions of (--seed, g), so a
/// run split into chunks by --start / --games gives the games of one long
/// run. The engine tosses the coin for the first player (CR 116).
///
/// One CSV line per game: game, deck1, deck2, engine_seed, first (1/2),
/// winner (0 none, 1, 2), reason, turn, error.

#include "agents/mcts_agent.h"
#include "agents/random_agent.h"
#include "agents/random_player_agent.h"
#include "core/events.h"
#include "engine/game_engine.h"
#include "openspiel/riftbound_game.h"
#include "rules/deck_validator.h"
#include "rules/random_deck.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace riftbound;

namespace {

uint64_t mix(uint64_t x) {
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}

struct Spec {
    std::string kind;
    int sims = 0;
};

Spec parseSpec(const std::string& s) {
    Spec out;
    const auto colon = s.find(':');
    out.kind = s.substr(0, colon);
    if (colon != std::string::npos) out.sims = std::stoi(s.substr(colon + 1));
    if (out.kind == "ismcts" && out.sims <= 0) throw std::invalid_argument("ismcts needs :N");
    if (out.kind != "ismcts" && out.kind != "random" && out.kind != "random-player")
        throw std::invalid_argument("unknown agent '" + s + "'");
    return out;
}

std::unique_ptr<AgentInterface> build(const Spec& s, const std::string& d1, const std::string& d2,
                                      uint64_t engine_seed, uint64_t agent_seed,
                                      MatchFormat format) {
    if (s.kind == "random") return std::make_unique<RandomAgent>(agent_seed);
    if (s.kind == "random-player") return std::make_unique<RandomPlayerAgent>(agent_seed);
    return std::make_unique<IsMctsAgent>(d1, d2, "", engine_seed, agent_seed, s.sims,
                                         IsMctsAgent::WorldModel::Sampled, format);
}

int seatNum(PlayerId p) {
    return p == PlayerId::Player1 ? 1 : p == PlayerId::Player2 ? 2 : 0;
}

}  // namespace

int main(int argc, char** argv) {
    std::string agent_spec = "random", source = "lists", deck_dir = "decks", out_path, tmp_dir = ".";
    int games = 100, start = 0, threads = 3;
    bool draw_pairs = false;
    uint64_t seed = 1;
    MatchFormat format = MatchFormat::BestOf1;
    for (int i = 1; i < argc; ++i) {
        const std::string k = argv[i];
        auto next = [&]() -> std::string {
            if (i + 1 >= argc) throw std::invalid_argument(k + " needs a value");
            return argv[++i];
        };
        if (k == "--agent") agent_spec = next();
        else if (k == "--source") source = next();
        else if (k == "--games") games = std::stoi(next());
        else if (k == "--start") start = std::stoi(next());
        else if (k == "--seed") seed = std::stoull(next());
        else if (k == "--threads") threads = std::stoi(next());
        else if (k == "--deck-dir") deck_dir = next();
        else if (k == "--out") out_path = next();
        else if (k == "--tmp-dir") tmp_dir = next();
        else if (k == "--draw-pairs") draw_pairs = true;
        else if (k == "--game-mode") format = matchFormatFromString(next());
        else { std::cerr << "unknown flag " << k << "\n"; return 2; }
    }
    if (out_path.empty()) { std::cerr << "--out is required\n"; return 2; }
    if (source != "lists" && source != "random") { std::cerr << "--source lists|random\n"; return 2; }
    const Spec spec = parseSpec(agent_spec);

    const auto& cards = openspiel::sharedCards();
    std::vector<std::string> paths;
    std::vector<DeckSubmission> decks;
    if (source == "lists") {
        for (const auto& e : std::filesystem::directory_iterator(deck_dir))
            if (e.path().extension() == ".txt") paths.push_back(e.path().string());
        std::sort(paths.begin(), paths.end());
        for (const auto& p : paths) decks.push_back(DeckValidator::loadFromDeckList(p, cards.db));
        if (decks.size() < 2) { std::cerr << "need two deck lists\n"; return 2; }
    }
    const RandomDeckBuilder builder(cards.db);
    const DeckValidator validator(cards.db);
    const uint64_t pairings = decks.size() * (decks.size() > 0 ? decks.size() - 1 : 0);

    std::ofstream out(out_path);
    out << "game,deck1,deck2,engine_seed,first,winner,reason,turn,error\n";
    std::mutex m;
    std::atomic<int> next_game{start};
    const int end = start + games;

    auto worker = [&] {
        for (int g = next_game++; g < end; g = next_game++) {
            const uint64_t gs = mix(seed * 1000003u + static_cast<uint64_t>(g));
            const uint64_t engine_seed = gs & 0x7FFFFFFFu;
            DeckSubmission d1, d2;
            std::string n1, n2, p1, p2;
            if (source == "lists") {
                const uint64_t k = draw_pairs ? mix(gs) % pairings
                                              : static_cast<uint64_t>(g) % pairings;
                const size_t i = k / (decks.size() - 1);
                size_t j = k % (decks.size() - 1);
                if (j >= i) ++j;
                d1 = decks[i]; d2 = decks[j];
                p1 = paths[i]; p2 = paths[j];
                n1 = std::filesystem::path(p1).stem().string();
                n2 = std::filesystem::path(p2).stem().string();
            } else {
                auto pick = [&](uint64_t s, DeckSubmission& d, std::string& name) {
                    for (;; ++s) {
                        d = builder.build(s);
                        if (validator.validate(d).is_legal) { name = std::to_string(s); return; }
                    }
                };
                pick(mix(seed * 7919u + 2u * static_cast<uint64_t>(g)), d1, n1);
                pick(mix(seed * 7919u + 2u * static_cast<uint64_t>(g) + 1u), d2, n2);
                if (spec.kind == "ismcts") {
                    p1 = tmp_dir + "/deck_" + n1 + ".txt";
                    p2 = tmp_dir + "/deck_" + n2 + ".txt";
                    std::ofstream(p1) << builder.toDeckList(d1);
                    std::ofstream(p2) << builder.toDeckList(d2);
                    // The search agent rebuilds the game from the files: the
                    // live game must be played from the same lists (a list
                    // read back can order its cards differently, and then the
                    // search's mirror is another game and it plays at random).
                    d1 = DeckValidator::loadFromDeckList(p1, cards.db);
                    d2 = DeckValidator::loadFromDeckList(p2, cards.db);
                }
            }
            std::string error;
            GameResult r;
            int first = 0, turn = 0;
            try {
                auto a1 = build(spec, p1, p2, engine_seed, mix(gs + 1), format);
                auto a2 = build(spec, p1, p2, engine_seed, mix(gs + 2), format);
                EventBus bus;
                GameEngine engine(cards.db, bus, cards.registry);
                engine.setMatchFormat(format);
                r = engine.runGame(d1, d2, *a1, *a2, engine_seed);
                if (r.termination_reason == "engine_error") error = "engine_error";
                first = seatNum(engine.state().turn.starting_player);
                turn = engine.state().turn.turn_number;
            } catch (const std::exception& e) {
                error = e.what();
            }
            for (auto& c : error) if (c == ',' || c == '\n') c = ' ';
            std::string reason = r.termination_reason;
            for (auto& c : reason) if (c == ',' || c == '\n') c = ' ';
            std::lock_guard<std::mutex> lock(m);
            out << g << ',' << n1 << ',' << n2 << ',' << engine_seed << ',' << first << ','
                << seatNum(r.winner) << ',' << reason << ',' << turn << ','
                << error.substr(0, 200) << '\n';
        }
    };
    std::vector<std::thread> pool;
    for (int t = 0; t < std::max(1, threads); ++t) pool.emplace_back(worker);
    for (auto& t : pool) t.join();
    return 0;
}
