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

class TreasureHoard : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouConquerHere; }

    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ps = ctx.state.player(ctx.controller);
        auto still_legal = [&ps]() { return ps.rune_pool.energy >= 1; };
        if (!still_legal()) return;
        int conf = confirmOptional(ctx,
            "Treasure Hoard: pay [1] for a Gold token?", still_legal);
        if (conf == -1) return;
        if (conf < 1) return;
        ps.rune_pool.energy -= 1;

        // "here" = the battlefield this card lives at; place the token there.
        LocationId loc{BaseLocation{ctx.controller}};
        for (auto& bf : ctx.state.battlefields) {
            if (bf.card_object_id == ctx.source) {
                loc = LocationId{BattlefieldLocation{bf.id}};
                break;
            }
        }
        auto tok = ctx.executor.createToken(ctx.controller, CardType::Gear, "Gold",
                                             0, {}, {}, loc, /*enter_ready=*/false);
        // "exhausted" — gear normally enters ready, so force-exhaust.
        if (ctx.state.objectExists(tok)) {
            ctx.state.getObject(tok).is_exhausted = true;
        }
        ctx.events.logTrace("TREASURE HOARD: paid [1] -> Gold gear token (exhausted)");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 534;
        d.def_id = R"RB(sfd-220-221)RB";
        d.name = R"RB(Treasure Hoard)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-220/221)RB";
        d.collector_number = 220;
        d.artist = R"RB(Caravan Studio)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you conquer here, you may pay [1] to play a Gold gear token exhausted.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/cb39bf9f8c30d1ed756ba1b83c975a89d3635159-1039x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_534(CardRegistry& r) {
    r.registerCard(534, std::make_unique<TreasureHoard>());
}

} // namespace riftbound
