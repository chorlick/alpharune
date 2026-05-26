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

class WatchfulSentry : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        ctx.executor.drawCards(ctx.controller, 1);
    }
    TriggerType triggerType() const override { return TriggerType::WhenIDie; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 96;
        d.def_id = R"RB(ogn-096-298)RB";
        d.name = R"RB(Watchful Sentry)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-096/298)RB";
        d.collector_number = 96;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Freljord)RB"};
        d.energy_cost = 2;
        d.might = 1;
        d.keywords.set(Keyword::Deathknell);
        d.ability_text = R"RB([Deathknell] — Draw 1. (When I die, get the effect.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/d84da2c62b1e218a0a74b227c49ce8a953918ebd-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_96(CardRegistry& r) {
    r.registerCard(96, std::make_unique<WatchfulSentry>());
}

} // namespace riftbound
