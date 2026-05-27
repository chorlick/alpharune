/// @file test_wave_b_misc.cpp
/// Wave B misc single-card hooks:
///   Kayn, Unleashed (189)  — moves-twice damage immunity (moves_this_turn +
///                            GameObject::immune_to_damage + dealDamage no-op)
///   Ravenborn Tome (32)    — [E] arms PlayerState::next_spell_bonus_damage,
///                            bound onto the next spell (spell_bonus_damage),
///                            added to every instance in dealDamage.

#include "tests/cards/card_test_fixture.h"
#include <gtest/gtest.h>

namespace riftbound::test {
namespace {

using MiscTest = CardTestFixture;

constexpr CardDefId kKayn = 189;
constexpr CardDefId kRavenbornTome = 32;

// ── Kayn, Unleashed: moved twice this turn -> immune to damage ──
TEST_F(MiscTest, Kayn_ImmuneAfterTwoMoves) {
    auto kayn = addUnit(P1, kKayn, 6, /*at_bf=*/0);
    auto src = addUnit(P2, 1, 3, /*at_bf=*/0);
    Card* c = card_registry.get(kKayn);
    EffectExecutor exec(state, events, card_db);

    // 1 move so far -> NOT immune -> takes damage.
    state.getObject(kayn).moves_this_turn = 1;
    state.getObject(kayn).immune_to_damage = false;
    c->applyPassiveAura(state, P1);
    EXPECT_FALSE(state.getObject(kayn).immune_to_damage);
    exec.dealDamage(kayn, 3, src);
    EXPECT_EQ(state.getObject(kayn).damage_marked, 3);

    // Reset, now 2 moves -> immune -> further damage is prevented.
    state.getObject(kayn).damage_marked = 0;
    state.getObject(kayn).moves_this_turn = 2;
    state.getObject(kayn).immune_to_damage = false;  // recalc resets, aura re-asserts
    c->applyPassiveAura(state, P1);
    EXPECT_TRUE(state.getObject(kayn).immune_to_damage);
    exec.dealDamage(kayn, 4, src);
    EXPECT_EQ(state.getObject(kayn).damage_marked, 0)
        << "Kayn takes no damage after moving twice this turn";
}

// ── Ravenborn Tome: [E] arms the next-spell bonus damage rider ──
TEST_F(MiscTest, RavenbornTome_ArmsNextSpellBonus) {
    auto tome = addUnit(P1, kRavenbornTome, 0, /*at_bf=*/-1);
    state.getObject(tome).card_type = CardType::Gear;
    Card* c = card_registry.get(kRavenbornTome);
    ASSERT_TRUE(c->hasActivatedAbility());
    EXPECT_EQ(state.player(P1).next_spell_bonus_damage, 0);
    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, tome};
    c->onActivate(ctx, {});
    EXPECT_EQ(state.player(P1).next_spell_bonus_damage, 1);
}

// ── A spell carrying the bound rider deals +1 to every instance ──
TEST_F(MiscTest, RavenbornTome_BoundRiderAddsToEveryInstance) {
    // Simulate the binding done by executePlaySpell: a spell object whose
    // spell_bonus_damage was set from the player's armed rider.
    auto spell = pushSpellOnChain(P1);
    state.getObject(spell).spell_bonus_damage = 1;
    auto t1 = addUnit(P2, 1, 5, /*at_bf=*/0);
    auto t2 = addUnit(P2, 1, 5, /*at_bf=*/0);
    EffectExecutor exec(state, events, card_db);
    exec.dealDamage(t1, 2, spell);
    exec.dealDamage(t2, 2, spell);
    EXPECT_EQ(state.getObject(t1).damage_marked, 3) << "2 + 1 bonus";
    EXPECT_EQ(state.getObject(t2).damage_marked, 3) << "every instance gets the bonus";
}

}  // namespace
}  // namespace riftbound::test
