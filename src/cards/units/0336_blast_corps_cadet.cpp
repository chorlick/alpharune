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

class BlastCorpsCadet : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }

    // "You may pay [1][R] as an additional cost to play me.
    //  When you play me, if you paid the additional cost, deal 2 to a unit
    //  at a battlefield."
    OptionalAdditionalCost optionalAdditionalCost() const override {
        return {/*valid=*/true, /*energy=*/1, /*power=*/1, Domain::Fury,
                /*any_domain=*/false, /*paid_flag=*/"__blastcorps_paid"};
    }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        const auto& counters = ctx.state.getObject(ctx.source).card_counters;
        auto paid_it = counters.find("__blastcorps_paid");
        const bool paid = paid_it != counters.end() && paid_it->second == 1;
        // No return ahead of the marker when the cost was not paid: there is
        // then no unit to choose, and pickTarget stores that on the item.
        std::vector<GameObjectId> legal;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!paid) break;
            if (!obj.isUnit() || !obj.battlefieldId().has_value()) continue;
            if (obj.controller != ctx.controller && obj.untargetable_by_enemy)
                continue;
            legal.push_back(id);
        }
        GameObjectId tgt = pickTarget(ctx,
            "Blast Corps Cadet: deal 2 to a unit at a battlefield", legal);
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (!paid) return;
        // The payment covers this play only: a Cadet that is returned to hand
        // and played again without paying must not find the old flag.
        ctx.state.getObject(ctx.source).card_counters.erase("__blastcorps_paid");
        if (tgt == kInvalidId || !ctx.state.objectExists(tgt)) return;
        ctx.executor.dealDamage(tgt, 2, ctx.source);
        if (ctx.state.objectExists(tgt) &&
            ctx.state.getObject(tgt).hasLethalDamage())
            ctx.executor.killObject(tgt);
        ctx.events.logTrace("BLAST CORPS CADET: paid [1][R] -> dealt 2 to a BF unit");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 336;
        d.def_id = R"RB(sfd-013-221)RB";
        d.name = R"RB(Blast Corps Cadet)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-013/221)RB";
        d.collector_number = 13;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Piltover)RB"};
        d.energy_cost = 2;
        d.might = 2;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(You may pay [1][R] as an additional cost to play me.
When you play me, if you paid the additional cost, deal 2 to a unit at a battlefield.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/9efe5a8322a90822650ff18578cd5e1a8561fac5-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_336(CardRegistry& r) {
    r.registerCard(336, std::make_unique<BlastCorpsCadet>());
}

} // namespace riftbound
