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

class HunterSMachete : public SimpleEquipGear {
public:
    HunterSMachete() : SimpleEquipGear(Domain::Body) {}
    const CardDef& def() const override { return def_; }
    TriggerType equippedTriggerType() const override { return TriggerType::WhenIConquerOrHold; }
    void onEquippedTrigger(CardContext& ctx, GameObjectId unit,
                            const std::vector<GameObjectId>& targets) override {
        ctx.state.player(ctx.controller).xp += 1;
        ctx.events.logTrace("EQUIP_TRIGGER: Hunter's Machete +1 XP");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 658;
        d.def_id = R"RB(unl-096-219)RB";
        d.name = R"RB(Hunter's Machete)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-096/219)RB";
        d.collector_number = 96;
        d.artist = R"RB(黯荧岛Dark Glow)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Equipment)RB"};
        d.energy_cost = 3;
        d.might_bonus = 2;
        d.keywords.set(Keyword::Equip);
        d.ability_text = R"RB([Equip] [O] ([O]: Attach this to a unit you control.))RB";
        d.effect_text = R"RB([Hunt] (When I conquer or hold, gain 1 XP.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/374fd9220c204810c2b1abd48217b4d233362753-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_658(CardRegistry& r) {
    r.registerCard(658, std::make_unique<HunterSMachete>());
}

} // namespace riftbound
