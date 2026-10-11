#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include "cards/units/weaponmaster_base.h"

namespace riftbound {
namespace {

class VeteranPoro_779 : public WeaponmasterUnit {
public:
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 779;
        d.def_id = R"RB(unl-223-219)RB";
        d.name = R"RB(Veteran Poro)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-223/219)RB";
        d.collector_number = 223;
        d.artist = R"RB(FOREDAWN)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Poro)RB", R"RB(Freljord)RB"};
        d.energy_cost = 2;
        d.might = 2;
        d.rarity = Rarity::Showcase;
        // [Weaponmaster] is the only keyword: it lets an Equipment be [Equip]ped
        // to this unit as it is played, but the unit has no Equip ability of its
        // own, so no Keyword::Equip (reprint 421 matches).
        d.keywords.set(Keyword::Weaponmaster);
        d.ability_text = R"RB([Weaponmaster])RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/337f38fae6a4d801fc14abcda2a7d09c3e7e6ae3-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_779(CardRegistry& r) {
    r.registerCard(779, std::make_unique<VeteranPoro_779>());
}

} // namespace riftbound
