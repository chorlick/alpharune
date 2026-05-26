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

class PeakGuardian : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // "buff me. Then, if I am at a battlefield, buff all other friendly
        // units there." Buff = +1 [M] if it doesn't already have one.
        auto buff_if_unbuffed = [&](GameObjectId id) {
            if (!ctx.state.objectExists(id)) return;
            if (ctx.state.getObject(id).buff_count == 0) ctx.executor.buffUnit(id);
        };
        buff_if_unbuffed(ctx.source);
        if (!ctx.state.objectExists(ctx.source)) return;
        auto my_bf = ctx.state.getObject(ctx.source).battlefieldId();
        if (!my_bf) return;
        for (auto& [id, obj] : ctx.state.objects) {
            if (id == ctx.source) continue;
            if (!obj.isUnit() || obj.controller != ctx.controller) continue;
            if (obj.battlefieldId() != my_bf) continue;
            buff_if_unbuffed(id);
        }
        ctx.events.logTrace("PEAK GUARDIAN: buff self + other friendly units here");
    }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 223;
        d.def_id = R"RB(ogn-223-298)RB";
        d.name = R"RB(Peak Guardian)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-223/298)RB";
        d.collector_number = 223;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Mount Targon)RB"};
        d.energy_cost = 6;
        d.power_cost = 1;
        d.might = 5;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you play me, buff me. Then, if I am at a battlefield, buff all other friendly units there. (To buff a unit, give it a +1 [M] buff if it doesn't already have one.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/bb56433bc032fd31a957923c43babc48b66db24c-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_223(CardRegistry& r) {
    r.registerCard(223, std::make_unique<PeakGuardian>());
}

} // namespace riftbound
