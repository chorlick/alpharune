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

class KaiSaSurvivor : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIConquer; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        ctx.executor.drawCards(ctx.controller, 1);
        ctx.events.logTrace("KAI'SA SURVIVOR: conquer -> draw 1");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 39;
        d.def_id = R"RB(ogn-039-298)RB";
        d.name = R"RB(Kai'Sa, Survivor)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-039/298)RB";
        d.collector_number = 39;
        d.artist = R"RB(League Splash Team)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Kai'Sa)RB", R"RB(The Void)RB"};
        d.energy_cost = 4;
        d.might = 4;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Accelerate);
        d.ability_text = R"RB([Accelerate] (You may pay [1][R] as an additional cost to have me enter ready.)
When I conquer, draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/3d942c02ad4e96a36dba14d663c5105bb6614500-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_39(CardRegistry& r) {
    r.registerCard(39, std::make_unique<KaiSaSurvivor>());
}

} // namespace riftbound
