#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include "cards/gear/equip_base.h"

namespace riftbound {
namespace {

class SacredShears : public SimpleEquipGear {
public:
    SacredShears() : SimpleEquipGear(Domain::Order) {}
    const CardDef& def() const override { return def_; }
    TriggerType equippedTriggerType() const override { return TriggerType::WhenIDie; }
    void onEquippedTrigger(CardContext& ctx, GameObjectId unit,
                            const std::vector<GameObjectId>& targets) override {
        ctx.executor.drawCards(ctx.controller, 1);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 493;
        d.def_id = R"RB(sfd-172-221)RB";
        d.name = R"RB(Sacred Shears)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-172/221)RB";
        d.collector_number = 172;
        d.artist = R"RB(Polar Engine Studio)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Equipment)RB"};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.might_bonus = 1;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Equip);
        d.ability_text = R"RB([Equip] [Y] ([Y]: Attach this to a unit you control.))RB";
        d.effect_text = R"RB([Deathknell] — Draw 1. (When I die, get the effect.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/af1069aef68d413127dfa84e4e96b8d66a8720ca-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_493(CardRegistry& r) {
    r.registerCard(493, std::make_unique<SacredShears>());
}

} // namespace riftbound
