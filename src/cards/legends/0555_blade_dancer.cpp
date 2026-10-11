#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include "cards/card_helpers.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class BladeDancer : public LegendCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }

    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouChooseAFriendlyUnit, TriggerType::WhenIConquer};
    }

    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (ctx.firing_trigger == TriggerType::WhenIConquer) {
            // "you may pay [1] to ready me."
            if (!ctx.state.objectExists(ctx.source)) return;
            auto& self = ctx.state.getObject(ctx.source);
            if (!self.is_exhausted) return;  // already ready, nothing to do
            auto still_legal = [&ctx]() {
                if (!ctx.state.objectExists(ctx.source)) return false;
                if (!ctx.state.getObject(ctx.source).is_exhausted) return false;
                // CR 445.2 / 164.2.a: floating Energy or a ready rune can pay.
                return payableEnergy(ctx.state, ctx.controller) >= 1;
            };
            if (!costAlreadyPaid(ctx) && !still_legal()) return;
            int conf = confirmOptional(ctx, "Blade Dancer: pay [1] to ready me?",
                                       still_legal);
            if (conf < 1) return;
            if (costDueNow(ctx) && !payEnergy(ctx, ctx.controller, 1)) return;
            if (finalizingChoices(ctx)) return;   // choices and costs end here (CR 383.3)
            ctx.executor.readyObject(ctx.source);
            ctx.events.logTrace("BLADE DANCER: paid [1] -> ready me");
            return;
        }

        if (ctx.firing_trigger == TriggerType::WhenYouChooseAFriendlyUnit) {
            // "you may exhaust me and pay [A] to ready IT" — the friendly unit
            // that was just chosen, which the trigger carries as the chain
            // item's triggering_subject (TriggerManager: "chosen_by_controller").
            // Nothing else may be readied; with no known subject there is no
            // "it" and nothing happens.
            if (!ctx.state.objectExists(ctx.source)) return;
            const GameObjectId chosen = ctx.state.chain.resuming.has_value()
                ? ctx.state.chain.resuming->triggering_subject : kInvalidId;
            auto still_legal = [&ctx, chosen]() {
                if (!ctx.state.objectExists(ctx.source)) return false;
                if (ctx.state.getObject(ctx.source).is_exhausted) return false;  // "exhaust me"
                if (chosen == kInvalidId || !ctx.state.objectExists(chosen)) return false;
                const auto& u = ctx.state.getObject(chosen);
                if (!u.isUnit() || !u.location.has_value()) return false;
                if (u.controller != ctx.controller) return false;
                // Not gated on the unit being exhausted: the cost may be paid
                // whatever the unit's state is (CR 383.3.b, 445.2).
                // [A] = power of any Domain: floating power, or a rune recycled
                // for it (CR 164.2.b) — not only what already floats.
                return ctx.executor.payableAnyPower(ctx.controller) >= 1;
            };
            int conf = confirmOptional(ctx,
                "Blade Dancer: exhaust me + pay [A] to ready the chosen unit?", still_legal);
            if (conf != 1) return;
            if (costDueNow(ctx)) {
                if (!ctx.executor.payAnyPower(ctx.controller, 1)) return;
                ctx.executor.exhaustObject(ctx.source);
            }
            if (finalizingChoices(ctx)) return;   // choices and costs end here (CR 383.3)
            if (chosen == kInvalidId || !ctx.state.objectExists(chosen)) return;
            // "ready it": only a unit that is still on the board (readyObject
            // itself would ready a card that has gone to a hand or the trash).
            if (!ctx.state.getObject(chosen).isUnit() ||
                !ctx.state.getObject(chosen).location.has_value()) return;
            ctx.executor.readyObject(chosen);
            ctx.events.logTrace("BLADE DANCER: exhaust me + [A] -> ready " +
                                 ctx.state.getObject(chosen).name);
            return;
        }
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 555;
        d.def_id = R"RB(sfd-246-221)RB";
        d.name = R"RB(Blade Dancer)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-246/221)RB";
        d.collector_number = 246;
        d.artist = R"RB(Grafit Studio)RB";
        d.card_type = CardType::Legend;
        d.domains = {Domain::Calm, Domain::Chaos};
        d.tags = {R"RB(Irelia)RB"};
        d.rarity = Rarity::Showcase;
        d.ability_text = R"RB(When you choose a friendly unit, you may exhaust me and pay [A] to ready it.
When you conquer, you may pay [1] to ready me.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/8258072391bbb8d24e9d6e603c3ba1434979a911-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_555(CardRegistry& r) {
    r.registerCard(555, std::make_unique<BladeDancer>());
}

} // namespace riftbound
