/// @file test_wave_b_misc2.cpp
///   Mageseeker Investigator (725) — sets BattlefieldState::surcharge_enemy_multi_move
///   The Dreaming Tree (287)       — first friendly-unit choice here each turn -> draw 1

#include "tests/cards/card_test_fixture.h"
#include <gtest/gtest.h>

namespace riftbound::test {
namespace {

using Misc2Test = CardTestFixture;
constexpr CardDefId kInvestigator = 725;
constexpr CardDefId kDreamingTree = 287;

TEST_F(Misc2Test, MageseekerInvestigator_FlagsItsBattlefield) {
    addUnit(P1, kInvestigator, 4, /*at_bf=*/0);
    EXPECT_FALSE(state.battlefields[0].surcharge_enemy_multi_move);
    card_registry.get(kInvestigator)->applyPassiveAura(state, P1);
    EXPECT_TRUE(state.battlefields[0].surcharge_enemy_multi_move);
    EXPECT_FALSE(state.battlefields[1].surcharge_enemy_multi_move);
}

TEST_F(Misc2Test, DreamingTree_DrawsOncePerTurn) {
    auto tree = addUnit(P1, kDreamingTree, 0, /*at_bf=*/-1);
    state.getObject(tree).card_type = CardType::Battlefield;
    state.battlefields[0].card_object_id = tree;
    addToDeck(P1, 1);
    addToDeck(P1, 1);
    int hand0 = static_cast<int>(state.player(P1).hand.size());

    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, tree};
    ctx.firing_trigger = TriggerType::WhenAFriendlyUnitChosenHere;
    Card* c = card_registry.get(kDreamingTree);
    c->onTrigger(ctx, {});
    EXPECT_EQ(static_cast<int>(state.player(P1).hand.size()), hand0 + 1)
        << "first friendly-unit choice here this turn draws 1";
    c->onTrigger(ctx, {});
    EXPECT_EQ(static_cast<int>(state.player(P1).hand.size()), hand0 + 1)
        << "no draw on the second choice the same turn";
}

}  // namespace
}  // namespace riftbound::test
