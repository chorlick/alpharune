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

class DoranSRing : public SimpleEquipGear {
public:
    DoranSRing() : SimpleEquipGear(Domain::Chaos) {}
    const CardDef& def() const override { return def_; }
    TriggerType equippedTriggerType() const override { return TriggerType::WhenIConquer; }
    void onEquippedTrigger(CardContext& ctx, GameObjectId unit,
                            const std::vector<GameObjectId>& targets) override {
        ctx.executor.discardCards(ctx.controller, 1);
        ctx.executor.drawCards(ctx.controller, 1);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 445;
        d.def_id = R"RB(sfd-124-221)RB";
        d.name = R"RB(Doran's Ring)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-124/221)RB";
        d.collector_number = 124;
        d.artist = R"RB(黯荧岛Dark Glow)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Equipment)RB"};
        d.energy_cost = 1;
        d.might_bonus = 1;
        d.keywords.set(Keyword::Equip);
        d.ability_text = R"RB([Equip] [P] ([P]: Attach this to a unit you control.))RB";
        d.effect_text = R"RB(When I conquer, discard 1, then draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/054df5a11967b768430a2f763e1b5d4dbbac3b50-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_445(CardRegistry& r) {
    r.registerCard(445, std::make_unique<DoranSRing>());
}

} // namespace riftbound
