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

class SealOfUnity : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    bool hasActivatedAbility() const override { return true; }
    bool isReactionAbility() const override { return true; }
    ActivationCost getActivationCost() const override { return {.exhaust = true}; }
    void onActivate(CardContext& ctx,
                    const std::vector<GameObjectId>& /*targets*/) override {
        // "[E]: [Reaction] — [Add] [Y]." Resolves immediately (CR 429.2),
        // not via the chain — adds 1 Order power to the controller's pool.
        ctx.executor.addFloatingPower(ctx.controller, Domain::Order, 1);
        ctx.events.logTrace("ACTIVATE: Seal of Unity adds [Y] (Order power)");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 245;
        d.def_id = R"RB(ogn-245-298)RB";
        d.name = R"RB(Seal of Unity)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-245/298)RB";
        d.collector_number = 245;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Order};
        d.power_cost = 1;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([E]: [Reaction] — [Add] [Y]. (Abilities that add resources can't be reacted to.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/e6fbd41d69bc0d235ea7993d2e9fa74e75e17dff-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_245(CardRegistry& r) {
    r.registerCard(245, std::make_unique<SealOfUnity>());
}

} // namespace riftbound
