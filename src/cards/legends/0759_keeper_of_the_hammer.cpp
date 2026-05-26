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

class KeeperOfTheHammer : public LegendCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIConquerOrHold; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        ctx.state.player(ctx.controller).xp += 1;
        ctx.events.logTrace("KEEPER OF THE HAMMER: +1 XP on hold");
    }
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        return ActivationCost{.exhaust = true, .xp_cost = 3};
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        ctx.executor.drawCards(ctx.controller, 1);
        ctx.events.logTrace("KEEPER OF THE HAMMER: spent 3 XP, drew 1");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 759;
        d.def_id = R"RB(unl-203-219)RB";
        d.name = R"RB(Keeper of the Hammer)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-203/219)RB";
        d.collector_number = 203;
        d.artist = R"RB(Pandart Studio)RB";
        d.card_type = CardType::Legend;
        d.domains = {Domain::Body, Domain::Order};
        d.tags = {R"RB(Poppy)RB"};
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When you hold, gain 1 XP.
Spend 3 XP, [E]: Draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ee1393b9bace7bfea4e87405b793bb5462305ed0-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_759(CardRegistry& r) {
    r.registerCard(759, std::make_unique<KeeperOfTheHammer>());
}

} // namespace riftbound
