// Clone-cost breakdown. Walks the same checkpoint as the existing
// microbench, then runs two timed loops:
//
//   (A) Full Clone()         — measures the cost of the public API
//   (B) GameState alone      — raw deep-copy of just engineState()
//
// (What a clone that IS advanced pays on top — building its engine — is
// measured by clone_advance_microbench.)
//
// Note on (A): repeated Clone() calls on a state that has not been advanced
// share one immutable snapshot, so only the first call pays the GameState
// copy and the rest are a pointer copy. The median therefore shows the
// shared-snapshot cost and the max shows the copy.
//
// Gives an apples-to-apples view of whether Clone() is already pinned
// at the GameState deep-copy floor (in which case "memcpy Clone" — i.e.
// std::make_unique<RiftboundState>(*this) via copy-ctor — buys nothing
// without GameState-level optimizations) or whether there's wrapper
// overhead worth shaving.
//
// Build:  cmake --build build --target riftbound_clone_breakdown
// Run:    ./build/src/openspiel/riftbound_clone_breakdown
//
// Same env vars as clone_microbench: RIFTBOUND_DECK1/2,
// RIFTBOUND_CHECKPOINT, RIFTBOUND_NUM_CLONES, RIFTBOUND_SEED.

#include "open_spiel/spiel.h"
#include "core/game_state.h"
#include "riftbound_state.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <new>
#include <unordered_map>
#include <cstdlib>
#include <iostream>
#include <numeric>
#include <random>
#include <string>
#include <vector>

