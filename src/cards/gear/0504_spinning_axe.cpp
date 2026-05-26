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

class SpinningAxe : public UniversalEquipGear {
public:
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 504;
        d.def_id = R"RB(sfd-186-221)RB";
        d.name = R"RB(Spinning Axe)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-186/221)RB";
        d.collector_number = 186;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Gear;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Fury, Domain::Chaos};
        d.tags = {R"RB(Draven)RB", R"RB(Equipment)RB"};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.might_bonus = 3;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Equip);
        d.keywords.set(Keyword::QuickDraw);
        d.keywords.set(Keyword::Reaction);
        d.keywords.set(Keyword::Temporary);
        d.ability_text = R"RB([Quick-Draw] (This has [Reaction]. When you play it, attach it to a unit you control.)
[Equip] [A] ([A]: Attach this to a unit you control.)
[Temporary] (If this is unattached, kill it at the start of its controller's Beginning Phase, before scoring.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/e4e0328034e605d000e23ac5e820d233f0e3b520-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_504(CardRegistry& r) {
    r.registerCard(504, std::make_unique<SpinningAxe>());
}

} // namespace riftbound
