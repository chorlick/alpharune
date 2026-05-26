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

class Gold : public GearCard {
public:
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 564;
        d.def_id = R"RB(unl-t05)RB";
        d.name = R"RB(Gold)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-T05)RB";
        d.collector_number = 5;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction][>] Kill this, [E]: [Add] [A]. (Abilities that add resources can't be reacted to.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/e566605fa989e12e91cd3d3e0b7985f7a8be8e74-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_564(CardRegistry& r) {
    r.registerCard(564, std::make_unique<Gold>());
}

} // namespace riftbound
