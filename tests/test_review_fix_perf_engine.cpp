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
#include "core/board_state.h"
#include "cards/card_helpers.h"
#include "core/card_db.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include "engine/game_engine.h"
#include "engine/knowledge_tracker.h"
#include "engine/resource_payment.h"
#include "io/state_editor.h"
#include "ml/cfr_util.h"
#include "ml/feature_extractor.h"
#include "rules/deck_validator.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <random>
#include <regex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace test_review_fix_perf_engine {

using namespace riftbound;
using riftbound::test::CardTestFixture;
using riftbound::test::EnginePathAgent;

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
    // The unread lines were counted: this is the third line of decision 0
    // that both players read (the private "three" is not numbered for them,
    // batch 5b item T2), exactly what a listener attached from the start
    // would have been handed.
    EXPECT_EQ(heard.lines[0].message, "[D:000.03] four");
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
    // A line one player reads carries the number of the last line both read
    // (batch 5b item T2: the number would otherwise count the lines the other
    // player missed).
    EXPECT_EQ(a.lines[5].message, "[D:001.00] private");
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
    const uint64_t stamp_before = bus.lineStamp();
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
    // The quiet lines are counted (lineStamp, what the chain's "a finalization
    // that logged nothing" reads) but not numbered: how many lines a probe
    // wrote follows cards a player cannot see (batch 5b item T2; was
    // "[D:000.04] after").
    EXPECT_EQ(heard.lines[1].message, "[D:000.02] after");
    EXPECT_EQ(bus.lineStamp(), stamp_before + 3);
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

// ─── 6.6 / 6.11: cleanup ────────────────────────────────────────────────────

constexpr CardDefId kTCountingElder = 1840;   // unit: "any amount of your damage is enough to kill"

/// Elder Dragon's rule on a test unit that counts how often it is asked.
class TCountingElder : public UnitCard {
public:
    TCountingElder() {
        def_.id = kTCountingElder;
        def_.name = "Test Counting Elder";
        def_.card_type = CardType::Unit;
        def_.might = 4;
    }
    const CardDef& def() const override { return def_; }
    bool anyDamageIsLethal() const override { ++asked; return true; }
    static inline int asked = 0;
private:
    CardDef def_;
};

class PerfEngineCards : public CardTestFixture {
protected:
    void SetUp() override {
        CardTestFixture::SetUp();
        card_registry.registerCard(kTCountingElder, std::make_unique<TCountingElder>());
        card_db.buildFromClasses(card_registry);
        TCountingElder::asked = 0;
    }
    GameObject& o(GameObjectId id) { return state.getObject(id); }
    bool onBoard(GameObjectId id) { return o(id).location.has_value(); }
};

// 6.6 — processLethalDamage asked "is any amount of P's damage lethal?" for
// both players at the top of every call. It is now asked only for a unit that
// carries damage short of lethal, once per player.
TEST_F(PerfEngineCards, TheElderRuleIsAskedOnlyForAUnitCarryingDamageShortOfLethal) {
    addUnit(P1, kTCountingElder, 4);
    const GameObjectId healthy = addUnit(P2, kInvalidId, 3);
    const GameObjectId dead = addUnit(P2, kInvalidId, 2);
    o(dead).damage_marked = 2;
    o(dead).damage_marked_by[0] = 2;
    withEngine([](GameEngine& e) { e.testHook_processLethalDamage(); });
    EXPECT_EQ(TCountingElder::asked, 0)
        << "nothing carried damage short of lethal: nobody needed the rule";
    EXPECT_FALSE(onBoard(dead)) << "plain lethal damage still kills";
    EXPECT_TRUE(onBoard(healthy));

    // Two units with one damage each from P1: both die by the rule, which is
    // asked once (and not at all for P2, whose damage is on nobody).
    const GameObjectId scratched = addUnit(P2, kInvalidId, 5);
    o(healthy).damage_marked = 1;
    o(healthy).damage_marked_by[0] = 1;
    o(scratched).damage_marked = 1;
    o(scratched).damage_marked_by[0] = 1;
    withEngine([](GameEngine& e) { e.testHook_processLethalDamage(); });
    EXPECT_EQ(TCountingElder::asked, 1);
    EXPECT_FALSE(onBoard(healthy));
    EXPECT_FALSE(onBoard(scratched));
}

TEST_F(PerfEngineCards, DamageAUnitDidToItsOwnSideIsNotTheOtherPlayersDamage) {
    // The rule is about "your damage": a unit scratched by its own side stays,
    // with the Elder unit in play on the other side.
    addUnit(P1, kTCountingElder, 4);
    const GameObjectId own_goal = addUnit(P2, kInvalidId, 5);
    o(own_goal).damage_marked = 1;
    o(own_goal).damage_marked_by[1] = 1;
    withEngine([](GameEngine& e) { e.testHook_processLethalDamage(); });
    EXPECT_TRUE(onBoard(own_goal));
    EXPECT_EQ(TCountingElder::asked, 0);
}

// 6.11 / 9.KK3 — the "becomes [Mighty]" edge is a stamp on the unit
// (GameObject::mighty_seen), not a string-keyed counter and not a flag that a
// walk over every object has to clear: it fires once on the way up, re-arms on
// the way down and is forgotten off the board.
TEST_F(PerfEngineCards, BecomingMightyIsAnnouncedOncePerCrossing) {
    const GameObjectId unit = addUnit(P1, kInvalidId, 4);
    int announced = 0;
    auto conn = events.on_object_state_changed.connect([&](const ObjectStateChangedEvent& e) {
        if (e.object == unit && e.what_changed == "became_mighty") ++announced;
    });
    auto cleanup = [&] { withEngine([](GameEngine& e) { e.testHook_cleanup(); }); };
    // "Was Mighty at the last cleanup": the unit's stamp is that cleanup's.
    auto wasMighty = [&] {
        return o(unit).mighty_seen != 0 && o(unit).mighty_seen == state.mighty_checks;
    };
    cleanup();
    EXPECT_EQ(announced, 0);
    EXPECT_FALSE(wasMighty());
    o(unit).base_might = 5;
    cleanup();
    EXPECT_EQ(announced, 1);
    EXPECT_TRUE(wasMighty());
    cleanup();
    EXPECT_EQ(announced, 1) << "still Mighty: not announced again";
    o(unit).base_might = 4;
    cleanup();
    EXPECT_FALSE(wasMighty()) << "below 5 the edge is armed again";
    o(unit).base_might = 6;
    cleanup();
    EXPECT_EQ(announced, 2);
    // Off the board the edge is forgotten: back at 5+ it becomes Mighty again.
    o(unit).location = std::nullopt;
    o(unit).zone = ZoneType::Hand;
    cleanup();
    EXPECT_FALSE(wasMighty());
    o(unit).location = BaseLocation{P1};
    o(unit).zone = ZoneType::Base;
    cleanup();
    EXPECT_EQ(announced, 3);
    // Off the board for several cleanups and back below 5, then up again.
    o(unit).location = std::nullopt;
    o(unit).zone = ZoneType::Hand;
    cleanup();
    cleanup();
    o(unit).base_might = 4;
    o(unit).location = BaseLocation{P1};
    o(unit).zone = ZoneType::Base;
    cleanup();
    EXPECT_EQ(announced, 3);
    o(unit).base_might = 5;
    cleanup();
    EXPECT_EQ(announced, 4);
    // Two units crossing in the same cleanup are announced in table order,
    // each once.
    const GameObjectId other = addUnit(P1, kInvalidId, 7);
    int other_announced = 0;
    auto conn2 = events.on_object_state_changed.connect([&](const ObjectStateChangedEvent& e) {
        if (e.object == other && e.what_changed == "became_mighty") ++other_announced;
    });
    cleanup();
    cleanup();
    EXPECT_EQ(other_announced, 1);
    EXPECT_EQ(announced, 4);
    conn2.disconnect();
    EXPECT_TRUE(o(unit).card_counters.empty()) << "the edge no longer lives in card_counters";
    conn.disconnect();
}

// ─── 6.10: the object table's board index ───────────────────────────────────

/// What the board queries answered before there was an index: a walk over
/// every object, in the table's order.
struct Walk {
    static std::vector<GameObjectId> where(const GameState& st,
                                           const std::function<bool(const GameObject&)>& pred) {
        std::vector<GameObjectId> out;
        for (const auto& [id, obj] : st.objects)
            if (pred(obj)) out.push_back(id);
        return out;
    }
    template <class Range>
    static std::vector<GameObjectId> ids(const Range& range) {
        std::vector<GameObjectId> out;
        for (const auto& [id, obj] : range) out.push_back(id);
        return out;
    }
};

/// Every board query against the walk it replaced: same ids, same order.
void expectIndexMatchesAWalk(const GameState& st, const std::string& when) {
    SCOPED_TRACE(when);
    EXPECT_EQ(Walk::ids(st.objects.onBoard()),
              Walk::where(st, [](const GameObject& o) { return o.location.has_value(); }));
    EXPECT_EQ(Walk::ids(st.objects.inPlay()), Walk::where(st, [](const GameObject& o) {
        return o.location.has_value() || o.zone == ZoneType::LegendZone;
    }));
    std::vector<LocationId> locations = {LocationId{BaseLocation{PlayerId::Player1}},
                                         LocationId{BaseLocation{PlayerId::Player2}},
                                         LocationId{BattlefieldLocation{77}}};   // never used
    for (const auto& bf : st.battlefields) locations.push_back(LocationId{BattlefieldLocation{bf.id}});
    for (const auto& loc : locations) {
        EXPECT_EQ(Walk::ids(st.objects.atLocation(loc)), Walk::where(st, [&](const GameObject& o) {
            return o.location.has_value() && *o.location == loc;
        }));
        EXPECT_EQ(st.objectsAt(loc), Walk::where(st, [&](const GameObject& o) {
            return o.location.has_value() && *o.location == loc;
        }));
        for (PlayerId p : {PlayerId::Player1, PlayerId::Player2})
            EXPECT_EQ(st.unitsAt(loc, p), Walk::where(st, [&](const GameObject& o) {
                return o.isUnit() && o.controller == p && o.location.has_value() &&
                       *o.location == loc;
            }));
    }
    for (PlayerId p : {PlayerId::Player1, PlayerId::Player2}) {
        EXPECT_EQ(st.allUnitsControlledBy(p), Walk::where(st, [&](const GameObject& o) {
            return o.isUnit() && o.controller == p && o.location.has_value();
        }));
        const LocationId base{BaseLocation{p}};
        EXPECT_EQ(st.runesInBase(p), Walk::where(st, [&](const GameObject& o) {
            return o.isRune() && o.location.has_value() && *o.location == base;
        }));
        EXPECT_EQ(st.objectsInBase(p), Walk::where(st, [&](const GameObject& o) {
            return o.location.has_value() && *o.location == base;
        }));
        for (const auto& bf : st.battlefields) {
            const auto here = Walk::where(st, [&](const GameObject& o) {
                return o.isUnit() && o.controller == p && o.battlefieldId() == bf.id;
            });
            EXPECT_EQ(bf.unitsControlledBy(p, st.objects), here);
            EXPECT_EQ(bf.hasUnitsFrom(p, st.objects), !here.empty());
        }
    }
}

/// A state with objects in every kind of place.
GameState scatteredState(int objects) {
    GameState st;
    for (uint32_t slot = 0; slot < 3; ++slot) {
        const BattlefieldId b = BattlefieldId::fromRaw(slot);
        BattlefieldState bf;
        bf.id = b;
        st.battlefields.push_back(bf);
    }
    for (int i = 0; i < objects; ++i) {
        const GameObjectId id = st.createObject();
        GameObject& o = st.getObject(id);
        o.controller = o.owner = (i % 2) ? PlayerId::Player1 : PlayerId::Player2;
        o.card_type = (i % 5 == 0) ? CardType::Rune : (i % 5 == 1) ? CardType::Gear : CardType::Unit;
        switch (i % 7) {
            case 0: o.zone = ZoneType::Hand; break;
            case 1: o.zone = ZoneType::Base; o.location = BaseLocation{o.controller}; break;
            case 2: o.zone = ZoneType::BattlefieldZone; o.location = BattlefieldLocation{0}; break;
            case 3: o.zone = ZoneType::BattlefieldZone; o.location = BattlefieldLocation{2}; break;
            case 4: o.zone = ZoneType::Trash; break;
            case 5: o.zone = ZoneType::LegendZone; break;
            default: break;   // MainDeck
        }
    }
    return st;
}

// 6.10 — however a zone or a location is written, the next query is right.
// There is no call that tells the table: the fields do.
TEST(PerfEngineTable, EveryWayOfMovingAnObjectIsSeenByTheNextQuery) {
    GameState st = scatteredState(40);
    expectIndexMatchesAWalk(st, "as built");
    GameObject& a = st.getObject(GameObjectId::fromRaw(3));
    GameObject& b = st.getObject(GameObjectId::fromRaw(10));

    a.location = BattlefieldLocation{1};
    a.zone = ZoneType::BattlefieldZone;
    expectIndexMatchesAWalk(st, "a location assigned");
    a.location = std::nullopt;
    expectIndexMatchesAWalk(st, "nullopt assigned");
    a.location = LocationId{BaseLocation{PlayerId::Player2}};
    expectIndexMatchesAWalk(st, "a LocationId assigned");
    a.location.reset();
    expectIndexMatchesAWalk(st, "reset()");
    a.location = std::optional<LocationId>{BattlefieldLocation{2}};
    expectIndexMatchesAWalk(st, "an optional assigned");
    b.location = a.location;
    expectIndexMatchesAWalk(st, "one object's location assigned to another");
    a.zone = ZoneType::LegendZone;
    a.location.reset();
    expectIndexMatchesAWalk(st, "into the Legend Zone");
    b.zone = a.zone;
    b.location = a.location;
    expectIndexMatchesAWalk(st, "one object's zone assigned to another");

    // Whole objects: assigned from a local copy, swapped, replaced.
    GameObject local = st.getObject(GameObjectId::fromRaw(5));
    local.location = BattlefieldLocation{0};
    local.zone = ZoneType::BattlefieldZone;
    expectIndexMatchesAWalk(st, "a local copy edited: nothing in the table changed");
    st.getObject(GameObjectId::fromRaw(5)) = local;
    expectIndexMatchesAWalk(st, "an object assigned from a local copy");
    std::swap(st.getObject(GameObjectId::fromRaw(6)), st.getObject(GameObjectId::fromRaw(9)));
    std::swap(st.getObject(GameObjectId::fromRaw(6)).id, st.getObject(GameObjectId::fromRaw(9)).id);
    expectIndexMatchesAWalk(st, "two objects swapped");
    st.objects[GameObjectId::fromRaw(8)] = GameObject{};
    st.objects[GameObjectId::fromRaw(8)].id = GameObjectId::fromRaw(8);
    expectIndexMatchesAWalk(st, "an object replaced by a blank one");

    // New and removed objects.
    const GameObjectId fresh = st.createObject();
    st.getObject(fresh).card_type = CardType::Unit;
    st.getObject(fresh).controller = PlayerId::Player1;
    st.getObject(fresh).location = BattlefieldLocation{1};
    expectIndexMatchesAWalk(st, "an object created and placed");
    GameObject built;
    built.id = GameObjectId::fromRaw(999);
    built.card_type = CardType::Unit;
    built.controller = PlayerId::Player2;
    built.location = BaseLocation{PlayerId::Player2};
    st.objects.emplace(GameObjectId::fromRaw(999), built);
    expectIndexMatchesAWalk(st, "an object emplaced already placed");
    st.getObject(GameObjectId::fromRaw(999)).location = BattlefieldLocation{0};
    expectIndexMatchesAWalk(st, "... and moved afterwards");
    st.objects.erase(GameObjectId::fromRaw(999));
    expectIndexMatchesAWalk(st, "an object erased");

    // A field that is not tracked is read when the query is asked.
    st.getObject(fresh).controller = PlayerId::Player2;
    st.getObject(fresh).card_type = CardType::Gear;
    expectIndexMatchesAWalk(st, "controller and type changed in place");
}

TEST(PerfEngineTable, ACopyOfAStateHasAnIndexOfItsOwn) {
    GameState original = scatteredState(30);
    expectIndexMatchesAWalk(original, "original");
    GameState copy = original;
    expectIndexMatchesAWalk(copy, "copy");
    copy.getObject(GameObjectId::fromRaw(2)).location = BattlefieldLocation{1};
    copy.getObject(GameObjectId::fromRaw(4)).location.reset();
    expectIndexMatchesAWalk(copy, "copy, after moving two of its objects");
    expectIndexMatchesAWalk(original, "original, untouched by the copy");
    EXPECT_NE(Walk::ids(copy.objects.onBoard()), Walk::ids(original.objects.onBoard()));

    original.getObject(GameObjectId::fromRaw(2)).location = BattlefieldLocation{2};
    expectIndexMatchesAWalk(original, "original, moved after the copy was taken");
    expectIndexMatchesAWalk(copy, "copy, untouched by the original");

    // Assigned over an existing state (the engine's `state_ = snapshot`).
    copy = original;
    expectIndexMatchesAWalk(copy, "assigned");
    copy.getObject(GameObjectId::fromRaw(2)).location.reset();
    expectIndexMatchesAWalk(copy, "assigned, then moved");
    expectIndexMatchesAWalk(original, "source of the assignment");

    // Moved: the objects and their index travel together.
    GameState moved = std::move(copy);
    expectIndexMatchesAWalk(moved, "moved-to");
    moved.getObject(GameObjectId::fromRaw(7)).location = BattlefieldLocation{0};
    moved.getObject(GameObjectId::fromRaw(7)).zone = ZoneType::BattlefieldZone;
    expectIndexMatchesAWalk(moved, "moved-to, then an object moved");
    GameState target = scatteredState(5);
    target = std::move(moved);
    target.getObject(GameObjectId::fromRaw(7)).location.reset();
    expectIndexMatchesAWalk(target, "move-assigned, then an object moved");
    std::swap(target, original);
    target.getObject(GameObjectId::fromRaw(1)).location = BattlefieldLocation{1};
    original.getObject(GameObjectId::fromRaw(1)).location = BattlefieldLocation{2};
    expectIndexMatchesAWalk(target, "swapped (a)");
    expectIndexMatchesAWalk(original, "swapped (b)");
}

TEST(PerfEngineTable, QueriesBetweenTwoMovesShareOneWalk) {
    GameState st = scatteredState(60);
    (void)st.unitsAt(LocationId{BattlefieldLocation{0}}, PlayerId::Player1);
    const std::size_t built = st.objects.indexRebuilds();
    for (int i = 0; i < 50; ++i) {
        (void)st.unitsAt(LocationId{BattlefieldLocation{BattlefieldId::fromRaw(i % 3)}}, PlayerId::Player2);
        (void)st.allUnitsControlledBy(PlayerId::Player1);
        (void)st.runesInBase(PlayerId::Player2);
        st.getObject(GameObjectId::fromRaw(4)).is_exhausted = !st.getObject(GameObjectId::fromRaw(4)).is_exhausted;   // not a move
        st.getObject(GameObjectId::fromRaw(4)).controller = PlayerId::Player1;
    }
    EXPECT_EQ(st.objects.indexRebuilds(), built) << "150 queries, no object moved";
    // Writing the value a field already has is not a move either.
    st.getObject(GameObjectId::fromRaw(2)).location = st.getObject(GameObjectId::fromRaw(2)).location;
    st.getObject(GameObjectId::fromRaw(2)).zone = st.getObject(GameObjectId::fromRaw(2)).zone.get();
    (void)st.objects.onBoard();
    EXPECT_EQ(st.objects.indexRebuilds(), built);
    st.getObject(GameObjectId::fromRaw(2)).location = BattlefieldLocation{1};
    (void)st.objects.onBoard();
    (void)st.objects.inPlay();
    EXPECT_EQ(st.objects.indexRebuilds(), built + 1);
}

