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

class NamiHeadstrong : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "You may pay [G] as an additional cost to play me." Real play-time
    // optional cost now; the on-play stun is gated on whether it was paid.
    OptionalAdditionalCost optionalAdditionalCost() const override {
        return {/*valid=*/true, /*energy=*/0, /*power=*/1, Domain::Calm,
                /*any_domain=*/false, /*paid_flag=*/"__nami_paid"};
    }
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayMe, TriggerType::WhenIHold,
                TriggerType::WhenYouPlayAUnit};
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                   .must_be_enemy = true, .optional = true};
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (!ctx.state.objectExists(ctx.source)) return;

        if (ctx.firing_trigger == TriggerType::WhenYouPlayMe) {
            // "If you paid the additional cost, [Stun] an enemy unit." The [G]
            // cost was offered at play time (maybePayOptionalAdditionalCost);
            // gate on the flag it set.
            auto& self = ctx.state.getObject(ctx.source);
            if (self.card_counters["__nami_paid"] != 1) return;
            std::vector<GameObjectId> legal;
            for (auto& [id, obj] : ctx.state.objects) {
                if (!obj.isUnit() || obj.controller == ctx.controller) continue;
                if (!obj.location.has_value() || obj.untargetable_by_enemy) continue;
                legal.push_back(id);
            }
            GameObjectId enemy = pickTarget(ctx, "Nami: stun an enemy unit", legal);
            if (enemy == kInvalidId || !ctx.state.objectExists(enemy)) return;
            ctx.executor.stunUnitBy(enemy, ctx.source);
            ctx.events.logTrace("NAMI: paid [G] -> stunned an enemy unit");
            return;
        }

        if (ctx.firing_trigger == TriggerType::WhenIHold) {
            // Arm the one-shot "next unit you play this turn" effect.
            auto& self = ctx.state.getObject(ctx.source);
            self.card_counters["__nami_hold_arm_turn"] = ctx.state.turn.turn_number;
            ctx.events.logTrace("NAMI: armed next-unit ready+buff for this turn");
            return;
        }

        if (ctx.firing_trigger == TriggerType::WhenYouPlayAUnit) {
            auto& self = ctx.state.getObject(ctx.source);
            auto it = self.card_counters.find("__nami_hold_arm_turn");
            if (it == self.card_counters.end()) return;
            if (it->second != ctx.state.turn.turn_number) return;  // stale arm
            // One-shot: disarm now.
            self.card_counters.erase(it);
            // Ready+buff the just-played unit. The trigger carries no event
            // object, so let the agent pick among freshly-played friendly
            // units (units enter exhausted, so prefer exhausted candidates).
            std::vector<GameObjectId> legal;
            for (auto& [id, obj] : ctx.state.objects) {
                if (id == ctx.source) continue;
                if (!obj.isUnit() || obj.controller != ctx.controller) continue;
                if (!obj.location.has_value()) continue;
                if (!obj.is_exhausted) continue;  // just-played units are exhausted
                legal.push_back(id);
            }
            if (legal.empty()) return;
            GameObjectId pick = pickTarget(ctx, "Nami: ready + buff the played unit", legal);
            if (pick == kInvalidId || !ctx.state.objectExists(pick)) return;
            ctx.executor.readyObject(pick);
            ctx.executor.buffUnit(pick);
            ctx.events.logTrace("NAMI: readied + buffed the played unit");
            return;
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 614;
        d.def_id = R"RB(unl-052-219)RB";
        d.name = R"RB(Nami, Headstrong)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-052/219)RB";
        d.collector_number = 52;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Mount Targon)RB", R"RB(Nami)RB"};
        d.energy_cost = 3;
        d.might = 3;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(You may pay [G] as an additional cost to play me.
When you play me, if you paid the additional cost, [Stun] an enemy unit. (It doesn't deal combat damage this turn.)
When I hold, the next time you play a unit this turn, ready it and [Buff] it.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/c65521065700f308689608fc6c4fa8963f3264c0-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_614(CardRegistry& r) {
    r.registerCard(614, std::make_unique<NamiHeadstrong>());
}

} // namespace riftbound
