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

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
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

// 6.11 — the "becomes [Mighty]" edge is kept in GameObject::was_mighty, not in
// a string-keyed counter: it fires once on the way up, re-arms on the way
// down and is forgotten off the board.
TEST_F(PerfEngineCards, BecomingMightyIsAnnouncedOncePerCrossing) {
    const GameObjectId unit = addUnit(P1, kInvalidId, 4);
    int announced = 0;
    auto conn = events.on_object_state_changed.connect([&](const ObjectStateChangedEvent& e) {
        if (e.object == unit && e.what_changed == "became_mighty") ++announced;
    });
    auto cleanup = [&] { withEngine([](GameEngine& e) { e.testHook_cleanup(); }); };
    cleanup();
    EXPECT_EQ(announced, 0);
    EXPECT_FALSE(o(unit).was_mighty);
    o(unit).base_might = 5;
    cleanup();
    EXPECT_EQ(announced, 1);
    EXPECT_TRUE(o(unit).was_mighty);
    cleanup();
    EXPECT_EQ(announced, 1) << "still Mighty: not announced again";
    o(unit).base_might = 4;
    cleanup();
    EXPECT_FALSE(o(unit).was_mighty) << "below 5 the edge is armed again";
    o(unit).base_might = 6;
    cleanup();
    EXPECT_EQ(announced, 2);
    // Off the board the edge is forgotten: back at 5+ it becomes Mighty again.
    o(unit).location = std::nullopt;
    o(unit).zone = ZoneType::Hand;
    cleanup();
    EXPECT_FALSE(o(unit).was_mighty);
    o(unit).location = BaseLocation{P1};
    o(unit).zone = ZoneType::Base;
    cleanup();
    EXPECT_EQ(announced, 3);
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
    // One of them leaves the board (to a hand) before the next pass.
    o(gone).location = std::nullopt;
    o(gone).zone = ZoneType::Hand;
    state.player(P1).hand.push_back(gone);
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

}  // namespace test_review_fix_perf_engine
