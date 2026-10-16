/// search_strength: how strong a search agent is, against another agent, over
/// the deck lists of a directory (batch 5 item T3: the trainer's choice of a
/// policy teacher and its budget).
///
///   search_strength --a ismcts:100 --b random --games 200 --seed 1 --threads 3
///
/// Agents: random | random-player | ismcts:N (honest: sampled worlds) |
/// ismcts-perfect:N (sees hidden cards) | mcts:N (perfect-information MCTS).
///
/// Game g: a deck pair drawn from the deck lists (two different lists, by the
/// game's seed), agent A in seat 1 on even games and seat 2 on odd ones; the
/// engine tosses the coin for the first player. A game is a function of
/// (--seed, g). A draw counts half a win; an engine error is reported and left
/// out of the rate. The interval is Wilson's at 95%. Seconds per decision are
/// over decisions with more than one option (the ones a search searches).

#include "agents/mcts_agent.h"
#include "agents/random_agent.h"
#include "agents/random_player_agent.h"
#include "core/events.h"
#include "engine/game_engine.h"
#include "openspiel/riftbound_game.h"
#include "rules/deck_validator.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace riftbound;

namespace {

struct Spec {
    std::string kind;
    int sims = 0;
};

Spec parseSpec(const std::string& s) {
    Spec out;
    const auto colon = s.find(':');
    out.kind = s.substr(0, colon);
    if (colon != std::string::npos) out.sims = std::stoi(s.substr(colon + 1));
    const bool search = out.kind == "ismcts" || out.kind == "ismcts-perfect" || out.kind == "mcts";
    if (!search && out.kind != "random" && out.kind != "random-player")
        throw std::invalid_argument("unknown agent '" + s + "'");
    if (search && out.sims <= 0) throw std::invalid_argument("'" + s + "' needs :N simulations");
    return out;
}

/// Times an agent's decisions.
class Timed : public AgentInterface {
public:
    explicit Timed(std::unique_ptr<AgentInterface> inner) : inner_(std::move(inner)) {}
    Intent selectAction(const GameState& state, const std::vector<Intent>& legal) override {
        const auto t0 = std::chrono::steady_clock::now();
        Intent out = inner_->selectAction(state, legal);
        if (legal.size() > 1) {
            seconds += std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
            ++decisions;
        }
        return out;
    }
    double seconds = 0;
    long decisions = 0;
private:
    std::unique_ptr<AgentInterface> inner_;
};

std::unique_ptr<AgentInterface> build(const Spec& s, const std::string& d1, const std::string& d2,
                                      uint64_t engine_seed, uint64_t agent_seed,
                                      MatchFormat format) {
    if (s.kind == "random") return std::make_unique<RandomAgent>(agent_seed);
    if (s.kind == "random-player") return std::make_unique<RandomPlayerAgent>(agent_seed);
    if (s.kind == "mcts")
        return std::make_unique<MctsAgent>(d1, d2, "", engine_seed, agent_seed, s.sims, format);
    return std::make_unique<IsMctsAgent>(
        d1, d2, "", engine_seed, agent_seed, s.sims,
        s.kind == "ismcts" ? IsMctsAgent::WorldModel::Sampled : IsMctsAgent::WorldModel::Perfect,
        format);
}

uint64_t mix(uint64_t x) {
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}

struct Wilson { double lo, hi; };
Wilson wilson(double wins, double n) {
    if (n <= 0) return {0, 1};
    const double z = 1.96, p = wins / n;
    const double d = 1 + z * z / n;
    const double c = (p + z * z / (2 * n)) / d;
    const double h = z * std::sqrt(p * (1 - p) / n + z * z / (4 * n * n)) / d;
    return {std::max(0.0, c - h), std::min(1.0, c + h)};
}

}  // namespace

