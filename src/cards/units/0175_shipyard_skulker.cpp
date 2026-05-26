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

class ShipyardSkulker : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 175;
        d.def_id = R"RB(ogn-175-298)RB";
        d.name = R"RB(Shipyard Skulker)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-175/298)RB";
        d.collector_number = 175;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Pirate)RB", R"RB(Bilgewater)RB"};
        d.energy_cost = 3;
        d.might = 3;
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/000634f32b897638760b82e5365961f6d85bc0cb-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_175(CardRegistry& r) {
    r.registerCard(175, std::make_unique<ShipyardSkulker>());
}

} // namespace riftbound
