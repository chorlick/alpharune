#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class EnthrallingProtector : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIConquerOrHold; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        ctx.state.player(ctx.controller).xp += 1;  // [Hunt]
        ctx.events.logTrace("ENTHRALLING PROTECTOR: [Hunt] +1 XP");
    }
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        return ActivationCost{.xp_cost = 2};
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        ctx.executor.buffUnit(ctx.source);  // +1 [M] buff if it doesn't have one
        ctx.events.logTrace("ENTHRALLING PROTECTOR: spent 2 XP, buffed self");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 724;
        d.def_id = R"RB(unl-162-219)RB";
        d.name = R"RB(Enthralling Protector)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-162/219)RB";
        d.collector_number = 162;
        d.artist = R"RB(Dao Trong Le)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Fae)RB", R"RB(Ionia)RB"};
        d.energy_cost = 2;
        d.might = 2;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Hunt);
        d.ability_text = R"RB([Hunt] (When I conquer or hold, gain 1 XP.)
Spend 2 XP: [Buff] me. (Give me a +1 [M] buff if I don't have one.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/3fdfe3bc7c7f81b99aa715bf54b52930ad41dcfa-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_724(CardRegistry& r) {
    r.registerCard(724, std::make_unique<EnthrallingProtector>());
}

} // namespace riftbound
