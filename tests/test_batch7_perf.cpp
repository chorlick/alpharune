/// @file test_batch7_perf.cpp
/// Batch 7-perf (P1, safe part): what the throughput changes rely on.
///
///  - A probe plays on a copy assigned over the copy the previous probe of its
///    depth played on (GameEngine::parkGame). The proofs that games stay the
///    same rest on such a copy being the copy a constructor makes: the same
///    objects in the same table order (what every walk of the table follows).

#include <gtest/gtest.h>

#include "cards/card_registry.h"
#include "core/card_db.h"
#include "core/events.h"
#include "core/game_state.h"
#include "engine/game_engine.h"
#include "rules/deck_validator.h"

#include <cstdlib>
#include <memory>
#include <random>
#include <string>
#include <vector>

namespace riftbound::test::batch7_perf {

std::string deckPath(const char* name) {
    const char* root = std::getenv("RIFTBOUND_ROOT");
    return std::string(root ? root : "..") + "/decks/" + name;
}

/// The object ids in table order, with each object's zone and place.
std::vector<int64_t> tableOrder(const GameState& s) {
    std::vector<int64_t> out;
    for (const auto& [id, o] : s.objects) {
        out.push_back(id.raw());
        out.push_back(static_cast<int64_t>(o.zone.get()));
        out.push_back(o.location.has_value());
    }
    out.push_back(-1);
    for (const auto& [id, o] : s.objects.onBoard()) out.push_back(id.raw());
    return out;
}

TEST(Batch7Perf, ACopyAssignedOverAnotherStateIsTheCopyAConstructorMakes) {
    CardRegistry registry;
    registry.loadAll();
    CardDB db;
    db.buildFromClasses(registry);
    const auto d1 = DeckValidator::loadFromDeckList(deckPath("fiora.txt"), db);
    const auto d2 = DeckValidator::loadFromDeckList(deckPath("yi_rebuild.txt"), db);
    int compared = 0;
    for (uint64_t seed : {11u, 12u, 13u}) {
        EventBus bus;
        GameEngine engine(db, bus, registry);
        engine.beginGame(d1, d2, seed);
        std::mt19937_64 rng(seed);
        // A stale state of another position: what a parking slot holds.
        GameState stale = engine.state();
        for (int n = 0; n < 400 && !engine.isStepDone(); ++n) {
            if (n % 37 == 0) {
                const GameState fresh(engine.state());
                stale = engine.state();   // assigned over the stale copy's nodes
                ASSERT_EQ(tableOrder(stale), tableOrder(fresh)) << "seed " << seed << " decision " << n;
                ASSERT_EQ(tableOrder(stale), tableOrder(engine.state()));
                ++compared;
            }
            const auto& legal = engine.currentStep().legal;
            std::uniform_int_distribution<size_t> pick(0, legal.size() - 1);
            engine.applyChoice(static_cast<int>(pick(rng)));
        }
    }
    EXPECT_GT(compared, 10);
}

}  // namespace riftbound::test::batch7_perf
