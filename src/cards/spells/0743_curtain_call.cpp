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

class CurtainCall : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Choose one you haven't already chosen — Draw 1. / Deal 2 to a unit at a
    //  battlefield. / Deal 3 to a unit at a base. / Give a unit at a
    //  battlefield -4 [M] this turn."
    // The mode and then that mode's target are chosen as the spell is played.
    //
    // "[Repeat] — [1] / [A] / [1][A]": three separate Repeat costs, each
    // payable once (CR 820.1.c.2-3), so the spell runs up to four times and
    // each run picks a mode not yet chosen. The "haven't already chosen"
    // memory belongs to ONE play of the card (keyed by the chain item), so a
    // later cast starts fresh.
    std::vector<RepeatCost> repeatCosts() const override {
        return {RepeatCost{.energy = 1},
                RepeatCost{.power = 1, .power_domain = Domain::Count},
                RepeatCost{.energy = 1, .power = 1, .power_domain = Domain::Count}};
    }
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    // It can choose any unit its controller has on the board (the play gate).
    FinalizeChoice mayChooseAtFinalization(const GameState& state, PlayerId controller,
                                           GameObjectId object) const override {
        return friendlyUnitChoice(state, controller, object);
    }
    static constexpr const char* kMaskKey = "curtain_call_used_mask";
    static constexpr const char* kPlayKey = "curtain_call_play";

    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& counters = ctx.state.getObject(ctx.source).card_counters;
        // Modes used by earlier executions of THIS play (none unless
        // repeated). Direct invocations with no chain item count as one play.
        const int play = ctx.state.chain.resuming.has_value()
            ? static_cast<int>(ctx.state.chain.resuming->id) + 1 : 0;
        const int used_mask = counters[kPlayKey] == play ? counters[kMaskKey] : 0;

        const auto at_bf = chooseableUnits(ctx.state, ctx.controller,
            [](const GameObject& o) { return o.isAtBattlefield(); });
        const auto at_base = chooseableUnits(ctx.state, ctx.controller,
            [](const GameObject& o) { return o.isAtBase(); });
        // Mode 0 (Draw) needs no target; 1 and 3 a unit at a battlefield, 2 a
        // unit at a base.
        uint32_t legal = 0;
        if (!(used_mask & (1 << 0))) legal |= (1u << 0);
        if (!(used_mask & (1 << 1)) && !at_bf.empty())   legal |= (1u << 1);
        if (!(used_mask & (1 << 2)) && !at_base.empty()) legal |= (1u << 2);
        if (!(used_mask & (1 << 3)) && !at_bf.empty())   legal |= (1u << 3);

        int mode = pickMode(ctx, "Curtain Call", 4,
                             {"Draw 1", "Deal 2 (BF)",
                              "Deal 3 (base)", "-4M (BF)"},
                             legal);
        if (mode < 0) return;  // -1 pending, -2 no legal mode
        GameObjectId target = kInvalidId;
        if (mode != 0) {
            target = pickTarget(ctx, "Curtain Call", mode == 2 ? at_base : at_bf);
            if (target == kInvalidId && ctx.state.chain.resuming.has_value() &&
                ctx.state.chain.resuming->resume_point == 7) return;   // waiting
        }
        if (finalizingChoices(ctx)) return;   // choices end here (CR 355)

        counters[kPlayKey] = play;
        counters[kMaskKey] = used_mask | (1 << mode);
        const bool has_target = target != kInvalidId && ctx.state.objectExists(target);
        switch (mode) {
            case 0:
                RIFT_TRACE(ctx.events, "CURTAIN CALL: Draw 1");
                ctx.executor.drawCards(ctx.controller, 1);
                break;
            case 1:
                RIFT_TRACE(ctx.events, "CURTAIN CALL: Deal 2 (BF)");
                if (has_target) ctx.executor.dealDamage(target, 2, ctx.source);
                break;
            case 2:
                RIFT_TRACE(ctx.events, "CURTAIN CALL: Deal 3 (base)");
                if (has_target) ctx.executor.dealDamage(target, 3, ctx.source);
                break;
            case 3:
                RIFT_TRACE(ctx.events, "CURTAIN CALL: -4M (BF)");
                if (has_target) ctx.executor.giveTemporaryMight(target, -4, /*minimum=*/0);
                break;
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 743;
        d.def_id = R"RB(unl-182-219)RB";
        d.name = R"RB(Curtain Call)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-182/219)RB";
        d.collector_number = 182;
        d.artist = R"RB(华锐)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Fury, Domain::Mind};
        d.tags = {R"RB(Jhin)RB"};
        d.energy_cost = 4;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Repeat);
        d.ability_text = R"RB([Repeat] — [1] / [A] / [1][A] (You may pay each additional cost to repeat this spell's effect.)
Choose one you haven't already chosen —
Draw 1.Deal 2 to a unit at a battlefield.Deal 3 to a unit at a base.Give a unit at a battlefield -4 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/01496068d9a8d5567672d802bd008297bd1fe8cc-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_743(CardRegistry& r) {
    r.registerCard(743, std::make_unique<CurtainCall>());
}

} // namespace riftbound
