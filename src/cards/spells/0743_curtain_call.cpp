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
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true, .optional = true};
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        auto& src = ctx.state.getObject(ctx.source);
        int& used_mask = src.card_counters["curtain_call_used_mask"];

        // Compute legal-mode bitmask. Mode 0 (Draw) always legal if not
        // used. Modes 1, 2, 3 require a compatible target.
        uint32_t legal = 0;
        if (!(used_mask & (1 << 0))) legal |= (1u << 0);  // Draw 1 — no target needed
        bool has_target = !targets.empty() && ctx.state.objectExists(targets[0]);
        bool tgt_at_bf  = has_target && ctx.state.getObject(targets[0]).isAtBattlefield();
        bool tgt_at_base = has_target && ctx.state.getObject(targets[0]).isAtBase();
        if (!(used_mask & (1 << 1)) && tgt_at_bf)   legal |= (1u << 1);
        if (!(used_mask & (1 << 2)) && tgt_at_base) legal |= (1u << 2);
        if (!(used_mask & (1 << 3)) && tgt_at_bf)   legal |= (1u << 3);

        int mode = pickMode(ctx, "Curtain Call", 4,
                             {"Draw 1", "Deal 2 (BF)",
                              "Deal 3 (base)", "-4M (BF)"},
                             legal);
        if (mode < 0) return;  // -1 pending, -2 no legal mode

        used_mask |= (1 << mode);
        switch (mode) {
            case 0:
                ctx.events.logTrace("CURTAIN CALL: Draw 1");
                ctx.executor.drawCards(ctx.controller, 1);
                break;
            case 1:
                ctx.events.logTrace("CURTAIN CALL: Deal 2 (BF)");
                ctx.executor.dealDamage(targets[0], 2, ctx.source);
                break;
            case 2:
                ctx.events.logTrace("CURTAIN CALL: Deal 3 (base)");
                ctx.executor.dealDamage(targets[0], 3, ctx.source);
                break;
            case 3:
                ctx.events.logTrace("CURTAIN CALL: -4M (BF)");
                ctx.executor.giveTemporaryMight(targets[0], -4, /*minimum=*/0);
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
