// Search-path microbench. Times the per-node calls MCTS / ISMCTS make on a
// RiftboundState besides Clone() and ApplyAction (those are
// clone_microbench / clone_advance_microbench):
//
//   LoadGame                 — what every MctsAgent / IsMctsAgent pays once
//   CurrentPlayer / LegalActions / legalIntents
//                            — the pending-decision accessors, on a live
//                              state and on a snapshot-only clone
//   InfoSetId / ObservationString / ObservationTensor
//                            — the observation extraction
//
// It walks one random game and measures at every decision from the
// checkpoint on, so the numbers are means over real mid-game positions. It
// also prints a checksum of every value the observation calls returned:
// two builds that print the same checksum computed the same observations.
//
// Build:  cmake --build build --target riftbound_search_microbench
// Run:    ./build/src/openspiel/riftbound_search_microbench
//
// Environment variables:
//   RIFTBOUND_DECK1, RIFTBOUND_DECK2 : deck paths
//   RIFTBOUND_CHECKPOINT             : decisions before the first measurement (default 20)
//   RIFTBOUND_NUM_STATES             : decisions to measure at (default 150)
//   RIFTBOUND_REPS                   : calls per decision per operation (default 20)
//   RIFTBOUND_SEED                   : game seed (default 42)

#include "open_spiel/spiel.h"

#include "ml/cfr_util.h"
#include "ml/feature_extractor.h"
#include "riftbound_game.h"
#include "riftbound_state.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace {

std::string getenvOr(const char* name, std::string fallback) {
    if (const char* v = std::getenv(name)) return v;
    return fallback;
}

int getenvInt(const char* name, int fallback) {
    if (const char* v = std::getenv(name)) {
        try { return std::stoi(v); } catch (...) {}
    }
    return fallback;
}

struct Timer {
    const char* label;
    double total_ns = 0.0;
    long calls = 0;
};

using Clock = std::chrono::steady_clock;

template <typename Fn>
void timeIt(Timer& t, int reps, Fn&& fn) {
    const auto t0 = Clock::now();
    for (int i = 0; i < reps; ++i) fn();
    const auto t1 = Clock::now();
    t.total_ns += std::chrono::duration<double, std::nano>(t1 - t0).count();
    t.calls += reps;
}

void report(const Timer& t) {
    std::cout << "  " << std::left << std::setw(34) << t.label << std::right
              << std::setw(12) << std::fixed << std::setprecision(1)
              << (t.calls ? t.total_ns / static_cast<double>(t.calls) : 0.0)
              << " ns/call  (" << t.calls << " calls)\n";
}

uint64_t fnv(uint64_t h, const void* data, size_t n) {
    const auto* p = static_cast<const unsigned char*>(data);
    for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 1099511628211ULL; }
    return h;
}

} // namespace

