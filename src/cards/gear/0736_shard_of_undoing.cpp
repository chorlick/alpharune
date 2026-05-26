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

class ShardOfUndoing : public GearCard {
public:
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 736;
        d.def_id = R"RB(unl-174-219)RB";
        d.name = R"RB(Shard of Undoing)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-174/219)RB";
        d.collector_number = 174;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Order};
        d.energy_cost = 6;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(The first time a friendly unit dies during your Beginning Phase each turn, each opponent must kill one of their units.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/d167f4e37714f0f8983fce16a9c657bc7ce39642-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_736(CardRegistry& r) {
    r.registerCard(736, std::make_unique<ShardOfUndoing>());
}

} // namespace riftbound
