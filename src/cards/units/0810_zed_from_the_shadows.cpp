#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class ZedFromTheShadows : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    static constexpr const char* kPaidFlag = "zed_discard_paid";

    // "You may discard 1 as an additional cost to play me."
    // (Offered by GameEngine::maybePayOptionalAdditionalCost, which sets
    //  card_counters[kPaidFlag].)
    OptionalAdditionalCost optionalAdditionalCost() const override {
        OptionalAdditionalCost c;
        c.valid = true;
        c.discard_cards = 1;
        c.paid_flag = kPaidFlag;
        return c;
    }

    // "When you play me, if you paid the additional cost, play a 0 [M] Shadow
    //  Clone unit token."
    // (The token carries the registered Shadow Clone card, id 955.)
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    // "if you paid the additional cost" is part of the trigger condition (CR
    // 383.2.a.1): asked as I am played; unpaid, nothing is put on the chain.
    bool triggerCondition(const GameState& state, GameObjectId source, PlayerId /*controller*/,
                          TriggerType /*trigger*/, GameObjectId /*subject*/) const override {
        if (!state.objectExists(source)) return false;
        const auto& c = state.getObject(source).card_counters;
        const auto it = c.find(kPaidFlag);
        return it != c.end() && it->second != 0;
    }
    // The ability chooses nothing: where the token goes is a choice of the
    // token's play, made as the token is finalized while the ability resolves
    // (CR 420.3.b.2; testing phase M5, which reversed B5's move of it to the
    // ability's finalization).
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // No location is printed: the controller's base or a battlefield
        // they control, their choice (CR 355.2.a).
        // At resolution: an effect play's location is chosen as it plays (CR 420.3.b.2).
        const int dest = pickTokenDest(*this, ctx, "Zed: play the Shadow Clone to");
        if (dest < 0) return;     // waiting for the agent / nowhere to play it
        {
            ctx.executor.createToken(ctx.controller, CardType::Unit, "Shadow Clone",
                                 0, std::vector<std::string>{}, KeywordSet{},
                                 destLocation(ctx.controller, std::max(0, dest)), false,
                                 tokenPlayLocations(ctx),
                                 // the Shadow Clone token card ("When I attack"),
                                 // set before the play is announced
                                 /*token_card=*/955);
        }
        RIFT_TRACE(ctx.events, "ZED: additional cost paid -> 0 [M] Shadow Clone token");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 810;
        d.def_id = R"RB(ven-023-166)RB";
        d.name = R"RB(Zed, From the Shadows)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-023/166)RB";
        d.collector_number = 23;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Ionia)RB", R"RB(Zed)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 4;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(You may discard 1 as an additional cost to play me.
When you play me, if you paid the additional cost, play a 0 [M] Shadow Clone unit token. (It has "When I attack, you may banish a unit from your trash. If you do, give me [Assault 4] this turn."))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/06793f17adbd827e468db40d644eb273a3932c7e-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_810(CardRegistry& r) {
    r.registerCard(810, std::make_unique<ZedFromTheShadows>());
}

}  // namespace riftbound
