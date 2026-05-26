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

class PoroSnax : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayThis; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        ctx.executor.drawCards(ctx.controller, 1);
        ctx.events.logTrace("PORO SNAX: enter -> draw 1");
    }
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        // [1][G], [E], Kill this. Approximated as [1] energy + exhaust
        // + recycle_self (closest match for "kill this as cost"). The
        // G domain isn't enforced at this layer.
        return {.exhaust = true, .energy = 1, .recycle_self = true};
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        ctx.executor.drawCards(ctx.controller, 1);
        ctx.events.logTrace("PORO SNAX: activated -> draw 1");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 369;
        d.def_id = R"RB(sfd-046-221)RB";
        d.name = R"RB(Poro Snax)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-046/221)RB";
        d.collector_number = 46;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Calm};
        d.energy_cost = 1;
        d.power_cost = 1;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you play this, draw 1.
[1][G], [E], Kill this: Draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/8caaa64513b5ba57d676c7b2b9ee64af61711539-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_369(CardRegistry& r) {
    r.registerCard(369, std::make_unique<PoroSnax>());
}

} // namespace riftbound
