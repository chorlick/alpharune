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

class EnergyConduit : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::Activated; }
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override { return {.exhaust = true}; }
    // "[E]: [Reaction] — [Add] [1]." (Add abilities can't be reacted to.)
    bool isReactionAbility() const override { return true; }
    void onActivate(CardContext& ctx,
                    const std::vector<GameObjectId>& /*targets*/) override {
        ctx.executor.addFloatingEnergy(ctx.controller, 1);
        ctx.events.logTrace("ENERGY CONDUIT: [Add] [1] (1 energy)");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 98;
        d.def_id = R"RB(ogn-098-298)RB";
        d.name = R"RB(Energy Conduit)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-098/298)RB";
        d.collector_number = 98;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Mind};
        d.energy_cost = 3;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([E]: [Reaction] — [Add] [1]. (Abilities that add resources can't be reacted to.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/f0e0a67e9e9b1d45d12d248df78b4e643b70bdc1-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_98(CardRegistry& r) {
    r.registerCard(98, std::make_unique<EnergyConduit>());
}

} // namespace riftbound
