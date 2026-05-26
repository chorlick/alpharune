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

class MegaMech : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 88;
        d.def_id = R"RB(ogn-088-298)RB";
        d.name = R"RB(Mega-Mech)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-088/298)RB";
        d.collector_number = 88;
        d.artist = R"RB(Valentin Gloaguen)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Mech)RB", R"RB(Bandle City)RB"};
        d.energy_cost = 7;
        d.might = 8;
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/f71db7b62798a2a10a1ab0d293c26ac9dd163c6a-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_88(CardRegistry& r) {
    r.registerCard(88, std::make_unique<MegaMech>());
}

} // namespace riftbound
