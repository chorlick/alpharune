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
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
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
    // "the next time you play a unit this turn": only while a hold this turn
    // has armed it.
    bool triggerCondition(const GameState& state, GameObjectId source, PlayerId /*controller*/,
                          TriggerType trigger, GameObjectId /*subject*/) const override {
        if (trigger != TriggerType::WhenYouPlayAUnit) return true;
        return state.objectExists(source) && armedThisTurn(state, state.getObject(source));
    }
    static bool armedThisTurn(const GameState& state, const GameObject& self) {
        auto it = self.card_counters.find("__nami_hold_arm_turn");
        return it != self.card_counters.end() && it->second == state.turn.turn_number;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& self = ctx.state.getObject(ctx.source);
        if (ctx.firing_trigger == TriggerType::WhenYouPlayMe) {
            // "If you paid the additional cost, [Stun] an enemy unit." The [G]
            // cost was offered at play time (maybePayOptionalAdditionalCost);
            // gate on the flag it set.
            // No return ahead of the marker when it was not paid: there is
            // then no unit to choose, and pickTarget stores that on the item.
            auto paid_it = self.card_counters.find("__nami_paid");
            const bool paid = paid_it != self.card_counters.end() && paid_it->second == 1;
            const auto legal = paid
                ? chooseableUnits(ctx.state, ctx.controller,
                    [&](const GameObject& u) { return u.controller != ctx.controller; })
                : std::vector<GameObjectId>{};
            GameObjectId enemy = pickTarget(ctx, "Nami: stun an enemy unit", legal);
            if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
            if (!paid) return;
            // Still waiting for a pick made as this resolves: come back.
            if (enemy == kInvalidId && !legal.empty() && ctx.state.chain.resuming.has_value() &&
                ctx.state.chain.resuming->resume_point == 7) return;
            // The engine never clears the flag: consume it, or a replay
            // without paying would stun again.
            self.card_counters.erase("__nami_paid");
            if (enemy == kInvalidId) return;
            ctx.executor.stunUnitBy(enemy, ctx.source);
            ctx.events.logTrace("NAMI: paid [G] -> stunned an enemy unit");
            return;
        }
        if (ctx.firing_trigger == TriggerType::WhenIHold) {
            if (finalizingChoices(ctx)) return;   // no choices
            // Arm the one-shot "next unit you play this turn" effect.
            self.card_counters["__nami_hold_arm_turn"] = ctx.state.turn.turn_number;
            ctx.events.logTrace("NAMI: armed next-unit ready+buff for this turn");
            return;
        }
        if (ctx.firing_trigger == TriggerType::WhenYouPlayAUnit) {
            if (finalizingChoices(ctx)) return;   // no choices: "it" is the unit played
            if (!armedThisTurn(ctx.state, self)) return;   // used by an earlier play
            self.card_counters.erase("__nami_hold_arm_turn");   // one-shot
            const GameObjectId played = ctx.state.chain.resuming.has_value()
                ? ctx.state.chain.resuming->triggering_subject : kInvalidId;
            if (played == kInvalidId || !ctx.state.objectExists(played)) return;
            if (!ctx.state.getObject(played).location.has_value()) return;   // already gone
            ctx.executor.readyObject(played);
            ctx.executor.buffUnit(played);
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
