/// @file test_finish_batch1.cpp
/// Focused per-card tests for the "finish batch 1" card set.
///
/// Most cards in this batch require engine primitives that do not yet exist
/// (see the ESCALATE(...) notes in each card .cpp) and are therefore NOT
/// tested here — testing un-implemented behavior would be dishonest. The one
/// card whose printed ability is fully implementable with existing primitives
/// is Wily Newtfish (#670), whose continuous conditional self-buff is exercised
/// below.

#include "tests/cards/card_test_fixture.h"

#include "cards/card.h"
#include "core/game_object.h"
#include "core/types.h"

namespace riftbound::test {
namespace {

constexpr CardDefId kWilyNewtfish = 670;

class FinishBatch1Test : public CardTestFixture {
protected:
    // Mirror GameEngine::recalculateAuras for a single card: clear the unit's
    // aura state, invoke the card's applyPassiveAura, then aggregate the pushed
    // AuraEffects into the cached fields and recompute might — exactly what the
    // engine does each cleanup pass.
    void recalcAuraFor(CardDefId def_id, PlayerId controller) {
        for (auto& [id, obj] : state.objects) {
            obj.aura_effects.clear();
            obj.aura_might_bonus = 0;
            obj.aura_keywords.reset();
        }
        Card* card = card_registry.get(def_id);
        ASSERT_NE(card, nullptr);
        card->applyPassiveAura(state, controller);
        for (auto& [id, obj] : state.objects) {
            obj.aura_might_bonus = 0;
            obj.aura_keywords.reset();
            for (auto& ae : obj.aura_effects) {
                obj.aura_might_bonus += ae.might_bonus;
                if (ae.keyword != Keyword::Count) obj.aura_keywords.set(ae.keyword);
            }
            if (obj.isUnit() && obj.location.has_value()) obj.recomputeMight();
        }
    }
};

// With XP gained this turn, Wily Newtfish gains +1 [M] and [Ganking].
TEST_F(FinishBatch1Test, WilyNewtfishBuffedWhenXpGained) {
    auto id = addUnit(P1, kWilyNewtfish, /*might=*/0, /*at_bf=*/0);
    ASSERT_EQ(state.getObject(id).base_might, 4);  // printed might

    // Ganking is NOT a base keyword (it is conditional).
    EXPECT_FALSE(state.getObject(id).keywords.has(Keyword::Ganking));

    state.player(P1).xp_gained_this_turn = 1;
    recalcAuraFor(kWilyNewtfish, P1);

    auto& obj = state.getObject(id);
    EXPECT_EQ(obj.current_might, 5);                  // 4 base + 1 aura
    EXPECT_TRUE(obj.hasKeyword(Keyword::Ganking));    // granted via aura
}

// With no XP gained this turn, no buff and no Ganking.
TEST_F(FinishBatch1Test, WilyNewtfishUnbuffedWithoutXp) {
    auto id = addUnit(P1, kWilyNewtfish, /*might=*/0, /*at_bf=*/0);

    state.player(P1).xp_gained_this_turn = 0;
    recalcAuraFor(kWilyNewtfish, P1);

    auto& obj = state.getObject(id);
    EXPECT_EQ(obj.current_might, 4);                  // unchanged
    EXPECT_FALSE(obj.hasKeyword(Keyword::Ganking));   // not granted
}

// The conditional buff only applies to the controller's own Newtfish — an
// opponent gaining XP does not buff my Newtfish.
TEST_F(FinishBatch1Test, WilyNewtfishIgnoresOpponentXp) {
    auto id = addUnit(P1, kWilyNewtfish, /*might=*/0, /*at_bf=*/0);

    state.player(P2).xp_gained_this_turn = 3;  // opponent gained XP
    state.player(P1).xp_gained_this_turn = 0;  // I did not
    recalcAuraFor(kWilyNewtfish, P1);

    auto& obj = state.getObject(id);
    EXPECT_EQ(obj.current_might, 4);
    EXPECT_FALSE(obj.hasKeyword(Keyword::Ganking));
}

}  // namespace
}  // namespace riftbound::test
