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

class MaraiSpire : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    // ENGINE GAP: "While you control this battlefield, friendly [Repeat] costs
    // cost [1] less." The Repeat-cost loop is engine-internal and reads only
    // the spell's own printed Repeat cost; there is no external reduction hook
    // (no CostModifier path targets the Repeat additional cost specifically).
    // Left unimplemented pending a Repeat-cost-reduction hook.
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 525;
        d.def_id = R"RB(sfd-211-221)RB";
        d.name = R"RB(Marai Spire)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-211/221)RB";
        d.collector_number = 211;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Repeat);
        d.ability_text = R"RB(While you control this battlefield, friendly [Repeat] costs cost [1] less.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ff64dcbac9b392d4f6930a7359114aa68aa14ebe-1039x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_525(CardRegistry& r) {
    r.registerCard(525, std::make_unique<MaraiSpire>());
}

} // namespace riftbound
