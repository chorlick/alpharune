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

class RuinedRex : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIDie; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        // Deal 4 to an enemy unit at a battlefield
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller == ctx.controller) continue;
            if (!obj.isAtBattlefield()) continue;
            ctx.executor.dealDamage(id, 4, ctx.source);
            if (ctx.state.objectExists(id) && ctx.state.getObject(id).hasLethalDamage())
                ctx.executor.killObject(id);
            break;
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 629;
        d.def_id = R"RB(unl-067-219)RB";
        d.name = R"RB(Ruined Rex)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-067/219)RB";
        d.collector_number = 67;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Bilgewater)RB"};
        d.energy_cost = 6;
        d.power_cost = 1;
        d.might = 6;
        d.keywords.set(Keyword::Deathknell);
        d.ability_text = R"RB([Deathknell][>] Deal 4 to an enemy unit. (When I die, get the effect.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/05fc9613bd3a3c3c5002ff1d7d665b37fd18dcb7-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_629(CardRegistry& r) {
    r.registerCard(629, std::make_unique<RuinedRex>());
}

} // namespace riftbound
