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

class GarenRugged : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 305;
        d.def_id = R"RB(ogs-007-024)RB";
        d.name = R"RB(Garen, Rugged)RB";
        d.set_code = R"RB(OGS)RB";
        d.set_name = R"RB(Proving Grounds)RB";
        d.public_code = R"RB(OGS-007/024)RB";
        d.collector_number = 7;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Elite)RB", R"RB(Garen)RB", R"RB(Demacia)RB"};
        d.energy_cost = 6;
        d.power_cost = 1;
        d.might = 5;
        d.rarity = Rarity::Rare;
        d.assault_value = 2;
        d.shield_value = 2;
        d.keywords.set(Keyword::Assault);
        d.keywords.set(Keyword::Shield);
        d.ability_text = R"RB([Assault 2], [Shield 2] (+2 [M] while I'm an attacker or defender.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/67c22dc29a7a28dabe0f169a7848c25bef1fbda4-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_305(CardRegistry& r) {
    r.registerCard(305, std::make_unique<GarenRugged>());
}

} // namespace riftbound
