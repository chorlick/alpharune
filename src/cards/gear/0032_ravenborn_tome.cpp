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

class RavenbornTome : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    // "[E]: The next spell you play this turn deals 1 Bonus Damage."
    // ESCALATE(per_player_spell_bonus_damage): there is no "next spell deals N
    // bonus damage" rider on PlayerState (no per-player spell-damage modifier the
    // dealDamage path consults), so the activated ability is declared (cost +
    // exhaust) but its effect cannot be applied from the card layer.
    TriggerType triggerType() const override { return TriggerType::Activated; }
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override { return {.exhaust = true}; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 32;
        d.def_id = R"RB(ogn-032-298)RB";
        d.name = R"RB(Ravenborn Tome)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-032/298)RB";
        d.collector_number = 32;
        d.artist = R"RB(Polar Engine Studio)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Fury};
        d.energy_cost = 3;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB([E]: The next spell you play this turn deals 1 Bonus Damage. (Each instance of damage the spell deals is increased by 1.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/81f968126026635e5b9a2fa1048fc979fcd13a1d-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_32(CardRegistry& r) {
    r.registerCard(32, std::make_unique<RavenbornTome>());
}

} // namespace riftbound
