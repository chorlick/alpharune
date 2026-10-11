#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include "cards/card_helpers.h"
#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class TheBoss : public LegendCard {
public:
    const CardDef& def() const override { return def_; }

    // ── Part 2: "When you conquer, ready me." ──
    TriggerType triggerType() const override { return TriggerType::WhenIConquer; }
    void onTrigger(CardContext& ctx,
                   const std::vector<GameObjectId>& /*targets*/) override {
        // NOTE: "When YOU conquer" (any friendly conquer), but legends have no
        // location so WhenIConquer firing on the legend itself is the available
        // hook; engine fires legend conquer/hold triggers via the legend-zone
        // sweep. Ready the legend.
        if (!ctx.state.objectExists(ctx.source)) return;
        ctx.executor.readyObject(ctx.source);
        RIFT_TRACE(ctx.events, "THE BOSS: conquer -> ready me");
    }

    // ── Part 1: buffed-unit death replacement ──
    bool hasReplacementEffect() const override { return true; }
    // "If a buffed unit you control would die, you may pay [C], exhaust me,
    // and spend its buff to heal it, exhaust it, and recall it instead."
    // A replacement is not on the chain, so the "may" is a direct agent
    // decision (EffectExecutor::confirmNow); it is only asked when the whole
    // cost can be paid. [C] is one Power of this card's Domains — Body or
    // Order (CR 135.2.e.6.c).
    // The conditions, before the "may" and the payment.
    Applies replacementAppliesTo(const GameState& state, GameObjectId self_object,
                                 PlayerId controller, GameObjectId dying_unit) const override {
        return conditionsHold(state, self_object, controller, dying_unit) ? Applies::Yes
                                                                          : Applies::No;
    }
    bool conditionsHold(const GameState& state, GameObjectId self_id, PlayerId /*controller*/,
                        GameObjectId dying_unit) const {
        if (!state.objectExists(self_id) || !state.objectExists(dying_unit)) return false;
        const auto& self = state.getObject(self_id);
        const auto& victim = state.getObject(dying_unit);
        if (!victim.isUnit() || !victim.location.has_value()) return false;
        if (victim.controller != self.controller || victim.buff_count <= 0) return false;
        if (self.is_exhausted) return false;
        for (Domain d : def().domains)
            if (payableOnePower(state, self.controller, d)) return true;
        return false;
    }
    bool applyReplacement(CardContext& ctx, GameObjectId dying_unit) override {
        // Only a BUFFED unit I control that is on the board, only while I'm
        // ready (must "exhaust me" to pay) and [C] can be paid
        // (replacementApplies).
        if (!replacementApplies(ctx.state, ctx.source, ctx.controller, dying_unit)) return false;
        auto& self = ctx.state.getObject(ctx.source);
        auto& victim = ctx.state.getObject(dying_unit);
        const PlayerId me = self.controller;
        std::optional<Domain> pay_with;
        for (Domain d : def().domains)
            if (payableOnePower(ctx.state, me, d)) { pay_with = d; break; }
        if (!pay_with.has_value()) return false;
        if (!ctx.executor.confirmNow(me, "The Boss: pay [C], exhaust me and spend " +
                                         victim.name + "'s buff to recall it instead of dying"))
            return false;
        if (!payOnePower(ctx, me, *pay_with)) return false;
        ctx.executor.exhaustObject(ctx.source);
        ctx.executor.spendBuff(dying_unit, me);
        RIFT_TRACE(ctx.events, "THE BOSS: save buffed unit " + victim.name +
                            " (pay [C], exhaust me, spend buff) -> heal/exhaust/recall");
        // Heal, exhaust, recall to base — out of the combat it was in.
        // "(Send it to base. This isn't a move.)" — CR 457-460.
        ctx.executor.healExhaustRecall(dying_unit);
        return true;  // replacement consumed — the unit does not die
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 264;
        d.def_id = R"RB(ogn-269-298)RB";
        d.name = R"RB(The Boss)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-269/298)RB";
        d.collector_number = 269;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Legend;
        d.domains = {Domain::Body, Domain::Order};
        d.tags = {R"RB(Sett)RB"};
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(If a buffed unit you control would die, you may pay [C], exhaust me, and spend its buff to heal it, exhaust it, and recall it instead. (Send it to base. This isn't a move.)
When you conquer, ready me.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/b132becb843b2cf418cb110ead64758f49f51554-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_264(CardRegistry& r) {
    r.registerCard(264, std::make_unique<TheBoss>());
}

} // namespace riftbound
