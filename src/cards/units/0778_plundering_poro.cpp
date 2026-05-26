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

class PlunderingPoro : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIConquer; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        createGoldToken(ctx);
        ctx.events.logTrace("PLUNDERING PORO: Gold gear token created");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 778;
        d.def_id = R"RB(unl-222-219)RB";
        d.name = R"RB(Plundering Poro)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-222/219)RB";
        d.collector_number = 222;
        d.artist = R"RB(FOREDAWN)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Poro)RB", R"RB(Pirate)RB", R"RB(Bilgewater)RB"};
        d.energy_cost = 2;
        d.might = 2;
        d.rarity = Rarity::Showcase;
        d.ability_text = R"RB(When I conquer, play a Gold gear token exhausted.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/c6c996fcead364682a031f32948a084101d83493-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_778(CardRegistry& r) {
    r.registerCard(778, std::make_unique<PlunderingPoro>());
}

} // namespace riftbound
