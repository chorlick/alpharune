/// @file test_the_list.cpp
/// Tests for [700] The List — string-valued per-object storage + named-tag
/// targeting.
///
/// "As you play this, name a tag. [E]: Give a unit with the named tag -2 [M]
///  this turn."
///   - onPlay names a tag (heuristic: most common enemy-unit tag) and persists
///     it on the gear instance via GameObject::string_state["named_tag"].
///   - The [E] ability only debuffs units carrying that named tag.

#include "tests/cards/card_test_fixture.h"

#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/events.h"
#include "core/game_state.h"
#include "engine/effect_executor.h"

#include <gtest/gtest.h>

namespace riftbound::test {
namespace {

constexpr CardDefId kTheList = 700;

// Create a The List gear object controlled by `owner`.
GameObjectId makeList(GameState& s, PlayerId owner) {
    auto id = s.createObject();
    auto& o = s.getObject(id);
    o.owner = owner; o.controller = owner;
    o.card_def_id = kTheList;
    o.card_type = CardType::Gear;
    o.name = "The List";
    o.zone = ZoneType::Base;
    o.location = BaseLocation{owner};
    return id;
}

// A unit with explicit tags at a battlefield.
GameObjectId taggedUnit(GameState& s, PlayerId owner, int might,
                        std::vector<std::string> tags) {
    auto id = s.createObject();
    auto& o = s.getObject(id);
    o.owner = owner; o.controller = owner;
    o.card_type = CardType::Unit;
    o.base_might = might; o.current_might = might;
    o.tags = std::move(tags);
    o.zone = ZoneType::BattlefieldZone;
    o.location = BattlefieldLocation{0};
    return id;
}

TEST_F(CardTestFixture, TheList_OnPlayNamesMostCommonEnemyTag) {
    auto gear = makeList(state, P1);
    taggedUnit(state, P2, 3, {"Poro"});
    taggedUnit(state, P2, 3, {"Poro", "Demacia"});
    taggedUnit(state, P2, 3, {"Demacia"});  // Poro:2, Demacia:2 → lexical "Demacia"

    EffectExecutor exec(state, events, card_db, &card_registry);
    CardContext ctx{state, events, exec, P1, gear};
    Card* c = card_registry.get(kTheList);
    ASSERT_NE(c, nullptr);
    c->onPlay(ctx);

    // Poro:2, Demacia:2 → tie broken lexically (std::map order) → "Demacia".
    EXPECT_EQ(state.getObject(gear).string_state["named_tag"], "Demacia");
}

TEST_F(CardTestFixture, TheList_OnPlayClearWinnerNamed) {
    auto gear = makeList(state, P1);
    taggedUnit(state, P2, 3, {"Poro"});
    taggedUnit(state, P2, 3, {"Poro"});
    taggedUnit(state, P2, 3, {"Demacia"});  // Poro:2 > Demacia:1

    EffectExecutor exec(state, events, card_db, &card_registry);
    CardContext ctx{state, events, exec, P1, gear};
    card_registry.get(kTheList)->onPlay(ctx);

    EXPECT_EQ(state.getObject(gear).string_state["named_tag"], "Poro");
}

TEST_F(CardTestFixture, TheList_OnPlayNoEnemyUnitsNamesNothing) {
    auto gear = makeList(state, P1);
    EffectExecutor exec(state, events, card_db, &card_registry);
    CardContext ctx{state, events, exec, P1, gear};
    card_registry.get(kTheList)->onPlay(ctx);

    EXPECT_TRUE(state.getObject(gear).string_state.find("named_tag") ==
                state.getObject(gear).string_state.end())
        << "no enemy units → no tag named";
}

TEST_F(CardTestFixture, TheList_AbilityDebuffsOnlyNamedTagUnit) {
    auto gear = makeList(state, P1);
    state.getObject(gear).string_state["named_tag"] = "Poro";

    auto poro     = taggedUnit(state, P2, /*might=*/4, {"Poro"});
    auto not_poro = taggedUnit(state, P2, /*might=*/4, {"Demacia"});

    EffectExecutor exec(state, events, card_db, &card_registry);
    CardContext ctx{state, events, exec, P1, gear};
    // No chain.resuming → pickTarget auto-picks legal.front(), which is built
    // only from named-tag units.
    card_registry.get(kTheList)->onActivate(ctx, 0, {});

    EXPECT_EQ(state.getObject(poro).current_might, 2) << "named-tag unit gets -2";
    EXPECT_EQ(state.getObject(not_poro).current_might, 4) << "other tag untouched";
}

TEST_F(CardTestFixture, TheList_AbilityNoOpWhenNoNamedTagUnitPresent) {
    auto gear = makeList(state, P1);
    state.getObject(gear).string_state["named_tag"] = "Poro";
    auto demacia = taggedUnit(state, P2, /*might=*/4, {"Demacia"});

    EffectExecutor exec(state, events, card_db, &card_registry);
    CardContext ctx{state, events, exec, P1, gear};
    card_registry.get(kTheList)->onActivate(ctx, 0, {});

    EXPECT_EQ(state.getObject(demacia).current_might, 4)
        << "no unit carries the named tag → no debuff";
}

}  // namespace
}  // namespace riftbound::test
