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

class DeadlyFlourish : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};
    }
    // Phase 6q — defer target selection so the policy head gets
    // distinct vocab slots per target choice.
    bool needsPlayTimeTarget() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
        GameObjectId picked = pickTarget(ctx, "Deadly Flourish", legal);
        if (picked == kInvalidId) return;
        auto victim = picked;

        // Register the death-watch BEFORE dealing damage so that if the
        // damage kills the unit, the delayed ability is already in place
        // to be fired by onUnitDied.
        DelayedAbility da;
        da.source = ctx.source;
        da.card_def_id = cardDefId();
        da.controller = ctx.controller;
        da.trigger = TriggerType::WhenIDie;
        da.target_filter = victim;
        ctx.state.delayed_abilities.push_back(da);
        ctx.events.logTrace("DEADLY FLOURISH: armed death-watch on victim "
                             "(id=" + std::to_string(victim) + ")");

        ctx.executor.dealDamage(victim, 3, ctx.source);
        // Engine's processLethalDamage (or combat resolution) will kill
        // the unit if damage_marked >= might. We don't manually kill
        // here — the engine handles it via the SBA pass.
    }
    // When the delayed ability fires, spawn the Gold gear token.
    TriggerType triggerType() const override { return TriggerType::WhenIDie; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        ctx.executor.createToken(ctx.controller, CardType::Gear, "Gold",
                                  /*might=*/0, /*tags=*/{},
                                  /*kw=*/KeywordSet{},
                                  BaseLocation{ctx.controller},
                                  /*exhausted=*/true);
        ctx.events.logTrace("DEADLY FLOURISH: victim died — spawned Gold token");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 635;
        d.def_id = R"RB(unl-073-219)RB";
        d.name = R"RB(Deadly Flourish)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-073/219)RB";
        d.collector_number = 73;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Mind};
        d.energy_cost = 4;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB(Deal 3 to an enemy unit. When it dies this turn, play a Gold gear token exhausted. (It has "[Reaction][>] Kill this, [E]: [Add] [A]."))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/81195df7122beaba38ebc8b8212ceb0e7593afe9-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_635(CardRegistry& r) {
    r.registerCard(635, std::make_unique<DeadlyFlourish>());
}

} // namespace riftbound