int main() {
    const std::string deck1 = getenvOr("RIFTBOUND_DECK1", "decks/fiora.txt");
    const std::string deck2 = getenvOr("RIFTBOUND_DECK2", "decks/yi_rebuild.txt");
    const int checkpoint    = std::max(0, getenvInt("RIFTBOUND_CHECKPOINT", 20));
    const int num_states    = std::max(1, getenvInt("RIFTBOUND_NUM_STATES", 150));
    const int reps          = std::max(1, getenvInt("RIFTBOUND_REPS", 20));
    const int seed          = getenvInt("RIFTBOUND_SEED", 42);

    const std::string game_str = "riftbound(deck1=" + deck1 +
                                  ",deck2=" + deck2 +
                                  ",seed=" + std::to_string(seed) + ")";
    std::cout << "Loading: " << game_str << "\n";

    Timer t_load{"LoadGame"};
    std::shared_ptr<const ::open_spiel::Game> game;
    timeIt(t_load, 20, [&] { game = ::open_spiel::LoadGame(game_str); });

    Timer t_cur{"CurrentPlayer (live)"}, t_legal{"LegalActions (live)"},
          t_intents{"legalIntents (live)"},
          t_ccur{"CurrentPlayer (clone)"}, t_clegal{"LegalActions (clone)"},
          t_cintents{"legalIntents (clone)"},
          t_info{"InfoSetId"}, t_obs_str{"ObservationString"},
          t_obs{"ObservationTensor"},
          t_extract{"  ml::extractStateFeatures"},
          t_hash{"  ml::computeInfoSetId"};
    const auto& card_db =
        static_cast<const ::riftbound::openspiel::RiftboundGame&>(*game).cardDb();

    auto state = game->NewInitialState();
    std::mt19937_64 rng(static_cast<uint64_t>(seed));
    std::vector<float> tensor(::riftbound::ml::kStateFeatureDim);
    uint64_t checksum = 14695981039346656037ULL;
    long sink = 0;
    int walked = 0, measured = 0;

    while (!state->IsTerminal() && measured < num_states) {
        const auto legal = state->LegalActions();
        if (legal.empty()) break;
        if (walked >= checkpoint) {
            const auto& rb =
                static_cast<const ::riftbound::openspiel::RiftboundState&>(*state);
            const auto clone_owner = state->Clone();
            const auto& clone =
                static_cast<const ::riftbound::openspiel::RiftboundState&>(*clone_owner);

            timeIt(t_cur,     reps, [&] { sink += rb.CurrentPlayer(); });
            timeIt(t_legal,   reps, [&] { sink += static_cast<long>(rb.LegalActions().size()); });
            timeIt(t_intents, reps, [&] { sink += static_cast<long>(rb.legalIntents().size()); });
            timeIt(t_ccur,     reps, [&] { sink += clone.CurrentPlayer(); });
            timeIt(t_clegal,   reps, [&] { sink += static_cast<long>(clone.LegalActions().size()); });
            timeIt(t_cintents, reps, [&] { sink += static_cast<long>(clone.legalIntents().size()); });

            for (int p = 0; p < 2; ++p) {
                uint64_t id = 0;
                std::string s;
                timeIt(t_info,    reps, [&] { id = rb.InfoSetId(p); });
                timeIt(t_obs_str, reps, [&] { s = rb.ObservationString(p); });
                timeIt(t_obs,     reps, [&] { rb.ObservationTensor(p, absl::Span<float>(tensor)); });
                // The two halves of InfoSetId, separately.
                const auto seat = p == 0 ? ::riftbound::PlayerId::Player1
                                         : ::riftbound::PlayerId::Player2;
                std::vector<float> feats;
                timeIt(t_extract, reps, [&] {
                    feats = ::riftbound::ml::extractStateFeatures(
                        rb.engineState(), seat, card_db);
                });
                timeIt(t_hash, reps, [&] {
                    sink += static_cast<long>(
                        ::riftbound::ml::computeInfoSetId(feats, p) & 1);
                });
                checksum = fnv(checksum, &id, sizeof(id));
                checksum = fnv(checksum, s.data(), s.size());
                checksum = fnv(checksum, tensor.data(),
                               tensor.size() * sizeof(float));
                // A clone must report what its original does.
                if (clone.InfoSetId(p) != id) {
                    std::cerr << "ERROR: clone InfoSetId differs at decision "
                              << walked << "\n";
                    return 1;
                }
            }
            ++measured;
        }
        std::uniform_int_distribution<size_t> d(0, legal.size() - 1);
        state->ApplyAction(legal[d(rng)]);
        ++walked;
    }

    if (measured == 0) {
        std::cerr << "ERROR: game ended before the checkpoint (decision "
                  << walked << ")\n";
        return 1;
    }

    std::cout << "\n=== mean cost per call over " << measured
              << " decisions (from decision " << checkpoint << ") ===\n";
    for (const Timer* t : {&t_load, &t_cur, &t_legal, &t_intents, &t_ccur,
                           &t_clegal, &t_cintents, &t_info, &t_obs_str, &t_obs,
                           &t_extract, &t_hash}) {
        report(*t);
    }
    std::cout << "observation checksum: " << std::hex << checksum << std::dec
              << "  (sink " << sink << ")\n";
    return 0;
}
