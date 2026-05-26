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

class VanguardSergeant : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 219;
        d.def_id = R"RB(ogn-219-298)RB";
        d.name = R"RB(Vanguard Sergeant)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-219/298)RB";
        d.collector_number = 219;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Elite)RB", R"RB(Demacia)RB"};
        d.energy_cost = 4;
        d.might = 4;
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/6bcff5c718cfcff1c2466bf5e1a2a1ea9a9cf09b-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_219(CardRegistry& r) {
    r.registerCard(219, std::make_unique<VanguardSergeant>());
}

} // namespace riftbound
