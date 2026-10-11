#include "cards/card.h"
#include "cards/card_helpers.h"
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

class MasaCrashingThunder : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    // ── behaviour (Vendetta batch C) ──
    // "You may pay [Y] as an additional cost to play me."
    OptionalAdditionalCost optionalAdditionalCost() const override {
        return {/*valid=*/true, /*energy=*/0, /*power=*/1, Domain::Order,
                /*any_domain=*/false, /*paid_flag=*/"__masa_paid"};
    }
    // "When you play me, if you paid the additional cost, [Stun] an enemy
    //  unit at a battlefield."
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        // No return ahead of the marker when the cost was not paid: there is
        // then no unit to choose, and pickTarget stores that on the item.
        bool paid = false;
        {
            const auto& self = ctx.state.getObject(ctx.source);
            auto it = self.card_counters.find("__masa_paid");
            paid = it != self.card_counters.end() && it->second == 1;
        }
        auto legal = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& obj) {
            return paid && obj.controller != ctx.controller && obj.isAtBattlefield();
        });
        GameObjectId picked = pickTarget(ctx, "Masa, Crashing Thunder: stun", legal);
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        ctx.executor.stunUnitBy(picked, ctx.source);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 907;
        d.def_id = R"RB(ven-120-166)RB";
        d.name = R"RB(Masa, Crashing Thunder)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-120/166)RB";
        d.collector_number = 120;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Yordle)RB", R"RB(Bandle City)RB"};
        d.energy_cost = 4;
        d.might = 4;
        d.rarity = Rarity::Common;
        d.ability_text = R"RB(You may pay [Y] as an additional cost to play me.
When you play me, if you paid the additional cost, [Stun] an enemy unit at a battlefield. (It doesn't deal combat damage this turn.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/c02046374935d38a8e4bb28a8192dfc46e2596a0-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_907(CardRegistry& r) {
    r.registerCard(907, std::make_unique<MasaCrashingThunder>());
}

}  // namespace riftbound
