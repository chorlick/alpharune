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

class EagerDrakehound : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    bool entersReadyOnPlay() const override { return true; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 329;
        d.def_id = R"RB(sfd-006-221)RB";
        d.name = R"RB(Eager Drakehound)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-006/221)RB";
        d.collector_number = 6;
        d.artist = R"RB(Michal Ivan)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Dog)RB", R"RB(Dragon)RB", R"RB(Noxus)RB"};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.might = 3;
        d.ability_text = R"RB(I enter ready.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ac0fac5af309842ba91b4b8d80e1abd47622e14a-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_329(CardRegistry& r) {
    r.registerCard(329, std::make_unique<EagerDrakehound>());
}

} // namespace riftbound
