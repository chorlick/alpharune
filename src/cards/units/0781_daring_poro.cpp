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

class DaringPoro : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 781;
        d.def_id = R"RB(unl-225-219)RB";
        d.name = R"RB(Daring Poro)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-225/219)RB";
        d.collector_number = 225;
        d.artist = R"RB(FOREDAWN)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Poro)RB"};
        d.energy_cost = 2;
        d.might = 2;
        d.rarity = Rarity::Showcase;
        d.assault_value = 1;
        d.keywords.set(Keyword::Assault);
        d.ability_text = R"RB([Assault])RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/9b097e8f9866788e0de2bfada48627c752d3e4b9-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_781(CardRegistry& r) {
    r.registerCard(781, std::make_unique<DaringPoro>());
}

} // namespace riftbound