// A range is the board as it was when it was taken. Moving an object while
// one is being walked does not invalidate it (the lists it views are kept
// until it is done); a checked build (-DRIFTBOUND_CHECK_BOARD_INDEX=ON) makes
// asking the board again inside such a loop an error instead.
TEST(PerfEngineTable, ARangeBeingWalkedSurvivesAMove) {
    GameState st = scatteredState(40);
    const auto before = Walk::ids(st.objects.onBoard());
    ASSERT_GE(before.size(), 6u);
    std::vector<GameObjectId> walked;
#ifdef RIFTBOUND_CHECK_BOARD_INDEX
    bool threw = false;
    try {
        for (auto& [id, obj] : st.objects.onBoard()) {
            obj.location.reset();
            (void)st.allUnitsControlledBy(PlayerId::Player1);
        }
    } catch (const std::logic_error&) {
        threw = true;
    }
    EXPECT_TRUE(threw) << "a checked build reports the loop";
    (void)walked;
#else
    for (auto& [id, obj] : st.objects.onBoard()) {
        walked.push_back(id);
        obj.location.reset();                                  // off the board
        EXPECT_TRUE(st.allUnitsControlledBy(obj.controller).size() < before.size());
    }
    EXPECT_EQ(walked, before) << "the walk saw the board as it was when it began";
    EXPECT_TRUE(st.objects.onBoard().empty());
#endif
    expectIndexMatchesAWalk(st, "after the loop");
}

// 6.10 — over whole games: at every decision of three random games every
// board query answers what a walk over all objects answers, in that order.
TEST_F(PerfEngineGame, BoardQueriesMatchAWalkAtEveryDecisionOfAGame) {
    int decisions = 0;
    for (const uint64_t seed : {2u, 9u, 20261010u}) {
        Game g(seed, seed * 3 + 5);
        do {
            expectIndexMatchesAWalk(g.engine->state(),
                                    "seed " + std::to_string(seed) + ", decision " +
                                        std::to_string(g.applied));
            ++decisions;
            if (::testing::Test::HasFailure()) return;
        } while (g.advance(1));
    }
    EXPECT_GT(decisions, 300);
}

// ─── 6.3: trigger dispatch ──────────────────────────────────────────────────

// Card::firesOn answers from a set built once per card; CardRegistry::get is a
// table lookup. For every registered card both say what the definitions they
// replaced say.
TEST_F(PerfEngineCards, EveryCardFiresOnExactlyItsTriggerTypes) {
    int cards = 0, with_trigger = 0;
    for (const CardDefId id : card_registry.classDefIds()) {
        Card* card = card_registry.get(id);
        ASSERT_NE(card, nullptr) << id;
        EXPECT_EQ(card->def().id, id);
        ++cards;
        const std::vector<TriggerType> types = card->triggerTypes();
        if (!types.empty()) ++with_trigger;
        for (int t = 0; t < 256; ++t) {
            const auto type = static_cast<TriggerType>(t);
            const bool listed = std::find(types.begin(), types.end(), type) != types.end();
            ASSERT_EQ(card->firesOn(type), listed) << card->def().name << ", trigger " << t;
        }
    }
    EXPECT_GT(cards, 900);
    EXPECT_GT(with_trigger, 300);
    // Ids nobody registered, small and huge.
    EXPECT_EQ(card_registry.get(CardDefId::fromRaw(0)), nullptr);
    EXPECT_EQ(card_registry.get(2047), nullptr);   // spare range: no set is planned there
    EXPECT_EQ(card_registry.get(70000), nullptr);
    EXPECT_EQ(card_registry.get(kInvalidId), nullptr);
    EXPECT_FALSE(card_registry.has(kInvalidId));
}

// A card object that was never registered (a test driving it directly)
// answers the same way.
TEST(PerfEngineFacts, AnUnregisteredCardAnswersFromItsOwnDeclarations) {
    TCountingElder lone;
    EXPECT_FALSE(lone.firesOn(TriggerType::WhenIDie));
    EXPECT_TRUE(lone.abilities().empty());
    const TCountingElder copy = lone;   // a copy shares what was built
    EXPECT_FALSE(copy.firesOn(TriggerType::WhenIDie));
}

// ObjectStateChangedEvent's key refers to its literal: it compares by text.
TEST(PerfEngineFacts, AStateChangeKeyComparesByItsText) {
    const ObjectStateChangedEvent e{GameObjectId::fromRaw(7), "chosen_by_controller"};
    EXPECT_TRUE(e.what_changed == "chosen_by_controller");
    EXPECT_FALSE(e.what_changed == "chosen_by_controlle");
    EXPECT_TRUE(e.what_changed != "buffed");
    EXPECT_EQ(e.what_changed.str(), "chosen_by_controller");
    const ObjectStateChangedEvent copied = e;   // what a held emission keeps
    EXPECT_TRUE(copied.what_changed == std::string("chosen_by_controller"));
    static_assert(sizeof(StateChangeKey) <= 2 * sizeof(void*), "a key is a pointer and a length");
}

// ─── 6.2: the aura pass ─────────────────────────────────────────────────────

// recalculateAuras walks every object once to clear what the last pass left
// (it was three walks: the clear, the "can't be chosen" reset, and the fold's
// own reset), and the rest of the pass walks the board. What a pass leaves on
// a card that is no longer in play must still be cleared, and a card off the
// board that a hook gives an effect to must still have it folded.
TEST_F(PerfEngineCards, AnAuraPassClearsWhatTheLastOneLeftOnACardOffTheBoard) {
    const GameObjectId gone = addUnit(P1, kInvalidId, 3);
    const GameObjectId stays = addUnit(P1, kInvalidId, 3);
    for (GameObjectId id : {gone, stays}) {
        GameObject::AuraEffect ae;
        ae.source = stays;
        ae.might_bonus = 2;
        ae.keyword = Keyword::Tank;
        o(id).aura_effects.push_back(ae);
        o(id).aura_might_bonus = 2;
        o(id).aura_keywords.set(Keyword::Tank);
        o(id).aura_no_combat_damage = true;
        o(id).aura_bonus_damage_taken = 1;
        o(id).untargetable_by_enemy = true;
        o(id).empowered = 1;
    }
    // One of them leaves the board (returned to hand) before the next pass.
    withEngine([&](GameEngine& e) { e.testHook_executor().bounceToHand(gone); });
    ASSERT_FALSE(o(gone).location.has_value());
    // A hook may still give an effect to a card that is not in play (Jax:
    // Equipment in a hand): put one back by hand; the pass must clear it.
    {
        GameObject::AuraEffect ae;
        ae.source = stays;
        ae.might_bonus = 2;
        ae.keyword = Keyword::Tank;
        o(gone).aura_effects.push_back(ae);
        o(gone).aura_might_bonus = 2;
        o(gone).aura_keywords.set(Keyword::Tank);
        o(gone).aura_no_combat_damage = true;
        o(gone).aura_bonus_damage_taken = 1;
        o(gone).untargetable_by_enemy = true;
    }
    withEngine([](GameEngine& e) { e.testHook_cleanup(); });
    for (GameObjectId id : {gone, stays}) {
        EXPECT_TRUE(o(id).aura_effects.empty()) << id;
        EXPECT_EQ(o(id).aura_might_bonus, 0) << id;
        EXPECT_FALSE(o(id).aura_keywords.has(Keyword::Tank)) << id;
        EXPECT_FALSE(o(id).aura_no_combat_damage) << id;
        EXPECT_EQ(o(id).aura_bonus_damage_taken, 0) << id;
        EXPECT_FALSE(o(id).untargetable_by_enemy) << id;
    }
    EXPECT_EQ(o(gone).empowered, 0) << "Empowered does not survive leaving the board";
    EXPECT_EQ(o(stays).empowered, 1);
    EXPECT_EQ(o(stays).current_might, 3) << "Might recomputed without the stale bonus";
}

// ─── 6.5: legal-action generation ───────────────────────────────────────────

// Card::abilities() is activatedAbilities() built once. For every registered
// card it holds the descriptors a fresh call returns.
TEST_F(PerfEngineCards, EveryCardsCachedAbilitiesAreItsActivatedAbilities) {
    int with_abilities = 0;
    for (const CardDefId id : card_registry.classDefIds()) {
        const Card* card = card_registry.get(id);
        const std::vector<ActivatedAbility> fresh = card->activatedAbilities();
        const std::vector<ActivatedAbility>& cached = card->abilities();
        ASSERT_EQ(cached.size(), fresh.size()) << card->def().name;
        EXPECT_EQ(&cached, &card->abilities()) << "one vector per card, not one per call";
        if (!fresh.empty()) ++with_abilities;
        for (size_t i = 0; i < fresh.size(); ++i) {
            const auto& a = cached[i];
            const auto& b = fresh[i];
            SCOPED_TRACE(card->def().name + " ability " + std::to_string(i));
            EXPECT_EQ(a.is_action, b.is_action);
            EXPECT_EQ(a.is_reaction, b.is_reaction);
            EXPECT_EQ(a.is_empower, b.is_empower);
            EXPECT_EQ(a.needs_activation_time_target, b.needs_activation_time_target);
            EXPECT_EQ(a.cost.energy, b.cost.energy);
            EXPECT_EQ(a.cost.power, b.cost.power);
            EXPECT_EQ(a.cost.exhaust, b.cost.exhaust);
            EXPECT_EQ(a.cost.xp_cost, b.cost.xp_cost);
            EXPECT_EQ(a.cost.discard_count, b.cost.discard_count);
            EXPECT_EQ(a.targets.count, b.targets.count);
            EXPECT_EQ(a.targets.optional, b.targets.optional);
            EXPECT_EQ(a.targets.must_be_unit, b.targets.must_be_unit);
            EXPECT_EQ(a.targets.must_be_enemy, b.targets.must_be_enemy);
            EXPECT_EQ(a.targets.must_be_friendly, b.targets.must_be_friendly);
        }
        EXPECT_EQ(card->empowerAbilityIndex() >= 0,
                  std::any_of(fresh.begin(), fresh.end(),
                              [](const ActivatedAbility& a) { return a.is_empower; }));
    }
    EXPECT_GT(with_abilities, 100);
}

// The generators read the board through the index. The options they offer at
// every decision of a game are what a second engine, rebuilt from the same
// state, offers — and the order is the table's, so a fixed seed still plays
// the same game (the smoke split and the observation checksum pin that; this
// pins that an option list does not depend on when the index was last built).
TEST_F(PerfEngineGame, TheOptionsOfADecisionDoNotDependOnWhenTheIndexWasBuilt) {
    int compared = 0;
    for (const uint64_t seed : {4u, 17u}) {
        Game g(seed, seed + 9);
        do {
            const std::vector<Intent> offered = g.engine->currentStep().legal;
            if (auto again = g.engine->recomputePendingLegalActions()) {
                // A copy of the state has an index of its own, built afresh.
                ASSERT_EQ(again->size(), offered.size()) << "seed " << seed << ", decision " << g.applied;
                for (size_t i = 0; i < offered.size(); ++i)
                    ASSERT_TRUE((*again)[i] == offered[i]) << "option " << i;
                ++compared;
            }
        } while (g.advance(1));
    }
    EXPECT_GT(compared, 100);
}

// ─── 9.JJ1: knowledge::prune ────────────────────────────────────────────────

// prune runs before every decision. It no longer builds a table of every
// hidden card when nobody knows anything, and looks cards up in a sorted list
// when somebody does. What it drops and keeps is as before.
TEST(PerfEngineKnowledge, PruneDropsAndKeepsWhatItDid) {
    GameState st;
    st.players[0].id = PlayerId::Player1;
    st.players[1].id = PlayerId::Player2;
    auto card = [&](PlayerId owner, ZoneType zone) {
        const GameObjectId id = st.createObject();
        st.getObject(id).owner = st.getObject(id).controller = owner;
        st.getObject(id).zone = zone;
        return id;
    };
    const PlayerId p1 = PlayerId::Player1;
    // Nothing known: nothing to do, whatever the zones hold.
    const GameObjectId in_hand = card(p1, ZoneType::Hand);
    const GameObjectId in_deck = card(p1, ZoneType::MainDeck);
    const GameObjectId in_trash = card(p1, ZoneType::Trash);
    const GameObjectId nowhere = card(p1, ZoneType::Banishment);
    st.players[0].hand = {in_hand};
    st.players[0].main_deck = {in_deck};
    st.players[0].trash = {in_trash};
    knowledge::prune(st);
    EXPECT_TRUE(st.knowledge.facts[0].empty());
    EXPECT_TRUE(st.knowledge.facts[1].empty());
    EXPECT_TRUE(st.knowledge.groups.empty());

    // The opponent's facts: a card still in the hand (kept), one in the deck
    // (kept), one that went to the trash (dropped: it is public), one that is
    // in no hidden zone any more (dropped).
    auto& theirs = st.knowledge.facts[1];
    HiddenCardFact hand_fact;
    hand_fact.zone = ZoneType::Hand;
    theirs[in_hand] = hand_fact;
    theirs[in_deck] = HiddenCardFact{};
    theirs[in_trash] = hand_fact;
    theirs[nowhere] = hand_fact;
    // A card being looked at ("held") that has since gone to the trash.
    HiddenCardFact held;
    held.held = true;
    const GameObjectId looked_at = card(p1, ZoneType::Trash);
    st.players[0].trash.push_back(looked_at);
    theirs[looked_at] = held;
    // The owner's own facts: about a card in the own hand (dropped: simply
    // known) and in the own deck (kept: a known position).
    auto& own = st.knowledge.facts[0];
    own[in_hand] = hand_fact;
    own[in_deck] = HiddenCardFact{};
    // A group: only its members still in a deck stay.
    DeckGroup group;
    group.owner = p1;
    group.members = {in_hand, in_deck, in_trash};
    st.knowledge.groups[5] = group;
    DeckGroup gone;
    gone.owner = p1;
    gone.members = {in_trash, nowhere};
    st.knowledge.groups[6] = gone;

    knowledge::prune(st);
    EXPECT_EQ(theirs.count(in_hand), 1u);
    EXPECT_EQ(theirs.count(in_deck), 1u);
    EXPECT_EQ(theirs.count(in_trash), 0u);
    EXPECT_EQ(theirs.count(nowhere), 0u);
    EXPECT_EQ(theirs.count(looked_at), 0u);
    EXPECT_EQ(own.count(in_hand), 0u);
    EXPECT_EQ(own.count(in_deck), 1u);
    ASSERT_EQ(st.knowledge.groups.count(5), 1u);
    EXPECT_EQ(st.knowledge.groups[5].members, std::vector<GameObjectId>{in_deck});
    EXPECT_EQ(st.knowledge.groups.count(6), 0u) << "a group with no member left in a deck is gone";

    // Pruning what is already pruned changes nothing.
    const auto theirs_before = theirs.size();
    const auto own_before = own.size();
    knowledge::prune(st);
    EXPECT_EQ(theirs.size(), theirs_before);
    EXPECT_EQ(own.size(), own_before);
    EXPECT_EQ(st.knowledge.groups.size(), 1u);
}

// ─── 9.KK4: a const state may be queried from several threads ───────────────

// Four threads ask a const state the board at the same moment, right after an
// object moved (so the lists are stale and the first query has to rebuild
// them), four hundred times over. Every answer must be the walk's. Without
// the guarded rebuild two threads refill the same lists at once: wrong
// answers, or a crash.
TEST(PerfEngineTable, AConstStateCanBeQueriedFromSeveralThreadsAtOnce) {
    GameState state = scatteredState(120);
    const GameState& shared = state;
    constexpr int kThreads = 4;
    std::atomic<long> wrong{0};
    std::atomic<long> asked{0};
    for (int round = 0; round < 400; ++round) {
        // A move by the only writer, before the readers start.
        const GameObjectId mover = GameObjectId::fromRaw(1 + round % 100);
        if (state.getObject(mover).location.has_value()) state.getObject(mover).location.reset();
        else state.getObject(mover).location = BattlefieldLocation{BattlefieldId::fromRaw(round % 3)};
        const auto board = Walk::where(shared, [](const GameObject& o) { return o.location.has_value(); });
        const LocationId here{BattlefieldLocation{BattlefieldId::fromRaw(round % 3)}};
        const auto units_here = Walk::where(shared, [&](const GameObject& o) {
            return o.isUnit() && o.controller == PlayerId::Player1 && o.location.has_value() &&
                   *o.location == here;
        });
        const std::size_t built = shared.objects.indexRebuilds();
        std::atomic<bool> go{false};
        std::vector<std::thread> readers;
        for (int t = 0; t < kThreads; ++t)
            readers.emplace_back([&] {
                while (!go.load(std::memory_order_acquire)) {}
                for (int i = 0; i < 20; ++i) {
                    if (Walk::ids(shared.objects.onBoard()) != board) ++wrong;
                    if (shared.unitsAt(here, PlayerId::Player1) != units_here) ++wrong;
                    if (shared.allUnitsControlledBy(PlayerId::Player2).size() > board.size()) ++wrong;
                    asked += 3;
                }
            });
        go.store(true, std::memory_order_release);
        for (auto& r : readers) r.join();
        ASSERT_EQ(shared.objects.indexRebuilds(), built + 1)
            << "the stale lists were rebuilt once, by one of the threads (round " << round << ")";
    }
    EXPECT_EQ(wrong.load(), 0);
    EXPECT_EQ(asked.load(), 400L * kThreads * 60);
}

// A table that never held an object answers without building anything.
TEST(PerfEngineTable, AnEmptyTableAnswersWithEmptyRanges) {
    const GameState empty;
    EXPECT_TRUE(empty.objects.onBoard().empty());
    EXPECT_TRUE(empty.objects.inPlay().empty());
    EXPECT_TRUE(empty.objects.atLocation(LocationId{BaseLocation{PlayerId::Player1}}).empty());
    EXPECT_TRUE(empty.unitsAt(LocationId{BattlefieldLocation{0}}, PlayerId::Player1).empty());
    EXPECT_EQ(empty.objects.indexRebuilds(), 0u);
}

// ─── 9.KK2: card code reads the board through the object table's lists ──────

