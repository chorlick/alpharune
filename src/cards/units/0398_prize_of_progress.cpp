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

class PrizeOfProgress : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 398;
        d.def_id = R"RB(sfd-075-221)RB";
        d.name = R"RB(Prize of Progress)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-075/221)RB";
        d.collector_number = 75;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Mech)RB", R"RB(Piltover)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 3;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you use an activated ability of a gear, give me +1 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/c08f0b6ed24415e6a440a17931c7e2a2db3ab2cc-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_398(CardRegistry& r) {
    r.registerCard(398, std::make_unique<PrizeOfProgress>());
}

} // namespace riftbound
