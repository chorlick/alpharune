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

class KatarinaReckless : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "When you hide a card, ready me. When you play a card from face down,
    // deal 2 to an enemy unit." Now both events have engine triggers.
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouHideACard, TriggerType::WhenYouPlayFromFacedown};
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        if (ctx.firing_trigger == TriggerType::WhenYouHideACard) {
            ctx.executor.readyObject(ctx.source);
            ctx.events.logTrace("KATARINA RECKLESS: hid a card -> ready me");
        } else if (ctx.firing_trigger == TriggerType::WhenYouPlayFromFacedown) {
            std::vector<GameObjectId> legal;
            PlayerId opp = opponent(ctx.controller);
            for (auto& [id, obj] : ctx.state.objects) {
                if (obj.isUnit() && obj.controller == opp && obj.location.has_value())
                    legal.push_back(id);
            }
            GameObjectId t = pickTarget(ctx, "Katarina: deal 2 to an enemy unit", legal);
            if (t == kInvalidId || !ctx.state.objectExists(t)) return;
            ctx.executor.dealDamage(t, 2, ctx.source);
            ctx.events.logTrace("KATARINA RECKLESS: played from facedown -> deal 2");
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 585;
        d.def_id = R"RB(unl-023-219)RB";
        d.name = R"RB(Katarina, Reckless)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-023/219)RB";
        d.collector_number = 23;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Katarina)RB", R"RB(Noxus)RB"};
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 5;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When you hide a card, ready me.
When you play a card from face down, deal 2 to an enemy unit.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/12da73cb78a9cfa052749f317ed56a27106908c6-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_585(CardRegistry& r) {
    r.registerCard(585, std::make_unique<KatarinaReckless>());
}

} // namespace riftbound