// A source scan: a card file may walk every object of the game
// (`for (... : state.objects)`) only where it has to see a zone the board
// lists do not hold, and then it says so. 344 such walks in src/cards were
// walks over the board in disguise (they skipped everything without a
// location first); they now name the list. The ones below are the walks that
// stay, each with its reason. A new card that walks every object fails here:
// use state.objects.onBoard() / inPlay() / atLocation(loc) (CLAUDE.md, "Object
// table and board index"), or add the loop to this list with its reason and
// the "// (Every object: ...)" comment above it.
TEST(PerfEngineSources, CardCodeWalksEveryObjectOnlyWhereItIsListed) {
    const std::map<std::string, std::pair<int, std::string>> allowed = {
        {"card.cpp", {1, "costStateFingerprint digests cards in every zone"}},
        {"gear/0101_mushroom_pouch.cpp", {1, "facedown cards"}},
        {"gear/0181_pack_of_wonders.cpp", {1, "the board and facedown cards"}},
        {"gear/0365_brutalizer.cpp", {1, "its own instances by card id; clears a counter off the board"}},
        {"gear/0509_shurelya_s_requiem.cpp", {1, "its own instances by card id"}},
        {"gear/0710_cursed_sarcophagus.cpp", {1, "its own instances by card id"}},
        {"gear/0860_jagged_cutlass.cpp", {1, "its own instances by card id"}},
        {"spells/0576_monster_harpoon.cpp", {1, "facedown cards"}},
        {"units/0189_kayn_unleashed.cpp", {1, "its own instances by card id"}},
        {"units/0362_royal_entourage.cpp", {1, "Legends (the Legend Zone)"}},
        {"units/0377_jax_unmatched.cpp", {1, "Equipment you own in any zone, the hand included"}},
        {"units/0432_fiora_peerless.cpp", {1, "its own instances by card id; clears a counter off the board"}},
        {"units/0716_crimson_pigeons.cpp", {2, "its own instances by card id; combat designations"}},
        {"battlefields/0764_black_flame_altar.cpp", {1, "battlefield cards, which have no location"}},
    };
    const char* root_env = std::getenv("RIFTBOUND_ROOT");
    const std::filesystem::path cards = std::filesystem::path(root_env ? root_env : "..") / "src" / "cards";
    ASSERT_TRUE(std::filesystem::is_directory(cards)) << cards;
    // `for (<anything> : <something>objects)` — a range-for over the table
    // itself, not over one of its lists (those end in `()`).
    const std::regex full_walk(R"(for \([^;()]*:\s*[A-Za-z_][A-Za-z0-9_.>()-]*\bobjects\))");
    std::map<std::string, int> found;
    int files = 0;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(cards)) {
        const auto ext = entry.path().extension();
        if (!entry.is_regular_file() || (ext != ".cpp" && ext != ".h")) continue;
        ++files;
        std::ifstream in(entry.path());
        std::string line, previous;
        const std::string name = std::filesystem::relative(entry.path(), cards).generic_string();
        while (std::getline(in, line)) {
            const auto comment = line.find("//");
            const std::string code = line.substr(0, comment);
            if (std::regex_search(code, full_walk)) {
                ++found[name];
                EXPECT_NE(previous.find("// (Every object:"), std::string::npos)
                    << name << ": a walk over every object without the comment that says "
                       "which zone it has to see:\n" << line;
            }
            if (line.find_first_not_of(" \t") != std::string::npos) previous = line;
        }
    }
    EXPECT_GT(files, 900);
    for (const auto& [name, count] : found) {
        const auto it = allowed.find(name);
        if (it == allowed.end()) {
            ADD_FAILURE() << "src/cards/" << name << " walks every object of the game " << count
                          << " time(s). Use state.objects.onBoard() / inPlay() / atLocation(loc) "
                             "if it only looks at the board; otherwise list it here with its reason.";
            continue;
        }
        EXPECT_EQ(count, it->second.first) << "src/cards/" << name << " (" << it->second.second << ")";
    }
    for (const auto& [name, why] : allowed)
        EXPECT_EQ(found.count(name), 1u) << "src/cards/" << name << " no longer walks every object ("
                                         << why.second << "): take it off the list";
}

// ─── 9.LL1: cards that looked at objects off the board ──────────────────────

class PerfEngineOffBoard : public PerfEngineCards {
protected:
    static constexpr CardDefId kMonch = 597;
    static constexpr CardDefId kEmperorOfTheSands = 512;
    static constexpr CardDefId kPurifier = 502;
    static constexpr CardDefId kLongSword = 345;   // an Equipment

    GameObjectId addLegend(PlayerId owner, CardDefId def_id) {
        const GameObjectId id = state.createObject();
        GameObject& l = o(id);
        l.owner = l.controller = owner;
        l.card_def_id = def_id;
        l.card_type = CardType::Legend;
        l.name = card_db.get(def_id).name;
        l.zone = ZoneType::LegendZone;
        state.player(owner).legend_zone = id;
        return id;
    }
    GameObjectId addEquipment(PlayerId owner, CardDefId def_id) {
        const GameObjectId id = state.createObject();
        GameObject& g = o(id);
        g.owner = g.controller = owner;
        g.card_def_id = def_id;
        g.card_type = CardType::Gear;
        const auto& def = card_db.get(def_id);
        g.name = def.name;
        g.tags = def.tags;
        g.keywords = def.keywords;
        g.might_bonus = def.might_bonus;
        g.zone = ZoneType::Base;
        g.location = BaseLocation{owner};
        return id;
    }
    void cleanup() { withEngine([](GameEngine& e) { e.testHook_cleanup(); }); }
    bool hasAuraKeyword(GameObjectId id, Keyword kw) {
        for (const auto& ae : o(id).aura_effects)
            if (ae.keyword == kw) return true;
        return false;
    }
};

// CR 124.1: a Game Object that changes zones to a Non-Board Zone is a new
// object and "all Temporary Modifications of all kinds cease to be tracked on
// it"; CR 424.1 stuns "Units on the Board". A unit that is stunned and then
// returned to its owner's hand is not a stunned unit any more — it kept the
// status until the Ending Step, and Monch ("If an opponent controls a stunned
// unit, I cost [2] less and enter ready") counted it.
TEST_F(PerfEngineOffBoard, AStunnedUnitThatLeavesTheBoardIsNoLongerStunned) {
    const GameObjectId bounced = addUnit(P2, kInvalidId, 3);
    const GameObjectId killed = addUnit(P2, kInvalidId, 3, 0);
    const GameObjectId stays = addUnit(P1, kInvalidId, 3);
    const Card* monch = card_registry.get(kMonch);
    ASSERT_NE(monch, nullptr);
    withEngine([&](GameEngine& e) {
        e.testHook_executor().stunUnit(bounced);
        e.testHook_executor().stunUnit(killed);
        e.testHook_executor().stunUnit(stays);
        e.testHook_cleanup();
    });
    ASSERT_TRUE(o(bounced).is_stunned);
    EXPECT_EQ(monch->selfCostReduction(state, P1), 2) << "an opponent controls a stunned unit";
    withEngine([&](GameEngine& e) {
        e.testHook_executor().bounceToHand(bounced);
        e.testHook_executor().killObject(killed);
        e.testHook_cleanup();
    });
    ASSERT_FALSE(o(bounced).location.has_value());
    ASSERT_FALSE(o(killed).location.has_value());
    EXPECT_FALSE(o(bounced).is_stunned) << "the card in the hand is a new object (CR 124.1)";
    EXPECT_FALSE(o(killed).is_stunned);
    EXPECT_TRUE(o(stays).is_stunned) << "a unit still on the board stays stunned";
    EXPECT_EQ(monch->selfCostReduction(state, P1), 0)
        << "the opponent controls no stunned unit: the only ones left the board";
    EXPECT_EQ(monch->selfCostReduction(state, P2), 2) << "P1's unit on the board is still stunned";
}

// A unit that is on the board and stunned still counts, and one played again
// after leaving does not come back stunned.
TEST_F(PerfEngineOffBoard, MonchLooksAtTheBoardOnly) {
    const Card* monch = card_registry.get(kMonch);
    const GameObjectId unit = addUnit(P2, kInvalidId, 3);
    // Stunned set by hand on a card in a hand (no engine path leaves it so
    // after a cleanup; this is the walk itself): not "a stunned unit".
    o(unit).location = std::nullopt;
    o(unit).zone = ZoneType::Hand;
    o(unit).is_stunned = true;
    EXPECT_EQ(monch->selfCostReduction(state, P1), 0);
    o(unit).location = BaseLocation{P2};
    o(unit).zone = ZoneType::Base;
    EXPECT_EQ(monch->selfCostReduction(state, P1), 2);
}

// Emperor of the Sands: "Your Sand Soldiers have [Weaponmaster]." A Sand
// Soldier is a token; off the board it has ceased to exist (CR 186.1), and
// the object the engine keeps for it is not one of "your Sand Soldiers".
TEST_F(PerfEngineOffBoard, EmperorOfTheSandsGivesNothingToASandSoldierThatHasCeasedToExist) {
    addLegend(P1, kEmperorOfTheSands);
    auto soldier = [&](int at_bf) {
        const GameObjectId id = addUnit(P1, kInvalidId, 2, at_bf);
        o(id).name = "Sand Soldier";
        o(id).is_token = true;
        return id;
    };
    const GameObjectId alive = soldier(0);
    const GameObjectId dead = soldier(-1);
    cleanup();
    ASSERT_TRUE(hasAuraKeyword(alive, Keyword::Weaponmaster));
    ASSERT_TRUE(hasAuraKeyword(dead, Keyword::Weaponmaster));
    withEngine([&](GameEngine& e) {
        e.testHook_executor().killObject(dead);
        e.testHook_cleanup();
    });
    ASSERT_FALSE(o(dead).location.has_value());
    EXPECT_TRUE(hasAuraKeyword(alive, Keyword::Weaponmaster));
    EXPECT_TRUE(o(alive).hasKeyword(Keyword::Weaponmaster));
    EXPECT_TRUE(o(dead).aura_effects.empty()) << "nothing is given to a token that is gone";
    EXPECT_FALSE(o(dead).hasKeyword(Keyword::Weaponmaster));
}

// Purifier: "Your Equipment each give [Assault]." An Equipment that leaves
// the board comes off its unit on the way (EffectExecutor::unattachGear, CR
// 719.5 / 124.1), by every route, so no Equipment off the board is still
// "attached": the unit loses the [Assault] with it.
TEST_F(PerfEngineOffBoard, PurifierGivesAssaultOnlyThroughEquipmentOnTheBoard) {
    addLegend(P1, kPurifier);
    const GameObjectId bearer_a = addUnit(P1, kInvalidId, 3);
    const GameObjectId bearer_b = addUnit(P1, kInvalidId, 3);
    const GameObjectId bearer_c = addUnit(P1, kInvalidId, 3);
    const GameObjectId sword_a = addEquipment(P1, kLongSword);
    const GameObjectId sword_b = addEquipment(P1, kLongSword);
    const GameObjectId sword_c = addEquipment(P1, kLongSword);
    withEngine([&](GameEngine& e) {
        ASSERT_TRUE(e.testHook_executor().attachGear(sword_a, bearer_a));
        ASSERT_TRUE(e.testHook_executor().attachGear(sword_b, bearer_b));
        ASSERT_TRUE(e.testHook_executor().attachGear(sword_c, bearer_c));
        e.testHook_cleanup();
    });
    for (GameObjectId u : {bearer_a, bearer_b, bearer_c})
        ASSERT_TRUE(hasAuraKeyword(u, Keyword::Assault)) << u;
    // One Equipment is returned to hand, one is killed; the third stays.
    withEngine([&](GameEngine& e) {
        e.testHook_executor().bounceToHand(sword_a);
        e.testHook_executor().killObject(sword_b);
        e.testHook_cleanup();
    });
    ASSERT_FALSE(o(sword_a).location.has_value());
    ASSERT_FALSE(o(sword_b).location.has_value());
    EXPECT_FALSE(o(sword_a).attached_to.has_value()) << "an Equipment in a hand is attached to nothing";
    EXPECT_FALSE(o(sword_b).attached_to.has_value());
    EXPECT_TRUE(o(bearer_a).attachments.empty());
    EXPECT_FALSE(hasAuraKeyword(bearer_a, Keyword::Assault));
    EXPECT_FALSE(hasAuraKeyword(bearer_b, Keyword::Assault));
    EXPECT_TRUE(hasAuraKeyword(bearer_c, Keyword::Assault));
    // And if its unit leaves, the Equipment stays on the board unattached
    // (CR 719.5) and gives nothing to the card that left.
    withEngine([&](GameEngine& e) {
        e.testHook_executor().bounceToHand(bearer_c);
        e.testHook_cleanup();
    });
    EXPECT_TRUE(o(sword_c).location.has_value());
    EXPECT_FALSE(o(sword_c).attached_to.has_value());
    EXPECT_TRUE(o(bearer_c).aura_effects.empty());
}

// ─── 9.LL4: leaving the board, by every path, makes a new object ────────────

/// One fixture per way off the board. Each test dirties an object with every
/// kind of board state, takes it off the board by ONE path and looks at it
/// at once — no cleanup, no aura pass in between.
class PerfEngineLeaveBoard : public PerfEngineOffBoard {
protected:
    static constexpr CardDefId kShipyardSkulker = 175;   // unit, 3 [M], no text

    /// A unit of P2's that P1 controls until end of turn, with every status,
    /// counter and "this turn" modification on it and an Equipment attached.
    GameObjectId dirtyUnit(bool token = false, GameObjectId* gear_out = nullptr) {
        const GameObjectId id = addUnit(P2, token ? kInvalidId : kShipyardSkulker, token ? 5 : 0, 0);
        const GameObjectId sword = addEquipment(P1, kLongSword);
        o(sword).location = o(id).location;
        o(sword).zone = ZoneType::BattlefieldZone;
        withEngine([&](GameEngine& e) { ASSERT_TRUE(e.testHook_executor().attachGear(sword, id)); });
        GameObject& u = o(id);
        if (token) { u.is_token = true; u.name = "Test Token"; }
        u.controller = P1;                       // taken ...
        u.control_reverts_eot = true;            // ... until end of turn
        u.original_controller_eot = P2;
        u.is_exhausted = true;
        u.is_stunned = true;
        u.damage_marked = 1;
        u.damage_marked_by[0] = 1;
        u.damage_taken_this_turn = 1;
        u.buff_count = 1;
        u.empowered = 1;
        u.temp_might_bonus = 2;
        u.temp_keywords.set(Keyword::Ganking);
        u.keywords.set(Keyword::Ganking);
        u.temp_assault_value = 2;
        u.assault_value += 2;
        u.combat_designation = CombatDesignation::Attacker;
        u.cant_move_this_turn = true;
        u.untargetable_by_enemy_this_turn = true;
        u.banish_if_would_die_this_turn = false;
        u.moves_this_turn = 2;
        u.recomputeMight();
        if (gear_out) *gear_out = sword;
        return id;
    }
    /// What CR 124.1 leaves of it.
    void expectNewObject(GameObjectId id, const char* path, bool has_card = true) {
        SCOPED_TRACE(path);
        const GameObject& u = o(id);
        EXPECT_FALSE(u.location.has_value());
        EXPECT_FALSE(carriesBoardState(u)) << "something of the board is still on it";
        EXPECT_FALSE(u.is_stunned) << "Stunned";
        EXPECT_FALSE(u.is_exhausted) << "Exhausted";
        EXPECT_EQ(u.damage_marked, 0) << "damage";
        EXPECT_EQ(u.damage_marked_by[0], 0);
        EXPECT_EQ(u.damage_taken_this_turn, 0);
        EXPECT_EQ(u.buff_count, 0) << "Buffed";
        EXPECT_EQ(u.empowered, 0) << "Empowered";
        EXPECT_EQ(u.temp_might_bonus, 0) << "this-turn Might";
        EXPECT_FALSE(u.keywords.has(Keyword::Ganking)) << "a keyword given this turn";
        EXPECT_EQ(u.temp_keywords.bits, 0u);
        EXPECT_EQ(u.temp_assault_value, 0);
        EXPECT_EQ(u.combat_designation, CombatDesignation::None) << "Attacking";
        EXPECT_FALSE(u.cant_move_this_turn);
        EXPECT_FALSE(u.untargetable_by_enemy_this_turn);
        EXPECT_EQ(u.moves_this_turn, 0);
        EXPECT_FALSE(u.control_reverts_eot) << "Controlled";
        EXPECT_EQ(u.controller, u.owner) << "off the board a card is its owner's";
        EXPECT_TRUE(u.attachments.empty()) << "Equipped";
        EXPECT_EQ(u.attachment_might_bonus, 0);
        EXPECT_EQ(u.current_might, u.base_might) << "printed Might off the board (CR 711)";
        if (has_card) {
            const auto& def = card_db.get(u.card_def_id);
            EXPECT_EQ(u.keywords.bits, def.keywords.bits);
            EXPECT_EQ(u.assault_value, def.assault_value);
        }
    }
    void expectGearLeftBehind(GameObjectId gear) {
        EXPECT_TRUE(o(gear).location.has_value()) << "the Equipment stays on the board (CR 719.5)";
        EXPECT_FALSE(o(gear).attached_to.has_value());
    }
};

TEST_F(PerfEngineLeaveBoard, KilledByAnEffect) {
    GameObjectId gear;
    const GameObjectId u = dirtyUnit(false, &gear);
    withEngine([&](GameEngine& e) { e.testHook_executor().killObject(u); });
    EXPECT_EQ(o(u).zone, ZoneType::Trash);
    expectNewObject(u, "EffectExecutor::killObject (unit)");
    expectGearLeftBehind(gear);
    // What "When I die" abilities read is kept: the unit as it died.
    EXPECT_TRUE(o(u).last_known.valid);
    EXPECT_TRUE(o(u).last_known.stunned);
    EXPECT_EQ(o(u).last_known.buff_count, 1);
    EXPECT_EQ(o(u).last_known.empowered, 1);
    EXPECT_GT(o(u).last_known.might, o(u).base_might);
    EXPECT_TRUE(o(u).last_known.keywords.has(Keyword::Ganking));
    EXPECT_EQ(o(u).last_known.attachments, std::vector<GameObjectId>{gear});
    EXPECT_TRUE(o(u).last_location.has_value());
}

TEST_F(PerfEngineLeaveBoard, KilledByLethalDamage) {
    GameObjectId gear;
    const GameObjectId u = dirtyUnit(false, &gear);
    o(u).damage_marked = 99;
    withEngine([&](GameEngine& e) { e.testHook_processLethalDamage(); });
    EXPECT_EQ(o(u).zone, ZoneType::Trash);
    expectNewObject(u, "GameEngine::killUnit");
    expectGearLeftBehind(gear);
    EXPECT_TRUE(o(u).last_known.valid);
    EXPECT_TRUE(o(u).last_known.stunned);
}

TEST_F(PerfEngineLeaveBoard, AGearKilledByAnEffect) {
    GameObjectId gear;
    const GameObjectId u = dirtyUnit(false, &gear);
    o(gear).is_exhausted = true;
    withEngine([&](GameEngine& e) { e.testHook_executor().killObject(gear); });
    EXPECT_EQ(o(gear).zone, ZoneType::Trash);
    EXPECT_FALSE(carriesBoardState(o(gear)));
    EXPECT_FALSE(o(gear).is_exhausted);
    EXPECT_FALSE(o(gear).attached_to.has_value());
    EXPECT_FALSE(o(gear).is_rules_text_inactive);
    EXPECT_TRUE(o(u).attachments.empty());
}

