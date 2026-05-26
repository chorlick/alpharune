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

class PetriciteMonument : public GearCard {
public:
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 426;
        d.def_id = R"RB(sfd-104-221)RB";
        d.name = R"RB(Petricite Monument)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-104/221)RB";
        d.collector_number = 104;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Body};
        d.energy_cost = 2;
        d.rarity = Rarity::Uncommon;
        d.deflect_value = 1;
        d.keywords.set(Keyword::Deflect);
        d.keywords.set(Keyword::Temporary);
        d.ability_text = R"RB([Temporary] (Kill this at the start of its controller's Beginning Phase, before scoring.)
Friendly units have [Deflect]. (Opponents must pay [A] to choose them with a spell or ability.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/c994dcefe24e00f8033bc31327bf7dbe4a8703d1-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_426(CardRegistry& r) {
    r.registerCard(426, std::make_unique<PetriciteMonument>());
}

} // namespace riftbound
