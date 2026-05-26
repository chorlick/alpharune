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

class SealOfInsight : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    bool hasActivatedAbility() const override { return true; }
    bool isReactionAbility() const override { return true; }
    ActivationCost getActivationCost() const override { return {.exhaust = true}; }
    void onActivate(CardContext& ctx,
                    const std::vector<GameObjectId>& /*targets*/) override {
        // "[E]: [Reaction] — [Add] [B]." Resolves immediately (CR 429.2),
        // not via the chain — adds 1 Mind power to the controller's pool.
        ctx.executor.addFloatingPower(ctx.controller, Domain::Mind, 1);
        ctx.events.logTrace("ACTIVATE: Seal of Insight adds [B] (Mind power)");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 120;
        d.def_id = R"RB(ogn-120-298)RB";
        d.name = R"RB(Seal of Insight)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-120/298)RB";
        d.collector_number = 120;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Mind};
        d.power_cost = 1;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([E]: [Reaction] — [Add] [B]. (Abilities that add resources can't be reacted to.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/9ee0dc0221f83d569e0f458374e40f7238f306c2-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_120(CardRegistry& r) {
    r.registerCard(120, std::make_unique<SealOfInsight>());
}

} // namespace riftbound