int main(int argc, char** argv) {
    std::string a_spec = "ismcts:50", b_spec = "random", deck_dir = "decks";
    int games = 100, threads = 3;
    uint64_t seed = 1;
    MatchFormat format = MatchFormat::BestOf1;
    for (int i = 1; i < argc; ++i) {
        const std::string k = argv[i];
        auto next = [&]() -> std::string {
            if (i + 1 >= argc) throw std::invalid_argument(k + " needs a value");
            return argv[++i];
        };
        if (k == "--a") a_spec = next();
        else if (k == "--b") b_spec = next();
        else if (k == "--games") games = std::stoi(next());
        else if (k == "--seed") seed = std::stoull(next());
        else if (k == "--threads") threads = std::stoi(next());
        else if (k == "--deck-dir") deck_dir = next();
        else if (k == "--game-mode") format = matchFormatFromString(next());
        else { std::cerr << "unknown flag " << k << "\n"; return 2; }
    }
    const Spec a = parseSpec(a_spec), b = parseSpec(b_spec);

    const auto& cards = openspiel::sharedCards();
    std::vector<std::string> paths;
    for (const auto& e : std::filesystem::directory_iterator(deck_dir))
        if (e.path().extension() == ".txt") paths.push_back(e.path().string());
    std::sort(paths.begin(), paths.end());
    std::vector<DeckSubmission> decks;
    for (const auto& p : paths) decks.push_back(DeckValidator::loadFromDeckList(p, cards.db));
    if (decks.size() < 2) { std::cerr << "need two deck lists\n"; return 2; }

    std::mutex m;
    double a_score = 0, a_seat1 = 0, a_first = 0;
    int done = 0, errors = 0, draws = 0, n_seat1 = 0, n_first = 0;
    double a_sec = 0, b_sec = 0;
    long a_dec = 0, b_dec = 0;
    std::vector<std::string> error_lines;
    std::atomic<int> next_game{0};
    const auto t0 = std::chrono::steady_clock::now();

    auto worker = [&] {
        for (int g = next_game++; g < games; g = next_game++) {
            const uint64_t gs = mix(seed * 1000003u + static_cast<uint64_t>(g));
            const size_t i = gs % decks.size();
            size_t j = mix(gs) % (decks.size() - 1);
            if (j >= i) ++j;
            const bool a_first_seat = g % 2 == 0;
            const size_t d1 = a_first_seat ? i : j, d2 = a_first_seat ? j : i;
            const uint64_t engine_seed = gs & 0x7FFFFFFFu;
            // Seat order: deck d1 is seat 1.
            std::unique_ptr<Timed> agent_a, agent_b;
            agent_a = std::make_unique<Timed>(
                build(a, paths[d1], paths[d2], engine_seed, mix(gs + 1), format));
            agent_b = std::make_unique<Timed>(
                build(b, paths[d1], paths[d2], engine_seed, mix(gs + 2), format));
            EventBus bus;
            GameEngine engine(cards.db, bus, cards.registry);
            engine.setMatchFormat(format);
            GameResult r;
            std::string error;
            try {
                r = a_first_seat
                    ? engine.runGame(decks[d1], decks[d2], *agent_a, *agent_b, engine_seed)
                    : engine.runGame(decks[d1], decks[d2], *agent_b, *agent_a, engine_seed);
                if (r.termination_reason == "engine_error") error = "engine_error";
            } catch (const std::exception& e) {
                error = e.what();
            }
            const PlayerId a_seat = a_first_seat ? PlayerId::Player1 : PlayerId::Player2;
            std::lock_guard<std::mutex> lock(m);
            a_sec += agent_a->seconds; a_dec += agent_a->decisions;
            b_sec += agent_b->seconds; b_dec += agent_b->decisions;
            if (!error.empty()) {
                ++errors;
                if (error_lines.size() < 5)
                    error_lines.push_back("game " + std::to_string(g) + " (" + paths[d1] + " v " +
                                          paths[d2] + ", seed " + std::to_string(engine_seed) +
                                          "): " + error.substr(0, 300));
                continue;
            }
            ++done;
            double s = r.winner == a_seat ? 1.0 : r.winner == PlayerId::None ? 0.5 : 0.0;
            if (r.winner == PlayerId::None) ++draws;
            a_score += s;
            if (a_first_seat) { a_seat1 += s; ++n_seat1; }
            if (engine.state().turn.starting_player == a_seat) { a_first += s; ++n_first; }
        }
    };
    std::vector<std::thread> pool;
    for (int t = 0; t < std::max(1, threads); ++t) pool.emplace_back(worker);
    for (auto& t : pool) t.join();
    const double wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();

    const Wilson w = wilson(a_score, done);
    std::printf("A=%s B=%s games=%d decided=%d errors=%d draws=%d | A score %.1f/%d = %.3f "
                "[%.3f, %.3f] | A as seat 1 %.3f (n=%d), A going first %.3f (n=%d), "
                "A going second %.3f (n=%d) | s/decision A %.4f B %.4f | wall %.0fs\n",
                a_spec.c_str(), b_spec.c_str(), games, done, errors, draws, a_score, done,
                done ? a_score / done : 0.0, w.lo, w.hi,
                n_seat1 ? a_seat1 / n_seat1 : 0.0, n_seat1,
                n_first ? a_first / n_first : 0.0, n_first,
                done - n_first ? (a_score - a_first) / (done - n_first) : 0.0, done - n_first,
                a_dec ? a_sec / a_dec : 0.0, b_dec ? b_sec / b_dec : 0.0, wall);
    for (const auto& l : error_lines) std::printf("  error: %s\n", l.c_str());
    return 0;
}
