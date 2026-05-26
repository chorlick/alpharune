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

class AlphaWildclaw : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "[Tank]" is engine-handled.
    // "Your units here with less Might than me can't be chosen by enemy spells
    // and abilities." is an ENGINE GAP. Granting targeting-protection to OTHER
    // units is not expressible: GameObject::untargetable_by_enemy is RESET to
    // false per-object inside the same cleanup loop that calls applyPassiveAura
    // (game_engine.cpp ~3548), and is only ever set from the stateless
    // canBeChosenByEnemy() of the object itself. Setting the flag on other
    // units from applyPassiveAura would be clobbered by their own reset
    // depending on GameObjectId iteration order, so it cannot be done reliably.
    // Left unimplemented.
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 619;
        d.def_id = R"RB(unl-057-219)RB";
        d.name = R"RB(Alpha Wildclaw)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-057/219)RB";
        d.collector_number = 57;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Cat)RB", R"RB(Freljord)RB"};
        d.energy_cost = 6;
        d.power_cost = 2;
        d.might = 7;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Tank);
        d.ability_text = R"RB([Tank] (I must be assigned combat damage first.)
Your units here with less Might than me can't be chosen by enemy spells and abilities.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/39ec974dae18bf82fcc18a95044d8df04cc14d3c-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_619(CardRegistry& r) {
    r.registerCard(619, std::make_unique<AlphaWildclaw>());
}

} // namespace riftbound
