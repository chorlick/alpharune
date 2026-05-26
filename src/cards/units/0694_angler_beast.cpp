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

class AnglerBeast : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        std::vector<GameObjectId> to_bounce;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit()) continue;
            if (!obj.location.has_value()) continue;
            if (obj.current_might > 2) continue;  // "2 [M] or less"
            to_bounce.push_back(id);
        }
        for (auto id : to_bounce) {
            if (ctx.state.objectExists(id)) ctx.executor.bounceToHand(id);
        }
        ctx.events.logTrace("ANGLER BEAST: bounced " +
                             std::to_string(to_bounce.size()) + " small units");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 694;
        d.def_id = R"RB(unl-132-219)RB";
        d.name = R"RB(Angler Beast)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-132/219)RB";
        d.collector_number = 132;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Bilgewater)RB"};
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 5;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you play me, return all units with 2 [M] or less to their owners' hands.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/68872e2119146768f8fa113376876fa64699297d-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_694(CardRegistry& r) {
    r.registerCard(694, std::make_unique<AnglerBeast>());
}

} // namespace riftbound
