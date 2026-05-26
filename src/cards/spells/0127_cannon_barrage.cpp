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

class CannonBarrage : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        // AoE: deal 2 to all matching
        {
            std::vector<GameObjectId> to_damage;
            for (auto& [id, obj] : ctx.state.objects) {
                if (!obj.location.has_value()) continue;
                if (!obj.isUnit()) continue;
                if (obj.controller == ctx.controller) continue;
                to_damage.push_back(id);
            }
            for (auto id : to_damage)
                ctx.executor.dealDamage(id, 2, ctx.source);
            for (auto id : to_damage) {
                if (ctx.state.objectExists(id) &&
                    ctx.state.getObject(id).hasLethalDamage()) {
                    ctx.executor.killObject(id);
                }
            }
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 127;
        d.def_id = R"RB(ogn-127-298)RB";
        d.name = R"RB(Cannon Barrage)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-127/298)RB";
        d.collector_number = 127;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Body};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Deal 2 to all enemy units in combat.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/07ef531829e5c6084a8d31044ebd783a9266f59e-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_127(CardRegistry& r) {
    r.registerCard(127, std::make_unique<CannonBarrage>());
}

} // namespace riftbound
