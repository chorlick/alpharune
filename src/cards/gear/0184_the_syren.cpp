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

class TheSyren : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (!targets.empty())
            ctx.executor.moveToBase(targets[0]);
    }
    TriggerType triggerType() const override { return TriggerType::Activated; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true, .must_be_friendly = true, .must_be_at_battlefield = true};
    }
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override { return {.exhaust = true, .energy = 1}; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 184;
        d.def_id = R"RB(ogn-184-298)RB";
        d.name = R"RB(The Syren)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-184/298)RB";
        d.collector_number = 184;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Chaos};
        d.energy_cost = 2;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB([1], [E]: Move a friendly unit at a battlefield to its base.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/cafdd212b542243e10e5fac587616ab77e3d9cf1-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_184(CardRegistry& r) {
    r.registerCard(184, std::make_unique<TheSyren>());
}

} // namespace riftbound
