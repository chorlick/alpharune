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

class HeraldOfSpring : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        ctx.state.player(ctx.controller).xp += 2;
        ctx.events.logTrace("HERALD OF SPRING: +2 XP on play");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 596;
        d.def_id = R"RB(unl-034-219)RB";
        d.name = R"RB(Herald of Spring)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-034/219)RB";
        d.collector_number = 34;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Ionia)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 4;
        d.keywords.set(Keyword::Hunt);
        d.ability_text = R"RB([Hunt] (When I conquer or hold, gain 1 XP.)
When you play me, gain 2 XP.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/5bf7ba27b809d22296deb755b6f5324ac46a5a20-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_596(CardRegistry& r) {
    r.registerCard(596, std::make_unique<HeraldOfSpring>());
}

} // namespace riftbound
