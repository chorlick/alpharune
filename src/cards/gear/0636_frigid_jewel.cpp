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

class FrigidJewel : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    // "When you draw your second card each turn, give a friendly unit +2 [M]
    // this turn." ENGINE GAP: there is no "when you draw" TriggerType and
    // TriggerManager does not subscribe to CardsDrawnEvent, so there is no
    // draw-trigger dispatch to hook. Left unimplemented.
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 636;
        d.def_id = R"RB(unl-074-219)RB";
        d.name = R"RB(Frigid Jewel)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-074/219)RB";
        d.collector_number = 74;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Mind};
        d.energy_cost = 2;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you draw your second card each turn, give a friendly unit +2 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/f1a6c28649bf4478b4728379b6d548058739f0e9-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_636(CardRegistry& r) {
    r.registerCard(636, std::make_unique<FrigidJewel>());
}

} // namespace riftbound
