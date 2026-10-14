/// @file test_the_list.cpp
/// Tests for [700] The List — string-valued per-object storage + named-tag
/// targeting.
///
/// "As you play this, name a tag. [E]: Give a unit with the named tag -2 [M]
///  this turn."
///   - "As you play this" the controller names a tag (an AsYouPlayMe ability;
///     with no chain item to suspend on it defaults to the first tag offered,
///     the most common enemy-unit tag). It persists on the gear instance via
///     GameObject::string_state["named_tag"]. The choice itself is tested
///     through the engine in test_review_fix_c1.cpp.
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

// Testing phase D46 (by design). These three tests pinned the old naming: a
// triggered ability on the chain that named the most common enemy tag (later a
// choice among at most 16 public tags), and named nothing with no tagged unit
// in view. "As you play this, name a tag" is a choice made as the card is
// played (CR 355.1) among every tag that exists (CR 763, 763.1).
// Old: TheList_OnPlayNamesMostCommonEnemyTag -> "Demacia" by a tie-break.
// New: the player is offered every tag of CR 763.1 and names the one chosen.
TEST_F(CardTestFixture, TheList_AsItIsPlayedThePlayerNamesAnyTagOfTheList) {
    taggedUnit(state, P2, 3, {"Poro"});
    taggedUnit(state, P2, 3, {"Demacia"});
    auto list = addToHand(P1, kTheList);
    size_t offered = 0;
    EnginePathAgent agent([&](const std::vector<Intent>& legal) {
        offered = legal.size();
        for (const auto& i : legal)
            if (i.choice_label == "Name the tag: Yordle") return i;
        return legal.front();
    });
    playThroughEngine(list, &agent);
    EXPECT_EQ(offered, 137u) << "the 137 tags of CR 763.1";
    EXPECT_EQ(state.getObject(list).string_state["named_tag"], "Yordle");
}

// Old: TheList_OnPlayClearWinnerNamed -> "Poro", the most common enemy tag.
// New: the most common enemy tag is named only if the player names it.
TEST_F(CardTestFixture, TheList_AsItIsPlayedTheMostCommonEnemyTagIsNamedIfChosen) {
    taggedUnit(state, P2, 3, {"Poro"});
    taggedUnit(state, P2, 3, {"Poro"});
    taggedUnit(state, P2, 3, {"Demacia"});
    auto list = addToHand(P1, kTheList);
    EnginePathAgent agent([](const std::vector<Intent>& legal) {
        for (const auto& i : legal)
            if (i.choice_label == "Name the tag: Poro") return i;
        return legal.front();
    });
    playThroughEngine(list, &agent);
    EXPECT_EQ(state.getObject(list).string_state["named_tag"], "Poro");
}

// Old: TheList_OnPlayNoEnemyUnitsNamesNothing -> no tag named.
// New: with no tagged unit anywhere a tag is still named (CR 763.1).
TEST_F(CardTestFixture, TheList_AsItIsPlayedATagIsNamedWithNoTaggedUnitInView) {
    auto list = addToHand(P1, kTheList);
    EnginePathAgent agent([](const std::vector<Intent>& legal) {
        for (const auto& i : legal)
            if (i.choice_label == "Name the tag: Zilean") return i;
        return legal.front();
    });
    playThroughEngine(list, &agent);
    EXPECT_EQ(state.getObject(list).string_state["named_tag"], "Zilean");
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
