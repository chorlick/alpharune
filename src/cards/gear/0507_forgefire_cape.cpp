#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include "cards/gear/equip_base.h"

namespace riftbound {
namespace {

class ForgefireCape : public UniversalEquipGear {
public:
    const CardDef& def() const override { return def_; }
    TriggerType equippedTriggerType() const override { return TriggerType::WhenIAttackOrDefend; }
    void onEquippedTrigger(CardContext& ctx, GameObjectId unit,
                            const std::vector<GameObjectId>& targets) override {
        auto bf = ctx.state.getObject(unit).battlefieldId();
        if (!bf) return;
        std::vector<GameObjectId> enemies;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller == ctx.controller) continue;
            auto obf = obj.battlefieldId();
            if (obf && *obf == *bf) enemies.push_back(id);
        }
        for (auto eid : enemies) ctx.executor.dealDamage(eid, 2, ctx.source);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 507;
        d.def_id = R"RB(sfd-190-221)RB";
        d.name = R"RB(Forgefire Cape)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-190/221)RB";
        d.collector_number = 190;
        d.artist = R"RB(Grafit Studio)RB";
        d.card_type = CardType::Gear;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Calm, Domain::Mind};
        d.tags = {R"RB(Ornn)RB", R"RB(Equipment)RB"};
        d.energy_cost = 4;
        d.power_cost = 2;
        d.might_bonus = 3;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Equip);
        d.keywords.set(Keyword::Unique);
        d.ability_text = R"RB([Unique] (Your deck can have only 1 card with this name.)
[Equip] [A] ([A]: Attach this to a unit you control.))RB";
        d.effect_text = R"RB(When I attack or defend, deal 2 to all enemy units here.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/9bc49433a6ec5a8f4f1b44351094523d51b6bc11-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_507(CardRegistry& r) {
    r.registerCard(507, std::make_unique<ForgefireCape>());
}

} // namespace riftbound
