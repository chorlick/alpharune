/// Regression tests for the performance-review fixes in the search path
/// (plans/06-performance.md items 6.4, 6.7 and 6.9): the step API hands out
/// its pending decision by reference, an engine abandoned mid-game is torn
/// down instead of being played to the end, and the observation features and
/// their hash are computed without the repeated scans — to the same values.
///
/// The OpenSpiel-side tests for 6.8 / 6.9 are in
/// src/openspiel/openspiel_tests.cpp (they need the Game).

#include <gtest/gtest.h>

#include "cards/card_registry.h"
#include "core/card_db.h"
#include "core/events.h"
#include "engine/game_engine.h"
#include "engine/step_driver.h"
#include "ml/cfr_util.h"
#include "ml/feature_extractor.h"
#include "rules/deck_validator.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace test_review_fix_perf_search {

using namespace riftbound;

std::string deckFile(const char* name) {
    const char* root = std::getenv("RIFTBOUND_ROOT");
    return std::string(root ? root : "..") + "/decks/" + name;
}

class PerfSearchTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        registry_ = new CardRegistry();
        registry_->loadAll();
        db_ = new CardDB();
        db_->buildFromClasses(*registry_);
        deck1_ = new DeckSubmission(
            DeckValidator::loadFromDeckList(deckFile("yi_rebuild.txt"), *db_));
        deck2_ = new DeckSubmission(
            DeckValidator::loadFromDeckList(deckFile("fiora.txt"), *db_));
    }
    static void TearDownTestSuite() {
        delete deck2_; delete deck1_; delete db_; delete registry_;
    }

    /// One step-machine game: its own bus and engine. `logs` counts every
    /// line the engine logs, `decisions` every decision it has been given
    /// an answer to — both only move while the engine is running.
    struct Game {
        EventBus bus;
        long logs = 0;
        long decisions = 0;
        boost::signals2::scoped_connection log_conn;
        std::unique_ptr<GameEngine> engine;
        Game() {
            log_conn = bus.on_log.connect([this](const LogEvent&) { ++logs; });
            engine = std::make_unique<GameEngine>(*db_, bus, *registry_);
            engine->on_decision = [this](const GameState&,
                                         const std::vector<Intent>&,
                                         const Intent&) { ++decisions; };
        }
    };

    static std::unique_ptr<Game> begin(uint64_t seed) {
        auto g = std::make_unique<Game>();
        g->engine->beginGame(*deck1_, *deck2_, seed);
        return g;
    }

    /// Play `n` random decisions (or to the end of the game).
    static void advance(Game& g, std::mt19937_64& rng, int n) {
        for (int i = 0; i < n && !g.engine->isStepDone(); ++i) {
            const auto& legal = g.engine->currentStep().legal;
            std::uniform_int_distribution<size_t> d(0, legal.size() - 1);
            g.engine->applyChoice(static_cast<int>(d(rng)));
        }
    }

    static CardRegistry* registry_;
    static CardDB* db_;
    static DeckSubmission* deck1_;
    static DeckSubmission* deck2_;
};

CardRegistry* PerfSearchTest::registry_ = nullptr;
CardDB* PerfSearchTest::db_ = nullptr;
DeckSubmission* PerfSearchTest::deck1_ = nullptr;
DeckSubmission* PerfSearchTest::deck2_ = nullptr;

// ─── 6.7: abandoning a step-machine engine ──────────────────────────────────

/// Sets a flag when the fiber frame that owns it is unwound.
struct UnwindFlag {
    bool& flag;
    explicit UnwindFlag(bool& f) : flag(f) {}
    ~UnwindFlag() { flag = true; }
};

// 6.7 — destroying a driver whose fiber is parked at a decision must not
// feed it answers until its game function returns (that is "playing the
// abandoned game out"); it unwinds the fiber, running the destructors of
// what is on its stack.
TEST(StepDriverTeardown, DestroyingAParkedDriverUnwindsInsteadOfDraining) {
    GameState state;
    const std::vector<Intent> legal{Intent::concede(PlayerId::Player1),
                                    Intent::concede(PlayerId::Player1)};
    int asked = 0;
    bool unwound = false;
    bool returned = false;
    {
        StepDriver driver;
        driver.runInFiber([&] {
            UnwindFlag on_stack(unwound);
            for (int i = 0; i < 1000; ++i) {
                ++asked;
                driver.seat(PlayerId::Player1).selectAction(state, legal);
            }
            returned = true;
            driver.markDone();
        });
        ASSERT_FALSE(driver.isDone());
        ASSERT_EQ(asked, 1);
        driver.provideChoice(1);
        ASSERT_EQ(asked, 2);
        EXPECT_FALSE(unwound);
    }
    EXPECT_EQ(asked, 2) << "the destructor went on answering decisions";
    EXPECT_FALSE(returned) << "the abandoned game function ran to its end";
    EXPECT_TRUE(unwound) << "the fiber's stack was not unwound (leak)";
}

