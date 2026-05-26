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

class RoyalGuard : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // "play a 2 [M] Sand Soldier unit token here." (my location)
        if (!ctx.state.objectExists(ctx.source)) return;
        auto loc = ctx.state.getObject(ctx.source).location;
        if (!loc) loc = LocationId{BaseLocation{ctx.controller}};
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Sand Soldier",
                                 2, {"Sand Soldier"}, KeywordSet{}, *loc, false);
        ctx.events.logTrace("ROYAL GUARD: play 2[M] Sand Soldier token here");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 478;
        d.def_id = R"RB(sfd-157-221)RB";
        d.name = R"RB(Royal Guard)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-157/221)RB";
        d.collector_number = 157;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Shurima)RB"};
        d.energy_cost = 4;
        d.might = 2;
        d.ability_text = R"RB(When you play me, play a 2 [M] Sand Soldier unit token here.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/919d79a3f5a38c78476e6414a898a0dc7b385629-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_478(CardRegistry& r) {
    r.registerCard(478, std::make_unique<RoyalGuard>());
}

} // namespace riftbound
