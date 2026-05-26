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

class MutatedMouser : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 598;
        d.def_id = R"RB(unl-036-219)RB";
        d.name = R"RB(Mutated Mouser)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-036/219)RB";
        d.collector_number = 36;
        d.artist = R"RB(Caravan Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Cat)RB", R"RB(Bilgewater)RB"};
        d.energy_cost = 2;
        d.might = 1;
        d.shield_value = 2;
        d.keywords.set(Keyword::Shield);
        d.keywords.set(Keyword::Tank);
        d.ability_text = R"RB([Shield 2] (+2 [M] while I'm a defender.)
[Tank] (I must be assigned combat damage first.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/20276519677a4d1ebe230aaad7692588e1e8d046-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_598(CardRegistry& r) {
    r.registerCard(598, std::make_unique<MutatedMouser>());
}

} // namespace riftbound
