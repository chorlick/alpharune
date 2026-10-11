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
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& self = ctx.state.getObject(ctx.source);
        auto it = self.card_counters.find(kPaidFlag);
        if (it == self.card_counters.end() || it->second == 0) return;
        // No location is printed: the controller's base or a battlefield
        // they control, their choice (CR 355.2.a).
        const int dest = pickTokenDest(*this, ctx, "Zed: play the Shadow Clone to");
        if (dest == -1) return;   // waiting for the agent
        it->second = 0;  // consumed: a later replay must pay again
        {
            auto clone_id = ctx.executor.createToken(ctx.controller, CardType::Unit, "Shadow Clone",
                                 0, std::vector<std::string>{}, KeywordSet{},
                                 destLocation(ctx.controller, std::max(0, dest)), false);
            // Carry the Shadow Clone token card so its "When I attack" ability fires.
            if (ctx.state.objectExists(clone_id))
                ctx.state.getObject(clone_id).card_def_id = 955;
        }
        ctx.events.logTrace("ZED: additional cost paid -> 0 [M] Shadow Clone token");
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
