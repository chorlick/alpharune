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

class LongSword : public SimpleEquipGear {
public:
    LongSword() : SimpleEquipGear(Domain::Fury) {}
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 345;
        d.def_id = R"RB(sfd-022-221)RB";
        d.name = R"RB(Long Sword)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-022/221)RB";
        d.collector_number = 22;
        d.artist = R"RB(黯荧岛Dark Glow)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Equipment)RB"};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.might_bonus = 2;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Equip);
        d.keywords.set(Keyword::QuickDraw);
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Quick-Draw] (This has [Reaction]. When you play it, attach it to a unit you control.)
[Equip] [R] ([R]: Attach this to a unit you control.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/48d798bf93afd1ef139dcafc6cd8705742b93169-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_345(CardRegistry& r) {
    r.registerCard(345, std::make_unique<LongSword>());
}

} // namespace riftbound