// 6.7 — the other ways a driver can die: never started, finished, failed.
TEST(StepDriverTeardown, DestroyingAnIdleFinishedOrFailedDriverIsHarmless) {
    GameState state;
    const std::vector<Intent> legal{Intent::concede(PlayerId::Player1)};
    { StepDriver never_started; }
    {
        StepDriver finished;
        bool unwound = false;
        finished.runInFiber([&] {
            UnwindFlag on_stack(unwound);
            finished.selectAction(state, legal);
            finished.markDone();
        });
        finished.provideChoice(0);
        EXPECT_TRUE(finished.isDone());
        EXPECT_TRUE(unwound);
        EXPECT_FALSE(finished.failure());
    }
    {
        StepDriver failed;
        failed.runInFiber([&] {
            failed.selectAction(state, legal);
            throw std::runtime_error("boom");
        });
        failed.provideChoice(0);
        EXPECT_TRUE(failed.isDone());
        ASSERT_TRUE(failed.failure());
        EXPECT_THROW(std::rethrow_exception(failed.failure()),
                     std::runtime_error);
    }
}

// 6.7 — the same through the real engine: dropping an engine in the middle
// of a game makes no further decisions and logs nothing more. (It used to
// run the rest of the game — every remaining turn — inside the destructor.)
TEST_F(PerfSearchTest, DroppingAnEngineMidGameDoesNotPlayTheGameOut) {
    auto g = begin(20261010);
    std::mt19937_64 rng(5);
    advance(*g, rng, 60);
    ASSERT_FALSE(g->engine->isStepDone());
    ASSERT_FALSE(g->engine->state().game_over);

    const long logs_before = g->logs;
    const long decisions_before = g->decisions;
    g->engine.reset();
    EXPECT_EQ(g->decisions, decisions_before)
        << "the destructor answered decisions of the abandoned game";
    EXPECT_EQ(g->logs, logs_before)
        << "the destructor ran engine code of the abandoned game";
}

// 6.7 — an engine can be dropped at any decision: mulligan, main phase,
// chain priority, showdown, combat damage, cost payment, a card's prompt.
// The fiber is parked deep inside different engine calls at each; unwinding
// every one of them must be clean. One engine per decision of a whole game.
TEST_F(PerfSearchTest, AnEngineCanBeDroppedAtEveryDecisionOfAGame) {
    int dropped = 0;
    for (int depth = 0; depth < 400; ++depth) {
        auto g = begin(77);
        std::mt19937_64 rng(77);
        advance(*g, rng, depth);
        if (g->engine->isStepDone()) break;   // past the end of this game
        const long decisions_before = g->decisions;
        g->engine.reset();
        ASSERT_EQ(g->decisions, decisions_before) << "at decision " << depth;
        ++dropped;
    }
    EXPECT_GT(dropped, 50) << "the game was too short to cover much";
}

// 6.7 / 1.7 — tearing down an engine whose game FAILED still reports the
// failure up to the moment it is dropped, and dropping it is harmless.
TEST_F(PerfSearchTest, AFailedEngineStillReportsItsFailureAndCanBeDropped) {
    auto g = begin(41);
    int seen = 0;
    g->engine->on_decision = [&](const GameState&, const std::vector<Intent>&,
                                 const Intent&) {
        if (++seen == 12) throw std::runtime_error("boom in the engine");
    };
    std::mt19937_64 rng(41);
    EXPECT_THROW(advance(*g, rng, 200), std::runtime_error);
    EXPECT_TRUE(g->engine->stepFailed());
    EXPECT_THROW(g->engine->currentStep(), std::runtime_error);
    g->engine.reset();
}

// ─── 6.4: the pending decision is handed out by reference ───────────────────

const StepResult* addressOf(const StepResult& step) { return &step; }

