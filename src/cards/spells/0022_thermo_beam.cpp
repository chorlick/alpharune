#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include "cards/card_helpers.h"

namespace riftbound {
namespace {

class ThermoBeam : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        std::vector<GameObjectId> gear_ids;
        for (auto& [id, obj] : ctx.state.objects) {
            if (obj.isGear() && obj.location.has_value()) {
                gear_ids.push_back(id);
            }
        }
        ctx.events.logTrace("THERMO BEAM: killing " +
                             std::to_string(gear_ids.size()) + " gear");
        for (auto id : gear_ids) ctx.executor.killObject(id);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 22;
        d.def_id = R"RB(ogn-022-298)RB";
        d.name = R"RB(Thermo Beam)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-022/298)RB";
        d.collector_number = 22;
        d.artist = R"RB(Max Grecke)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Fury};
        d.energy_cost = 5;
        d.power_cost = 2;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Kill all gear.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/88661e852b864491e6d7f5f087caf4263f61c361-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_22(CardRegistry& r) {
    r.registerCard(22, std::make_unique<ThermoBeam>());
}

} // namespace riftbound
