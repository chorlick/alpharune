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

class KogMawCaustic : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        // AoE: deal 4 to all matching
        {
            std::vector<GameObjectId> to_damage;
            for (auto& [id, obj] : ctx.state.objects) {
                if (!obj.location.has_value()) continue;
                if (!obj.isUnit()) continue;
                to_damage.push_back(id);
            }
            for (auto id : to_damage)
                ctx.executor.dealDamage(id, 4, ctx.source);
            for (auto id : to_damage) {
                if (ctx.state.objectExists(id) &&
                    ctx.state.getObject(id).hasLethalDamage()) {
                    ctx.executor.killObject(id);
                }
            }
        }
    }
    TriggerType triggerType() const override { return TriggerType::WhenIDie; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 190;
        d.def_id = R"RB(ogn-190-298)RB";
        d.name = R"RB(Kog'Maw, Caustic)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-190/298)RB";
        d.collector_number = 190;
        d.artist = R"RB(League Splash Team)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Kog'Maw)RB", R"RB(The Void)RB"};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.might = 1;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Deathknell);
        d.ability_text = R"RB([Deathknell] — Deal 4 to all units at my battlefield. (When I die, get the effect.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/1ba2b780619714f2bf97597a180f3b118f4faf55-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_190(CardRegistry& r) {
    r.registerCard(190, std::make_unique<KogMawCaustic>());
}

} // namespace riftbound
