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

class JeweledColossus : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 86;
        d.def_id = R"RB(ogn-086-298)RB";
        d.name = R"RB(Jeweled Colossus)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-086/298)RB";
        d.collector_number = 86;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Mount Targon)RB"};
        d.energy_cost = 5;
        d.might = 5;
        d.shield_value = 1;
        d.keywords.set(Keyword::Shield);
        d.keywords.set(Keyword::Vision);
        d.ability_text = R"RB([Vision] (When you play me, look at the top card of your Main Deck. You may recycle it.)
[Shield] (+1 [M] while I'm a defender.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/775bea14038165fd9feb15c796ed84aa00a032e1-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_86(CardRegistry& r) {
    r.registerCard(86, std::make_unique<JeweledColossus>());
}

} // namespace riftbound
