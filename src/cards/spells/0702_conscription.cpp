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

class Conscription : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "You may spend 5 XP as an additional cost to play this.
    //  Choose an enemy unit at a battlefield with 3 [M] or less. If you paid the
    //  additional cost, choose any enemy unit at a battlefield instead. Take
    //  control of it, exhaust it, and recall it."
    // The additional cost is decided and paid, and the unit chosen from the
    // matching set, as the spell is played (CR 355).
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    // It chooses no unit of its controller's (the play gate, CR 355 -> 356).
    FinalizeChoice mayChooseAtFinalization(const GameState& state, PlayerId controller,
                                           GameObjectId object) const override {
        (void)state; (void)controller; (void)object;
        return FinalizeChoice::No;
    }
    static std::vector<GameObjectId> candidates(const GameState& state, PlayerId me,
                                                bool any_might) {
        return chooseableUnits(state, me, [&](const GameObject& o) {
            return o.controller != me && o.isAtBattlefield() &&
                   (any_might || o.current_might <= 3);
        });
    }
    bool hasLegalTargets(const GameState& state, PlayerId controller) const override {
        return !candidates(state, controller, state.player(controller).xp >= 5).empty();
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ps = ctx.state.player(ctx.controller);
        // Optional additional cost: spend 5 XP. Paying or not is offered only
        // when the unit can then be chosen (CR 355.16: no choice that leads to
        // an illegal one): with no enemy unit of 3 [M] or less, the cost is
        // the only way to play it and is not asked (it was asked, and
        // declining it left the spell with no target — a fuzz finding of
        // testing phase batch 3).
        const bool narrow_ok = !candidates(ctx.state, ctx.controller, false).empty();
        const bool wide_ok = ps.xp >= 5 && !candidates(ctx.state, ctx.controller, true).empty();
        const uint32_t mask = (narrow_ok ? 1u : 0u) | (wide_ok ? 2u : 0u);
        int mode = -2;
        if (mask == 3u) {
            mode = pickMode(ctx, "Conscription: spend 5 XP as an additional cost?", 2,
                            {"Don't spend 5 XP", "Spend 5 XP (any enemy unit)"}, mask);
            if (mode == -1) return;  // waiting on agent
        } else {
            settleMode(ctx, mask == 2u ? 1 : 0);
            mode = pickMode(ctx, "Conscription: spend 5 XP as an additional cost?", 2, {},
                            mask == 0u ? 1u : mask);
        }
        const bool widened = (mode == 1);

        GameObjectId picked = pickTarget(ctx, "Conscription target",
                                         candidates(ctx.state, ctx.controller, widened));
        if (picked == kInvalidId && ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->resume_point == 7) {
            return;  // suspended for agent decision
        }
        // The cost is paid once, after the last choice (this callback is
        // re-entered from the top with each answer).
        if (widened && costDueNow(ctx)) {
            if (ps.xp < 5) return;
            ps.xp -= 5;
            RIFT_TRACE(ctx.events, "CONSCRIPTION: spent 5 XP");
        }
        if (finalizingChoices(ctx)) return;   // choices end here (CR 355)
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;

        // Take control of it, exhaust it, and recall it.
        ctx.executor.takeControl(picked, ctx.controller, /*until_end_of_turn=*/false);
        ctx.executor.exhaustObject(picked);
        // Recall (CR 457-458): relocated to its controller's base. Not a Move,
        // so no UnitMovedEvent and no "when I move" triggers. Attached gear
        // goes with it; damage and statuses are unaffected.
        ctx.executor.recallToBase(picked);
        RIFT_TRACE(ctx.events, "CONSCRIPTION: take control + exhaust + recall (paid="
                            + std::to_string(widened) + ")");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 702;
        d.def_id = R"RB(unl-140-219)RB";
        d.name = R"RB(Conscription)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-140/219)RB";
        d.collector_number = 140;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Chaos};
        d.energy_cost = 5;
        d.power_cost = 2;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(You may spend 5 XP as an additional cost to play this.
Choose an enemy unit at a battlefield with 3 [M] or less. If you paid the additional cost, choose any enemy unit at a battlefield instead. Take control of it, exhaust it, and recall it.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/1c05bff48666586a2b3552b5638deaf20d9006f7-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_702(CardRegistry& r) {
    r.registerCard(702, std::make_unique<Conscription>());
}

} // namespace riftbound
