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

// "When a friendly unit is played this turn, buff it. (If it doesn't have a
//  buff, it gets a +1 [M] buff.)  Draw 1."

class RallyTheTroops : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // The rest-of-turn rider is a delayed ability that fires for EVERY
        // friendly unit played until the turn ends (DelayedAbility::repeats);
        // the unit that was played reaches onTrigger as the item's
        // triggering_subject.
        DelayedAbility da;
        da.source = ctx.source;
        da.card_def_id = cardDefId();
        da.controller = ctx.controller;
        da.trigger = TriggerType::WhenYouPlayAUnit;
        da.expires_on_turn = ctx.state.turn.turn_number;
        da.repeats = true;
        ctx.state.delayed_abilities.push_back(da);
        ctx.executor.drawCards(ctx.controller, 1);
        ctx.events.logTrace("RALLY THE TROOPS: friendly units played this turn are buffed; draw 1");
    }
    // "When a friendly unit is played this turn, buff it."
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.chain.resuming.has_value()) return;
        const GameObjectId unit = ctx.state.chain.resuming->triggering_subject;
        if (unit == kInvalidId || !ctx.state.objectExists(unit)) return;
        const auto& u = ctx.state.getObject(unit);
        if (!u.isUnit() || !u.location.has_value() || u.controller != ctx.controller) return;
        if (ctx.executor.buffUnit(unit))
            ctx.events.logTrace("RALLY THE TROOPS: buffed " + u.name);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 487;
        d.def_id = R"RB(sfd-166-221)RB";
        d.name = R"RB(Rally the Troops)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-166/221)RB";
        d.collector_number = 166;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Order};
        d.energy_cost = 2;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
When a friendly unit is played this turn, buff it. (If it doesn't have a buff, it gets a +1 [M] buff.)
Draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/952476dd51338ff97774946ded134a2072b2e6c9-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_487(CardRegistry& r) {
    r.registerCard(487, std::make_unique<RallyTheTroops>());
}

} // namespace riftbound
