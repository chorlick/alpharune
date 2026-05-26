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

class DisarmingRake : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto gearTargets = [&]() {
            std::vector<GameObjectId> gears;
            for (auto& [id, obj] : ctx.state.objects) {
                if (!obj.isGear()) continue;
                if (!obj.location.has_value()) continue;  // must be on board
                gears.push_back(id);
            }
            return gears;
        };
        // "you may"
        auto still_legal = [&]() { return !gearTargets().empty(); };
        int conf = confirmOptional(ctx, "Disarming Rake: kill a gear?", still_legal);
        if (conf == -1) return;  // yielded for agent input
        if (conf == 0) return;   // declined or no legal gear

        auto gears = gearTargets();
        GameObjectId picked = pickTarget(ctx, "Disarming Rake: choose a gear", gears);
        if (picked == kInvalidId) return;  // yielded or fizzled
        ctx.executor.killObject(picked);
        ctx.events.logTrace("DISARMING RAKE: killed a gear");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 355;
        d.def_id = R"RB(sfd-032-221)RB";
        d.name = R"RB(Disarming Rake)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-032/221)RB";
        d.collector_number = 32;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Demacia)RB"};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.might = 2;
        d.ability_text = R"RB(When you play me, you may kill a gear.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/523732db17d8cf3c7c55f57c5dfb397b73e2b116-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_355(CardRegistry& r) {
    r.registerCard(355, std::make_unique<DisarmingRake>());
}

} // namespace riftbound
