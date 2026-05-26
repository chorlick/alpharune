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

class ArenaBar : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (!targets.empty())
            ctx.executor.buffUnit(targets[0]);
    }
    TriggerType triggerType() const override { return TriggerType::Activated; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true, .must_be_friendly = true};
    }
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override { return {.exhaust = true}; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 124;
        d.def_id = R"RB(ogn-124-298)RB";
        d.name = R"RB(Arena Bar)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-124/298)RB";
        d.collector_number = 124;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Body};
        d.energy_cost = 3;
        d.ability_text = R"RB([E]: Buff an exhausted friendly unit. (If it doesn't have a buff, it gets a +1 [M] buff.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/519306d1d4a36f2d54fe0982268a197a257f5e5d-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_124(CardRegistry& r) {
    r.registerCard(124, std::make_unique<ArenaBar>());
}

} // namespace riftbound
