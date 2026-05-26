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

class FrostcoatCub : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "You may pay [B] as an additional cost to play me.
    //  When you play me, if you paid the additional cost, give a unit -2 [M]
    //  this turn." [B] = Mind domain.
    OptionalAdditionalCost optionalAdditionalCost() const override {
        return {/*valid=*/true, /*energy=*/0, /*power=*/1, Domain::Mind,
                /*any_domain=*/false, /*paid_flag=*/"__frostcoat_paid"};
    }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& self = ctx.state.getObject(ctx.source);
        if (self.card_counters["__frostcoat_paid"] != 1) return;
        std::vector<GameObjectId> legal;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || !obj.location.has_value()) continue;
            if (obj.controller != ctx.controller && obj.untargetable_by_enemy)
                continue;
            legal.push_back(id);
        }
        GameObjectId tgt = pickTarget(ctx, "Frostcoat Cub: give a unit -2 [M]", legal);
        if (tgt == kInvalidId || !ctx.state.objectExists(tgt)) return;
        ctx.executor.giveTemporaryMight(tgt, -2);
        ctx.events.logTrace("FROSTCOAT CUB: paid [B] -> gave a unit -2 [M] this turn");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 390;
        d.def_id = R"RB(sfd-067-221)RB";
        d.name = R"RB(Frostcoat Cub)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-067/221)RB";
        d.collector_number = 67;
        d.artist = R"RB(Caravan Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Dog)RB", R"RB(Freljord)RB"};
        d.energy_cost = 3;
        d.might = 3;
        d.ability_text = R"RB(You may pay [B] as an additional cost to play me.
When you play me, if you paid the additional cost, give a unit -2 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/0b5f1f6ed65e070d2878b8aa21fd95e43688fc1d-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_390(CardRegistry& r) {
    r.registerCard(390, std::make_unique<FrostcoatCub>());
}

} // namespace riftbound