TEST_F(PerfEngineLeaveBoard, ReturnedToHand) {
    GameObjectId gear;
    const GameObjectId u = dirtyUnit(false, &gear);
    withEngine([&](GameEngine& e) { e.testHook_executor().bounceToHand(u); });
    EXPECT_EQ(o(u).zone, ZoneType::Hand);
    expectNewObject(u, "EffectExecutor::bounceToHand");
    expectGearLeftBehind(gear);
    EXPECT_EQ(state.player(P2).hand, std::vector<GameObjectId>{u}) << "its owner's hand";
}

TEST_F(PerfEngineLeaveBoard, ATokenReturnedToHandCeasesToExist) {
    GameObjectId gear;
    const GameObjectId u = dirtyUnit(true, &gear);
    withEngine([&](GameEngine& e) { e.testHook_executor().bounceToHand(u); });
    EXPECT_EQ(o(u).zone, ZoneType::Banishment);
    expectNewObject(u, "EffectExecutor::bounceToHand (token)", /*has_card=*/false);
    expectGearLeftBehind(gear);
}

TEST_F(PerfEngineLeaveBoard, Banished) {
    GameObjectId gear;
    const GameObjectId u = dirtyUnit(false, &gear);
    withEngine([&](GameEngine& e) { e.testHook_executor().banishObject(u); });
    EXPECT_EQ(o(u).zone, ZoneType::Banishment);
    expectNewObject(u, "EffectExecutor::banishObject");
    expectGearLeftBehind(gear);
}

TEST_F(PerfEngineLeaveBoard, Recycled) {
    GameObjectId gear, token_gear;
    const GameObjectId u = dirtyUnit(false, &gear);
    const GameObjectId t = dirtyUnit(true, &token_gear);
    const GameObjectId rune = addRune(P1, Domain::Calm, /*exhausted=*/true);
    withEngine([&](GameEngine& e) { e.testHook_executor().recycleCards(P1, {u, t, rune}); });
    EXPECT_EQ(o(u).zone, ZoneType::MainDeck);
    expectNewObject(u, "EffectExecutor::recycleCards (card)");
    expectGearLeftBehind(gear);
    EXPECT_EQ(o(t).zone, ZoneType::Banishment);
    expectNewObject(t, "EffectExecutor::recycleCards (token)", /*has_card=*/false);
    EXPECT_EQ(o(rune).zone, ZoneType::RuneDeck);
    EXPECT_FALSE(carriesBoardState(o(rune)));
    EXPECT_FALSE(o(rune).is_exhausted);
}

TEST_F(PerfEngineLeaveBoard, PutIntoItsOwnersDeck) {
    // Keeper's Verdict's move, which the card used to make by hand.
    GameObjectId gear, gear2, gear3;
    const GameObjectId top = dirtyUnit(false, &gear);
    const GameObjectId bottom = dirtyUnit(false, &gear2);
    const GameObjectId token = dirtyUnit(true, &gear3);
    int left = 0;
    auto conn = events.on_left_board.connect([&](const LeftBoardEvent&) { ++left; });
    withEngine([&](GameEngine& e) {
        e.testHook_executor().putIntoDeckFromBoard(top, /*to_top=*/true);
        e.testHook_executor().putIntoDeckFromBoard(bottom, /*to_top=*/false);
        e.testHook_executor().putIntoDeckFromBoard(token, true);
    });
    conn.disconnect();
    EXPECT_EQ(left, 3);
    expectNewObject(top, "EffectExecutor::putIntoDeckFromBoard (top)");
    expectNewObject(bottom, "EffectExecutor::putIntoDeckFromBoard (bottom)");
    expectNewObject(token, "EffectExecutor::putIntoDeckFromBoard (token)", /*has_card=*/false);
    EXPECT_EQ(o(top).zone, ZoneType::MainDeck);
    EXPECT_EQ(o(token).zone, ZoneType::Banishment);
    const auto& deck = state.player(P2).main_deck;   // its owner's
    ASSERT_EQ(deck.size(), 2u);
    EXPECT_EQ(deck.back(), top) << "top = back";
    EXPECT_EQ(deck.front(), bottom);
    expectGearLeftBehind(gear);
    expectGearLeftBehind(gear2);
}

TEST_F(PerfEngineLeaveBoard, ARuneRecycledForPower) {
    const GameObjectId rune = addRune(P1, Domain::Calm, /*exhausted=*/true);
    o(rune).is_stunned = true;          // nothing stuns a rune; the rule is not per type
    o(rune).damage_marked = 1;
    EXPECT_TRUE(resource::pay(state, events, P1, 0, 1, Domain::Calm));
    EXPECT_EQ(o(rune).zone, ZoneType::RuneDeck);
    EXPECT_FALSE(o(rune).location.has_value());
    EXPECT_FALSE(carriesBoardState(o(rune)));
    // The engine's own payment of a card's cost recycles through the same rule.
    const GameObjectId rune2 = addRune(P1, Domain::Calm, /*exhausted=*/true);
    o(rune2).is_stunned = true;
    CardContext* none = nullptr;
    (void)none;
    withEngine([&](GameEngine& e) {
        EXPECT_TRUE(e.testHook_payAdditionalCost(P1, 0, 1, Domain::Calm));
    });
    EXPECT_EQ(o(rune2).zone, ZoneType::RuneDeck);
    EXPECT_FALSE(carriesBoardState(o(rune2)));
}

TEST_F(PerfEngineLeaveBoard, MovedOffTheBoardByTheStateEditor) {
    GameObjectId gear;
    const GameObjectId u = dirtyUnit(false, &gear);
    StateEditor editor;
    ASSERT_TRUE(editor.moveObject(state, u, P2, ZoneType::Hand).ok);
    EXPECT_EQ(o(u).zone, ZoneType::Hand);
    EXPECT_FALSE(carriesBoardState(o(u)));
    EXPECT_FALSE(o(u).is_stunned);
    EXPECT_EQ(o(u).buff_count, 0);
    EXPECT_EQ(o(u).controller, P2);
}

// With no path left that forgets something, an aura pass has nothing to mop
// up off the board: after any of the moves above a cleanup changes nothing on
// the object that left.
TEST_F(PerfEngineLeaveBoard, ACleanupFindsNothingLeftToClear) {
    const GameObjectId u = dirtyUnit();
    withEngine([&](GameEngine& e) { e.testHook_executor().bounceToHand(u); });
    const GameObject before = o(u);
    cleanup();
    const GameObject& after = o(u);
    EXPECT_EQ(after.is_stunned, before.is_stunned);
    EXPECT_EQ(after.empowered, before.empowered);
    EXPECT_EQ(after.buff_count, before.buff_count);
    EXPECT_EQ(after.keywords.bits, before.keywords.bits);
    EXPECT_EQ(after.current_might, before.current_might);
    EXPECT_FALSE(carriesBoardState(after));
}


// ─── 9.MM1: what reads an object after it has left the board ────────────────
//
// Off the board an object is a new one (CR 124.1): its owner's again, with
// its printed Might and no statuses. What an effect or a "When I die" ability
// says about the card AS IT LEFT is read from the snapshot every leave path
// takes first (GameObject::last_known; CR 359.3.e.13 "can look back at its
// characteristics before it changes zones", CR 383.2.c.3 for the abilities of
// the object that left).

constexpr CardDefId kTWitness = 1841;     // unit: "When I die", writes down itself and another unit
constexpr CardDefId kTLookBack = 1842;    // spell: moves a unit off the board, then reads it
constexpr CardDefId kTPowerUnit = 1843;   // unit costing one Calm Power
constexpr CardDefId kTWatcher = 1844;     // unit: "When another friendly unit dies", counts

struct Saw {
    PlayerId ability_controller = PlayerId::None;
    bool valid = false;
    bool died = false;
    PlayerId controller_then = PlayerId::None;   // last_known.controller
    PlayerId controller_now = PlayerId::None;    // the object in its new zone
    int might_then = 0;
    int might_now = 0;
    bool stunned_then = false;
    int buffs_then = 0;
    size_t gear_then = 0;
    std::optional<LocationId> at_then;
    bool listed_with_me = false;                 // in MY last_known.other_friendly_units_here
    bool on_board_now = false;
};
static Saw lookWith(const GameObject::LastKnown& lk, const GameObject& u) {
    Saw s;
    s.valid = lk.valid;
    s.died = lk.died;
    s.controller_then = lk.controller;
    s.controller_now = u.controller;
    s.might_then = lk.might;
    s.might_now = u.current_might;
    s.stunned_then = lk.stunned;
    s.buffs_then = lk.buff_count;
    s.gear_then = lk.attachments.size();
    s.at_then = lk.location;
    s.on_board_now = u.location.has_value();
    return s;
}
static Saw look(const GameState& state, GameObjectId id) {
    return lookWith(state.getObject(id).last_known, state.getObject(id));
}

/// "When I die": writes down what it reads of itself and of `other[me]`.
class TWitness : public UnitCard {
public:
    TWitness() {
        def_.id = kTWitness;
        def_.name = "Test Witness";
        def_.card_type = CardType::Unit;
        def_.might = 2;
    }
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIDie; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        // Itself as THIS death left it: the snapshot that came with the ability.
        Saw me = lookWith(asItLeft(ctx, ctx.source), ctx.state.getObject(ctx.source));
        me.ability_controller = ctx.controller;
        myself[ctx.source] = me;
        in_order.push_back(me);
        controllers_in_order.push_back(controllerWhenItLeft(ctx, ctx.source));
        auto it = other.find(ctx.source);
        if (it == other.end() || !ctx.state.objectExists(it->second)) return;
        Saw them = look(ctx.state, it->second);
        const auto& with_me = ctx.state.getObject(ctx.source).last_known.other_friendly_units_here;
        them.listed_with_me =
            std::find(with_me.begin(), with_me.end(), it->second) != with_me.end();
        the_other[ctx.source] = them;
    }
    static inline std::map<GameObjectId, GameObjectId> other;
    static inline std::map<GameObjectId, Saw> myself, the_other;
    static inline std::vector<Saw> in_order;                    // every resolution, in order
    static inline std::vector<PlayerId> controllers_in_order;
private:
    CardDef def_;
};

/// "When another friendly unit dies": counts, per watcher.
class TWatcher : public UnitCard {
public:
    TWatcher() {
        def_.id = kTWatcher;
        def_.name = "Test Watcher";
        def_.card_type = CardType::Unit;
        def_.might = 2;
    }
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenAFriendlyUnitDies; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        ++resolved[ctx.source];
    }
    static inline std::map<GameObjectId, int> resolved;
private:
    CardDef def_;
};

/// "[Move] a unit [off the board]. Then ...": makes the move, then reads the
/// unit and deals 1 to each unit where it was.
class TLookBack : public SpellCard {
public:
    enum class Move { Kill, ReturnToHand, Banish, Recycle, PutOnDeck };
    TLookBack() {
        def_.id = kTLookBack;
        def_.name = "Test Look Back";
        def_.card_type = CardType::Spell;
    }
    const CardDef& def() const override { return def_; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (targets.empty() || !ctx.state.objectExists(targets[0])) return;
        const GameObjectId unit = targets[0];
        switch (move) {
            case Move::Kill:         ctx.executor.killObject(unit); break;
            case Move::ReturnToHand: ctx.executor.bounceToHand(unit); break;
            case Move::Banish:       ctx.executor.banishObject(unit); break;
            case Move::Recycle:      ctx.executor.recycleCards(ctx.controller, {unit}); break;
            case Move::PutOnDeck:    ctx.executor.putIntoDeckFromBoard(unit, true); break;
        }
        saw = look(ctx.state, unit);
        its_controller = controllerWhenItLeft(ctx.state, unit);
        // "... deal 1 to each unit there."
        if (!saw.at_then.has_value()) return;
        std::vector<GameObjectId> there;
        for (const auto& [id, obj] : ctx.state.objects.atLocation(*saw.at_then))
            if (obj.isUnit()) there.push_back(id);
        for (auto id : there) ctx.executor.dealDamage(id, 1, ctx.source);
    }
    static inline Move move = Move::Kill;
    static inline Saw saw;
    static inline PlayerId its_controller = PlayerId::None;
private:
    CardDef def_;
};

class TPowerUnit : public UnitCard {
public:
    TPowerUnit() {
        def_.id = kTPowerUnit;
        def_.name = "Test Power Unit";
        def_.card_type = CardType::Unit;
        def_.might = 1;
        def_.domains = {Domain::Calm};
        def_.power_cost = 1;
    }
    const CardDef& def() const override { return def_; }
private:
    CardDef def_;
};

class PerfEngineLookBack : public PerfEngineLeaveBoard {
protected:
    static constexpr CardDefId kViktorLeader = 246;
    static constexpr CardDefId kImmortalPhoenix = 37;
    static constexpr CardDefId kKogMaw = 190;
    static constexpr CardDefId kHiddenBlade = 213;
    static constexpr CardDefId kKeepersVerdict = 760;

    void SetUp() override {
        PerfEngineLeaveBoard::SetUp();
        card_registry.registerCard(kTWitness, std::make_unique<TWitness>());
        card_registry.registerCard(kTLookBack, std::make_unique<TLookBack>());
        card_registry.registerCard(kTPowerUnit, std::make_unique<TPowerUnit>());
        card_registry.registerCard(kTWatcher, std::make_unique<TWatcher>());
        card_db.buildFromClasses(card_registry);
        TWitness::other.clear();
        TWitness::myself.clear();
        TWitness::the_other.clear();
        TWitness::in_order.clear();
        TWitness::controllers_in_order.clear();
        TWatcher::resolved.clear();
        TLookBack::saw = Saw{};
        TLookBack::its_controller = PlayerId::None;
    }
    /// P2's unit that P1 has taken, at battlefield 0.
    GameObjectId taken(CardDefId def) {
        const GameObjectId id = addUnit(P2, def, 0, 0);
        o(id).controller = P1;
        return id;
    }
    /// P1's unit at battlefield 0: buffed, stunned, carrying a Long Sword.
    GameObjectId buffedStunnedArmed(CardDefId def, GameObjectId* gear_out = nullptr) {
        const GameObjectId id = addUnit(P1, def, 0, 0);
        const GameObjectId sword = addEquipment(P1, kLongSword);
        o(sword).location = o(id).location;
        o(sword).zone = ZoneType::BattlefieldZone;
        withEngine([&](GameEngine& e) {
            ASSERT_TRUE(e.testHook_executor().attachGear(sword, id));
            e.mutableState().getObject(id).buff_count = 1;
            e.mutableState().getObject(id).is_stunned = true;
            e.testHook_recalculateAuras();
        });
        if (gear_out) *gear_out = sword;
        return id;
    }
    /// Play the look-back spell on `unit` as `caster`, through the engine.
    void lookBack(TLookBack::Move move, GameObjectId unit, PlayerId caster = P1) {
        TLookBack::move = move;
        const GameObjectId spell = addToHand(caster, kTLookBack);
        Intent play;
        play.type = IntentType::PlayCard;
        play.player = caster;
        play.card = spell;
        play.targets = {unit};
        // The caster's play, on whatever turn the test is in (testing phase B8).
        withEngineDefaultTurn([&](GameEngine& e) {
            e.testHook_executeIntent(play);
            settleLikeTheEngine(e);
        }, nullptr, nullptr, caster);
    }
    int recruits(PlayerId p) {
        int n = 0;
        for (const auto& [id, obj] : state.objects.onBoard())
            if (obj.controller == p && obj.isUnit() && obj.name == "Recruit") ++n;
        return n;
    }
};

// "Its controller", of a unit taken from its owner and then killed, is the
// player who controlled it as it died. The dead card is in its OWNER's trash
// and is its owner's again; the snapshot says who had it.
TEST_F(PerfEngineLookBack, ATakenUnitThatIsKilled_ItsControllerIsWhoHadItThen) {
    const GameObjectId u = taken(kShipyardSkulker);
    lookBack(TLookBack::Move::Kill, u, P2);
    ASSERT_EQ(o(u).zone, ZoneType::Trash);
    EXPECT_EQ(o(u).controller, P2) << "the card in the trash is its owner's";
    EXPECT_TRUE(TLookBack::saw.valid);
    EXPECT_TRUE(TLookBack::saw.died);
    EXPECT_EQ(TLookBack::saw.controller_then, P1);
    EXPECT_EQ(TLookBack::saw.controller_now, P2);
    EXPECT_EQ(TLookBack::its_controller, P1) << "controllerWhenItLeft";
}

// Hidden Blade 213, "Kill a unit at a battlefield. Its controller draws 2
// cards.": on a unit P1 took from P2, P1 draws.
TEST_F(PerfEngineLookBack, HiddenBlade_TheControllerOfATakenUnitDraws) {
    const GameObjectId u = taken(kShipyardSkulker);
    for (int i = 0; i < 3; ++i) { addToDeck(P1, kShipyardSkulker); addToDeck(P2, kShipyardSkulker); }
    const GameObjectId blade = addToHand(P2, kHiddenBlade);
    const size_t p1_hand = state.player(P1).hand.size();
    const size_t p2_hand = state.player(P2).hand.size();
    EnginePathAgent picker(EnginePathAgent::choosing(u));
    playThroughEngine(blade, nullptr, std::nullopt, &picker);
    ASSERT_EQ(o(u).zone, ZoneType::Trash);
    EXPECT_EQ(state.player(P1).hand.size(), p1_hand + 2) << "its controller as it died";
    EXPECT_EQ(state.player(P2).hand.size(), p2_hand - 1) << "its owner only played the spell";
}

// Every other way off the board takes the same snapshot: a unit that was
// buffed and then returned to its owner's hand is a 3 [M] card there, and
// "its Might" as it left is what an effect that counts it afterwards reads.
// Before, only a kill took one — so after a return the snapshot was missing,
// or was the one from the last time the unit had died.
TEST_F(PerfEngineLookBack, EveryWayOffTheBoardRecordsTheUnitAsItLeft) {
    using Move = TLookBack::Move;
    for (Move move : {Move::ReturnToHand, Move::Banish, Move::Recycle, Move::PutOnDeck}) {
        SCOPED_TRACE(static_cast<int>(move));
        const GameObjectId u = dirtyUnit();           // P2's, taken by P1, buffed, stunned, armed
        // It has died before (and come back): that snapshot is not this leave's.
        o(u).last_known.valid = true;
        o(u).last_known.died = true;
        o(u).last_known.might = 99;
        o(u).last_known.controller = P2;
        o(u).last_known.stunned = false;
        // Its Might as the game sees it: after a Cleanup (one runs before
        // the spell resolves; it drops the hand-set Attacker designation,
        // CR 323.2.c, and with it the Assault).
        withEngine([&](GameEngine& e) { e.testHook_cleanup(); });
        const int might_on_board = o(u).current_might;
        const auto was_at = o(u).location;
        ASSERT_GT(might_on_board, o(u).base_might);
        // P2 casts on a unit P1 controls: "can't be chosen by enemy spells
        // this turn" would make it no target at all once a Cleanup has
        // applied it (one now runs before the spell resolves).
        o(u).untargetable_by_enemy_this_turn = false;
        lookBack(move, u, P2);
        ASSERT_FALSE(o(u).location.has_value());
        const Saw& saw = TLookBack::saw;
        EXPECT_TRUE(saw.valid);
        EXPECT_FALSE(saw.died) << "it left, it did not die";
        EXPECT_EQ(saw.might_then, might_on_board) << "its Might as it left";
        EXPECT_EQ(saw.might_now, o(u).base_might) << "the card where it is now";
        EXPECT_EQ(saw.controller_then, P1);
        EXPECT_EQ(saw.controller_now, P2);
        EXPECT_TRUE(saw.stunned_then);
        EXPECT_EQ(saw.buffs_then, 1);
        EXPECT_EQ(saw.gear_then, 1u);
        EXPECT_EQ(saw.at_then, was_at);
        EXPECT_EQ(TLookBack::its_controller, P1);
        EXPECT_EQ(o(u).last_known.killed_by, PlayerId::None);
    }
}

