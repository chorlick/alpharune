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

class WujuApprentice : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayMe, TriggerType::WhenIConquerOrHold};
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (ctx.firing_trigger == TriggerType::WhenIConquerOrHold) {
            // [Hunt] — gain 1 XP on conquer or hold.
            ctx.state.player(ctx.controller).xp += 1;
            ctx.events.logTrace("WUJU APPRENTICE: [Hunt] +1 XP");
            return;
        }
        if (ctx.firing_trigger == TriggerType::WhenYouPlayMe) {
            // [Level 6] — only with 6+ XP.
            if (ctx.state.player(ctx.controller).xp < 6) return;
            ctx.executor.drawCards(ctx.controller, 1);
            ctx.events.logTrace("WUJU APPRENTICE: Level 6, draw 1");
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 602;
        d.def_id = R"RB(unl-040-219)RB";
        d.name = R"RB(Wuju Apprentice)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-040/219)RB";
        d.collector_number = 40;
        d.artist = R"RB(Polar Engine Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Ionia)RB"};
        d.energy_cost = 2;
        d.might = 2;
        d.keywords.set(Keyword::Hunt);
        d.keywords.set(Keyword::Level);
        d.ability_text = R"RB([Hunt] (When I conquer or hold, gain 1 XP.)
[Level 6][>] When you play me, draw 1. (While you have 6+ XP, get the effect.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/543789dd1b4b2654392a151d4bb1b0c6263c47dc-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_602(CardRegistry& r) {
    r.registerCard(602, std::make_unique<WujuApprentice>());
}

} // namespace riftbound
