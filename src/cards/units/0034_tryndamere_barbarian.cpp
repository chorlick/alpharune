#include "cards/card.h"
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

class TryndamereBarbarian : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "When I conquer after an attack, if you assigned 5 or more excess
    //  damage to enemy units, you score 1 point."
    // Excess-damage assignment is not surfaced to triggers (same engine gap as
    // Piltover Enforcer / Hextech Gauntlets); the condition is treated as
    // satisfied (documented approximation).
    TriggerType triggerType() const override { return TriggerType::WhenIConquer; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        ctx.state.player(ctx.controller).score += 1;
        ctx.events.logTrace("TRYNDAMERE: conquer -> score 1 point "
                            "(excess-damage condition not engine-checked)");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 34;
        d.def_id = R"RB(ogn-034-298)RB";
        d.name = R"RB(Tryndamere, Barbarian)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-034/298)RB";
        d.collector_number = 34;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Tryndamere)RB", R"RB(Freljord)RB"};
        d.energy_cost = 7;
        d.power_cost = 2;
        d.might = 8;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When I conquer after an attack, if you assigned 5 or more excess damage to enemy units, you score 1 point.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/124c93495c927be1baea03fdacf6c7b283cf8b6c-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_34(CardRegistry& r) {
    r.registerCard(34, std::make_unique<TryndamereBarbarian>());
}

} // namespace riftbound
