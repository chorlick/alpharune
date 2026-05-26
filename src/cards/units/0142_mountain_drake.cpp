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

class MountainDrake : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 142;
        d.def_id = R"RB(ogn-142-298)RB";
        d.name = R"RB(Mountain Drake)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-142/298)RB";
        d.collector_number = 142;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Dragon)RB"};
        d.energy_cost = 9;
        d.might = 10;
        d.rarity = Rarity::Uncommon;
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/8a9c1d334b217e9bd0b23dfca2054de4d0b90ff1-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_142(CardRegistry& r) {
    r.registerCard(142, std::make_unique<MountainDrake>());
}

} // namespace riftbound
