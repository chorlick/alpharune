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

class LonelyPoro : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "[Deathknell] — If I died alone, draw 1." Alone = no OTHER friendly
    // units at the location where I died.
    TriggerType triggerType() const override { return TriggerType::WhenIDie; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        const auto& self = ctx.state.getObject(ctx.source);
        std::optional<LocationId> died_at = self.last_location;
        if (!died_at.has_value()) died_at = self.location;
        if (!died_at.has_value()) {
            ctx.executor.drawCards(ctx.controller, 1);
            ctx.events.logTrace("LONELY PORO: death location unknown -> draw 1 (approx alone)");
            return;
        }
        bool other_friendly_here = false;
        for (auto& [id, obj] : ctx.state.objects) {
            if (id == ctx.source) continue;
            if (!obj.isUnit() || obj.controller != ctx.controller) continue;
            if (!obj.location.has_value() || *obj.location != *died_at) continue;
            other_friendly_here = true;
            break;
        }
        if (!other_friendly_here) {
            ctx.executor.drawCards(ctx.controller, 1);
            ctx.events.logTrace("LONELY PORO: died alone -> draw 1");
        } else {
            ctx.events.logTrace("LONELY PORO: did not die alone -> no draw");
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 359;
        d.def_id = R"RB(sfd-036-221)RB";
        d.name = R"RB(Lonely Poro)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-036/221)RB";
        d.collector_number = 36;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Poro)RB", R"RB(Freljord)RB"};
        d.energy_cost = 2;
        d.might = 2;
        d.keywords.set(Keyword::Deathknell);
        d.ability_text = R"RB([Deathknell] — If I died alone, draw 1. (When I die, get the effect. I'm alone if there are no other friendly units here.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/f5ab933035b7a6d6bb1d35c10c13f58c7fbf3398-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_359(CardRegistry& r) {
    r.registerCard(359, std::make_unique<LonelyPoro>());
}

} // namespace riftbound
