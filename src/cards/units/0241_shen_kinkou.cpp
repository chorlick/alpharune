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

class ShenKinkou : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 241;
        d.def_id = R"RB(ogn-241-298)RB";
        d.name = R"RB(Shen, Kinkou)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-241/298)RB";
        d.collector_number = 241;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Shen)RB", R"RB(Ionia)RB"};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.might = 3;
        d.rarity = Rarity::Rare;
        d.shield_value = 2;
        d.keywords.set(Keyword::Reaction);
        d.keywords.set(Keyword::Shield);
        d.keywords.set(Keyword::Tank);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve, including to a battlefield you control.)
[Shield 2] (+2 [M] while I'm a defender.)
[Tank] (I must be assigned combat damage first.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/25f3a9fa33201278ebf475b2d02dae8c0c0cb20c-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_241(CardRegistry& r) {
    r.registerCard(241, std::make_unique<ShenKinkou>());
}

} // namespace riftbound
