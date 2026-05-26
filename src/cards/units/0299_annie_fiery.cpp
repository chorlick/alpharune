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

class AnnieFiery : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "Your spells and abilities deal 1 Bonus Damage." While Annie is on board,
    // raise her controller's bonus_damage_dealt; dealDamage adds it to each
    // instance of spell/ability damage from that controller. (Reset + summed in
    // recalculateAuras, so multiple Annies stack.)
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        state.player(controller).bonus_damage_dealt += 1;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 299;
        d.def_id = R"RB(ogs-001-024)RB";
        d.name = R"RB(Annie, Fiery)RB";
        d.set_code = R"RB(OGS)RB";
        d.set_name = R"RB(Proving Grounds)RB";
        d.public_code = R"RB(OGS-001/024)RB";
        d.collector_number = 1;
        d.artist = R"RB(Polar Engine Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Annie)RB", R"RB(Noxus)RB"};
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 4;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(Your spells and abilities deal 1 Bonus Damage. (Each instance of damage the spell deals is increased by 1.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/532d75dc36a16eb5954253a77366fcceac7aec62-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_299(CardRegistry& r) {
    r.registerCard(299, std::make_unique<AnnieFiery>());
}

} // namespace riftbound