// "Kill a unit at a battlefield, then deal 1 to each unit THERE": where it
// was, read after the kill.
TEST_F(PerfEngineLookBack, AUnitKilledAtABattlefield_ThereIsWhereItWas) {
    const GameObjectId victim = addUnit(P2, kShipyardSkulker, 0, 1);
    const GameObjectId beside = addUnit(P2, kShipyardSkulker, 0, 1);
    const GameObjectId elsewhere = addUnit(P2, kShipyardSkulker, 0, 0);
    const GameObjectId at_base = addUnit(P2, kShipyardSkulker, 0, -1);
    lookBack(TLookBack::Move::Kill, victim);
    ASSERT_EQ(o(victim).zone, ZoneType::Trash);
    ASSERT_TRUE(TLookBack::saw.at_then.has_value());
    EXPECT_EQ(*TLookBack::saw.at_then, LocationId{BattlefieldLocation{1}});
    EXPECT_EQ(o(beside).damage_marked, 1);
    EXPECT_EQ(o(elsewhere).damage_marked, 0);
    EXPECT_EQ(o(at_base).damage_marked, 0);
}

// Kog'Maw, Caustic 190, "[Deathknell] — Deal 4 to all units at my
// battlefield": the battlefield it died at. If the card is back on the board
// somewhere else when the ability resolves (played from the trash in
// response), it is a new object; the ability read where THAT one is.
TEST_F(PerfEngineLookBack, KogMaw_BackOnTheBoardElsewhere_StillDealsWhereItDied) {
    const GameObjectId kog = addUnit(P1, kKogMaw, 0, 0);
    const GameObjectId where_it_died = addUnit(P2, kShipyardSkulker, 9, 0);
    const GameObjectId where_it_is_now = addUnit(P2, kShipyardSkulker, 9, 1);
    withEngine([&](GameEngine& e) {
        e.testHook_executor().killObject(kog);          // its Deathknell is on the chain
        ASSERT_EQ(e.mutableState().getObject(kog).zone, ZoneType::Trash);
        e.testHook_executor().playIgnoringCost(P1, kog, LocationId{BattlefieldLocation{1}});
        ASSERT_TRUE(e.mutableState().getObject(kog).location.has_value());
        settleLikeTheEngine(e);
    });
    EXPECT_EQ(o(where_it_died).damage_marked, 4);
    EXPECT_EQ(o(where_it_is_now).damage_marked, 0);
}

// Units that die together (one cleanup, CR 370.1.a.2): every snapshot is
// taken before the first of them leaves (EffectExecutor::beginDeathBatch), so
// each one's "When I die" reads itself and the others as they were on the
// board — whichever was killed first. One is P2's unit that P1 had taken; the
// other is buffed, stunned and carries an Equipment.
TEST_F(PerfEngineLookBack, UnitsThatDieTogether_EachDeathAbilityReadsBothAsTheyWere) {
    for (bool through_a_card : {false, true}) {
        SCOPED_TRACE(through_a_card ? "DeathBatch + killObject (a card's kills)"
                                    : "processLethalDamage (a cleanup)");
        TWitness::myself.clear();
        TWitness::the_other.clear();
        const GameObjectId stolen = taken(kTWitness);
        GameObjectId sword;
        const GameObjectId armed = buffedStunnedArmed(kTWitness, &sword);
        TWitness::other[stolen] = armed;
        TWitness::other[armed] = stolen;
        const int armed_might = o(armed).current_might;
        ASSERT_EQ(armed_might, 2 + 1 + 2) << "printed 2, a buff, Long Sword +2";
        withEngine([&](GameEngine& e) {
            if (through_a_card) {
                {
                    DeathBatch together(e.testHook_executor(), {stolen, armed});
                    e.testHook_executor().killObject(stolen);
                    e.testHook_executor().killObject(armed);
                }
            } else {
                e.mutableState().getObject(stolen).damage_marked = 99;
                e.mutableState().getObject(armed).damage_marked = 99;
                e.testHook_processLethalDamage();
            }
            // Both are gone before either ability resolves.
            ASSERT_FALSE(e.mutableState().getObject(stolen).location.has_value());
            ASSERT_FALSE(e.mutableState().getObject(armed).location.has_value());
            settleLikeTheEngine(e);
        });
        ASSERT_EQ(TWitness::myself.count(stolen), 1u);
        ASSERT_EQ(TWitness::myself.count(armed), 1u);
        EXPECT_EQ(o(stolen).zone, ZoneType::Trash);
        EXPECT_NE(std::find(state.player(P2).trash.begin(), state.player(P2).trash.end(), stolen),
                  state.player(P2).trash.end()) << "its owner's trash";
        expectGearLeftBehind(sword);

        // The taken unit's ability: P1 controls it (CR 383: the controller as
        // it died), though the card is P2's again.
        const Saw& s_me = TWitness::myself[stolen];
        EXPECT_EQ(s_me.ability_controller, P1);
        EXPECT_EQ(s_me.controller_then, P1);
        EXPECT_EQ(s_me.controller_now, P2);
        EXPECT_EQ(s_me.might_then, 2);
        EXPECT_FALSE(s_me.stunned_then);
        EXPECT_TRUE(s_me.died);
        // ... and what it read of the unit that died with it.
        const Saw& s_them = TWitness::the_other[stolen];
        EXPECT_TRUE(s_them.valid);
        EXPECT_FALSE(s_them.on_board_now);
        EXPECT_EQ(s_them.controller_then, P1);
        EXPECT_EQ(s_them.might_then, armed_might);
        EXPECT_EQ(s_them.might_now, 2) << "the card in the trash";
        EXPECT_TRUE(s_them.stunned_then);
        EXPECT_EQ(s_them.buffs_then, 1);
        EXPECT_EQ(s_them.gear_then, 1u);
        EXPECT_TRUE(s_them.listed_with_me) << "it did not die alone";

        // The armed unit's ability: itself as it died ...
        const Saw& a_me = TWitness::myself[armed];
        EXPECT_EQ(a_me.ability_controller, P1);
        EXPECT_EQ(a_me.might_then, armed_might);
        EXPECT_EQ(a_me.might_now, 2);
        EXPECT_TRUE(a_me.stunned_then);
        EXPECT_EQ(a_me.buffs_then, 1);
        EXPECT_EQ(a_me.gear_then, 1u);
        // ... and the taken unit: P1's as it died, P2's card now.
        const Saw& a_them = TWitness::the_other[armed];
        EXPECT_TRUE(a_them.valid);
        EXPECT_EQ(a_them.controller_then, P1);
        EXPECT_EQ(a_them.controller_now, P2);
        EXPECT_EQ(a_them.might_then, 2);
        EXPECT_TRUE(a_them.listed_with_me) << "a friendly unit there as they died";
    }
}

// CR 383.2.c.2: "A Game Object will not be able to successfully evaluate its
// Trigger Condition ... if it leaves the zone that its Trigger is active from
// at the same time that its Trigger is satisfied." Two watchers ("When
// another friendly unit dies") that die together saw nothing; before, the one
// killed second had already triggered for the first (table order decided).
// A watcher that lives sees both.
TEST_F(PerfEngineLookBack, UnitsThatDieTogetherDoNotSeeEachOtherDie) {
    const GameObjectId first = addUnit(P1, kTWatcher, 0, 0);
    const GameObjectId second = addUnit(P1, kTWatcher, 0, 0);
    const GameObjectId survivor = addUnit(P1, kTWatcher, 0, 1);
    withEngine([&](GameEngine& e) {
        e.mutableState().getObject(first).damage_marked = 99;
        e.mutableState().getObject(second).damage_marked = 99;
        e.testHook_processLethalDamage();
        settleLikeTheEngine(e);
        EXPECT_EQ(e.mutableState().turn.oc_state, OpenClosedState::Open);
        EXPECT_TRUE(e.mutableState().chain.items.empty());
    });
    EXPECT_EQ(TWatcher::resolved[first], 0);
    EXPECT_EQ(TWatcher::resolved[second], 0) << "it left with the unit it watched";
    EXPECT_EQ(TWatcher::resolved[survivor], 2) << "on the board after both deaths";
}

// The same with nobody left to see it: the abilities taken back were the only
// things on the chain, so there is no chain and the state is Open again.
TEST_F(PerfEngineLookBack, UnitsThatDieTogether_NothingIsLeftOnTheChain) {
    const GameObjectId first = addUnit(P1, kTWatcher, 0, 0);
    const GameObjectId second = addUnit(P1, kTWatcher, 0, 0);
    withEngine([&](GameEngine& e) {
        e.mutableState().getObject(first).damage_marked = 99;
        e.mutableState().getObject(second).damage_marked = 99;
        e.testHook_processLethalDamage();
        EXPECT_TRUE(e.mutableState().chain.items.empty());
        EXPECT_EQ(e.mutableState().turn.oc_state, OpenClosedState::Open);
    });
    EXPECT_EQ(TWatcher::resolved[first] + TWatcher::resolved[second], 0);
}

// The rule's own example: "Viktor, Leader ... does not trigger if Viktor and
// another non-Recruit unit you control die during the same game action". In
// both table orders; and a Viktor that lives plays a Recruit for each.
TEST_F(PerfEngineLookBack, Viktor_DyingWithAnotherUnitPlaysNoRecruit) {
    for (bool viktor_first : {true, false}) {
        SCOPED_TRACE(viktor_first ? "Viktor is killed first" : "Viktor is killed second");
        GameObjectId viktor, other;
        if (viktor_first) { viktor = addUnit(P1, kViktorLeader, 0, 0); other = addUnit(P1, kShipyardSkulker, 0, 0); }
        else              { other = addUnit(P1, kShipyardSkulker, 0, 0); viktor = addUnit(P1, kViktorLeader, 0, 0); }
        const int before = recruits(P1);
        withEngine([&](GameEngine& e) {
            e.mutableState().getObject(viktor).damage_marked = 99;
            e.mutableState().getObject(other).damage_marked = 99;
            e.testHook_processLethalDamage();
            settleLikeTheEngine(e);
        });
        ASSERT_EQ(o(viktor).zone, ZoneType::Trash);
        ASSERT_EQ(o(other).zone, ZoneType::Trash);
        EXPECT_EQ(recruits(P1), before);
    }
    const GameObjectId viktor = addUnit(P1, kViktorLeader, 0, 0);
    const GameObjectId a = addUnit(P1, kShipyardSkulker, 0, 0);
    const GameObjectId b = addUnit(P1, kShipyardSkulker, 0, 0);
    const int before = recruits(P1);
    withEngine([&](GameEngine& e) {
        e.mutableState().getObject(a).damage_marked = 99;
        e.mutableState().getObject(b).damage_marked = 99;
        e.testHook_processLethalDamage();
        settleLikeTheEngine(e);
    });
    ASSERT_TRUE(onBoard(viktor));
    EXPECT_EQ(recruits(P1), before + 2);
}

// CR 383.2.c.1's example: Immortal Phoenix "triggers if Immortal Phoenix is
// in your trash immediately after you kill a unit with a spell, even if the
// unit you killed with a spell was that Immortal Phoenix."
TEST_F(PerfEngineLookBack, ImmortalPhoenix_KilledByItsOwnersSpellTriggersFromTheTrash) {
    const GameObjectId phoenix = addUnit(P1, kImmortalPhoenix, 0, 0);
    const GameObjectId spell = addToHand(P1, kTLookBack);     // a P1 spell as the damage source
    int queued = 0;
    withEngine([&](GameEngine& e) {
        e.testHook_executor().dealDamage(phoenix, 99, spell);
        e.testHook_processLethalDamage();
        ASSERT_EQ(e.mutableState().getObject(phoenix).zone, ZoneType::Trash);
        for (const auto& ci : e.mutableState().chain.items)
            if (ci.source == phoenix &&
                ci.fired_trigger == TriggerType::WhenYouKillAUnitWithASpell) ++queued;
        e.mutableState().chain.items.clear();
        e.mutableState().turn.oc_state = OpenClosedState::Open;
    });
    EXPECT_EQ(queued, 1);
}

// Every announcement of a leave is made while the object is still as it left;
// it becomes the new object after the last of them. (A return to hand and a
// banish used to forget the board first and announce afterwards, so a
// listener to UnitReturnedToHandEvent / "banished_owned" / "recycled_main"
// was handed the new object.)
TEST_F(PerfEngineLookBack, ALeaveIsAnnouncedBeforeTheObjectIsANewOne) {
    struct Heard { std::string what; PlayerId controller; bool stunned; };
    std::vector<Heard> heard;
    GameObjectId watched = kInvalidId;
    // (withEngine works on the engine's own copy of the state: read that one.)
    GameEngine* live = nullptr;
    auto note_live = [&](const std::string& what, GameObjectId id) {
        if (id != watched || !live) return;
        const auto& obj = live->mutableState().getObject(id);
        heard.push_back({what, obj.controller, obj.is_stunned});
    };
    boost::signals2::scoped_connection c1 = events.on_unit_returned_to_hand.connect(
        [&](const UnitReturnedToHandEvent& e) { note_live("returned", e.object); });
    boost::signals2::scoped_connection c2 = events.on_object_state_changed.connect(
        [&](const ObjectStateChangedEvent& e) {
            if (e.what_changed == "banished_owned") note_live("banished_owned", e.object);
            if (e.what_changed == "recycled_main") note_live("recycled_main", e.object);
        });
    boost::signals2::scoped_connection c3 = events.on_left_board.connect(
        [&](const LeftBoardEvent& e) { note_live("left", e.object); });
    auto run = [&](const std::function<void(EffectExecutor&, GameObjectId)>& move) {
        heard.clear();
        watched = dirtyUnit();
        withEngine([&](GameEngine& e) { live = &e; move(e.testHook_executor(), watched); live = nullptr; });
        return heard;
    };
    auto expectAsItLeft = [&](const std::vector<Heard>& h, std::vector<std::string> want) {
        ASSERT_EQ(h.size(), want.size());
        for (size_t i = 0; i < h.size(); ++i) {
            EXPECT_EQ(h[i].what, want[i]);
            EXPECT_EQ(h[i].controller, P1) << want[i] << ": who controlled it as it left";
            EXPECT_TRUE(h[i].stunned) << want[i] << ": as it left";
        }
        EXPECT_FALSE(carriesBoardState(o(watched))) << "and a new object afterwards";
        EXPECT_EQ(o(watched).controller, P2);
    };
    expectAsItLeft(run([](EffectExecutor& x, GameObjectId u) { x.bounceToHand(u); }),
                   {"left", "returned"});
    expectAsItLeft(run([](EffectExecutor& x, GameObjectId u) { x.banishObject(u); }),
                   {"left", "banished_owned"});
    expectAsItLeft(run([](EffectExecutor& x, GameObjectId u) { x.recycleCards(P1, {u}); }),
                   {"left", "recycled_main"});
    expectAsItLeft(run([](EffectExecutor& x, GameObjectId u) { x.killObject(u); }), {"left"});
    expectAsItLeft(run([](EffectExecutor& x, GameObjectId u) { x.putIntoDeckFromBoard(u, true); }),
                   {"left"});
}

// ─── 9.MM2: the rune-recycle sites and Keeper's Verdict, each on its own ────

// GameEngine::advanceCostPayment: a card's Power cost paid with a rune the
// agent picks, when the card is played.
TEST_F(PerfEngineLookBack, ARuneRecycledToPlayACardIsANewObject) {
    const GameObjectId rune = addRune(P1, Domain::Calm, /*exhausted=*/true);
    o(rune).is_stunned = true;
    o(rune).damage_marked = 1;
    const GameObjectId card = addToHand(P1, kTPowerUnit);
    playThroughEngine(card, nullptr, std::nullopt, nullptr, /*fund=*/false);
    ASSERT_TRUE(onBoard(card)) << "the play went through";
    ASSERT_EQ(o(rune).zone, ZoneType::RuneDeck);
    EXPECT_FALSE(o(rune).location.has_value());
    EXPECT_FALSE(carriesBoardState(o(rune)));
    EXPECT_FALSE(o(rune).is_stunned);
    EXPECT_EQ(o(rune).damage_marked, 0);
}

// The effect-play cost in effect_executor.cpp (plainPay): what
// EffectExecutor::playPayingCost charges when no engine has handed the
// executor its own payment — an executor on its own, as here. (On an engine
// the same call pays through GameEngine::payCardCost, the test above.)
TEST_F(PerfEngineLookBack, ARuneRecycledByAnEffectPlayWithoutAnEngineIsANewObject) {
    const GameObjectId rune = addRune(P1, Domain::Calm, /*exhausted=*/true);
    o(rune).is_stunned = true;
    o(rune).damage_marked = 1;
    const GameObjectId card = addToHand(P1, kTPowerUnit);
    EffectExecutor exec(state, events, card_db, &card_registry);
    ASSERT_TRUE(exec.playPayingCost(P1, card, LocationId{BaseLocation{P1}}));
    ASSERT_TRUE(onBoard(card));
    ASSERT_EQ(o(rune).zone, ZoneType::RuneDeck);
    EXPECT_FALSE(carriesBoardState(o(rune)));
    EXPECT_FALSE(o(rune).is_stunned);
    EXPECT_EQ(o(rune).damage_marked, 0);
}

// Keeper's Verdict 760 played through the engine: the unit its owner puts on
// their deck is a new object there.
TEST_F(PerfEngineLookBack, KeepersVerdict_TheUnitOnTheDeckIsANewObject) {
    GameObjectId gear;
    const GameObjectId u = dirtyUnit(false, &gear);          // P2's, controlled by P1, at a battlefield
    o(u).untargetable_by_enemy_this_turn = false;
    const GameObjectId verdict = addToHand(P2, kKeepersVerdict);
    EnginePathAgent p2(EnginePathAgent::script({EnginePathAgent::kRest}));
    playThroughEngine(verdict, nullptr, std::nullopt, &p2);
    ASSERT_EQ(o(u).zone, ZoneType::MainDeck);
    expectNewObject(u, "Keeper's Verdict");
    expectGearLeftBehind(gear);
    EXPECT_EQ(state.player(P2).main_deck.back(), u) << "its owner's deck";
    EXPECT_EQ(o(u).last_known.controller, P1) << "who had it as it left";
}


