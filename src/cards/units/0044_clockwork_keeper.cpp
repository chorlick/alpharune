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

class ClockworkKeeper : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "You may pay [C] as an additional cost to play me." [C] = one Power of
    // this card's own Domain (Calm / [G]). Offered at play time.
    OptionalAdditionalCost optionalAdditionalCost() const override {
        return {/*valid=*/true, /*energy=*/0, /*power=*/1, Domain::Calm,
                /*any_domain=*/false, /*paid_flag=*/"__clockwork_paid"};
    }
    // "When you play me, if you paid the additional cost, draw 1."
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        if (ctx.state.getObject(ctx.source).card_counters["__clockwork_paid"] != 1) return;
        ctx.executor.drawCards(ctx.controller, 1);
        ctx.events.logTrace("CLOCKWORK KEEPER: paid [C] -> draw 1");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 44;
        d.def_id = R"RB(ogn-044-298)RB";
        d.name = R"RB(Clockwork Keeper)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-044/298)RB";
        d.collector_number = 44;
        d.artist = R"RB(Polar Engine Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Shurima)RB"};
        d.energy_cost = 2;
        d.might = 2;
        d.ability_text = R"RB(You may pay [C] as an additional cost to play me.
When you play me, if you paid the additional cost, draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/417e606418349bb25d4b07d460fa043ad85f2778-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_44(CardRegistry& r) {
    r.registerCard(44, std::make_unique<ClockworkKeeper>());
}

} // namespace riftbound
