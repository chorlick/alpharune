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

class DoranSBlade : public SimpleEquipGear {
public:
    DoranSBlade() : SimpleEquipGear(Domain::Body) {}
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 417;
        d.def_id = R"RB(sfd-095-221)RB";
        d.name = R"RB(Doran's Blade)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-095/221)RB";
        d.collector_number = 95;
        d.artist = R"RB(黯荧岛Dark Glow)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Equipment)RB"};
        d.energy_cost = 2;
        d.might_bonus = 2;
        d.keywords.set(Keyword::Equip);
        d.ability_text = R"RB([Equip] [O] ([O]: Attach this to a unit you control.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/53fc0c307c83871c9755a496764ef7d6aa11f510-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_417(CardRegistry& r) {
    r.registerCard(417, std::make_unique<DoranSBlade>());
}

} // namespace riftbound
