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

class SkyfallOfAreion : public SimpleEquipGear {
public:
    SkyfallOfAreion() : SimpleEquipGear(Domain::Fury, 1) {}
    const CardDef& def() const override { return def_; }
    bool crossesHoldConquerTriggers() const override { return true; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 353;
        d.def_id = R"RB(sfd-030-221)RB";
        d.name = R"RB(Skyfall of Areion)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-030/221)RB";
        d.collector_number = 30;
        d.artist = R"RB(黯荧岛Dark Glow)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Equipment)RB"};
        d.energy_cost = 3;
        d.might_bonus = 2;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Equip);
        d.ability_text = R"RB([Equip] [1][R] ([1][R]: Attach this to a unit you control.))RB";
        d.effect_text = R"RB(My hold effects are also conquer effects, and vice versa.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/bea8390beb651502413cf644319c8ca6a8db1102-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_353(CardRegistry& r) {
    r.registerCard(353, std::make_unique<SkyfallOfAreion>());
}

} // namespace riftbound