// 6.4 — currentStep() is a view of the engine's pending decision: asking
// twice yields the same object (no copy of the legal list per call), and it
// shows the next decision once a choice has been applied.
TEST_F(PerfSearchTest, CurrentStepIsAViewOfThePendingDecision) {
    auto g = begin(9);
    const StepResult& first = g->engine->currentStep();
    EXPECT_EQ(&first, addressOf(g->engine->currentStep()));
    ASSERT_EQ(first.kind, StepKind::NeedDecision);
    ASSERT_FALSE(first.legal.empty());

    // A copy taken before a choice is the caller's own.
    const StepResult before = first;
    const long history_before =
        static_cast<long>(g->engine->state().action_history.size());
    const StepResult& returned = g->engine->applyChoice(0);
    EXPECT_EQ(&returned, addressOf(g->engine->currentStep()));
    EXPECT_EQ(static_cast<long>(g->engine->state().action_history.size()),
              history_before + 1);
    EXPECT_EQ(before.kind, StepKind::NeedDecision);
    EXPECT_FALSE(before.legal.empty());

    // The view follows the game to its end.
    std::mt19937_64 rng(9);
    advance(*g, rng, 100000);
    ASSERT_TRUE(g->engine->isStepDone());
    EXPECT_EQ(g->engine->currentStep().kind, StepKind::Done);
    EXPECT_TRUE(g->engine->currentStep().legal.empty());
}

// ─── 6.9: observation features and their hash ───────────────────────────────

/// FNV-1a over every byte of every float, one byte at a time: the definition
/// `ml::computeInfoSetId` must keep producing (trainer-side tables are keyed
/// on it).
uint64_t byteWiseInfoSetId(const std::vector<float>& features, int player) {
    uint64_t h = 14695981039346656037ULL;
    h ^= static_cast<uint64_t>(player + 1);
    h *= 1099511628211ULL;
    for (float v : features) {
        uint32_t bits;
        std::memcpy(&bits, &v, sizeof(bits));
        for (int b = 0; b < 4; ++b) {
            h ^= static_cast<uint64_t>((bits >> (b * 8)) & 0xFF);
            h *= 1099511628211ULL;
        }
    }
    return h;
}

// 6.9 — the hash skips runs of zero floats; the value is unchanged for every
// run length (including runs longer than its power table, at either end, and
// the whole vector) and -0.0f is not treated as a zero.
TEST(PerfSearchHash, SkippingZeroRunsDoesNotChangeTheInfoSetId) {
    for (int player = 0; player < 2; ++player) {
        EXPECT_EQ(ml::computeInfoSetId({}, player), byteWiseInfoSetId({}, player));
        for (size_t run : {1u, 2u, 3u, 254u, 255u, 256u, 257u, 510u, 511u,
                           1100u, 6188u, 6192u, 6442u, 10944u}) {
            std::vector<float> zeros(run, 0.0f);
            EXPECT_EQ(ml::computeInfoSetId(zeros, player),
                      byteWiseInfoSetId(zeros, player)) << "all zeros, " << run;
            std::vector<float> framed(run + 2, 0.0f);
            framed.front() = 3.0f;
            framed.back() = -1.0f;
            EXPECT_EQ(ml::computeInfoSetId(framed, player),
                      byteWiseInfoSetId(framed, player)) << "framed run " << run;
            framed.back() = 0.0f;
            EXPECT_EQ(ml::computeInfoSetId(framed, player),
                      byteWiseInfoSetId(framed, player)) << "trailing run " << run;
            framed[run / 2 + 1] = -0.0f;
            EXPECT_EQ(ml::computeInfoSetId(framed, player),
                      byteWiseInfoSetId(framed, player)) << "-0.0f in run " << run;
        }
    }
    // Sparse and dense random vectors of the real size.
    std::mt19937_64 rng(69);
    for (int trial = 0; trial < 200; ++trial) {
        std::vector<float> v(ml::kStateFeatureDim, 0.0f);
        const uint64_t density = 1 + rng() % 400;   // 1 in `density` non-zero
        for (auto& f : v) {
            if (rng() % density == 0) {
                f = static_cast<float>(static_cast<int>(rng() % 1200)) - 1.0f;
            }
        }
        ASSERT_EQ(ml::computeInfoSetId(v, trial & 1),
                  byteWiseInfoSetId(v, trial & 1)) << "trial " << trial;
    }
    // A run's length is part of the hash.
    EXPECT_NE(ml::computeInfoSetId(std::vector<float>(300, 0.0f), 0),
              ml::computeInfoSetId(std::vector<float>(301, 0.0f), 0));
}

