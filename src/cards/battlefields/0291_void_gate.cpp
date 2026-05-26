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

class VoidGate : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    // ENGINE GAP: "Spells and abilities deal 1 Bonus Damage to units here."
    // EffectExecutor::dealDamage takes a fixed amount with no per-location
    // damage-bonus modifier, and there is no bonus-damage hook the engine
    // consults. Implementing this requires an engine-side damage modifier that
    // adds 1 when the target is a unit at a Void Gate battlefield.
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 291;
        d.def_id = R"RB(ogn-296-298)RB";
        d.name = R"RB(Void Gate)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-296/298)RB";
        d.collector_number = 296;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(Spells and abilities deal 1 Bonus Damage to units here. (Each instance of damage the spell deals to a unit here is increased by 1.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/14a52a367fd41fd84745e050e62d1f281f733467-1038x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_291(CardRegistry& r) {
    r.registerCard(291, std::make_unique<VoidGate>());
}

} // namespace riftbound
