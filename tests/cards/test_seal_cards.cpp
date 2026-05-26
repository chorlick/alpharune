/// @file test_seal_cards.cpp
/// The Seal gear cards: "[E]: [Reaction] — [Add] [<color>]." Activating a Seal
/// must add exactly one power of its domain to the controller's rune pool.
/// Regression for the generated stubs that declared the ability but never
/// implemented onActivate (so activation added nothing). Both printings of each
/// Seal (duplicate names across sets) are covered.

#include "tests/cards/card_test_fixture.h"

#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/types.h"
#include "engine/effect_executor.h"

#include <gtest/gtest.h>

namespace riftbound::test {
namespace {

class SealTest : public CardTestFixture {
protected:
    // Activate the Seal card `id` (controlled by P1) and return how much power
    // of `dom` landed in P1's pool.
    int powerAddedBySeal(CardDefId id, Domain dom) {
        auto src = state.createObject();
        auto& o = state.getObject(src);
        o.owner = PlayerId::Player1; o.controller = PlayerId::Player1;
        o.card_type = CardType::Gear;
        o.zone = ZoneType::Base;
        o.location = BaseLocation{PlayerId::Player1};

        Card* c = card_registry.get(id);
        EXPECT_NE(c, nullptr) << "seal " << id << " not registered";
        if (!c) return -1;
        EXPECT_TRUE(c->hasActivatedAbility());
        EXPECT_TRUE(c->isReactionAbility()) << "Seals are [Reaction] abilities";

        int before = state.player(PlayerId::Player1)
                         .rune_pool.power[static_cast<int>(dom)];
        EffectExecutor exec(state, events, card_db, &card_registry);
        CardContext ctx{state, events, exec, PlayerId::Player1, src};
        c->onActivate(ctx, {});
        int after = state.player(PlayerId::Player1)
                        .rune_pool.power[static_cast<int>(dom)];
        return after - before;
    }
};

TEST_F(SealTest, EachSealAddsOnePowerOfItsDomain) {
    struct Case { CardDefId id; Domain dom; const char* name; };
    const Case cases[] = {
        {40,  Domain::Fury,  "Seal of Rage [R]"},
        {81,  Domain::Calm,  "Seal of Focus [G]"},
        {120, Domain::Mind,  "Seal of Insight [B]"},
        {163, Domain::Body,  "Seal of Strength [O]"},
        {204, Domain::Chaos, "Seal of Discord [P]"},
        {245, Domain::Order, "Seal of Unity [Y]"},
        // second printings (duplicate names, different ids)
        {536, Domain::Fury,  "Seal of Rage (set 2)"},
        {541, Domain::Mind,  "Seal of Insight (set 2)"},
        {549, Domain::Order, "Seal of Unity (set 2)"},
    };
    // powerAddedBySeal measures a delta, so running them against the same
    // fixture state is fine even when two Seals share a domain.
    for (const auto& c : cases) {
        EXPECT_EQ(powerAddedBySeal(c.id, c.dom), 1)
            << c.name << " should add exactly 1 " << static_cast<int>(c.dom)
            << "-power";
    }
}

TEST_F(SealTest, SealDoesNotAddOtherDomains) {
    // Seal of Rage adds Fury only — Calm/Mind/etc. stay at 0.
    EXPECT_EQ(powerAddedBySeal(40, Domain::Calm), 0);
}

}  // namespace
}  // namespace riftbound::test
