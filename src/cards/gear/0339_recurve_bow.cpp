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

class RecurveBow : public SimpleEquipGear {
public:
    RecurveBow() : SimpleEquipGear(Domain::Fury) {}
    const CardDef& def() const override { return def_; }
    TriggerType equippedTriggerType() const override { return TriggerType::WhenIAttackOrDefend; }
    void onEquippedTrigger(CardContext& ctx, GameObjectId unit,
                            const std::vector<GameObjectId>& targets) override {
        // Deal 2 to an enemy unit here
        auto bf = ctx.state.getObject(unit).battlefieldId();
        if (!bf) return;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller == ctx.controller) continue;
            auto obf = obj.battlefieldId();
            if (obf && *obf == *bf) {
                ctx.executor.dealDamage(id, 2, ctx.source);
                break;
            }
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 339;
        d.def_id = R"RB(sfd-016-221)RB";
        d.name = R"RB(Recurve Bow)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-016/221)RB";
        d.collector_number = 16;
        d.artist = R"RB(黯荧岛Dark Glow)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Equipment)RB"};
        d.energy_cost = 2;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Equip);
        d.ability_text = R"RB([Equip] [R] ([R]: Attach this to a unit you control.))RB";
        d.effect_text = R"RB(When I attack or defend, deal 2 to an enemy unit here.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/763604237b4b59df5df98f61528be2233b850dd5-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_339(CardRegistry& r) {
    r.registerCard(339, std::make_unique<RecurveBow>());
}

} // namespace riftbound
