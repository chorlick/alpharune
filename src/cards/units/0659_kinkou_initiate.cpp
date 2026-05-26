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

class KinkouInitiate : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        int total = 0;
        for (auto& [id, obj] : ctx.state.objects) {
            if (id == ctx.source) continue;  // "other" units
            if (!obj.isUnit() || obj.controller != ctx.controller) continue;
            if (!obj.location.has_value()) continue;
            total += obj.current_might;
        }
        if (total >= 5) {
            ctx.executor.drawCards(ctx.controller, 1);
            ctx.events.logTrace("KINKOU INITIATE: other units total Might " +
                                std::to_string(total) + " >= 5 -> draw 1");
        } else {
            ctx.events.logTrace("KINKOU INITIATE: other units total Might " +
                                std::to_string(total) + " < 5 -> no draw");
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 659;
        d.def_id = R"RB(unl-097-219)RB";
        d.name = R"RB(Kinkou Initiate)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-097/219)RB";
        d.collector_number = 97;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Ionia)RB"};
        d.energy_cost = 3;
        d.might = 3;
        d.ability_text = R"RB(When you play me, draw 1 if your other units have total Might 5 or more.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/358c56479ff4a02df622827092265fe4e8373fc2-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_659(CardRegistry& r) {
    r.registerCard(659, std::make_unique<KinkouInitiate>());
}

} // namespace riftbound
