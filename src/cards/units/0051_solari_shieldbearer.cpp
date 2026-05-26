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

class SolariShieldbearer : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (!targets.empty())
            ctx.executor.stunUnit(targets[0]);
    }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 51;
        d.def_id = R"RB(ogn-051-298)RB";
        d.name = R"RB(Solari Shieldbearer)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-051/298)RB";
        d.collector_number = 51;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Mount Targon)RB"};
        d.energy_cost = 3;
        d.might = 2;
        d.ability_text = R"RB(When you play me, stun a unit. (It doesn't deal combat damage this turn.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/9387ea6480c9fd6991760b07a178c07a5fcf1c57-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_51(CardRegistry& r) {
    r.registerCard(51, std::make_unique<SolariShieldbearer>());
}

} // namespace riftbound
