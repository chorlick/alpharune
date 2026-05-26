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

class SealOfRage : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    bool hasActivatedAbility() const override { return true; }
    bool isReactionAbility() const override { return true; }
    ActivationCost getActivationCost() const override { return {.exhaust = true}; }
    void onActivate(CardContext& ctx,
                    const std::vector<GameObjectId>& /*targets*/) override {
        // "[E]: [Reaction] — [Add] [R]." Resolves immediately (CR 429.2),
        // not via the chain — adds 1 Fury power to the controller's pool.
        ctx.executor.addFloatingPower(ctx.controller, Domain::Fury, 1);
        ctx.events.logTrace("ACTIVATE: Seal of Rage adds [R] (Fury power)");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 40;
        d.def_id = R"RB(ogn-040-298)RB";
        d.name = R"RB(Seal of Rage)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-040/298)RB";
        d.collector_number = 40;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Fury};
        d.power_cost = 1;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([E]: [Reaction] — [Add] [R]. (Abilities that add resources can't be reacted to.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/fbdd14adb40b0ca46b89f476a356fa21413d812e-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_40(CardRegistry& r) {
    r.registerCard(40, std::make_unique<SealOfRage>());
}

} // namespace riftbound
