#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include "cards/card_helpers.h"

namespace riftbound {
namespace {

class DravenShowboat : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 28;
        d.def_id = R"RB(ogn-028-298)RB";
        d.name = R"RB(Draven, Showboat)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-028/298)RB";
        d.collector_number = 28;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Draven)RB", R"RB(Noxus)RB"};
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 3;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(My Might is increased by your points.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/7600d6cea66e8146ea2202f72cb9035cb44608a3-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_28(CardRegistry& r) {
    r.registerCard(28, std::make_unique<DravenShowboat>());
}

} // namespace riftbound
