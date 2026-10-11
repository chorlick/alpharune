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
#include "core/card_db.h"
#include "core/events.h"
#include "engine/game_engine.h"
#include "engine/knowledge_tracker.h"
#include "engine/resource_payment.h"
#include "io/state_editor.h"
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

// ─── 6.6 / 6.11: cleanup ────────────────────────────────────────────────────

constexpr CardDefId kTCountingElder = 1040;   // unit: "any amount of your damage is enough to kill"

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
    for (BattlefieldId b = 0; b < 3; ++b) {
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
    GameObject& a = st.getObject(3);
    GameObject& b = st.getObject(10);

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
    GameObject local = st.getObject(5);
    local.location = BattlefieldLocation{0};
    local.zone = ZoneType::BattlefieldZone;
    expectIndexMatchesAWalk(st, "a local copy edited: nothing in the table changed");
    st.getObject(5) = local;
    expectIndexMatchesAWalk(st, "an object assigned from a local copy");
    std::swap(st.getObject(6), st.getObject(9));
    std::swap(st.getObject(6).id, st.getObject(9).id);
    expectIndexMatchesAWalk(st, "two objects swapped");
    st.objects[8] = GameObject{};
    st.objects[8].id = 8;
    expectIndexMatchesAWalk(st, "an object replaced by a blank one");

    // New and removed objects.
    const GameObjectId fresh = st.createObject();
    st.getObject(fresh).card_type = CardType::Unit;
    st.getObject(fresh).controller = PlayerId::Player1;
    st.getObject(fresh).location = BattlefieldLocation{1};
    expectIndexMatchesAWalk(st, "an object created and placed");
    GameObject built;
    built.id = 999;
    built.card_type = CardType::Unit;
    built.controller = PlayerId::Player2;
    built.location = BaseLocation{PlayerId::Player2};
    st.objects.emplace(GameObjectId{999}, built);
    expectIndexMatchesAWalk(st, "an object emplaced already placed");
    st.getObject(999).location = BattlefieldLocation{0};
    expectIndexMatchesAWalk(st, "... and moved afterwards");
    st.objects.erase(GameObjectId{999});
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
    copy.getObject(2).location = BattlefieldLocation{1};
    copy.getObject(4).location.reset();
    expectIndexMatchesAWalk(copy, "copy, after moving two of its objects");
    expectIndexMatchesAWalk(original, "original, untouched by the copy");
    EXPECT_NE(Walk::ids(copy.objects.onBoard()), Walk::ids(original.objects.onBoard()));

    original.getObject(2).location = BattlefieldLocation{2};
    expectIndexMatchesAWalk(original, "original, moved after the copy was taken");
    expectIndexMatchesAWalk(copy, "copy, untouched by the original");

    // Assigned over an existing state (the engine's `state_ = snapshot`).
    copy = original;
    expectIndexMatchesAWalk(copy, "assigned");
    copy.getObject(2).location.reset();
    expectIndexMatchesAWalk(copy, "assigned, then moved");
    expectIndexMatchesAWalk(original, "source of the assignment");

    // Moved: the objects and their index travel together.
    GameState moved = std::move(copy);
    expectIndexMatchesAWalk(moved, "moved-to");
    moved.getObject(7).location = BattlefieldLocation{0};
    moved.getObject(7).zone = ZoneType::BattlefieldZone;
    expectIndexMatchesAWalk(moved, "moved-to, then an object moved");
    GameState target = scatteredState(5);
    target = std::move(moved);
    target.getObject(7).location.reset();
    expectIndexMatchesAWalk(target, "move-assigned, then an object moved");
    std::swap(target, original);
    target.getObject(1).location = BattlefieldLocation{1};
    original.getObject(1).location = BattlefieldLocation{2};
    expectIndexMatchesAWalk(target, "swapped (a)");
    expectIndexMatchesAWalk(original, "swapped (b)");
}

TEST(PerfEngineTable, QueriesBetweenTwoMovesShareOneWalk) {
    GameState st = scatteredState(60);
    (void)st.unitsAt(LocationId{BattlefieldLocation{0}}, PlayerId::Player1);
    const std::size_t built = st.objects.indexRebuilds();
    for (int i = 0; i < 50; ++i) {
        (void)st.unitsAt(LocationId{BattlefieldLocation{static_cast<BattlefieldId>(i % 3)}}, PlayerId::Player2);
        (void)st.allUnitsControlledBy(PlayerId::Player1);
        (void)st.runesInBase(PlayerId::Player2);
        st.getObject(4).is_exhausted = !st.getObject(4).is_exhausted;   // not a move
        st.getObject(4).controller = PlayerId::Player1;
    }
    EXPECT_EQ(st.objects.indexRebuilds(), built) << "150 queries, no object moved";
    // Writing the value a field already has is not a move either.
    st.getObject(2).location = st.getObject(2).location;
    st.getObject(2).zone = st.getObject(2).zone.get();
    (void)st.objects.onBoard();
    EXPECT_EQ(st.objects.indexRebuilds(), built);
    st.getObject(2).location = BattlefieldLocation{1};
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
    EXPECT_EQ(card_registry.get(0), nullptr);
    EXPECT_EQ(card_registry.get(999), nullptr);
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
    const ObjectStateChangedEvent e{7, "chosen_by_controller"};
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
        const GameObjectId mover = static_cast<GameObjectId>(1 + round % 100);
        if (state.getObject(mover).location.has_value()) state.getObject(mover).location.reset();
        else state.getObject(mover).location = BattlefieldLocation{static_cast<BattlefieldId>(round % 3)};
        const auto board = Walk::where(shared, [](const GameObject& o) { return o.location.has_value(); });
        const LocationId here{BattlefieldLocation{static_cast<BattlefieldId>(round % 3)}};
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

}  // namespace test_review_fix_perf_engine
