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

class VoraciousGromp : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        ctx.state.player(ctx.controller).xp += 3;
    }
    TriggerType triggerType() const override { return TriggerType::WhenIConquerOrHold; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 662;
        d.def_id = R"RB(unl-100-219)RB";
        d.name = R"RB(Voracious Gromp)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-100/219)RB";
        d.collector_number = 100;
        d.artist = R"RB(Polar Engine Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Freljord)RB"};
        d.energy_cost = 5;
        d.might = 5;
        d.keywords.set(Keyword::Hunt);
        d.ability_text = R"RB([Hunt 3] (When I conquer or hold, gain 3 XP.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/3c006b8c9aa4581ad90c258cc6cfac9098d3c296-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_662(CardRegistry& r) {
    r.registerCard(662, std::make_unique<VoraciousGromp>());
}

} // namespace riftbound
