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

class WilyNewtfish : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "If you've gained XP this turn, I have +1 [M] and [Ganking]." is an
    // ENGINE GAP: there is no "XP gained this turn" tracking. PlayerState stores
    // only the cumulative `xp` total — no per-turn delta and no turn-start
    // baseline — and resetTurnTracking records nothing for XP. The condition
    // cannot be evaluated without a new PlayerState field (e.g.
    // gained_xp_this_turn), which is out of scope here. Left unimplemented.
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 670;
        d.def_id = R"RB(unl-108-219)RB";
        d.name = R"RB(Wily Newtfish)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-108/219)RB";
        d.collector_number = 108;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Bilgewater)RB"};
        d.energy_cost = 4;
        d.might = 4;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Ganking);
        d.ability_text = R"RB(If you've gained XP this turn, I have +1 [M] and [Ganking]. (I can move from battlefield to battlefield.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/4a0b0ebe9f47dadeb3b5f0520ec566a528df9c94-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_670(CardRegistry& r) {
    r.registerCard(670, std::make_unique<WilyNewtfish>());
}

} // namespace riftbound
