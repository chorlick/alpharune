/// Regression tests for the performance-review fixes in the search path
/// (plans/06-performance.md items 6.4 and 6.7): the step API hands out its
/// pending decision by reference, and an engine abandoned mid-game is torn
/// down instead of being played to the end.
///
/// The OpenSpiel-side tests for 6.8 / 6.9 are in
/// src/openspiel/openspiel_tests.cpp (they need the Game).

#include <gtest/gtest.h>

#include "cards/card_registry.h"
#include "core/card_db.h"
#include "core/events.h"
#include "engine/game_engine.h"
#include "engine/step_driver.h"
#include "rules/deck_validator.h"

#include <cstdlib>
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

} // namespace test_review_fix_perf_search
