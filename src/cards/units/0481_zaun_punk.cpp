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

// "You may kill a friendly gear as an additional cost to play me.
//  When you play me, if you paid the additional cost, kill a gear."
//
// OptionalAdditionalCost only models energy/power, not "kill a gear", so we
// fold the optional cost + its payoff into the WhenYouPlayMe trigger: you may
// kill a friendly gear (the cost); if you do, kill any gear (the effect).

class ZaunPunk : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto friendly_gear = [&]() {
            std::vector<GameObjectId> out;
            for (auto& [id, obj] : ctx.state.objects) {
                if (id == ctx.source) continue;
                if (!obj.isGear() || obj.controller != ctx.controller) continue;
                if (!obj.location.has_value()) continue;
                out.push_back(id);
            }
            return out;
        };
        int conf = confirmOptional(ctx, "Zaun Punk: kill a friendly gear (cost) to kill a gear?",
                                   [&]() { return !friendly_gear().empty(); });
        if (conf == -1) return;  // waiting on agent
        if (conf == 0) return;   // declined / no friendly gear

        // Two sequential picks: A = friendly gear to kill (cost), B = any gear
        // to kill (effect, B excludes A since A is gone). pickTargetPair (9..14)
        // composes with confirmOptional (0/1/2).
        auto any_gear_fn = [&](GameObjectId a) {
            std::vector<GameObjectId> out;
            for (auto& [id, obj] : ctx.state.objects) {
                if (id == a) continue;
                if (!obj.isGear() || !obj.location.has_value()) continue;
                out.push_back(id);
            }
            return out;
        };
        auto [cost, victim] = pickTargetPair(ctx, "Zaun Punk", friendly_gear(), any_gear_fn);
        bool suspending = (cost == kInvalidId || victim == kInvalidId) &&
                          ctx.state.chain.resuming.has_value() &&
                          (ctx.state.chain.resuming->resume_point == 10 ||
                           ctx.state.chain.resuming->resume_point == 12);
        if (suspending) return;
        if (cost == kInvalidId || !ctx.state.objectExists(cost)) return;
        // Pay the additional cost: kill the chosen friendly gear.
        ctx.executor.killObject(cost);
        // Effect: kill the chosen gear.
        if (victim != kInvalidId && ctx.state.objectExists(victim))
            ctx.executor.killObject(victim);
        ctx.events.logTrace("ZAUN PUNK: killed a friendly gear (cost) -> killed a gear");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 481;
        d.def_id = R"RB(sfd-160-221)RB";
        d.name = R"RB(Zaun Punk)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-160/221)RB";
        d.collector_number = 160;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Zaun)RB"};
        d.energy_cost = 3;
        d.might = 3;
        d.ability_text = R"RB(You may kill a friendly gear as an additional cost to play me.
When you play me, if you paid the additional cost, kill a gear.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/af4eec8bb065708bf790b940fa065ea3e735afa0-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_481(CardRegistry& r) {
    r.registerCard(481, std::make_unique<ZaunPunk>());
}

} // namespace riftbound
