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

class VeteranPoro : public WeaponmasterUnit {
public:
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 421;
        d.def_id = R"RB(sfd-099-221)RB";
        d.name = R"RB(Veteran Poro)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-099/221)RB";
        d.collector_number = 99;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Poro)RB", R"RB(Freljord)RB"};
        d.energy_cost = 2;
        d.might = 2;
        d.keywords.set(Keyword::Equip);
        d.keywords.set(Keyword::Weaponmaster);
        d.ability_text = R"RB([Weaponmaster] (When you play me, you may [Equip] one of your Equipment to me for [A] less, even if it's already attached.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/54b8aa5aa80e74e0c545d4c484353c5b9e78bdfc-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_421(CardRegistry& r) {
    r.registerCard(421, std::make_unique<VeteranPoro>());
}

} // namespace riftbound
