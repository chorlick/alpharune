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
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
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
                                   [&]() { return costAlreadyPaid(ctx) || !friendly_gear().empty(); });
        // Two sequential picks: A = friendly gear to kill (cost), B = any gear
        // to kill (effect, B excludes A since A is gone).
        auto any_gear_fn = [&](GameObjectId a) {
            std::vector<GameObjectId> out;
            for (auto& [id, obj] : ctx.state.objects) {
                if (id == a) continue;
                if (!obj.isGear() || !obj.location.has_value()) continue;
                out.push_back(id);
            }
            return out;
        };
        std::pair<GameObjectId, GameObjectId> picks{kInvalidId, kInvalidId};
        if (conf == 1) picks = pickTargetPair(ctx, "Zaun Punk", friendly_gear(), any_gear_fn);
        // The additional cost — killing the chosen friendly gear — is paid as
        // the trigger is finalized (CR 383.3), the nearest point to "as you
        // play me"; nobody can respond before it is paid.
        bool paid = costAlreadyPaid(ctx);
        if (!paid && conf == 1 && picks.first != kInvalidId && costDueNow(ctx)) {
            ctx.executor.killObject(picks.first);
            paid = true;
        }
        if (finalizingChoices(ctx)) return;   // choices and the cost end here (CR 383.3)
        if (!paid) return;   // waiting / declined / no friendly gear
        // Effect: kill the chosen gear (kInvalidId if it left the board since).
        if (picks.second != kInvalidId) ctx.executor.killObject(picks.second);
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
