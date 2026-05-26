#include "cards/card.h"
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

class GardensOfBecoming : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    // "Units here have '[E]: Gain 1 XP.'"
    // ENGINE GAP: the aura system grants only Might/keywords/combat-damage
    // suppression (GameObject::AuraEffect). There is no surface for an aura to
    // grant an *activated ability* to a unit, nor does the action generator
    // enumerate battlefield-granted abilities. Implementing this would require
    // engine/header changes (a granted-activated-ability list on GameObject +
    // action-gen + dispatch), which are out of scope here. Left unimplemented.
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 769;
        d.def_id = R"RB(unl-213-219)RB";
        d.name = R"RB(Gardens of Becoming)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-213/219)RB";
        d.collector_number = 213;
        d.artist = R"RB(Polar Engine Studio)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(Units here have "[E]: Gain 1 XP.")RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/0497a44ab302ea055b6f1f0d00a36c8023ed2344-1039x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_769(CardRegistry& r) {
    r.registerCard(769, std::make_unique<GardensOfBecoming>());
}

} // namespace riftbound
