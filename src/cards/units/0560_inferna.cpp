#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include "cards/card_helpers.h"

namespace riftbound {
namespace {

class Inferna : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 560;
        d.def_id = R"RB(unl-002-219)RB";
        d.name = R"RB(Inferna)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-002/219)RB";
        d.collector_number = 2;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Bilgewater)RB"};
        d.energy_cost = 2;
        d.might = 1;
        d.assault_value = 2;
        d.keywords.set(Keyword::Ambush);
        d.keywords.set(Keyword::Assault);
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Ambush] (You may play me as a [Reaction] to a battlefield where you have units.)
[Assault 2] (+2 [M] while I'm an attacker.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/5db9d66fc22887e8686a13ffdfe480106cbd3b35-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_560(CardRegistry& r) {
    r.registerCard(560, std::make_unique<Inferna>());
}

} // namespace riftbound
