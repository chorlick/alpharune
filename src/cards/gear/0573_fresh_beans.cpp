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

class FreshBeans : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayAUnit; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        bool in_showdown = false;
        for (const auto& bf : ctx.state.battlefields) {
            if (bf.showdown_in_progress) { in_showdown = true; break; }
        }
        if (!in_showdown) return;
        int conf = confirmOptional(ctx, "Fresh Beans: exhaust to draw 1",
            [&] {
                return ctx.state.objectExists(ctx.source) &&
                       !ctx.state.getObject(ctx.source).is_exhausted;
            });
        if (conf != 1) return;
        ctx.executor.exhaustObject(ctx.source);
        ctx.executor.drawCards(ctx.controller, 1);
        ctx.events.logTrace("FRESH BEANS: exhaust to draw 1");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 573;
        d.def_id = R"RB(unl-011-219)RB";
        d.name = R"RB(Fresh Beans)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-011/219)RB";
        d.collector_number = 11;
        d.artist = R"RB(Wild Blue Studios)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Fury};
        d.energy_cost = 2;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you play a unit during a showdown, you may exhaust this to draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/1737d7d02cd0619a520aba258d1b40fdbb5d1129-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_573(CardRegistry& r) {
    r.registerCard(573, std::make_unique<FreshBeans>());
}

} // namespace riftbound
