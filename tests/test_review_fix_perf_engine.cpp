/// Regression tests for the performance-review fixes in the engine itself
/// (plans/06-performance.md items 6.1, 6.2, 6.3, 6.5, 6.6, 6.10, 6.11): a log
/// line nobody reads is not built, and the board is asked through an index
/// instead of a walk over every object — with the same answers, in the same
/// order, as before.
///
/// The probe tests of plans/09 items 9.HH4 / 9.II1 are in
/// tests/cards/test_sweep_costs.cpp, beside the probe's other tests.

#include <gtest/gtest.h>

#include "cards/card.h"
#include "cards/card_registry.h"
#include "cards/card_test_fixture.h"
#include "core/card_db.h"
#include "core/events.h"
#include "engine/game_engine.h"
#include "rules/deck_validator.h"

#include <cstdint>
#include <cstdlib>
#include <memory>
#include <random>
#include <string>
#include <vector>

namespace test_review_fix_perf_engine {

using namespace riftbound;
using riftbound::test::CardTestFixture;

// ─── 6.1: a log line nobody reads is not built ──────────────────────────────

/// What a listener was handed.
struct Heard {
    std::vector<LogEvent> lines;
    boost::signals2::scoped_connection conn;
    explicit Heard(EventBus& bus) {
        conn = bus.on_log.connect([this](const LogEvent& e) { lines.push_back(e); });
    }
};

/// A message expression with a visible side effect: counts how often it runs.
struct Counted {
    int built = 0;
    std::string operator()(const char* text) { ++built; return text; }
};

TEST(PerfEngineLog, AMessageNobodyReadsIsNotBuilt) {
    EventBus bus;
    Counted msg;
    EXPECT_FALSE(bus.logging());
    RIFT_TRACE(bus, msg("one"));
    RIFT_DEBUG(bus, msg("two"));
    RIFT_TRACE_PRIVATE(bus, PlayerId::Player1, msg("three"));
    EXPECT_EQ(msg.built, 0) << "a message was built with nobody listening";

    Heard heard(bus);
    EXPECT_TRUE(bus.logging());
    RIFT_TRACE(bus, msg("four"));
    EXPECT_EQ(msg.built, 1);
    ASSERT_EQ(heard.lines.size(), 1u);
    // The three unread lines were counted: this is the fourth of decision 0,
    // exactly what a listener attached from the start would have been handed.
    EXPECT_EQ(heard.lines[0].message, "[D:000.04] four");
}

TEST(PerfEngineLog, EveryMacroDeliversWhatItsFunctionDelivers) {
    // Two buses given the same lines, one through the macros and one through
    // the functions: the listeners must be handed the same events.
    EventBus by_macro, by_function;
    Heard a(by_macro), b(by_function);
    const LogAudience nobody = LogAudience::neither();

    RIFT_TRACE(by_macro, std::string("trace"));
    RIFT_DEBUG(by_macro, std::string("debug"));
    RIFT_INFO(by_macro, std::string("info"));
    RIFT_WARN(by_macro, std::string("warn"));
    RIFT_TRACE_DECISION(by_macro, "DECISION #1 (P1): 3 legal actions");
    RIFT_TRACE_PRIVATE(by_macro, PlayerId::Player2, std::string("private"));
    RIFT_TRACE_FOR(by_macro, nobody, std::string("for nobody"));
    RIFT_LOG_FOR(by_macro, LogLevel::Debug, LogAudience::only(PlayerId::Player1),
                 std::string("debug for one"));

    by_function.logTrace("trace");
    by_function.logDebug("debug");
    by_function.logInfo("info");
    by_function.logWarn("warn");
    by_function.logTrace("DECISION #1 (P1): 3 legal actions");
    by_function.logTracePrivate(PlayerId::Player2, "private");
    by_function.logTraceFor(nobody, "for nobody");
    by_function.logFor(LogLevel::Debug, LogAudience::only(PlayerId::Player1), "debug for one");

    ASSERT_EQ(a.lines.size(), 8u);
    ASSERT_EQ(b.lines.size(), 8u);
    for (size_t i = 0; i < a.lines.size(); ++i) {
        EXPECT_EQ(a.lines[i].message, b.lines[i].message) << i;
        EXPECT_EQ(a.lines[i].level, b.lines[i].level) << i;
        EXPECT_EQ(a.lines[i].audience.player1, b.lines[i].audience.player1) << i;
        EXPECT_EQ(a.lines[i].audience.player2, b.lines[i].audience.player2) << i;
    }
    EXPECT_EQ(a.lines[0].message, "[D:000.01] trace");
    EXPECT_EQ(a.lines[1].level, LogLevel::Debug);
    EXPECT_EQ(a.lines[2].level, LogLevel::Info);
    EXPECT_EQ(a.lines[3].level, LogLevel::Warning);
    EXPECT_EQ(a.lines[4].message, "[D:001.00] DECISION #1 (P1): 3 legal actions");
    EXPECT_EQ(a.lines[5].message, "[D:001.01] private");
    EXPECT_FALSE(a.lines[5].visibleTo(PlayerId::Player1));
    EXPECT_TRUE(a.lines[5].visibleTo(PlayerId::Player2));
    EXPECT_TRUE(a.lines[6].audience.isNeither());
    EXPECT_EQ(a.lines[7].level, LogLevel::Debug);
    EXPECT_TRUE(a.lines[7].visibleTo(PlayerId::Player1));
}

TEST(PerfEngineLog, AnUnreadDecisionLineStillStartsANewDecision) {
    // The "[D:NNN.NN]" prefix numbers lines per decision. Lines that were
    // never built are counted all the same — through the macros and through
    // the functions — so the prefix does not depend on when a listener came.
    for (const bool macros : {true, false}) {
        EventBus bus;
        if (macros) {
            RIFT_TRACE(bus, std::string("before"));
            RIFT_TRACE_DECISION(bus, "DECISION #7 (P2): 2 legal actions");
            RIFT_TRACE(bus, std::string("after"));
        } else {
            bus.logTrace("before");
            bus.logTrace("DECISION #7 (P2): 2 legal actions");
            bus.logDebug("after");
        }
        Heard heard(bus);
        RIFT_TRACE(bus, std::string("heard"));
        ASSERT_EQ(heard.lines.size(), 1u);
        EXPECT_EQ(heard.lines[0].message, "[D:001.02] heard") << (macros ? "macros" : "functions");
    }
}

TEST(PerfEngineLog, AListenerThatLeavesStopsTheBuilding) {
    EventBus bus;
    Counted msg;
    {
        Heard heard(bus);
        RIFT_TRACE(bus, msg("a"));
        EXPECT_EQ(heard.lines.size(), 1u);
    }   // the scoped connection ends here
    EXPECT_FALSE(bus.logging());
    RIFT_TRACE(bus, msg("b"));
    EXPECT_EQ(msg.built, 1);

    int direct = 0;
    boost::signals2::connection c = bus.on_log.connect([&](const LogEvent&) { ++direct; });
    boost::signals2::connection d = bus.on_log.connect([&](const LogEvent&) { ++direct; });
    EXPECT_EQ(bus.on_log.num_slots(), 2u);
    c.disconnect();
    EXPECT_TRUE(bus.logging()) << "one of the two listeners is still there";
    RIFT_TRACE(bus, msg("c"));
    EXPECT_EQ(direct, 1);
    d.disconnect();
    EXPECT_FALSE(bus.logging());
    EXPECT_TRUE(bus.on_log.empty());

    bus.on_log.connect([&](const LogEvent&) { ++direct; });
    EXPECT_TRUE(bus.logging());
    bus.disconnectAll();
    EXPECT_FALSE(bus.logging());
    RIFT_TRACE(bus, msg("d"));
    EXPECT_EQ(msg.built, 2);
    EXPECT_EQ(direct, 1);
}

TEST(PerfEngineLog, AQuietStretchBuildsNothingAndIsStillCounted) {
    // EventBus::beginQuiet: "a stretch whose emissions are dropped". Its log
    // lines are not built either, and what it emits is not kept.
    EventBus bus;
    Heard heard(bus);
    int moved = 0;
    boost::signals2::scoped_connection conn =
        bus.on_unit_readied.connect([&](const UnitReadiedEvent&) { ++moved; });
    Counted msg;
    RIFT_TRACE(bus, msg("before"));
    const auto quiet = bus.beginQuiet();
    EXPECT_FALSE(bus.logging());
    EXPECT_TRUE(bus.holding());
    RIFT_TRACE(bus, msg("inside"));
    bus.logTrace("inside, by the function");
    bus.emit(UnitReadiedEvent{});
    bus.endQuiet(quiet);
    EXPECT_FALSE(bus.holding());
    EXPECT_TRUE(bus.logging());
    RIFT_TRACE(bus, msg("after"));
    EXPECT_EQ(msg.built, 2) << "the line inside the quiet stretch was built";
    EXPECT_EQ(moved, 0);
    ASSERT_EQ(heard.lines.size(), 2u);
    EXPECT_EQ(heard.lines[0].message, "[D:000.01] before");
    EXPECT_EQ(heard.lines[1].message, "[D:000.04] after");
}

std::string deckFile(const char* name) {
    const char* root = std::getenv("RIFTBOUND_ROOT");
    return std::string(root ? root : "..") + "/decks/" + name;
}

class PerfEngineGame : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        registry_ = new CardRegistry();
        registry_->loadAll();
        db_ = new CardDB();
        db_->buildFromClasses(*registry_);
        deck1_ = new DeckSubmission(
            DeckValidator::loadFromDeckList(deckFile("fiora.txt"), *db_));
        deck2_ = new DeckSubmission(
            DeckValidator::loadFromDeckList(deckFile("yi_rebuild.txt"), *db_));
    }
    static void TearDownTestSuite() {
        delete deck2_; delete deck1_; delete db_; delete registry_;
    }

    /// One step-machine game played by a seeded sequence of random choices.
    struct Game {
        EventBus bus;
        std::unique_ptr<GameEngine> engine;
        std::mt19937_64 rng;
        int applied = 0;
        Game(uint64_t seed, uint64_t choice_seed) : rng(choice_seed) {
            engine = std::make_unique<GameEngine>(*db_, bus, *registry_);
            engine->beginGame(*deck1_, *deck2_, seed);
        }
        /// Up to `n` more decisions; false once the game is over.
        bool advance(int n) {
            for (int i = 0; i < n && !engine->isStepDone(); ++i, ++applied) {
                const auto& legal = engine->currentStep().legal;
                std::uniform_int_distribution<size_t> d(0, legal.size() - 1);
                engine->applyChoice(static_cast<int>(d(rng)));
            }
            return !engine->isStepDone();
        }
    };

    static CardRegistry* registry_;
    static CardDB* db_;
    static DeckSubmission* deck1_;
    static DeckSubmission* deck2_;
};

