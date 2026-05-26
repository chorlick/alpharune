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

class MasterBingwen : public WeaponmasterUnit {
public:
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 448;
        d.def_id = R"RB(sfd-127-221)RB";
        d.name = R"RB(Master Bingwen)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-127/221)RB";
        d.collector_number = 127;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Ionia)RB"};
        d.energy_cost = 6;
        d.might = 6;
        d.keywords.set(Keyword::Equip);
        d.keywords.set(Keyword::Weaponmaster);
        d.ability_text = R"RB([Weaponmaster] (When you play me, you may [Equip] one of your Equipment to me for [A] less, even if it's already attached.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/52c7f0fe49f24c29cb797613444f86a810292ec2-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_448(CardRegistry& r) {
    r.registerCard(448, std::make_unique<MasterBingwen>());
}

} // namespace riftbound
