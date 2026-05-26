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

class GrimResolve : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                  .must_be_friendly = true};
    }
    bool needsPlayTimeTarget() const override { return true; }

    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
        GameObjectId picked = pickTarget(ctx, "Grim Resolve", legal);
        if (picked == kInvalidId) return;  // yielded / fizzle

        ctx.executor.giveTemporaryMight(picked, 3);

        // Arm the "when it wins a combat this turn, gain 2 XP" rider.
        DelayedAbility da;
        da.source = ctx.source;
        da.card_def_id = cardDefId();
        da.controller = ctx.controller;
        da.trigger = TriggerType::WhenIWinCombat;
        da.target_filter = picked;
        da.expires_on_turn = ctx.state.turn.turn_number;  // this turn only
        ctx.state.delayed_abilities.push_back(da);
        ctx.events.logTrace("GRIM RESOLVE: +3M and armed win-combat XP rider");
    }

    // Single non-play trigger: when the delayed ability fires (the buffed unit
    // won a combat), gain 2 XP.
    TriggerType triggerType() const override { return TriggerType::WhenIWinCombat; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        ctx.state.player(ctx.controller).xp += 2;
        ctx.events.logTrace("GRIM RESOLVE: buffed unit won combat -> gain 2 XP");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 657;
        d.def_id = R"RB(unl-095-219)RB";
        d.name = R"RB(Grim Resolve)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-095/219)RB";
        d.collector_number = 95;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Body};
        d.energy_cost = 2;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Give a friendly unit +3 [M] this turn. When it wins a combat this turn, gain 2 XP.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/4705a4ef2589f2a82021f1b70dcdb7e289a88fdf-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_657(CardRegistry& r) {
    r.registerCard(657, std::make_unique<GrimResolve>());
}

} // namespace riftbound
