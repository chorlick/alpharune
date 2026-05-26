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

class Reflection : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 566;
        d.def_id = R"RB(unl-t06)RB";
        d.name = R"RB(Reflection)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-T06)RB";
        d.collector_number = 6;
        d.artist = R"RB(Kudos Productions & 黯荧岛Dark Glow)RB";
        d.card_type = CardType::Unit;
        d.ability_text = R"RB((I become a copy of something when played. I don't get that card's play effects.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/80327b196c59841a67a65327974a93223a3c541a-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_566(CardRegistry& r) {
    r.registerCard(566, std::make_unique<Reflection>());
}

} // namespace riftbound
