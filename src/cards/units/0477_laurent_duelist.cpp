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

class LaurentDuelist : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 477;
        d.def_id = R"RB(sfd-156-221)RB";
        d.name = R"RB(Laurent Duelist)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-156/221)RB";
        d.collector_number = 156;
        d.artist = R"RB(JiHun Lee)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Demacia)RB"};
        d.energy_cost = 4;
        d.might = 3;
        d.assault_value = 2;
        d.keywords.set(Keyword::Assault);
        d.ability_text = R"RB([Assault 2] (+2 [M] while I'm an attacker.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/7b3410a1d9ee175c4853453798bcd5f09d2e217c-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_477(CardRegistry& r) {
    r.registerCard(477, std::make_unique<LaurentDuelist>());
}

} // namespace riftbound
