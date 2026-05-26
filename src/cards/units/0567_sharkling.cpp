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

class Sharkling : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 567;
        d.def_id = R"RB(unl-006-219)RB";
        d.name = R"RB(Sharkling)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-006/219)RB";
        d.collector_number = 6;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Bilgewater)RB"};
        d.energy_cost = 3;
        d.might = 1;
        d.assault_value = 4;
        d.keywords.set(Keyword::Accelerate);
        d.keywords.set(Keyword::Assault);
        d.ability_text = R"RB([Accelerate] (You may pay [1][R] as an additional cost to have me enter ready.)
[Assault 4] (+4 [M] while I'm an attacker.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/f2aa213fe1e54d0d6f1507c7ed8829c8ba2bc610-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_567(CardRegistry& r) {
    r.registerCard(567, std::make_unique<Sharkling>());
}

} // namespace riftbound
