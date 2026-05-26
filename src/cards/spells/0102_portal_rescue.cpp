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

class PortalRescue : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (targets.empty() || !ctx.state.objectExists(targets[0])) return;
        GameObjectId unit = targets[0];
        PlayerId owner = ctx.state.getObject(unit).owner;
        // "Banish a friendly unit, then its owner plays it to their base,
        // ignoring its cost."
        ctx.executor.banishObject(unit);
        if (!ctx.state.objectExists(unit)) return;  // token ceased to exist
        // Remove from owner's banishment zone before re-playing.
        auto& bz = ctx.state.player(owner).banishment;
        auto it = std::find(bz.begin(), bz.end(), unit);
        if (it != bz.end()) bz.erase(it);
        ctx.executor.playIgnoringCost(owner, unit,
                                      LocationId{BaseLocation{owner}});
        ctx.events.logTrace("PORTAL RESCUE: banished then re-played to base");
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true, .must_be_friendly = true};
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 102;
        d.def_id = R"RB(ogn-102-298)RB";
        d.name = R"RB(Portal Rescue)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-102/298)RB";
        d.collector_number = 102;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Mind};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Banish a friendly unit, then its owner plays it to their base, ignoring its cost.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/bafb4c68fa9a3eb71fecd0cdffb9e20b9f68d532-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_102(CardRegistry& r) {
    r.registerCard(102, std::make_unique<PortalRescue>());
}

} // namespace riftbound