// Every heap allocation of the process, counted (the (C) table below).
// The replacements are kept out of line: inlined into a caller of this file,
// GCC pairs the malloc of one with the free of the other and reports a
// mismatched allocation (-Wmismatched-new-delete, testing phase R15) — a
// false report, since every operator new here is malloc and every operator
// delete free.
static uint64_t g_allocs = 0;
[[gnu::noinline]] void* operator new(std::size_t n) {
    ++g_allocs;
    if (void* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
[[gnu::noinline]] void operator delete(void* p) noexcept { std::free(p); }
[[gnu::noinline]] void operator delete(void* p, std::size_t) noexcept { std::free(p); }

namespace {

/// Keeps a value alive as far as the optimiser is concerned.
template <class T>
inline void keep(T* p) { asm volatile("" : : "g"(p) : "memory"); }

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

struct Stats {
    double min = 0, median = 0, mean = 0, p95 = 0, max = 0;
};

Stats summarize(std::vector<double> us) {
    std::sort(us.begin(), us.end());
    Stats s;
    s.min    = us.front();
    s.median = us[us.size() / 2];
    s.p95    = us[static_cast<size_t>(us.size() * 0.95)];
    s.max    = us.back();
    s.mean   = std::accumulate(us.begin(), us.end(), 0.0) / us.size();
    return s;
}

void print(const std::string& label, const Stats& s) {
    std::cout << "  " << label << "  min=" << s.min
              << " median=" << s.median
              << " mean=" << s.mean
              << " p95=" << s.p95
              << " max=" << s.max << " us\n";
}

} // namespace

int main() {
    const std::string deck1    = getenvOr("RIFTBOUND_DECK1",    "decks/yi_rebuild.txt");
    const std::string deck2    = getenvOr("RIFTBOUND_DECK2",    "decks/yi_rebuild.txt");
    const int checkpoint       = std::max(1, getenvInt("RIFTBOUND_CHECKPOINT", 100));
    const int num_clones       = std::max(1, getenvInt("RIFTBOUND_NUM_CLONES", 200));
    const int seed             = getenvInt("RIFTBOUND_SEED", 42);

    const std::string game_str = "riftbound(deck1=" + deck1 +
                                  ",deck2=" + deck2 +
                                  ",seed=" + std::to_string(seed) + ")";

    std::cout << "Loading: " << game_str << "\n";
    auto game = ::open_spiel::LoadGame(game_str);

    auto state = game->NewInitialState();
    std::mt19937_64 rng(static_cast<uint64_t>(seed));
    int walked = 0;
    while (!state->IsTerminal() && walked < checkpoint) {
        auto legal = state->LegalActions();
        if (legal.empty()) break;
        std::uniform_int_distribution<size_t> d(0, legal.size() - 1);
        state->ApplyAction(legal[d(rng)]);
        ++walked;
    }
    if (state->IsTerminal()) {
        std::cerr << "ERROR: game ended pre-checkpoint at decision " << walked << "\n";
        return 1;
    }
    std::cout << "Walked " << walked << " decisions to checkpoint.\n\n";

    auto* rb_state =
        static_cast<::riftbound::openspiel::RiftboundState*>(state.get());

    // (A) Full Clone()
    std::vector<double> us_clone(num_clones);
    for (int i = 0; i < num_clones; ++i) {
        auto t0 = std::chrono::steady_clock::now();
        auto clone = state->Clone();
        auto t1 = std::chrono::steady_clock::now();
        us_clone[i] = std::chrono::duration<double, std::micro>(t1 - t0).count();
        // Touch so the optimizer can't elide.
        if (clone->CurrentPlayer() == ::open_spiel::kInvalidPlayer) {
            std::cerr << "unreachable\n";
        }
    }

    // (B) Raw GameState deep-copy in isolation
    std::vector<double> us_gs(num_clones);
    for (int i = 0; i < num_clones; ++i) {
        auto t0 = std::chrono::steady_clock::now();
        ::riftbound::GameState gs_copy = rb_state->engineState();
        auto t1 = std::chrono::steady_clock::now();
        us_gs[i] = std::chrono::duration<double, std::micro>(t1 - t0).count();
        if (gs_copy.turn.turn_number < 0) {
            std::cerr << "unreachable\n";  // touch
        }
    }

    auto s_clone = summarize(us_clone);
    auto s_gs    = summarize(us_gs);

    std::cout << "=== " << num_clones << " calls at decision " << walked
              << " ===\n";
    print("(A) Full state->Clone()  ", s_clone);
    print("(B) GameState copy alone ", s_gs);
    std::cout << "\n";
    double overhead = s_clone.median - s_gs.median;
    double pct = (overhead / s_clone.median) * 100.0;
    std::cout << "Wrapper overhead (Clone - GameState copy): "
              << overhead << " us median (" << pct << "% of Clone)\n";


    // (C) What one GameState copy is made of (plans/09 item 9.KK1). Each row
    // copies ONE member of every object of the state into a fresh value and
    // destroys it, `reps` times in a row, so the allocator is in a steady
    // state (a single timed copy bills the previous teardown's fastbin
    // consolidation to whoever allocates next: plans/reports/perf-search.md
    // section 2). Heap allocations are counted, not inferred.
    {
        const ::riftbound::GameState& gs = rb_state->engineState();
        const int reps = std::max(1, getenvInt("RIFTBOUND_FIELD_REPS", 2000));
        std::cout << "\n=== (C) per-member copy cost, " << gs.objects.size()
                  << " objects, sizeof(GameObject)=" << sizeof(::riftbound::GameObject)
                  << ", " << reps << " reps, median of 9 ===\n";
        auto row = [&](const char* label, auto&& body) {
            std::vector<double> ns(9);
            uint64_t allocs = 0;
            for (auto& sample : ns) {
                const uint64_t a0 = g_allocs;
                auto t0 = std::chrono::steady_clock::now();
                for (int r = 0; r < reps; ++r) body();
                auto t1 = std::chrono::steady_clock::now();
                sample = std::chrono::duration<double, std::nano>(t1 - t0).count() / reps;
                allocs = (g_allocs - a0) / reps;
            }
            std::sort(ns.begin(), ns.end());
            std::printf("  %-34s %9.0f ns   %5llu allocations\n", label, ns[4],
                        static_cast<unsigned long long>(allocs));
        };
        row("whole GameState", [&] { ::riftbound::GameState c = gs; keep(&c); });
        row("  objects (the table)", [&] { auto c = gs.objects; keep(&c); });
        row("  players[2]", [&] { auto a = gs.players[0]; auto b = gs.players[1]; keep(&a); keep(&b); });
        row("  chain", [&] { auto c = gs.chain; keep(&c); });
        row("  battlefields", [&] { auto c = gs.battlefields; keep(&c); });
        row("  action_history", [&] { auto c = gs.action_history; keep(&c); });
        row("  delayed_abilities", [&] { auto c = gs.delayed_abilities; keep(&c); });
        row("  knowledge", [&] { auto c = gs.knowledge; keep(&c); });
        row("  turn", [&] { auto c = gs.turn; keep(&c); });
#define RIFT_FIELD_ROW(member) \
        row("    object." #member, [&] { \
            for (const auto& [id, o] : gs.objects) { auto c = o.member; keep(&c); } })
        RIFT_FIELD_ROW(name);
        RIFT_FIELD_ROW(tags);
        RIFT_FIELD_ROW(domains);
        RIFT_FIELD_ROW(card_counters);
        RIFT_FIELD_ROW(string_state);
        RIFT_FIELD_ROW(last_known);
        RIFT_FIELD_ROW(attachments);
        RIFT_FIELD_ROW(tracked_objects);
        RIFT_FIELD_ROW(pending_death_replacements);
        RIFT_FIELD_ROW(granted_abilities);
        RIFT_FIELD_ROW(temp_granted_abilities);
        RIFT_FIELD_ROW(aura_effects);
#undef RIFT_FIELD_ROW
        row("    one map node per object (empty)", [&] {
            std::unordered_map<uint32_t, char> m;
            m.reserve(gs.objects.size());
            for (const auto& [id, o] : gs.objects) m.emplace(id.raw(), 0);
            keep(&m); });
        size_t long_names = 0, with_tags = 0, with_domains = 0, with_counters = 0,
               with_strings = 0, with_attach = 0, with_auras = 0, tokens = 0;
        for (const auto& [id, o] : gs.objects) {
            long_names += o.name.size() > 15;
            with_tags += !o.tags.empty();
            with_domains += !o.domains.empty();
            with_counters += !o.card_counters.empty();
            with_strings += !o.string_state.empty();
            with_attach += !o.attachments.empty();
            with_auras += !o.aura_effects.empty();
            tokens += o.isToken();
        }
        std::cout << "  objects with: a name over 15 chars " << long_names
                  << ", tags " << with_tags << ", domains " << with_domains
                  << ", counters " << with_counters << ", string_state " << with_strings
                  << ", attachments " << with_attach << ", aura effects " << with_auras
                  << "; tokens " << tokens << "\n";
    }

    return 0;
}
