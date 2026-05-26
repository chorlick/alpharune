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

class MushroomPouch : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::AtStartOfBeginning; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // "if you control a facedown card at a battlefield, draw 1."
        bool has_facedown = false;
        for (auto& [id, obj] : ctx.state.objects) {
            if (obj.controller != ctx.controller) continue;
            if (!obj.is_hidden) continue;
            if (!obj.isAtBattlefield()) continue;
            has_facedown = true;
            break;
        }
        if (!has_facedown) return;
        ctx.executor.drawCards(ctx.controller, 1);
        ctx.events.logTrace("MUSHROOM POUCH: facedown card present -> draw 1");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 101;
        d.def_id = R"RB(ogn-101-298)RB";
        d.name = R"RB(Mushroom Pouch)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-101/298)RB";
        d.collector_number = 101;
        d.artist = R"RB(Polar Engine Studio)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Mind};
        d.energy_cost = 2;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(At the start of your Beginning Phase, if you control a facedown card at a battlefield, draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ef5b3125fda91c0d0a02b7e6dfb7d3d5bafa8bd0-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_101(CardRegistry& r) {
    r.registerCard(101, std::make_unique<MushroomPouch>());
}

} // namespace riftbound
