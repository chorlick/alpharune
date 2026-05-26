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

class RagingSoul : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 19;
        d.def_id = R"RB(ogn-019-298)RB";
        d.name = R"RB(Raging Soul)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-019/298)RB";
        d.collector_number = 19;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Spirit)RB", R"RB(Shadow Isles)RB"};
        d.energy_cost = 4;
        d.might = 4;
        d.rarity = Rarity::Uncommon;
        d.assault_value = 1;
        d.keywords.set(Keyword::Assault);
        d.keywords.set(Keyword::Ganking);
        d.ability_text = R"RB(If you've discarded a card this turn, I have [Assault] and [Ganking]. (+1 [M] while I'm an attacker. I can move from battlefield to battlefield.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/037647d0decc94ff4a5d53b11cf36afe9d849533-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_19(CardRegistry& r) {
    r.registerCard(19, std::make_unique<RagingSoul>());
}

} // namespace riftbound
