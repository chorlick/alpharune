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

class ScrutinizingSergeant : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // "gain 1 XP for each friendly unit" — count on-board friendly units
        // (including myself, who has just entered).
        int friendly_units = 0;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller != ctx.controller) continue;
            if (!obj.location.has_value()) continue;
            ++friendly_units;
        }
        ctx.state.player(ctx.controller).xp += friendly_units;
        ctx.events.logTrace("SCRUTINIZING SERGEANT: gained " +
                             std::to_string(friendly_units) + " XP");
    }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 719;
        d.def_id = R"RB(unl-157-219)RB";
        d.name = R"RB(Scrutinizing Sergeant)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-157/219)RB";
        d.collector_number = 157;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Elite)RB", R"RB(Demacia)RB"};
        d.energy_cost = 6;
        d.might = 6;
        d.ability_text = R"RB(When you play me, gain 1 XP for each friendly unit.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/18a034ba1f4eaf7204312ca68ffb413012f6cf6b-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_719(CardRegistry& r) {
    r.registerCard(719, std::make_unique<ScrutinizingSergeant>());
}

} // namespace riftbound
