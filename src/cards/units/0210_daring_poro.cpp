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

class DaringPoro : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 210;
        d.def_id = R"RB(ogn-210-298)RB";
        d.name = R"RB(Daring Poro)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-210/298)RB";
        d.collector_number = 210;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Poro)RB"};
        d.energy_cost = 2;
        d.might = 2;
        d.assault_value = 1;
        d.keywords.set(Keyword::Assault);
        d.ability_text = R"RB([Assault] (+1 [M] while I'm an attacker.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/41dee7ec46124c261352595cfbd8a6d38d32b947-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_210(CardRegistry& r) {
    r.registerCard(210, std::make_unique<DaringPoro>());
}

} // namespace riftbound
