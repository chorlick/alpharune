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

class LoyalPoro : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIDie; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        const auto& self = ctx.state.getObject(ctx.source);
        // Read where I died from. last_location is set by killObject when
        // a unit dies; current `location` will be empty (in trash now).
        if (!self.last_location.has_value()) return;
        const auto& death_loc = *self.last_location;
        // "Didn't die alone" — any OTHER friendly unit still at the same
        // location, not counting myself (already in trash).
        bool has_company = false;
        for (const auto& [oid, obj] : ctx.state.objects) {
            if (oid == ctx.source) continue;
            if (!obj.isUnit() || obj.controller != ctx.controller) continue;
            if (!obj.location.has_value()) continue;
            if (*obj.location == death_loc) { has_company = true; break; }
        }
        if (has_company) {
            ctx.events.logTrace("LOYAL PORO: not alone at death — drawing 1");
            ctx.executor.drawCards(ctx.controller, 1);
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 718;
        d.def_id = R"RB(unl-156-219)RB";
        d.name = R"RB(Loyal Poro)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-156/219)RB";
        d.collector_number = 156;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Poro)RB", R"RB(Freljord)RB"};
        d.energy_cost = 3;
        d.might = 3;
        d.keywords.set(Keyword::Deathknell);
        d.ability_text = R"RB([Deathknell][>] If I didn't die alone, draw 1. (When I die, get the effect. I wasn't alone if there were other friendly units here.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/860e1afc7364a002c6305ece7b6941e2e328d0a4-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_718(CardRegistry& r) {
    r.registerCard(718, std::make_unique<LoyalPoro>());
}

} // namespace riftbound
