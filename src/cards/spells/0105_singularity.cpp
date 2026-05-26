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

class Singularity : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 2, .must_be_unit = true, .optional = true};
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        for (auto t : targets) {
            if (ctx.state.objectExists(t)) ctx.executor.dealDamage(t, 6, ctx.source);
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 105;
        d.def_id = R"RB(ogn-105-298)RB";
        d.name = R"RB(Singularity)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-105/298)RB";
        d.collector_number = 105;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Mind};
        d.energy_cost = 6;
        d.power_cost = 2;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(Deal 6 to each of up to two units.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/792edce1a64d4a92740aa3fa240e21e161513944-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_105(CardRegistry& r) {
    r.registerCard(105, std::make_unique<Singularity>());
}

} // namespace riftbound
