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

class KadregrinTheInfernal : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        int mighty = 0;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller != ctx.controller) continue;
            if (!obj.location.has_value()) continue;
            if (obj.current_might >= 5) mighty++;
        }
        if (mighty > 0) {
            ctx.executor.drawCards(ctx.controller, mighty);
            ctx.events.logTrace("KADREGRIN THE INFERNAL: drew " +
                                std::to_string(mighty) + " (one per Mighty unit)");
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 38;
        d.def_id = R"RB(ogn-038-298)RB";
        d.name = R"RB(Kadregrin the Infernal)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-038/298)RB";
        d.collector_number = 38;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Dragon)RB", R"RB(Demacia)RB"};
        d.energy_cost = 9;
        d.power_cost = 2;
        d.might = 9;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(When you play me, draw 1 for each of your [Mighty] units. (A unit is Mighty while it has 5+ [M].))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/733c32b1fccc7e2983cbb3586358152f90a6df04-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_38(CardRegistry& r) {
    r.registerCard(38, std::make_unique<KadregrinTheInfernal>());
}

} // namespace riftbound
