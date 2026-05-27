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
    // Wired via PlayerState::next_spell_bonus_damage: this activation arms the
    // rider; executePlaySpell binds it onto the next spell object
    // (GameObject::spell_bonus_damage), and dealDamage adds it to every instance
    // that spell deals.
    TriggerType triggerType() const override { return TriggerType::Activated; }
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override { return {.exhaust = true}; }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>&) override {
        ctx.state.player(ctx.controller).next_spell_bonus_damage += 1;
        ctx.events.logTrace("RAVENBORN TOME: next spell will deal +1 Bonus Damage");
    }
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
