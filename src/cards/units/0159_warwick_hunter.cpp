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

class WarwickHunter : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "I enter ready."
    bool entersReadyOnPlay() const override { return true; }
    // "When I attack, kill all damaged enemy units here."
    TriggerType triggerType() const override { return TriggerType::WhenIAttack; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto my_bf = ctx.state.getObject(ctx.source).battlefieldId();
        if (!my_bf) return;
        // Collect-then-kill: gather all damaged enemy units here first.
        std::vector<GameObjectId> victims;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller == ctx.controller) continue;
            if (obj.battlefieldId() != my_bf) continue;
            if (obj.damage_marked <= 0) continue;  // "damaged"
            victims.push_back(id);
        }
        for (auto id : victims)
            if (ctx.state.objectExists(id)) ctx.executor.killObject(id);
        if (!victims.empty())
            ctx.events.logTrace("WARWICK: killed " + std::to_string(victims.size()) +
                                 " damaged enemy units here");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 159;
        d.def_id = R"RB(ogn-159-298)RB";
        d.name = R"RB(Warwick, Hunter)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-159/298)RB";
        d.collector_number = 159;
        d.artist = R"RB(League Splash Team)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Dog)RB", R"RB(Warwick)RB", R"RB(Zaun)RB"};
        d.energy_cost = 6;
        d.power_cost = 1;
        d.might = 5;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(I enter ready.
When I attack, kill all damaged enemy units here.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/fcbf878905001fa81858e9f081346fd05425f264-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_159(CardRegistry& r) {
    r.registerCard(159, std::make_unique<WarwickHunter>());
}

} // namespace riftbound
