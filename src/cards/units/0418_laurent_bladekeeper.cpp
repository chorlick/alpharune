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

class LaurentBladekeeper : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 418;
        d.def_id = R"RB(sfd-096-221)RB";
        d.name = R"RB(Laurent Bladekeeper)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-096/221)RB";
        d.collector_number = 96;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Demacia)RB"};
        d.energy_cost = 3;
        d.might = 3;
        // "Ganking" is an engine keyword (CR 144.4.c): the move generator
        // reads unit.hasKeyword(Keyword::Ganking) to allow BF→BF moves. The
        // card's entire printed ability IS this keyword, so set the bit.
        d.keywords.set(Keyword::Ganking);
        d.ability_text = R"RB(Ganking (I can move from battlefield to battlefield.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/d9dac8bbc898bfead8338ef387f2d144302e278f-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_418(CardRegistry& r) {
    r.registerCard(418, std::make_unique<LaurentBladekeeper>());
}

} // namespace riftbound
