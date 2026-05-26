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

class SealOfDiscord : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    bool hasActivatedAbility() const override { return true; }
    bool isReactionAbility() const override { return true; }
    ActivationCost getActivationCost() const override { return {.exhaust = true}; }
    void onActivate(CardContext& ctx,
                    const std::vector<GameObjectId>& /*targets*/) override {
        // "[E]: [Reaction] — [Add] [P]." Resolves immediately (CR 429.2),
        // not via the chain — adds 1 Chaos power to the controller's pool.
        ctx.executor.addFloatingPower(ctx.controller, Domain::Chaos, 1);
        ctx.events.logTrace("ACTIVATE: Seal of Discord adds [P] (Chaos power)");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 204;
        d.def_id = R"RB(ogn-204-298)RB";
        d.name = R"RB(Seal of Discord)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-204/298)RB";
        d.collector_number = 204;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Chaos};
        d.power_cost = 1;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([E]: [Reaction] — [Add] [P]. (Abilities that add resources can't be reacted to.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/dd8433e77e46ca77aaf0be35d1774218d9a2f037-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_204(CardRegistry& r) {
    r.registerCard(204, std::make_unique<SealOfDiscord>());
}

} // namespace riftbound
