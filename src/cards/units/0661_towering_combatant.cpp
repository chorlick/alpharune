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

class ToweringCombatant : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 661;
        d.def_id = R"RB(unl-099-219)RB";
        d.name = R"RB(Towering Combatant)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-099/219)RB";
        d.collector_number = 99;
        d.artist = R"RB(JiHun Lee)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Shurima)RB"};
        d.energy_cost = 4;
        d.might = 3;
        d.shield_value = 2;
        d.keywords.set(Keyword::Shield);
        d.keywords.set(Keyword::Tank);
        d.ability_text = R"RB([Shield 2] (+2 [M] while I'm a defender.)
[Tank] (I must be assigned combat damage first.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/2a2eefa9b85684489e97870145eb2f94b20b60ed-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_661(CardRegistry& r) {
    r.registerCard(661, std::make_unique<ToweringCombatant>());
}

} // namespace riftbound
