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

class TheHarrowing : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Play a unit from your trash, ignoring its Energy cost." (Power cost
    // approximated as free.) Target lives in trash — defer to resolve-time pick.
    bool needsPlayTimeTarget() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ps = ctx.state.player(ctx.controller);
        std::vector<GameObjectId> trash_units;
        for (auto cid : ps.trash) {
            if (!ctx.state.objectExists(cid)) continue;
            if (ctx.state.getObject(cid).isUnit()) trash_units.push_back(cid);
        }
        if (trash_units.empty()) return;
        GameObjectId picked = pickTarget(ctx, "The Harrowing (unit from trash)",
                                          trash_units);
        if (picked == kInvalidId && ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->resume_point == 7) {
            return;  // suspended
        }
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        auto it = std::find(ps.trash.begin(), ps.trash.end(), picked);
        if (it != ps.trash.end()) ps.trash.erase(it);
        ctx.executor.playIgnoringCost(ctx.controller, picked);
        ctx.events.logTrace("THE HARROWING: played a unit from trash for free");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 198;
        d.def_id = R"RB(ogn-198-298)RB";
        d.name = R"RB(The Harrowing)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-198/298)RB";
        d.collector_number = 198;
        d.artist = R"RB(Rafael Zanchetin)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Chaos};
        d.energy_cost = 6;
        d.power_cost = 2;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(Play a unit from your trash, ignoring its Energy cost. (You must still pay its Power cost.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/6914034f56dd50ab37df26e3541c997479bb5a6d-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_198(CardRegistry& r) {
    r.registerCard(198, std::make_unique<TheHarrowing>());
}

} // namespace riftbound