// ─── 9.NN1: what replaces a death in a batch is applied before any of them ──
// CR 373.1.a: the replacements' actions "are performed before any
// simultaneous unmodified events"; CR 370.4: "Soraka, Wanderer ... can be
// applied to any qualifying events that occur simultaneously with it leaving".
// Soraka 550 ("If another unit you control here would die, if it has less
// Might than me, instead heal it, exhaust it, and recall it") and a smaller
// friendly unit die together: the unit is recalled, whichever of the two the
// engine kills first. Before, replacements ran as each unit was killed, so
// with Soraka killed first she was in the trash and the unit died.
class PerfEngineDeathBatch : public PerfEngineLookBack {
protected:
    static constexpr CardDefId kSoraka = 550;
    void expectRecalled(GameObjectId unit) {
        ASSERT_TRUE(onBoard(unit)) << "its death was replaced";
        EXPECT_TRUE(o(unit).isAtBase());
        EXPECT_EQ(o(unit).damage_marked, 0);
        EXPECT_TRUE(o(unit).is_exhausted);
        EXPECT_FALSE(o(unit).last_known.batch_replaced) << "the batch is closed";
        EXPECT_FALSE(o(unit).last_known.batch_pending);
    }
};

TEST_F(PerfEngineDeathBatch, ACleanupAppliesSorakaBeforeAnyoneDies_InBothKillOrders) {
    for (bool soraka_first : {true, false}) {
        SCOPED_TRACE(soraka_first ? "Soraka is killed first" : "Soraka is killed second");
        GameObjectId soraka, small;
        if (soraka_first) { soraka = addUnit(P1, kSoraka, 0, 0); small = addUnit(P1, kShipyardSkulker, 0, 0); }
        else              { small = addUnit(P1, kShipyardSkulker, 0, 0); soraka = addUnit(P1, kSoraka, 0, 0); }
        withEngine([&](GameEngine& e) {
            e.testHook_recalculateAuras();
            e.mutableState().getObject(soraka).damage_marked = 99;
            e.mutableState().getObject(small).damage_marked = 99;
            e.testHook_processLethalDamage();
            settleLikeTheEngine(e);
        });
        EXPECT_EQ(o(soraka).zone, ZoneType::Trash) << "nothing replaces her own death";
        expectRecalled(small);
    }
}

TEST_F(PerfEngineDeathBatch, ACardsBatchAppliesSorakaBeforeAnyoneDies) {
    // "Kill all of them" (DeathBatch, every listed unit) with Soraka first.
    const GameObjectId soraka = addUnit(P1, kSoraka, 0, 0);
    const GameObjectId small = addUnit(P1, kShipyardSkulker, 0, 0);
    withEngine([&](GameEngine& e) {
        e.testHook_recalculateAuras();
        {
            DeathBatch together(e.testHook_executor(), {soraka, small});
            e.testHook_executor().killObject(soraka);
            e.testHook_executor().killObject(small);    // the replaced kill: does nothing
        }
        settleLikeTheEngine(e);
    });
    EXPECT_EQ(o(soraka).zone, ZoneType::Trash);
    expectRecalled(small);
    EXPECT_EQ(o(small).damage_marked, 0);
}

// "Deal damage to each, then kill what has lethal damage": the batch is told
// the units that were damaged and keeps the ones that die (LethalOnly). A
// listed unit that survives the damage is not a member: a "the next time it
// would die this turn" on it is not used up, and it keeps what it saw.
TEST_F(PerfEngineDeathBatch, ALethalOnlyBatchLeavesTheUnitsThatDoNotDieAlone) {
    const GameObjectId soraka = addUnit(P1, kSoraka, 0, 0);
    const GameObjectId small = addUnit(P1, kShipyardSkulker, 0, 0);
    const GameObjectId sturdy = addUnit(P1, kShipyardSkulker, 9, 0);
    o(sturdy).death_replacement_recall_pending = true;       // Tactical Retreat on it
    withEngine([&](GameEngine& e) {
        e.testHook_recalculateAuras();
        auto& x = e.testHook_executor();
        const std::vector<GameObjectId> hit = {soraka, small, sturdy};
        for (auto id : hit) x.dealDamage(id, 4, kInvalidId);
        {
            DeathBatch together(x, hit, DeathBatch::LethalOnly);
            for (auto id : hit)
                if (e.mutableState().getObject(id).location.has_value() &&
                    e.mutableState().getObject(id).hasLethalDamage())
                    x.killObject(id);
        }
        settleLikeTheEngine(e);
    });
    EXPECT_EQ(o(soraka).zone, ZoneType::Trash);
    expectRecalled(small);
    ASSERT_TRUE(onBoard(sturdy));
    EXPECT_EQ(o(sturdy).battlefieldId(), std::optional<BattlefieldId>{BattlefieldId::fromRaw(0)}) << "it was not about to die";
    EXPECT_TRUE(o(sturdy).death_replacement_recall_pending) << "its replacement was not used up";
    EXPECT_EQ(o(sturdy).damage_marked, 4);
}

TEST_F(PerfEngineDeathBatch, EveryCardBatchSaysWhoDies) {
    // Source scan. A card that kills "what the damage killed" calls
    // EffectExecutor::killLethallyDamagedTogether (21 sites) and has no loop
    // or lethal test of its own; a DeathBatch in card code is followed by a
    // loop that kills every listed unit (7 sites in 6 files).
    namespace fs = std::filesystem;
    const char* root = std::getenv("RIFTBOUND_ROOT");
    ASSERT_NE(root, nullptr);
    int helper = 0, everyone = 0;
    for (const auto& entry : fs::recursive_directory_iterator(fs::path(root) / "src" / "cards")) {
        if (entry.path().extension() != ".cpp") continue;
        std::ifstream in(entry.path());
        std::vector<std::string> lines;
        for (std::string l; std::getline(in, l);) lines.push_back(l);
        for (size_t i = 0; i < lines.size(); ++i) {
            if (lines[i].find("killLethallyDamagedTogether(") != std::string::npos) ++helper;
            EXPECT_EQ(lines[i].find("DeathBatch::LethalOnly"), std::string::npos)
                << entry.path().filename().string() << ":" << (i + 1)
                << " — call killLethallyDamagedTogether";
            if (lines[i].find("DeathBatch together(") == std::string::npos) continue;
            ++everyone;
            for (size_t j = i + 1; j < std::min(lines.size(), i + 7); ++j)
                EXPECT_EQ(lines[j].find("hasLethalDamage"), std::string::npos)
                    << entry.path().filename().string() << ":" << (j + 1)
                    << " — a lethal test of its own after a DeathBatch";
        }
    }
    EXPECT_EQ(helper, 22);   // + Falling Star (D20: both instructions, then the kills)
    EXPECT_EQ(everyone, 7);
}

// ─── 9.OO1: which dying unit a single replacement saves ─────────────────────
// CR 373: "Replacement Effects with the same controller are applied in the
// order of their controller's choosing. Example: Two units controlled by the
// same player die in the same cleanup. That player also controls Zhonya's
// Hourglass. They must decide which event to apply Zhonya's Hourglass to
// first." The player is asked once, options named by unit; the unit named is
// saved, the other dies. Before, the first unit in table order was saved and
// nobody was asked.
TEST_F(PerfEngineDeathBatch, ZhonyasHourglass_ItsControllerDecidesWhichDeathItReplaces) {
    constexpr CardDefId kZhonyasHourglass = 77;
    for (bool save_the_second : {false, true}) {
        SCOPED_TRACE(save_the_second ? "the second unit is named" : "the first unit is named");
        const GameObjectId hourglass = addEquipment(P1, kZhonyasHourglass);
        const GameObjectId first = addUnit(P1, kShipyardSkulker, 0, 0);
        const GameObjectId second = addUnit(P1, kShipyardSkulker, 0, 1);
        const GameObjectId enemy = addUnit(P2, kShipyardSkulker, 0, 0);   // dies too; not P1's to order
        const GameObjectId saved = save_the_second ? second : first;
        const GameObjectId lost = save_the_second ? first : second;
        EnginePathAgent p1(EnginePathAgent::script({static_cast<int>(saved.raw())}));
        EnginePathAgent p2(EnginePathAgent::script({}));
        withEngine([&](GameEngine& e) {
            for (auto id : {first, second, enemy}) e.mutableState().getObject(id).damage_marked = 99;
            e.testHook_processLethalDamage();
            settleLikeTheEngine(e);
        }, &p1, &p2);
        ASSERT_EQ(p1.prompts.size(), 1u) << "one prompt: which death first";
        ASSERT_EQ(p1.prompts[0].size(), 2u);
        EXPECT_TRUE(p1.wasOffered(first));
        EXPECT_TRUE(p1.wasOffered(second));
        EXPECT_FALSE(p1.wasOffered(enemy));
        EXPECT_TRUE(p2.prompts.empty());
        expectRecalled(saved);
        EXPECT_EQ(o(lost).zone, ZoneType::Trash);
        EXPECT_EQ(o(hourglass).zone, ZoneType::Trash) << "killed instead";
        EXPECT_EQ(o(enemy).zone, ZoneType::Trash);
    }
}

// One dying unit with something to replace its death: nothing to decide.
TEST_F(PerfEngineDeathBatch, Pin_OneReplaceableDeathAsksNothing) {
    constexpr CardDefId kZhonyasHourglass = 77;
    const GameObjectId hourglass = addEquipment(P1, kZhonyasHourglass);
    const GameObjectId only = addUnit(P1, kShipyardSkulker, 0, 0);
    const GameObjectId enemy = addUnit(P2, kShipyardSkulker, 0, 0);
    EnginePathAgent p1(EnginePathAgent::script({}));
    withEngine([&](GameEngine& e) {
        for (auto id : {only, enemy}) e.mutableState().getObject(id).damage_marked = 99;
        e.testHook_processLethalDamage();
        settleLikeTheEngine(e);
    }, &p1);
    EXPECT_TRUE(p1.prompts.empty());
    expectRecalled(only);
    EXPECT_EQ(o(hourglass).zone, ZoneType::Trash);
    EXPECT_EQ(o(enemy).zone, ZoneType::Trash);
}

// ─── 9.OO3: one test for "this damage kills this unit" ──────────────────────
// With an Elder Dragon ("any amount of your damage is enough to kill") a
// card's damage that is short of lethal still kills. The card's own kill step
// (EffectExecutor::killLethallyDamagedTogether) uses the cleanup's test, so
// those units die with the card's other kills, as one batch — before, the
// card killed only the lethally damaged one and the rest died alone in the
// next cleanup.
TEST_F(PerfEngineDeathBatch, DamageThatKillsByTheElderDragonRuleKillsInTheCardsBatch) {
    constexpr CardDefId kElderDragon = 680;
    const GameObjectId dragon = addUnit(P1, kElderDragon, 0, 1);
    const GameObjectId frail = addUnit(P2, kTWitness, 0, 0);        // 2 [M]: 2 damage is lethal
    const GameObjectId sturdy = addUnit(P2, kTWitness, 9, 0);       // 9 [M]: 2 damage is not
    const GameObjectId watcher = addUnit(P2, kTWatcher, 0, 1);      // lives, sees both
    const GameObjectId own = addUnit(P1, kShipyardSkulker, 9, 0);   // P1's own unit: the rule is about
                                                                     // damage to the OTHER player's
    TWitness::other[frail] = sturdy;
    TWitness::other[sturdy] = frail;
    std::vector<GameObjectId> died;
    withEngine([&](GameEngine& e) {
        auto& x = e.testHook_executor();
        EXPECT_FALSE(x.damageKills(sturdy));
        for (auto id : {frail, sturdy, own}) x.dealDamage(id, 2, dragon);
        EXPECT_TRUE(x.damageKills(frail));
        EXPECT_TRUE(x.damageKills(sturdy)) << "any amount of P1's damage";
        EXPECT_FALSE(x.damageKills(own)) << "its own side's damage does not count";
        died = x.killLethallyDamagedTogether({frail, sturdy, own});
        // Before any cleanup: the card's step killed both.
        EXPECT_FALSE(e.mutableState().getObject(sturdy).location.has_value());
        EXPECT_FALSE(e.mutableState().getObject(frail).location.has_value());
        settleLikeTheEngine(e);
    });
    EXPECT_EQ(died, (std::vector<GameObjectId>{frail, sturdy}));
    ASSERT_TRUE(onBoard(own));
    // They died together: each was beside the other as it died ...
    ASSERT_EQ(TWitness::the_other.count(frail), 1u);
    ASSERT_EQ(TWitness::the_other.count(sturdy), 1u);
    EXPECT_TRUE(TWitness::the_other[frail].listed_with_me) << "it did not die alone";
    EXPECT_TRUE(TWitness::the_other[sturdy].listed_with_me);
    EXPECT_EQ(TWitness::myself[sturdy].might_then, 9);
    // ... and a friendly watcher that lived saw both deaths.
    EXPECT_EQ(TWatcher::resolved[watcher], 2);
}

// The cleanup asks the same question (it did already): pinned so the two
// cannot drift apart again.
TEST_F(PerfEngineDeathBatch, Pin_TheCleanupAndTheCardStepAgreeOnWhatDamageKills) {
    constexpr CardDefId kElderDragon = 680;
    const GameObjectId dragon = addUnit(P1, kElderDragon, 0, 1);
    const GameObjectId sturdy = addUnit(P2, kShipyardSkulker, 9, 0);
    const GameObjectId untouched = addUnit(P2, kShipyardSkulker, 9, 0);
    withEngine([&](GameEngine& e) {
        e.testHook_executor().dealDamage(sturdy, 1, dragon);
        EXPECT_TRUE(e.testHook_executor().damageKills(sturdy));
        EXPECT_FALSE(e.testHook_executor().damageKills(untouched));
        e.testHook_processLethalDamage();
    });
    EXPECT_EQ(o(sturdy).zone, ZoneType::Trash);
    EXPECT_TRUE(onBoard(untouched));
}

// ─── 9.OO2: the Main Deck Void Hatchling look ───────────────────────────────
// The card looked at is named on one trace line that only the player who
// looked may read; the two options are labelled and do not name it; no line
// the other player may read names it — whether it is left or recycled.
TEST_F(PerfEngineDeathBatch, TheMainDeckHatchlingLookIsNamedToTheLookerOnly) {
    constexpr CardDefId kVoidHatchling = 341;
    constexpr CardDefId kSecret = 37;          // Immortal Phoenix: a name nothing else here has
    addUnit(P1, kVoidHatchling, 0, -1);
    for (bool recycle : {false, true}) {
        SCOPED_TRACE(recycle ? "recycled" : "left on top");
        addToDeck(P1, kShipyardSkulker);
        const GameObjectId top = addToDeck(P1, kSecret);
        const std::string name = o(top).name;
        ASSERT_EQ(state.player(P1).main_deck.back(), top);
        struct Line { std::string text; LogEvent event; };
        std::vector<LogEvent> lines;
        auto conn = events.on_log.connect([&](const LogEvent& e) { lines.push_back(e); });
        std::vector<std::string> labels;
        EnginePathAgent agent([&](const std::vector<Intent>& legal) {
            for (const auto& i : legal) labels.push_back(i.choice_label);
            for (const auto& i : legal)
                if (i.chosen_value == std::optional<int>{recycle ? 1 : 0}) return i;
            return legal.front();
        });
        withEngine([&](GameEngine& e) {
            e.testHook_cleanup();
            ASSERT_TRUE(e.mutableState().player(P1).has_reveal_peek);
            lines.clear();
            e.testHook_executor().lookBeforeReveal(P1);
        }, &agent);
        conn.disconnect();
        ASSERT_EQ(labels.size(), 2u);
        for (const auto& l : labels) {
            EXPECT_FALSE(l.empty()) << "the options are labelled";
            EXPECT_EQ(l.find(name), std::string::npos) << "and do not name the card: " << l;
        }
        int for_the_looker = 0;
        for (const auto& e : lines) {
            if (e.message.find(name) == std::string::npos) continue;
            EXPECT_TRUE(e.visibleTo(P1)) << e.message;
            EXPECT_FALSE(e.visibleTo(P2)) << "named to the other player: " << e.message;
            ++for_the_looker;
        }
        EXPECT_GE(for_the_looker, 1) << "the looker's log names the card";
        EXPECT_EQ(state.player(P1).main_deck.back() == top, !recycle);
        // Clear the deck for the next round.
        state.player(P1).main_deck.clear();
    }
}

// ─── 9.NN2: the snapshot travels with the chain item ────────────────────────
// A card dies (its "When I die" goes on the chain), is played again in
// response, and dies a second time (a second "When I die" above the first).
// The object has one last_known — the second death's. Each ability reads ITS
// death from the snapshot on its chain item (asItLeft). Before, both read the
// second death.
TEST_F(PerfEngineLookBack, ACardThatDiesTwice_EachDeathAbilityReadsItsOwnDeath) {
    const GameObjectId w = taken(kTWitness);                 // P2's card, P1 controls it, at #0
    o(w).buff_count = 1;
    o(w).recomputeMight();
    const int might_first = o(w).current_might;
    ASSERT_EQ(might_first, 3);
    withEngine([&](GameEngine& e) {
        auto& x = e.testHook_executor();
        auto& st = e.mutableState();
        x.killObject(w);                                     // first death: P1's, buffed, at #0
        ASSERT_EQ(st.chain.items.size(), 1u);
        ASSERT_TRUE(st.chain.items[0].subject_as_it_left.has_value());
        // In response it is played again by its owner, at battlefield 1 ...
        x.playIgnoringCost(P2, w, LocationId{BattlefieldLocation{1}});
        ASSERT_TRUE(st.getObject(w).location.has_value());
        // ... and killed again, stunned this time.
        st.getObject(w).is_stunned = true;
        x.killObject(w);
        size_t deaths = 0;
        for (const auto& ci : st.chain.items)
            if (ci.source == w && ci.fired_trigger == TriggerType::WhenIDie) ++deaths;
        ASSERT_EQ(deaths, 2u) << "both abilities are on the chain";
        settleLikeTheEngine(e);
    });
    ASSERT_EQ(TWitness::in_order.size(), 2u);
    // The chain is last in, first out: the second death's ability resolves first.
    const Saw& second = TWitness::in_order[0];
    const Saw& first = TWitness::in_order[1];
    EXPECT_EQ(second.controller_then, P2);
    EXPECT_TRUE(second.stunned_then);
    EXPECT_EQ(second.buffs_then, 0);
    EXPECT_EQ(second.might_then, 2);
    EXPECT_EQ(second.at_then, std::optional<LocationId>{LocationId{BattlefieldLocation{1}}});
    EXPECT_EQ(TWitness::controllers_in_order[0], P2);

    EXPECT_EQ(first.controller_then, P1) << "the first death: P1 had it";
    EXPECT_FALSE(first.stunned_then);
    EXPECT_EQ(first.buffs_then, 1);
    EXPECT_EQ(first.might_then, might_first);
    EXPECT_EQ(first.at_then, std::optional<LocationId>{LocationId{BattlefieldLocation{0}}});
    EXPECT_EQ(TWitness::controllers_in_order[1], P1) << "controllerWhenItLeft(ctx, ...)";
    EXPECT_EQ(first.ability_controller, P1);
    EXPECT_EQ(second.ability_controller, P2);
}

