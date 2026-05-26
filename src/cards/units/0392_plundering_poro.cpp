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
    // "When I conquer, play a Gold gear token exhausted."
    TriggerType triggerType() const override { return TriggerType::WhenIConquer; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        createGoldExhausted(ctx);
        ctx.events.logTrace("PLUNDERING PORO: conquer -> Gold gear token (exhausted)");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 392;
        d.def_id = R"RB(sfd-069-221)RB";
        d.name = R"RB(Plundering Poro)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-069/221)RB";
        d.collector_number = 69;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Poro)RB", R"RB(Pirate)RB", R"RB(Bilgewater)RB"};
        d.energy_cost = 2;
        d.might = 2;
        d.ability_text = R"RB(When I conquer, play a Gold gear token exhausted.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/4ff9e6c73f67b505e50ec2f5f513292fe5b20b95-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_392(CardRegistry& r) {
    r.registerCard(392, std::make_unique<PlunderingPoro>());
}

} // namespace riftbound
