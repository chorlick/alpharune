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

class ZephyrSage : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 303;
        d.def_id = R"RB(ogs-005-024)RB";
        d.name = R"RB(Zephyr Sage)RB";
        d.set_code = R"RB(OGS)RB";
        d.set_name = R"RB(Proving Grounds)RB";
        d.public_code = R"RB(OGS-005/024)RB";
        d.collector_number = 5;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Ionia)RB", R"RB(Bird)RB"};
        d.energy_cost = 6;
        d.power_cost = 1;
        d.might = 6;
        d.rarity = Rarity::Uncommon;
        d.shield_value = 1;
        d.keywords.set(Keyword::Shield);
        d.ability_text = R"RB([Shield] (+1 [M] while I'm a defender.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/7664b03bb107954195153d9f2f86c5d63682fa4b-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_303(CardRegistry& r) {
    r.registerCard(303, std::make_unique<ZephyrSage>());
}

} // namespace riftbound
