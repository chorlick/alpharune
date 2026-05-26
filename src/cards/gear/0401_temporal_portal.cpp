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

class TemporalPortal : public GearCard {
public:
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 401;
        d.def_id = R"RB(sfd-078-221)RB";
        d.name = R"RB(Temporal Portal)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-078/221)RB";
        d.collector_number = 78;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Mind};
        d.energy_cost = 3;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Repeat);
        d.ability_text = R"RB([A], [E]: Give the next spell you play this turn [Repeat] equal to its cost. (You may pay the additional cost to repeat the spell's effect.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/a6b8167486d552cc41d10cb9321bae4cef338fa7-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_401(CardRegistry& r) {
    r.registerCard(401, std::make_unique<TemporalPortal>());
}

} // namespace riftbound
