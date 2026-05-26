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

class HallOfLegends : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouConquerHere; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto legend_ready_state = [&]() {
            auto& ps = ctx.state.player(ctx.controller);
            if (ps.rune_pool.energy < 1) return false;
            GameObjectId lz = ps.legend_zone;
            if (lz == kInvalidId || !ctx.state.objectExists(lz)) return false;
            return ctx.state.getObject(lz).is_exhausted;  // only worth it if exhausted
        };
        int conf = confirmOptional(ctx, "Hall of Legends: pay [1] to ready your legend?",
                                   legend_ready_state);
        if (conf == -1) return;  // waiting on agent
        if (conf == 0) return;   // declined / can't pay
        auto& ps = ctx.state.player(ctx.controller);
        if (ps.rune_pool.energy < 1) return;
        ps.rune_pool.energy -= 1;
        GameObjectId lz = ps.legend_zone;
        if (lz != kInvalidId && ctx.state.objectExists(lz)) {
            ctx.executor.readyObject(lz);
            ctx.events.logTrace("HALL OF LEGENDS: paid [1] -> readied legend");
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 524;
        d.def_id = R"RB(sfd-210-221)RB";
        d.name = R"RB(Hall of Legends)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-210/221)RB";
        d.collector_number = 210;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you conquer here, you may pay [1] to ready your legend.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/3b6438877bd2bd95e7a3a8921ddf6bca26d3fd95-1039x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_524(CardRegistry& r) {
    r.registerCard(524, std::make_unique<HallOfLegends>());
}

} // namespace riftbound
