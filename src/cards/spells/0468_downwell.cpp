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

class Downwell : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        std::vector<GameObjectId> to_bounce;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.location.has_value()) continue;   // only on-board cards
            if (obj.isUnit() || obj.isGear()) to_bounce.push_back(id);
        }
        for (auto id : to_bounce) {
            if (ctx.state.objectExists(id)) ctx.executor.bounceToHand(id);
        }
        ctx.events.logTrace("DOWNWELL: returned " +
                             std::to_string(to_bounce.size()) +
                             " units/gear to hands");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 468;
        d.def_id = R"RB(sfd-147-221)RB";
        d.name = R"RB(Downwell)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-147/221)RB";
        d.collector_number = 147;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Chaos};
        d.energy_cost = 8;
        d.power_cost = 2;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(Return all units and gear to their owners' hands.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/950eff0d07ef1e25be46fdb340fe8510c551c159-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_468(CardRegistry& r) {
    r.registerCard(468, std::make_unique<Downwell>());
}

} // namespace riftbound
