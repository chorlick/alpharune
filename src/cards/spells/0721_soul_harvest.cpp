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

class SoulHarvest : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (!targets.empty())
            ctx.executor.killObject(targets[0]);
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true, .must_be_at_battlefield = true, .max_might = 3};
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 721;
        d.def_id = R"RB(unl-159-219)RB";
        d.name = R"RB(Soul Harvest)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-159/219)RB";
        d.collector_number = 159;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Order};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.ability_text = R"RB(Kill a unit at a battlefield with 3 [M] or less.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/baee7644d00219dfa4160d59e6f6e78e55f5e619-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_721(CardRegistry& r) {
    r.registerCard(721, std::make_unique<SoulHarvest>());
}

} // namespace riftbound
