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

class WarmogSArmor : public SimpleEquipGear {
public:
    WarmogSArmor() : SimpleEquipGear(Domain::Body) {}
    const CardDef& def() const override { return def_; }
    TriggerType equippedTriggerType() const override { return TriggerType::WhenIConquer; }
    void onEquippedTrigger(CardContext& ctx, GameObjectId unit,
                            const std::vector<GameObjectId>& targets) override {
        ctx.executor.buffUnit(unit);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 430;
        d.def_id = R"RB(sfd-108-221)RB";
        d.name = R"RB(Warmog's Armor)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-108/221)RB";
        d.collector_number = 108;
        d.artist = R"RB(黯荧岛Dark Glow)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Equipment)RB"};
        d.energy_cost = 1;
        d.might_bonus = 1;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Equip);
        d.ability_text = R"RB([Equip] [O] ([O]: Attach this to a unit you control.))RB";
        d.effect_text = R"RB(When I conquer, buff me. (If I don't have a buff, I get a +1 [M] buff.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/0d680867670dd75a66a632cd6f044a7f7e39b42b-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_430(CardRegistry& r) {
    r.registerCard(430, std::make_unique<WarmogSArmor>());
}

} // namespace riftbound
