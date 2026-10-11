#include "cards/card.h"
#include "cards/units/weaponmaster_base.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include "cards/card_helpers.h"

namespace riftbound {
namespace {

// [Weaponmaster] is the shared keyword ability (weaponmaster_base.h).
class SentinelAdept : public WeaponmasterUnit {
public:
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 331;
        d.def_id = R"RB(sfd-008-221)RB";
        d.name = R"RB(Sentinel Adept)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-008/221)RB";
        d.collector_number = 8;
        d.artist = R"RB(Wild Blue Studios)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Shadow Isles)RB", R"RB(Sentinel)RB"};
        d.energy_cost = 3;
        d.might = 3;
        d.keywords.set(Keyword::Weaponmaster);
        d.ability_text = R"RB([Weaponmaster] (When you play me, you may [Equip] one of your Equipment to me for [A] less, even if it's already attached.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/94e213b77131d9a67e51743a35fc5dff80fd79ec-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_331(CardRegistry& r) {
    r.registerCard(331, std::make_unique<SentinelAdept>());
}

} // namespace riftbound
