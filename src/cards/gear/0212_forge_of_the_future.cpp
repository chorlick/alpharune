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

class ForgeOfTheFuture : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (!targets.empty())
            ctx.executor.killObject(targets[0]);
    }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayThis; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 212;
        d.def_id = R"RB(ogn-212-298)RB";
        d.name = R"RB(Forge of the Future)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-212/298)RB";
        d.collector_number = 212;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Order};
        d.energy_cost = 2;
        d.ability_text = R"RB(When you play this, play a 1 [M] Recruit unit token at your base.
Kill this: Recycle up to 4 cards from trashes.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/8e6a76d28590bcd835b0a9a1e806fe0fa6883141-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_212(CardRegistry& r) {
    r.registerCard(212, std::make_unique<ForgeOfTheFuture>());
}

} // namespace riftbound
