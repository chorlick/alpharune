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

class NineTailedFox : public LegendCard {
public:
    const CardDef& def() const override { return def_; }
    // No engine trigger fires for "enemy unit attacks a BF you control"; see
    // header comment. Intentionally no triggerType() / onTrigger() wiring.
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 252;
        d.def_id = R"RB(ogn-255-298)RB";
        d.name = R"RB(Nine-Tailed Fox)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-255/298)RB";
        d.collector_number = 255;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Legend;
        d.domains = {Domain::Calm, Domain::Mind};
        d.tags = {R"RB(Ahri)RB"};
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When an enemy unit attacks a battlefield you control, give it -1 [M] this turn, to a minimum of 1 [M].)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/fbce641f5e4d8cdf2956e8ead5884b6cd3ccd90d-744x1040.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_252(CardRegistry& r) {
    r.registerCard(252, std::make_unique<NineTailedFox>());
}

} // namespace riftbound
