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

class RadiantDawn : public LegendCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouStun; }
    void onTrigger(CardContext& ctx,
                   const std::vector<GameObjectId>& /*targets*/) override {
        // Buff a friendly unit (first on-board friendly unit found).
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller != ctx.controller) continue;
            if (!obj.location.has_value()) continue;
            ctx.executor.buffUnit(id);
            ctx.events.logTrace("RADIANT DAWN: stunned enemy -> buff " + obj.name);
            return;
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 297;
        d.def_id = R"RB(ogn-306-298)RB";
        d.name = R"RB(Radiant Dawn)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-306/298)RB";
        d.collector_number = 306;
        d.artist = R"RB(Su Ke)RB";
        d.card_type = CardType::Legend;
        d.domains = {Domain::Calm, Domain::Order};
        d.tags = {R"RB(Leona)RB"};
        d.rarity = Rarity::Showcase;
        d.ability_text = R"RB(When you stun one or more enemy units, buff a friendly unit. (If it doesn't have a buff, it gets a +1 [M] buff.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/496d173b74d036a9e28ca1b4383551be0148f13d-1488x2078.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_297(CardRegistry& r) {
    r.registerCard(297, std::make_unique<RadiantDawn>());
}

} // namespace riftbound
