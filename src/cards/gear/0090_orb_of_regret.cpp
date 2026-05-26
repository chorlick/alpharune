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

class OrbOfRegret : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (!targets.empty())
            ctx.executor.giveTemporaryMight(targets[0], -1, /*minimum=*/1);
    }
    TriggerType triggerType() const override { return TriggerType::Activated; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};
    }
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override { return {.exhaust = true}; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 90;
        d.def_id = R"RB(ogn-090-298)RB";
        d.name = R"RB(Orb of Regret)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-090/298)RB";
        d.collector_number = 90;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Mind};
        d.energy_cost = 1;
        d.ability_text = R"RB([E]: Give a unit -1 [M] this turn, to a minimum of 1 [M].)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/fd883b646d1a3cdb1a03b0c19dc62c92ca552f4f-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_90(CardRegistry& r) {
    r.registerCard(90, std::make_unique<OrbOfRegret>());
}

} // namespace riftbound
