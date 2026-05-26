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

class TimeWarp : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        // Phase 5a's additional-turns queue lives on PlayerState. Push the
        // controller as the next turn-player.
        auto& ps = ctx.state.player(ctx.controller);
        ps.additional_turns.push_back(ctx.controller);
        ctx.events.logTrace("TIME WARP: queued extra turn for " +
                             std::string(toString(ctx.controller)));
        // "Banish this" — route source to banishment instead of trash.
        if (ctx.state.objectExists(ctx.source)) {
            ctx.executor.banishObject(ctx.source);
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 122;
        d.def_id = R"RB(ogn-122-298)RB";
        d.name = R"RB(Time Warp)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-122/298)RB";
        d.collector_number = 122;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Mind};
        d.energy_cost = 10;
        d.power_cost = 4;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(Take a turn after this one. Banish this.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/b97bbcf3cf6cb6e5f4baaa1bfbb85dc860eb950b-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_122(CardRegistry& r) {
    r.registerCard(122, std::make_unique<TimeWarp>());
}

} // namespace riftbound
