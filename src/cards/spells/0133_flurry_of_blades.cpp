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

class FlurryOfBlades : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // AoE: collect targets BEFORE killing (CR — avoid iterator invalidation).
        std::vector<GameObjectId> to_damage;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit()) continue;
            if (!obj.isAtBattlefield()) continue;  // exclude base units
            to_damage.push_back(id);
        }
        for (auto id : to_damage) {
            if (ctx.state.objectExists(id))
                ctx.executor.dealDamage(id, 1, ctx.source);
        }
        for (auto id : to_damage) {
            if (ctx.state.objectExists(id) &&
                ctx.state.getObject(id).hasLethalDamage()) {
                ctx.executor.killObject(id);
            }
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 133;
        d.def_id = R"RB(ogn-133-298)RB";
        d.name = R"RB(Flurry of Blades)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-133/298)RB";
        d.collector_number = 133;
        d.artist = R"RB(Rafael Zanchetin)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Body};
        d.energy_cost = 1;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Deal 1 to all units at battlefields.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/c72f872e7e71b55b2074c5be4f2d7a44fa6fafca-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_133(CardRegistry& r) {
    r.registerCard(133, std::make_unique<FlurryOfBlades>());
}

} // namespace riftbound
