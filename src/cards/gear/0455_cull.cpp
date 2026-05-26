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

class Cull : public SimpleEquipGear {
public:
    Cull() : SimpleEquipGear(Domain::Chaos) {}
    const CardDef& def() const override { return def_; }
    TriggerType equippedTriggerType() const override { return TriggerType::WhenIConquer; }
    void onEquippedTrigger(CardContext& ctx, GameObjectId unit,
                            const std::vector<GameObjectId>& targets) override {
        auto loc = ctx.state.getObject(unit).location.value_or(BaseLocation{ctx.controller});
        ctx.executor.createToken(ctx.controller, CardType::Gear, "Gold", 0, {}, {}, loc);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 455;
        d.def_id = R"RB(sfd-134-221)RB";
        d.name = R"RB(Cull)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-134/221)RB";
        d.collector_number = 134;
        d.artist = R"RB(黯荧岛Dark Glow)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Equipment)RB"};
        d.energy_cost = 1;
        d.might_bonus = 1;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Equip);
        d.ability_text = R"RB([Equip] [P] ([P]: Attach this to a unit you control.))RB";
        d.effect_text = R"RB(When I conquer, play a Gold gear token exhausted.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/885ee56a4153b4f9fd064e9d24c9d55400e1291f-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_455(CardRegistry& r) {
    r.registerCard(455, std::make_unique<Cull>());
}

} // namespace riftbound
