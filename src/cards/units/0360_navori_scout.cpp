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

class NavoriScout : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 360;
        d.def_id = R"RB(sfd-037-221)RB";
        d.name = R"RB(Navori Scout)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-037/221)RB";
        d.collector_number = 37;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Yordle)RB", R"RB(Ionia)RB"};
        d.energy_cost = 4;
        d.might = 4;
        d.deflect_value = 1;
        d.keywords.set(Keyword::Deflect);
        d.ability_text = R"RB([Deflect] (Opponents must pay [A] to choose me with a spell or ability.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/038625518f5b46f5217d300d9dc9fbf507c4ae09-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_360(CardRegistry& r) {
    r.registerCard(360, std::make_unique<NavoriScout>());
}

} // namespace riftbound