// The same for a real reader: Kog'Maw, Caustic 190 dies at battlefield 0, is
// played again and dies at battlefield 1 — each Deathknell deals its 4 at the
// battlefield of its own death.
TEST_F(PerfEngineLookBack, KogMaw_DiesTwice_EachDeathknellDealsWhereThatDeathWas) {
    const GameObjectId kog = addUnit(P1, kKogMaw, 0, 0);
    const GameObjectId at_first = addUnit(P2, kShipyardSkulker, 20, 0);
    const GameObjectId at_second = addUnit(P2, kShipyardSkulker, 20, 1);
    withEngine([&](GameEngine& e) {
        auto& x = e.testHook_executor();
        x.killObject(kog);
        x.playIgnoringCost(P1, kog, LocationId{BattlefieldLocation{1}});
        x.killObject(kog);
        settleLikeTheEngine(e);
    });
    EXPECT_EQ(o(at_first).damage_marked, 4);
    EXPECT_EQ(o(at_second).damage_marked, 4);
}

// ─── 9.NN3: a rune recycled for Power leaves like everything else ───────────
// Snapshot, move, announcement, then the new object. A listener to the
// rune's departure ("When you recycle a rune") is handed the rune as it left
// (exhausted, and with whatever else was on it); before, the three sites
// reset it first — and the effect-play cost announced nothing at all.
constexpr CardDefId kTRuneWatcher = 1845;   // unit: "When you recycle a rune", writes it down

class TRuneWatcher : public UnitCard {
public:
    TRuneWatcher() {
        def_.id = kTRuneWatcher;
        def_.name = "Test Rune Watcher";
        def_.card_type = CardType::Unit;
        def_.might = 1;
    }
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouRecycleARune; }
    // Evaluated as the event is announced: the rune itself, live.
    bool triggerCondition(const GameState& state, GameObjectId, PlayerId, TriggerType,
                          GameObjectId subject) const override {
        if (subject != kInvalidId && state.objectExists(subject)) {
            ++announced;
            exhausted_when_announced = state.getObject(subject).is_exhausted;
            stunned_when_announced = state.getObject(subject).is_stunned;
        }
        return true;
    }
    // Resolved later: the rune is a new object by now; the ability reads it
    // as it left.
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        const auto& item = ctx.state.chain.resuming;
        if (!item.has_value() || item->triggering_subject == kInvalidId) return;
        const GameObjectId rune = item->triggering_subject;
        const auto& left = asItLeft(ctx, rune);
        ++resolved;
        valid_then = left.valid;
        exhausted_then = left.exhausted;
        controller_then = left.controller;
        exhausted_now = ctx.state.getObject(rune).is_exhausted;
    }
    static void reset() {
        announced = resolved = 0;
        exhausted_when_announced = stunned_when_announced = false;
        valid_then = exhausted_then = false;
        exhausted_now = true;
        controller_then = PlayerId::None;
    }
    static inline int announced = 0, resolved = 0;
    static inline bool exhausted_when_announced = false, stunned_when_announced = false;
    static inline bool valid_then = false, exhausted_then = false, exhausted_now = true;
    static inline PlayerId controller_then = PlayerId::None;
private:
    CardDef def_;
};

class PerfEngineRuneLeaves : public PerfEngineLookBack {
protected:
    void SetUp() override {
        PerfEngineLookBack::SetUp();
        card_registry.registerCard(kTRuneWatcher, std::make_unique<TRuneWatcher>());
        card_db.buildFromClasses(card_registry);
        TRuneWatcher::reset();
    }
    GameObjectId dirtyRune() {
        const GameObjectId rune = addRune(P1, Domain::Calm, /*exhausted=*/true);
        o(rune).is_stunned = true;
        return rune;
    }
    void expectHeardAsItLeft(GameObjectId rune, const char* site) {
        SCOPED_TRACE(site);
        EXPECT_EQ(TRuneWatcher::announced, 1);
        EXPECT_TRUE(TRuneWatcher::exhausted_when_announced) << "the rune as it left";
        EXPECT_TRUE(TRuneWatcher::stunned_when_announced);
        EXPECT_EQ(o(rune).zone, ZoneType::RuneDeck);
        EXPECT_FALSE(carriesBoardState(o(rune))) << "a new object afterwards";
        EXPECT_FALSE(o(rune).is_exhausted);
        EXPECT_TRUE(o(rune).last_known.valid);
        EXPECT_TRUE(o(rune).last_known.exhausted);
        EXPECT_FALSE(o(rune).last_known.died);
        EXPECT_EQ(o(rune).last_known.controller, P1);
    }
};

TEST_F(PerfEngineRuneLeaves, PaidThroughTheEnginesCostPayment) {       // advanceCostPayment
    addUnit(P1, kTRuneWatcher, 0, -1);
    const GameObjectId rune = dirtyRune();
    const GameObjectId card = addToHand(P1, kTPowerUnit);
    playThroughEngine(card, nullptr, std::nullopt, nullptr, /*fund=*/false);
    ASSERT_TRUE(onBoard(card));
    expectHeardAsItLeft(rune, "GameEngine::advanceCostPayment");
    EXPECT_EQ(TRuneWatcher::resolved, 1);
    EXPECT_TRUE(TRuneWatcher::valid_then);
    EXPECT_TRUE(TRuneWatcher::exhausted_then) << "the ability reads the rune as it left";
    EXPECT_FALSE(TRuneWatcher::exhausted_now) << "in the Rune Deck it is a new object";
    EXPECT_EQ(TRuneWatcher::controller_then, P1);
}

TEST_F(PerfEngineRuneLeaves, PaidThroughResourcePay) {                 // resource::payIn
    addUnit(P1, kTRuneWatcher, 0, -1);
    const GameObjectId rune = dirtyRune();
    withEngine([&](GameEngine& e) {
        EXPECT_TRUE(e.testHook_payAdditionalCost(P1, 0, 1, Domain::Calm));
        settleLikeTheEngine(e);
    });
    expectHeardAsItLeft(rune, "resource::payIn");
    EXPECT_EQ(TRuneWatcher::resolved, 1);
    EXPECT_TRUE(TRuneWatcher::exhausted_then);
}

TEST_F(PerfEngineRuneLeaves, PaidByAnEffectPlayWithoutAnEngine) {      // plainPay
    const GameObjectId rune = dirtyRune();
    const GameObjectId card = addToHand(P1, kTPowerUnit);
    int heard = 0;
    bool exhausted_when_heard = false, stunned_when_heard = false;
    boost::signals2::scoped_connection c = events.on_left_board.connect(
        [&](const LeftBoardEvent& e) {
            if (e.object != rune) return;
            ++heard;
            EXPECT_EQ(e.destination, ZoneType::RuneDeck);
            EXPECT_EQ(e.controller, P1);
            exhausted_when_heard = state.getObject(rune).is_exhausted;
            stunned_when_heard = state.getObject(rune).is_stunned;
        });
    EffectExecutor exec(state, events, card_db, &card_registry);
    ASSERT_TRUE(exec.playPayingCost(P1, card, LocationId{BaseLocation{P1}}));
    EXPECT_EQ(heard, 1) << "this site announced nothing before";
    EXPECT_TRUE(exhausted_when_heard);
    EXPECT_TRUE(stunned_when_heard);
    EXPECT_FALSE(carriesBoardState(o(rune)));
    EXPECT_TRUE(o(rune).last_known.valid);
    EXPECT_TRUE(o(rune).last_known.exhausted);
}

// ─── 9.NN4: what is known about a Rune Deck reaches no observer ─────────────
// After the Void Hatchling ruling a player can know the top rune of their
// Rune Deck, and Twisted Fate's revealed rune goes to the bottom known to
// both. Nothing records that, and nothing can show it: these tests pin the
// three places that guarantee it (the comment at
// EffectExecutor::lookBeforeReveal names them).
class PerfEngineRuneDeck : public PerfEngineLookBack {
protected:
    static constexpr CardDefId kVoidHatchling = 341;
    GameObjectId runeInDeck(PlayerId p, Domain d, const char* name) {
        const GameObjectId rune = addRune(p, d);
        o(rune).location = std::nullopt;
        o(rune).zone = ZoneType::RuneDeck;
        o(rune).name = name;
        state.player(p).rune_deck.push_back(rune);
        return rune;
    }
};

// The state features (ObservationTensor) and the info-set id hashed from them
// are the same for every order of a Rune Deck, for both perspectives: the
// vector holds the deck's size and nothing else about it.
TEST_F(PerfEngineRuneDeck, TheOrderOfARuneDeckIsInNoObservationOrInfoSetId) {
    for (PlayerId p : {P1, P2}) {
        runeInDeck(p, Domain::Fury, "Fury Rune");
        runeInDeck(p, Domain::Mind, "Mind Rune");
        runeInDeck(p, Domain::Order, "Order Rune");
        runeInDeck(p, Domain::Calm, "Calm Rune");
    }
    GameState other = state;
    for (auto& ps : other.players) std::reverse(ps.rune_deck.begin(), ps.rune_deck.end());
    std::swap(other.players[0].rune_deck[1], other.players[0].rune_deck[2]);
    ASSERT_NE(other.players[0].rune_deck, state.players[0].rune_deck);
    for (PlayerId observer : {P1, P2}) {
        const auto a = ml::extractStateFeatures(state, observer, card_db);
        const auto b = ml::extractStateFeatures(other, observer, card_db);
        EXPECT_EQ(a, b) << "perspective " << toString(observer);
        const int seat = observer == P1 ? 0 : 1;
        EXPECT_EQ(ml::computeInfoSetId(a, seat), ml::computeInfoSetId(b, seat));
    }
    // One rune fewer does show: the size is there.
    GameState fewer = state;
    fewer.objects.erase(fewer.players[0].rune_deck.back());
    fewer.players[0].rune_deck.pop_back();
    EXPECT_NE(ml::extractStateFeatures(state, P1, card_db),
              ml::extractStateFeatures(fewer, P1, card_db));
}

// The Hatchling look and Twisted Fate's reveal, through the engine: the rune
// is named on one line only, readable by the player who looked; no public
// line names the rune that was looked at and recycled; and afterwards the
// knowledge view says "unknown" about every rune in the deck, for both
// players (a look and a reveal are not recorded for a Rune Deck).
TEST_F(PerfEngineRuneDeck, TheLookIsPrivateAndNothingAboutTheRuneDeckIsRecorded) {
    addUnit(P1, kVoidHatchling, 0, -1);
    const GameObjectId tf = addUnit(P1, 200 /* Twisted Fate, Gambler */, 0, 0);
    addToDeck(P1, kShipyardSkulker);
    const GameObjectId calm = runeInDeck(P1, Domain::Calm, "Calm Rune");
    const GameObjectId below = runeInDeck(P1, Domain::Mind, "Mind Rune");
    const GameObjectId top = runeInDeck(P1, Domain::Order, "Order Rune");
    struct Line { std::string text; LogAudience audience; };
    std::vector<Line> lines;
    auto conn = events.on_log.connect([&](const LogEvent& e) {
        lines.push_back({e.message, e.audience});
    });
    std::vector<std::vector<std::string>> labels;
    EnginePathAgent agent([&](const std::vector<Intent>& legal) {
        labels.emplace_back();
        for (const auto& i : legal) labels.back().push_back(i.choice_label);
        for (const auto& i : legal) if (i.chosen_value == std::optional<int>{1}) return i;   // recycle
        return legal.front();
    });
    withEngine([&](GameEngine& e) {
        e.testHook_cleanup();                                  // the Hatchling's permission
        ASSERT_TRUE(e.mutableState().player(P1).has_reveal_peek);
        e.testHook_executor().queueTriggeredAbility(tf, P1, TriggerType::WhenIAttack, kInvalidId);
        settleLikeTheEngine(e);
    }, &agent);
    conn.disconnect();

    ASSERT_FALSE(labels.empty());
    for (const auto& l : labels[0])
        EXPECT_EQ(l.find("Order"), std::string::npos) << "the option does not name the rune: " << l;
    int private_lines = 0;
    for (const auto& line : lines) {
        if (line.text.find("Order Rune") == std::string::npos) continue;
        EXPECT_TRUE(line.audience.includes(P1)) << line.text;
        EXPECT_FALSE(line.audience.includes(P2))
            << "the rune looked at and recycled is named to the other player: " << line.text;
        ++private_lines;
    }
    EXPECT_EQ(private_lines, 1) << "the look: one private line";
    // top was recycled at the look; `below` was revealed (public) and recycled.
    ASSERT_EQ(state.player(P1).rune_deck.size(), 3u);
    EXPECT_EQ(state.player(P1).rune_deck.front(), below);
    for (PlayerId observer : {P1, P2}) {
        knowledge::GameKnowledgeView view(state, observer);
        for (GameObjectId rune : {calm, below, top}) {
            const CardKnowledge k = view.about(rune);
            EXPECT_FALSE(k.identity_known) << toString(observer) << " / " << o(rune).name;
            EXPECT_EQ(k.last_known_zone, ZoneType::RuneDeck);
        }
        EXPECT_TRUE(view.knownDeckTop(P1).empty());
        EXPECT_TRUE(view.knownDeckBottom(P1).empty());
    }
}


// ─── 9.PP1: a decision's trace header says what was asked ───────────────────
// A prompt asked through the chain's query path (a card's pending choice, a
// prompt the executor makes in mid-effect) was headed "(P — chain priority):
// N legal actions" whatever it asked. The header is now the prompt's own
// text; "chain priority" is kept for the decision that offers PassPriority.
class PerfEngineDecisionHeaders : public PerfEngineDeathBatch {
protected:
    static constexpr CardDefId kZhonyasHourglass = 77;
    std::vector<std::string> headers;
    boost::signals2::scoped_connection conn;
    void listen() {
        headers.clear();
        conn = events.on_log.connect([this](const LogEvent& e) {
            if (e.message.find("DECISION #") != std::string::npos) headers.push_back(e.message);
        });
    }
    int count(const std::string& text) const {
        int n = 0;
        for (const auto& h : headers) if (h.find(text) != std::string::npos) ++n;
        return n;
    }
};

TEST_F(PerfEngineDecisionHeaders, WhichDeathIsReplacedFirst) {
    addEquipment(P1, kZhonyasHourglass);
    const GameObjectId first = addUnit(P1, kShipyardSkulker, 0, 0);
    const GameObjectId second = addUnit(P1, kShipyardSkulker, 0, 1);
    EnginePathAgent p1(EnginePathAgent::script({static_cast<int>(second.raw())}));
    listen();
    withEngine([&](GameEngine& e) {
        for (auto id : {first, second}) e.mutableState().getObject(id).damage_marked = 99;
        e.testHook_processLethalDamage();
    }, &p1);
    ASSERT_EQ(headers.size(), 1u);
    EXPECT_NE(headers[0].find("(P1): which death is replaced first (CR 373) [2 options]"),
              std::string::npos) << headers[0];
    EXPECT_EQ(count("chain priority"), 0);
}

TEST_F(PerfEngineDecisionHeaders, WhichReplacementAppliesFirst) {
    addEquipment(P1, kZhonyasHourglass);
    const GameObjectId unit = addUnit(P1, kShipyardSkulker, 0, 0);
    o(unit).death_replacement_recall_pending = true;      // a second thing that could replace it
    EnginePathAgent p1(EnginePathAgent::script({EnginePathAgent::kFirst}));
    listen();
    withEngine([&](GameEngine& e) {
        e.mutableState().getObject(unit).damage_marked = 99;
        e.testHook_processLethalDamage();
    }, &p1);
    ASSERT_EQ(headers.size(), 1u);
    EXPECT_NE(headers[0].find("(P1): " + o(unit).name +
                              " would die — which replacement applies first (CR 372) [2 options]"),
              std::string::npos) << headers[0];
    EXPECT_EQ(count("chain priority"), 0);
}

// An ordinary card prompt: Keeper's Verdict 760 asks the unit's OWNER "top or
// bottom" as it resolves. And the priority decisions around it keep their name.
TEST_F(PerfEngineDecisionHeaders, ACardsPromptCarriesItsOwnText) {
    const GameObjectId u = addUnit(P1, kShipyardSkulker, 0, 0);
    const GameObjectId verdict = addToHand(P2, kKeepersVerdict);
    EnginePathAgent p1(EnginePathAgent::script({EnginePathAgent::kRest}));
    EnginePathAgent p2(EnginePathAgent::script({EnginePathAgent::kRest}));
    listen();
    playThroughEngine(verdict, &p1, std::nullopt, &p2);
    ASSERT_EQ(o(u).zone, ZoneType::MainDeck);
    EXPECT_EQ(count("(P1): Keeper's Verdict: top or bottom of your Main Deck [2 options]"), 1)
        << "the prompt's own label, for the player it was asked of";
    for (const auto& h : headers)
        if (h.find("chain priority") != std::string::npos) {
            EXPECT_NE(h.find("legal actions"), std::string::npos) << h;
            EXPECT_EQ(h.find("Keeper"), std::string::npos) << h;
        }
    EXPECT_GE(count("chain priority"), 1) << "priority was passed on the spell";
}


// ─── 9.PP3: Zhonya's Hourglass hidden at a battlefield lost without a death ─
// "[Hidden] ... If a friendly unit would die, kill this instead. Heal that
// unit, exhaust it, and recall it." P1's only unit at the battlefield is
// returned to hand by an enemy spell: nothing dies.
//  - Played from facedown in response (CR 811.6 [Reaction]; 811.1.d.1.a: a
//    hidden gear is played to that battlefield): it is not used, and the
//    cleanup that follows the spell recalls it to its controller's base
//    (CR 149.3, 323.6, 459.1), from where it replaces a later death.
//  - Not played: when its controller no longer controls the battlefield
//    (CR 323.5) the facedown card goes to its owner's trash (CR 323.6.a).

/// Passes priority and takes first options, except that it plays `card`
/// from facedown the first time that play is offered (when `play_it`).
class HiddenCardAgent : public AgentInterface {
public:
    HiddenCardAgent(GameObjectId card, bool play_it) : card_(card), play_it_(play_it) {}
    Intent selectAction(const GameState&, const std::vector<Intent>& legal) override {
        const Intent* hidden_play = nullptr;
        for (const auto& i : legal)
            if (i.card == card_ && (i.type == IntentType::PlayReaction ||
                                    i.type == IntentType::PlayActionCard))
                hidden_play = &i;
        const bool priority = std::any_of(legal.begin(), legal.end(), [](const Intent& i) {
            return i.type == IntentType::PassPriority; });
        if (priority) { ++priority_windows; if (hidden_play) ++offered; }
        if (hidden_play && play_it_ && !played) { played = true; return *hidden_play; }
        for (const auto& i : legal)
            if (i.type == IntentType::PassPriority || i.type == IntentType::PassFocus) return i;
        return legal.front();
    }
    int offered = 0, priority_windows = 0;
    bool played = false;
private:
    GameObjectId card_;
    bool play_it_;
};

