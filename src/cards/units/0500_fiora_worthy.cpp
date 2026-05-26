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

// "When a unit you control becomes [Mighty], you may pay [Y] to ready it.
//  (A unit is Mighty while it has 5+ [M].)"
// ENGINE GAP: there is no "becomes Mighty" trigger event — nothing is emitted
// when a unit's Might crosses the 5+ threshold (Might is recomputed in cleanup
// with no edge-detection / event). Without that event there is no hook to fire
// this reactive ability, so it is left unimplemented pending a
// BecameMighty-style event from the might-recompute / cleanup path.

class FioraWorthy : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 500;
        d.def_id = R"RB(sfd-180-221)RB";
        d.name = R"RB(Fiora, Worthy)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-180/221)RB";
        d.collector_number = 180;
        d.artist = R"RB(League Splash Team)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Fiora)RB", R"RB(Demacia)RB"};
        d.energy_cost = 3;
        d.might = 3;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(When a unit you control becomes [Mighty], you may pay [Y] to ready it. (A unit is Mighty while it has 5+ [M].))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/7339e76b7796242a15e97cfddbe04c32d5a05063-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_500(CardRegistry& r) {
    r.registerCard(500, std::make_unique<FioraWorthy>());
}

} // namespace riftbound
