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
    TriggerType triggerType() const override { return TriggerType::WhenIDie; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        const auto& self = ctx.state.getObject(ctx.source);
        // Where did I die? Prefer last_location (set at death), else location.
        std::optional<LocationId> died_at = self.last_location;
        if (!died_at.has_value()) died_at = self.location;
        if (!died_at.has_value()) {
            // No known death location — can't evaluate "alone" reliably; treat
            // as alone (documented approximation) and draw.
            ctx.executor.drawCards(ctx.controller, 1);
            ctx.events.logTrace("LONELY PORO: death location unknown -> draw 1 (approx alone)");
            return;
        }
        // "alone" = no OTHER friendly units still at that location.
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
        d.id = 777;
        d.def_id = R"RB(unl-221-219)RB";
        d.name = R"RB(Lonely Poro)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-221/219)RB";
        d.collector_number = 221;
        d.artist = R"RB(FOREDAWN)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Poro)RB", R"RB(Freljord)RB"};
        d.energy_cost = 2;
        d.might = 2;
        d.rarity = Rarity::Showcase;
        d.keywords.set(Keyword::Deathknell);
        d.ability_text = R"RB([Deathknell][>] If I died alone, draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/82b7043ef6519a4c7e2eb7d45cb6ae91dfd749ca-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_777(CardRegistry& r) {
    r.registerCard(777, std::make_unique<LonelyPoro>());
}

} // namespace riftbound