class PerfEngineHiddenHourglass : public PerfEngineLookBack {
protected:
    static constexpr CardDefId kZhonyasHourglass = 77;
    static constexpr CardDefId kFoxFire = 253;
    GameObjectId hide(PlayerId owner, CardDefId def, int bf) {
        const GameObjectId card = addToHand(owner, def);
        auto& h = state.player(owner).hand;
        h.erase(std::remove(h.begin(), h.end(), card), h.end());
        GameObject& c = o(card);
        c.zone = ZoneType::BattlefieldZone;
        c.is_hidden = true;
        c.hidden_at = BattlefieldId::fromRaw(bf);
        c.hidden_on_turn = 2;
        state.battlefields[bf].facedown.push_back(card);
        return card;
    }
    bool facedownAt(GameObjectId card, int bf) {
        const auto& f = state.battlefields[bf].facedown;
        return std::find(f.begin(), f.end(), card) != f.end();
    }
    bool inTrashOf(PlayerId p, GameObjectId card) {
        const auto& t = state.player(p).trash;
        return std::find(t.begin(), t.end(), card) != t.end();
    }
    /// P2 plays "return a unit to its owner's hand" on `unit`; both players
    /// get priority on it; it resolves; the engine settles.
    void enemySpellReturns(GameObjectId unit, AgentInterface* p1_agent) {
        TLookBack::move = TLookBack::Move::ReturnToHand;
        const GameObjectId spell = addToHand(P2, kTLookBack);
        Intent play;
        play.type = IntentType::PlayCard;
        play.player = P2;
        play.card = spell;
        play.targets = {unit};
        withEngine([&](GameEngine& e) {
            e.testHook_executeIntent(play);
            settleLikeTheEngine(e);
        }, p1_agent, nullptr, P2);
    }
    void SetUp() override {
        PerfEngineLookBack::SetUp();
        state.turn.turn_number = 3;
        state.battlefields[0].controller = P1;
    }
};

// (a) and (d): the facedown play IS offered in the Closed State with the
// enemy spell on the chain; played, the Hourglass is at that battlefield,
// faceup; nothing dies, so it is not used; the cleanup recalls it to base;
// and from the base it replaces a later death.
TEST_F(PerfEngineHiddenHourglass, PlayedInResponse_NothingDies_ItIsRecalledToBaseAndStillWorks) {
    const GameObjectId only_unit = addUnit(P1, kShipyardSkulker, 0, 0);
    const GameObjectId hourglass = hide(P1, kZhonyasHourglass, 0);
    HiddenCardAgent p1(hourglass, /*play_it=*/true);
    enemySpellReturns(only_unit, &p1);

    EXPECT_GE(p1.offered, 1) << "the facedown play is offered in response to the spell";
    ASSERT_TRUE(p1.played);
    EXPECT_EQ(o(only_unit).zone, ZoneType::Hand) << "returned to hand: nothing died";
    EXPECT_FALSE(state.battlefields[0].controller.has_value()) << "control lost (CR 323.5)";
    const GameObject& g = o(hourglass);
    ASSERT_TRUE(g.location.has_value()) << "unused, and still on the board";
    EXPECT_TRUE(g.isAtBase()) << "recalled to its controller's base (CR 149.3 / 323.6)";
    EXPECT_EQ(*g.location, LocationId{BaseLocation{P1}});
    EXPECT_FALSE(g.is_hidden) << "faceup";
    EXPECT_FALSE(facedownAt(hourglass, 0));
    EXPECT_FALSE(inTrashOf(P1, hourglass));
    EXPECT_EQ(g.controller, P1);
    // Control of the battlefield went in the SAME cleanup that recalled it:
    // losing the battlefield does not discard a faceup gear (CR 323.6.a is
    // about Hidden cards), it is recalled like any unattached gear there.
    EXPECT_EQ(g.zone, ZoneType::Base);
    EXPECT_FALSE(g.attached_to.has_value());
    EXPECT_FALSE(g.is_exhausted) << "unused";

    // A later death, anywhere: it is replaced from the base.
    const GameObjectId later = addUnit(P1, kShipyardSkulker, 0, 1);
    withEngine([&](GameEngine& e) {
        e.mutableState().getObject(later).damage_marked = 99;
        e.testHook_processLethalDamage();
        settleLikeTheEngine(e);
    });
    ASSERT_TRUE(onBoard(later)) << "its death was replaced";
    EXPECT_TRUE(o(later).isAtBase());
    EXPECT_EQ(o(hourglass).zone, ZoneType::Trash) << "killed instead";
}

// (b): not played — in the owner's trash after the cleanup that follows the
// loss of control. It was offered, and declined.
TEST_F(PerfEngineHiddenHourglass, NotPlayed_TheFacedownCardGoesToItsOwnersTrash) {
    const GameObjectId only_unit = addUnit(P1, kShipyardSkulker, 0, 0);
    const GameObjectId hourglass = hide(P1, kZhonyasHourglass, 0);
    HiddenCardAgent p1(hourglass, /*play_it=*/false);
    enemySpellReturns(only_unit, &p1);

    EXPECT_GE(p1.offered, 1);
    EXPECT_FALSE(p1.played);
    EXPECT_FALSE(state.battlefields[0].controller.has_value());
    EXPECT_FALSE(facedownAt(hourglass, 0));
    EXPECT_TRUE(inTrashOf(P1, hourglass)) << "CR 323.6.a";
    EXPECT_EQ(o(hourglass).zone, ZoneType::Trash);
    EXPECT_FALSE(o(hourglass).is_hidden);
    EXPECT_FALSE(o(hourglass).location.has_value());
}

// (c): one cleanup, a faceup Hourglass and a different facedown card at the
// battlefield that is lost: control goes (323.5), the faceup gear is recalled
// (323.6), the facedown card is trashed (323.6.a). Neither takes the other's
// way out.
TEST_F(PerfEngineHiddenHourglass, OneCleanup_ControlLost_FaceupGearRecalled_FacedownCardTrashed) {
    const GameObjectId hourglass = addEquipment(P1, kZhonyasHourglass);
    o(hourglass).location = LocationId{BattlefieldLocation{0}};
    o(hourglass).zone = ZoneType::BattlefieldZone;
    const GameObjectId facedown = hide(P1, kFoxFire, 0);
    // No unit of P1's at battlefield 0; an enemy unit at the other one.
    addUnit(P2, kShipyardSkulker, 0, 1);
    std::vector<std::string> order;
    auto conn = events.on_log.connect([&](const LogEvent& e) {
        if (e.message.find("RECALL (CR 323.6)") != std::string::npos) order.push_back("recall");
        if (e.message.find("removed from facedown") != std::string::npos) order.push_back("hidden");
    });
    cleanup();
    conn.disconnect();
    EXPECT_FALSE(state.battlefields[0].controller.has_value());
    ASSERT_TRUE(o(hourglass).location.has_value());
    EXPECT_TRUE(o(hourglass).isAtBase());
    EXPECT_FALSE(inTrashOf(P1, hourglass));
    EXPECT_TRUE(inTrashOf(P1, facedown));
    EXPECT_FALSE(facedownAt(facedown, 0));
    // The engine removes the hidden card before it recalls the gear; the rule
    // lists the recall first. The two act on different objects, so the order
    // cannot be seen in the result: pinned as it is.
    EXPECT_EQ(order, (std::vector<std::string>{"hidden", "recall"}));
}

// (d), second half: with control already lost, the facedown play is not
// offered. In a game the same cleanup that takes control away also trashes
// the card, so there is no window; the state here is built by hand to ask
// the generator directly.
TEST_F(PerfEngineHiddenHourglass, WithControlLostTheFacedownPlayIsNotOffered) {
    const GameObjectId hourglass = hide(P1, kZhonyasHourglass, 0);
    addUnit(P1, kShipyardSkulker, 0, 1);
    auto offered = [&]() {
        int n = 0;
        state.turn.phase = TurnPhase::MainPhase;
        state.turn.turn_player = P2;
        withEngine([&](GameEngine& e) {
            e.mutableState().turn.oc_state = OpenClosedState::Closed;
            e.mutableState().turn.priority_holder = P1;
            for (const auto& i : e.generateLegalActions())
                if (i.card == hourglass) ++n;
            e.mutableState().turn.oc_state = OpenClosedState::Open;
            e.mutableState().turn.priority_holder = std::nullopt;
        }, nullptr, nullptr, P2);
        return n;
    };
    state.battlefields[0].controller = P1;
    EXPECT_EQ(offered(), 1) << "the control: it is offered while P1 controls the battlefield";
    state.battlefields[0].controller = std::nullopt;
    EXPECT_EQ(offered(), 0) << "nobody controls it";
    state.battlefields[0].controller = P2;
    EXPECT_EQ(offered(), 0) << "the other player controls it";
}


// Found while reading the patch notes for 9.PP3 — Vendetta rules FAQ, the
// Hidden Blade examples: "If a unit at a battlefield has been affected by
// Smite previously this turn and it is chosen by Hidden Blade, that unit's
// controller will not draw 2 ... the unit was moved to a non-board zone by an
// effect other than Hidden Blade and thus no longer has a controller that
// Hidden Blade can reference." And with Zhonya's Hourglass the unit "is on
// the board and can have its information checked": its controller draws.
TEST_F(PerfEngineHiddenHourglass, HiddenBlade_ADeathReplacedByABanishDrawsNobodyCards) {
    constexpr CardDefId kHiddenBladeCard = 213;
    for (int i = 0; i < 6; ++i) { addToDeck(P1, kShipyardSkulker); addToDeck(P2, kShipyardSkulker); }
    // Smite on it: "If it would die this turn, banish it instead."
    const GameObjectId smitten = addUnit(P1, kShipyardSkulker, 0, 0);
    o(smitten).banish_if_would_die_this_turn = true;
    const GameObjectId blade = addToHand(P2, kHiddenBladeCard);
    size_t p1_hand = state.player(P1).hand.size();
    EnginePathAgent picker(EnginePathAgent::choosing(smitten));
    playThroughEngine(blade, nullptr, std::nullopt, &picker);
    ASSERT_EQ(o(smitten).zone, ZoneType::Banishment);
    EXPECT_EQ(state.player(P1).hand.size(), p1_hand) << "no controller to reference: no draw";

    // Saved by Zhonya's Hourglass (recalled, on the board): its controller draws 2.
    const GameObjectId saved = addUnit(P1, kShipyardSkulker, 0, 0);
    addEquipment(P1, kZhonyasHourglass);
    const GameObjectId blade2 = addToHand(P2, kHiddenBladeCard);
    p1_hand = state.player(P1).hand.size();
    EnginePathAgent picker2(EnginePathAgent::choosing(saved));
    playThroughEngine(blade2, nullptr, std::nullopt, &picker2);
    ASSERT_TRUE(onBoard(saved));
    EXPECT_EQ(state.player(P1).hand.size(), p1_hand + 2);
}


// ─── 9.QQ1: every Hidden Blade example of the Vendetta rules FAQ ────────────
// "Kill a unit at a battlefield. Its controller draws 2." The FAQ passage on
// ignored and negated instructions:
//   1. the unit is returned to hand in reaction (Star-Crossed): the kill
//      mistargets, "Its controller draws 2" is ignored;
//   2. [Repeat] (the Academy) with both executions on the same unit: "the
//      unit's controller doesn't draw 4 cards" — the second mistargets;
//   3. "I can't be killed": negated, the controller still draws 2 — NOT
//      testable here: no card in the engine says "can't be killed" and there
//      is no such hook; the on-board branch it would take is example 5's;
//   4. moved to base in reaction (Flash): ignored, no draw;
//   5. Zhonya's Hourglass in reaction: on the board, its controller draws 2;
//   6. Smite earlier this turn: banished instead, nobody draws.
// 5 and 6 are HiddenBlade_ADeathReplacedByABanishDrawsNobodyCards above.
class PerfEngineHiddenBladeFaq : public PerfEngineHiddenHourglass {
protected:
    static constexpr CardDefId kHiddenBladeCard = 213;
    static constexpr CardDefId kDetonate = 328;
    void SetUp() override {
        PerfEngineHiddenHourglass::SetUp();
        for (int i = 0; i < 8; ++i) { addToDeck(P1, kShipyardSkulker); addToDeck(P2, kShipyardSkulker); }
    }
    size_t hand(PlayerId p) { return state.player(p).hand.size(); }
    /// P2 plays `spell` choosing `target`; `in_reaction` runs with the spell on
    /// the chain, before it resolves.
    void playChoosing(GameObjectId spell, GameObjectId target, bool pay_repeat,
                      const std::function<void(EffectExecutor&, GameState&)>& in_reaction = {}) {
        state.player(P2).rune_pool.energy += 12;
        state.player(P2).rune_pool.universal_power += 6;
        EnginePathAgent p2([&, target, pay_repeat](const std::vector<Intent>& legal) -> Intent {
            for (const auto& i : legal)
                if (!i.chosen_objects.empty() && i.chosen_objects.front() == target) return i;
            for (const auto& i : legal)
                if (i.chosen_value == std::optional<int>{pay_repeat ? 1 : 0}) return i;
            return legal.front();
        });
        Intent play;
        play.type = IntentType::PlayCard;
        play.player = P2;
        play.card = spell;
        if (state.getObject(spell).card_def_id == kDetonate) play.targets = {target};
        // "In reaction": the first time P1 is given priority with the spell
        // on the chain (finalized, not resolved).
        GameEngine* live = nullptr;
        bool reacted = false;
        riftbound::test::PickByPredicateAgent p1([&](int, const std::vector<Intent>& legal) {
            const bool priority = std::any_of(legal.begin(), legal.end(), [](const Intent& i) {
                return i.type == IntentType::PassPriority; });
            if (priority && !reacted && live && in_reaction) {
                reacted = true;
                in_reaction(live->testHook_executor(), live->mutableState());
            }
            for (const auto& i : legal)
                if (i.type == IntentType::PassPriority) return i;
            return legal.front();
        });
        withEngine([&](GameEngine& e) {
            live = &e;
            e.testHook_executeIntent(play);
            settleLikeTheEngine(e);
            live = nullptr;
        }, &p1, &p2, P2);
        if (in_reaction) { EXPECT_TRUE(reacted) << "P1 was never given priority on the spell"; }
    }
};

TEST_F(PerfEngineHiddenBladeFaq, ReturnedToHandInReaction_TheDrawIsIgnored) {            // example 1
    const GameObjectId unit = addUnit(P1, kShipyardSkulker, 0, 0);
    const GameObjectId blade = addToHand(P2, kHiddenBladeCard);
    const size_t before = hand(P1);
    playChoosing(blade, unit, false, [&](EffectExecutor& x, GameState&) { x.bounceToHand(unit); });
    EXPECT_EQ(o(unit).zone, ZoneType::Hand);
    EXPECT_EQ(hand(P1), before + 1) << "the returned unit only: no draw";
}

TEST_F(PerfEngineHiddenBladeFaq, MovedToBaseInReaction_TheDrawIsIgnored) {               // example 4
    const GameObjectId unit = addUnit(P1, kShipyardSkulker, 0, 0);
    const GameObjectId blade = addToHand(P2, kHiddenBladeCard);
    const size_t before = hand(P1);
    playChoosing(blade, unit, false, [&](EffectExecutor& x, GameState&) { x.moveToBase(unit); });
    ASSERT_TRUE(onBoard(unit)) << "not at a battlefield any more: not killed";
    EXPECT_TRUE(o(unit).isAtBase());
    EXPECT_EQ(hand(P1), before);
}

TEST_F(PerfEngineHiddenBladeFaq, RepeatedOnTheSameUnit_ItsControllerDrawsTwoNotFour) {    // example 2
    const GameObjectId unit = addUnit(P1, kShipyardSkulker, 0, 0);
    const GameObjectId blade = addToHand(P2, kHiddenBladeCard);
    state.player(P2).grant_repeat_base_to_next_spell = true;      // the Academy
    const size_t before = hand(P1);
    int repeats_executed = 0;
    auto conn = events.on_log.connect([&](const LogEvent& e) {
        if (e.message.find("REPEAT: paid 1 extra tranche(s) for Hidden Blade") != std::string::npos)
            ++repeats_executed;
    });
    playChoosing(blade, unit, /*pay_repeat=*/true);
    conn.disconnect();
    ASSERT_EQ(repeats_executed, 1) << "the Repeat was paid";
    EXPECT_EQ(o(unit).zone, ZoneType::Trash);
    EXPECT_EQ(hand(P1), before + 2) << "the second execution mistargets: no second draw";
}

// The control for example 2: a Repeat that chooses ANOTHER unit does draw again.
TEST_F(PerfEngineHiddenBladeFaq, RepeatedOnTwoUnits_EachControllerDraws) {
    const GameObjectId first = addUnit(P1, kShipyardSkulker, 0, 0);
    const GameObjectId second = addUnit(P1, kShipyardSkulker, 0, 0);
    const GameObjectId blade = addToHand(P2, kHiddenBladeCard);
    state.player(P2).grant_repeat_base_to_next_spell = true;
    state.player(P2).rune_pool.energy += 12;
    state.player(P2).rune_pool.universal_power += 6;
    const size_t before = hand(P1);
    int object_prompts = 0;
    EnginePathAgent p2([&](const std::vector<Intent>& legal) -> Intent {
        const bool objects = std::any_of(legal.begin(), legal.end(), [](const Intent& i) {
            return !i.chosen_objects.empty(); });
        if (objects) {
            const GameObjectId want = object_prompts++ == 0 ? first : second;
            for (const auto& i : legal)
                if (!i.chosen_objects.empty() && i.chosen_objects.front() == want) return i;
        }
        for (const auto& i : legal) if (i.chosen_value == std::optional<int>{1}) return i;
        return legal.front();
    });
    Intent play;
    play.type = IntentType::PlayCard;
    play.player = P2;
    play.card = blade;
    withEngine([&](GameEngine& e) {
        e.testHook_executeIntent(play);
        settleLikeTheEngine(e);
    }, nullptr, &p2, P2);
    EXPECT_EQ(o(first).zone, ZoneType::Trash);
    EXPECT_EQ(o(second).zone, ZoneType::Trash);
    EXPECT_EQ(hand(P1), before + 4);
}

// Detonate 328, "Kill a gear. Its controller draws 2.": the same passage.
TEST_F(PerfEngineHiddenBladeFaq, Detonate_FollowsTheSameRules) {
    // Killed: its controller draws 2.
    const GameObjectId gear = addEquipment(P1, kLongSword);
    size_t before = hand(P1);
    playChoosing(addToHand(P2, kDetonate), gear, false);
    EXPECT_EQ(o(gear).zone, ZoneType::Trash);
    EXPECT_EQ(hand(P1), before + 2);
    // Returned to hand in reaction: ignored.
    const GameObjectId gone = addEquipment(P1, kLongSword);
    before = hand(P1);
    playChoosing(addToHand(P2, kDetonate), gone, false,
                 [&](EffectExecutor& x, GameState&) { x.bounceToHand(gone); });
    EXPECT_EQ(o(gone).zone, ZoneType::Hand);
    EXPECT_EQ(hand(P1), before + 1) << "the returned gear only";
    // Repeated on the same gear: 2, not 4.
    const GameObjectId twice = addEquipment(P1, kLongSword);
    state.player(P2).grant_repeat_base_to_next_spell = true;
    before = hand(P1);
    playChoosing(addToHand(P2, kDetonate), twice, /*pay_repeat=*/true);
    EXPECT_EQ(o(twice).zone, ZoneType::Trash);
    EXPECT_EQ(hand(P1), before + 2);
}

}  // namespace test_review_fix_perf_engine