// 6.9 — the features are gathered in one pass over the objects. The numbers
// that used to come from per-battlefield / per-player queries of the state
// still equal those queries, at every decision of real games, from both seats.
TEST_F(PerfSearchTest, FeaturesStillMatchTheStateQueriesTheyReplaced) {
    // Layout (feature_extractor.h): 10 globals, 8 chain, then 70 per
    // player (self, opponent), then 49 per battlefield slot, whose two side
    // blocks are self then opponent as well since the 2026-10 layout (they
    // were Player1 then Player2: plans/01 item 1.9).
    constexpr int kSelf = 18, kOpp = 88, kBf = 158, kPerBf = 49;
    constexpr int kRunesReady = 39, kRunesExhausted = 40;
    constexpr int kBaseUnits = 53, kBaseMight = 54, kBaseExhausted = 56;
    constexpr int kBaseGear = 60;
    constexpr int kSideSelf = 9, kSideOpp = 29;     // count, then total might
    constexpr int kSideTopMight = 14;               // highest might on the side
    constexpr int kReadyBase = ml::layout::kExtGlobals;     // self, opp

    long units_seen = 0, checked = 0;
    for (uint64_t seed : {3u, 11u, 29u}) {
        auto g = begin(seed);
        std::mt19937_64 rng(seed);
        while (!g->engine->isStepDone()) {
            const GameState& st = g->engine->state();
            for (PlayerId me : {PlayerId::Player1, PlayerId::Player2}) {
                const auto f = ml::extractStateFeatures(st, me, *db_);
                ASSERT_EQ(f.size(), static_cast<size_t>(ml::kStateFeatureDim));
                const std::string where = "seed " + std::to_string(seed) +
                    " decision " + std::to_string(st.action_history.size());
                int side = 0;
                for (PlayerId p : {me, opponent(me)}) {
                    const int base = side++ == 0 ? kSelf : kOpp;
                    int ready = 0, exhausted = 0;
                    for (auto rid : st.runesInBase(p)) {
                        (st.getObject(rid).is_exhausted ? exhausted : ready)++;
                    }
                    int units = 0, might = 0, units_exhausted = 0, gear = 0;
                    for (const auto& [id, obj] : st.objects) {
                        if (obj.controller != p || !obj.isAtBase()) continue;
                        if (obj.isGear()) ++gear;
                        if (!obj.isUnit()) continue;
                        ++units;
                        might += obj.current_might;
                        if (obj.is_exhausted) ++units_exhausted;
                    }
                    ASSERT_EQ(f[base + kRunesReady], ready) << where;
                    ASSERT_EQ(f[base + kRunesExhausted], exhausted) << where;
                    ASSERT_EQ(f[base + kBaseUnits], units) << where;
                    ASSERT_EQ(f[base + kBaseMight], might) << where;
                    ASSERT_EQ(f[base + kBaseExhausted], units_exhausted) << where;
                    ASSERT_EQ(f[base + kBaseGear], gear) << where;
                    ASSERT_EQ(f[kReadyBase + (base == kSelf ? 0 : 1)],
                              units - units_exhausted) << where;
                }
                for (int i = 0; i < 4; ++i) {
                    const int bf = kBf + i * kPerBf;
                    if (i >= static_cast<int>(st.battlefields.size())) {
                        ASSERT_EQ(f[bf + kSideSelf], 0) << where;
                        continue;
                    }
                    const LocationId loc = BattlefieldLocation{st.battlefields[i].id};
                    int col = kSideSelf;
                    for (PlayerId p : {me, opponent(me)}) {
                        int might = 0, top = 0;
                        const auto here = st.unitsAt(loc, p);
                        for (auto uid : here) {
                            const int m = st.getObject(uid).current_might;
                            might += m;
                            top = std::max(top, m);
                        }
                        ASSERT_EQ(f[bf + col], static_cast<int>(here.size())) << where;
                        ASSERT_EQ(f[bf + col + 1], might) << where;
                        if (!here.empty()) {
                            ASSERT_EQ(f[bf + col + kSideTopMight], top) << where;
                        }
                        units_seen += static_cast<long>(here.size());
                        col = kSideOpp;
                    }
                }
                ++checked;
            }
            advance(*g, rng, 1);
        }
    }
    EXPECT_GT(checked, 300);
    EXPECT_GT(units_seen, 100) << "the games never put units on a battlefield";
}

// 6.9 — the buffer-reusing overload yields the same vector whatever the
// buffer held before, and reuses its storage.
TEST_F(PerfSearchTest, ExtractingIntoAReusedBufferMatchesAFreshExtraction) {
    auto g = begin(5);
    std::mt19937_64 rng(5);
    std::vector<float> buffer(3 * ml::kStateFeatureDim, 42.0f);   // stale, too long
    const float* storage = buffer.data();
    for (int step = 0; step < 120 && !g->engine->isStepDone(); ++step) {
        for (PlayerId me : {PlayerId::Player1, PlayerId::Player2}) {
            ml::extractStateFeatures(g->engine->state(), me, *db_, buffer);
            ASSERT_EQ(buffer, ml::extractStateFeatures(g->engine->state(), me, *db_))
                << "decision " << step;
            ASSERT_EQ(buffer.data(), storage) << "the buffer was reallocated";
        }
        advance(*g, rng, 1);
    }
}

} // namespace test_review_fix_perf_search
