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

class MysticPoro : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 780;
        d.def_id = R"RB(unl-224-219)RB";
        d.name = R"RB(Mystic Poro)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-224/219)RB";
        d.collector_number = 224;
        d.artist = R"RB(FOREDAWN)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Poro)RB"};
        d.energy_cost = 2;
        d.might = 2;
        d.rarity = Rarity::Showcase;
        d.keywords.set(Keyword::Vision);
        d.ability_text = R"RB([Vision])RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/2fdb5e1ff9aae4137910fa59065b69ef17144752-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_780(CardRegistry& r) {
    r.registerCard(780, std::make_unique<MysticPoro>());
}

} // namespace riftbound
