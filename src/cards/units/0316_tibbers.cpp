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

class Tibbers : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        // AoE: deal 3 to all matching
        {
            std::vector<GameObjectId> to_damage;
            for (auto& [id, obj] : ctx.state.objects) {
                if (!obj.location.has_value()) continue;
                if (!obj.isUnit()) continue;
                to_damage.push_back(id);
            }
            for (auto id : to_damage)
                ctx.executor.dealDamage(id, 3, ctx.source);
            for (auto id : to_damage) {
                if (ctx.state.objectExists(id) &&
                    ctx.state.getObject(id).hasLethalDamage()) {
                    ctx.executor.killObject(id);
                }
            }
        }
    }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 316;
        d.def_id = R"RB(ogs-018-024)RB";
        d.name = R"RB(Tibbers)RB";
        d.set_code = R"RB(OGS)RB";
        d.set_name = R"RB(Proving Grounds)RB";
        d.public_code = R"RB(OGS-018/024)RB";
        d.collector_number = 18;
        d.artist = R"RB(Polar Engine Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Fury, Domain::Chaos};
        d.tags = {R"RB(Annie)RB"};
        d.energy_cost = 8;
        d.power_cost = 2;
        d.might = 7;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(When you play me, deal 3 to all units at battlefields.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/74f8bc78573b33d9979b0e9c121b858d770a490c-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_316(CardRegistry& r) {
    r.registerCard(316, std::make_unique<Tibbers>());
}

} // namespace riftbound
