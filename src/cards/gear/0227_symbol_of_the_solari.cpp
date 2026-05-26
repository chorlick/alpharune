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

class SymbolOfTheSolari : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    // "If a combat where you are the attacker ends in a tie, recall ALL units
    //  instead."
    // ESCALATE(combat-tie-replacement): there is no combat-tie trigger/event nor a
    // combat-resolution replacement hook in the engine, so this cannot be wired
    // from the card layer. Needs an engine-side combat-tie replacement (recall all
    // units to base instead of the normal tie outcome) gated on the gear's
    // controller being the attacker.
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 227;
        d.def_id = R"RB(ogn-227-298)RB";
        d.name = R"RB(Symbol of the Solari)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-227/298)RB";
        d.collector_number = 227;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Order};
        d.energy_cost = 1;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(If a combat where you are the attacker ends in a tie, recall ALL units instead. (Send them to base. This isn't a move. Ties are calculated after combat damage is dealt.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/d4f746c396df48623ef8a4df763b5deb47f59339-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_227(CardRegistry& r) {
    r.registerCard(227, std::make_unique<SymbolOfTheSolari>());
}

} // namespace riftbound
