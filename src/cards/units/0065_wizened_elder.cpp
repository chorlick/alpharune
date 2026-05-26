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

class WizenedElder : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 65;
        d.def_id = R"RB(ogn-065-298)RB";
        d.name = R"RB(Wizened Elder)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-065/298)RB";
        d.collector_number = 65;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Ionia)RB"};
        d.energy_cost = 4;
        d.might = 4;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(While I'm buffed, I have an additional +1 [M].)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/7b98797789ad7e3a6cf52126b82a42ec6f269bb7-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_65(CardRegistry& r) {
    r.registerCard(65, std::make_unique<WizenedElder>());
}

} // namespace riftbound
