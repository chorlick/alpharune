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

class FaithfulManufactor : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // "Play a 1 [M] Recruit unit token here." (my location)
        if (!ctx.state.objectExists(ctx.source)) return;
        auto loc = ctx.state.getObject(ctx.source).location;
        if (!loc) loc = LocationId{BaseLocation{ctx.controller}};
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Recruit",
                                 1, {"Recruit"}, KeywordSet{}, *loc, false);
        ctx.events.logTrace("FAITHFUL MANUFACTOR: play 1[M] Recruit here");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 211;
        d.def_id = R"RB(ogn-211-298)RB";
        d.name = R"RB(Faithful Manufactor)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-211/298)RB";
        d.collector_number = 211;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Piltover)RB"};
        d.energy_cost = 3;
        d.might = 2;
        d.ability_text = R"RB(When you play me, play a 1 [M] Recruit unit token here.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/b41107b7456a2a38d203983b1e504e3789d6b6ea-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_211(CardRegistry& r) {
    r.registerCard(211, std::make_unique<FaithfulManufactor>());
}

} // namespace riftbound
