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

class RengarUnseen : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 586;
        d.def_id = R"RB(unl-024-219)RB";
        d.name = R"RB(Rengar, Unseen)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-024/219)RB";
        d.collector_number = 24;
        d.artist = R"RB(HCuu)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Cat)RB", R"RB(Rengar)RB", R"RB(Ixtal)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 4;
        d.rarity = Rarity::Rare;
        d.assault_value = 2;
        d.deflect_value = 1;
        d.keywords.set(Keyword::Accelerate);
        d.keywords.set(Keyword::Assault);
        d.keywords.set(Keyword::Deflect);
        d.keywords.set(Keyword::Ganking);
        d.ability_text = R"RB([Accelerate] (You may pay [1][R] as an additional cost to have me enter ready.)
[Assault 2] (+2 [M] while I'm an attacker.)
[Deflect] (Opponents must pay [A] to choose me with a spell or ability.)
[Ganking] (I can move from battlefield to battlefield.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/80c7d1c62301dd6e3ced910156f3fdeb8d34a621-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_586(CardRegistry& r) {
    r.registerCard(586, std::make_unique<RengarUnseen>());
}

} // namespace riftbound
