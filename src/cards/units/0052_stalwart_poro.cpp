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

class StalwartPoro : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 52;
        d.def_id = R"RB(ogn-052-298)RB";
        d.name = R"RB(Stalwart Poro)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-052/298)RB";
        d.collector_number = 52;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Poro)RB"};
        d.energy_cost = 2;
        d.might = 2;
        d.shield_value = 1;
        d.keywords.set(Keyword::Shield);
        d.ability_text = R"RB([Shield] (+1 [M] while I'm a defender.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/c4a5d7178e783c3975749271b6df333a82a2328a-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_52(CardRegistry& r) {
    r.registerCard(52, std::make_unique<StalwartPoro>());
}

} // namespace riftbound