CardRegistry* PerfEngineGame::registry_ = nullptr;
CardDB* PerfEngineGame::db_ = nullptr;
DeckSubmission* PerfEngineGame::deck1_ = nullptr;
DeckSubmission* PerfEngineGame::deck2_ = nullptr;

// 6.1 — the whole engine and every card on the path of these games: a
// listener that attaches after `attach_at` decisions is handed, line for line
// and prefix for prefix, what a listener attached from the first line was
// handed from that point on. A call site whose unread line is miscounted (a
// "DECISION #" line through the plain macro, a pre-built line with no
// noteUnheardLine) shows up here as a prefix that differs.
TEST_F(PerfEngineGame, AListenerAttachedInMidGameIsHandedWhatOneAttachedFromTheStartWas) {
    long compared = 0;
    for (const uint64_t seed : {3u, 11u, 20261010u}) {
        for (const int attach_at : {1, 40, 150}) {
            Game from_start(seed, seed * 7 + 1);
            Heard all(from_start.bus);
            from_start.advance(attach_at);
            const size_t lines_before = all.lines.size();
            while (from_start.advance(1000)) {}

            Game late(seed, seed * 7 + 1);
            late.advance(attach_at);
            EXPECT_FALSE(late.bus.logging());
            Heard tail(late.bus);
            while (late.advance(1000)) {}

            ASSERT_EQ(late.applied, from_start.applied) << "the two games diverged";
            ASSERT_EQ(lines_before + tail.lines.size(), all.lines.size())
                << "seed " << seed << ", attached after " << attach_at << " decisions";
            for (size_t i = 0; i < tail.lines.size(); ++i) {
                const LogEvent& want = all.lines[lines_before + i];
                ASSERT_EQ(tail.lines[i].message, want.message)
                    << "seed " << seed << ", attached after " << attach_at
                    << " decisions, line " << i;
                ASSERT_EQ(tail.lines[i].level, want.level);
                ASSERT_EQ(tail.lines[i].audience.player1, want.audience.player1);
                ASSERT_EQ(tail.lines[i].audience.player2, want.audience.player2);
                ++compared;
            }
        }
    }
    EXPECT_GT(compared, 3000) << "the games were long enough to mean something";
}

// 6.1 — building no log line changes nothing about the game: the same seed
// and the same choices lead through the same decisions to the same end with
// and without a listener.
TEST_F(PerfEngineGame, AGameIsTheSameWithAndWithoutAListener) {
    for (const uint64_t seed : {5u, 42u}) {
        Game heard_game(seed, seed + 100);
        Heard heard(heard_game.bus);
        while (heard_game.advance(1000)) {}
        Game silent(seed, seed + 100);
        while (silent.advance(1000)) {}
        const GameState& a = heard_game.engine->state();
        const GameState& b = silent.engine->state();
        EXPECT_GT(heard.lines.size(), 1000u);
        EXPECT_EQ(a.action_history, b.action_history) << "seed " << seed;
        EXPECT_EQ(a.winner, b.winner);
        EXPECT_EQ(a.turn.turn_number, b.turn.turn_number);
        EXPECT_EQ(a.players[0].score, b.players[0].score);
        EXPECT_EQ(a.players[1].score, b.players[1].score);
        EXPECT_EQ(a.next_object_id, b.next_object_id);
    }
}

}  // namespace test_review_fix_perf_engine
