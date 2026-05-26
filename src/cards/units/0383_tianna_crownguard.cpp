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

class TiannaCrownguard : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // Clause 1: "[Deflect]" — engine keyword, set in def (Deflect value 1).
    //   Fully handled by the engine's targeting/cost path.
    // Clause 2: "While I'm at a battlefield, opponents can't gain points."
    //   No engine primitive prevents scoring / point gain — neither
    //   GameState nor GameEngine has a "can't gain points" flag or a scoring
    //   prevention gate (the only score gate is per-battlefield
    //   min_turn_to_score). Implementing this would require engine changes,
    //   which are out of scope here.
    // ESCALATE(scoring-prevention): need a per-player "opponents can't gain
    //   points while a unit with this effect is at a battlefield" gate
    //   consulted by GameEngine's score / PointsGained paths.
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 383;
        d.def_id = R"RB(sfd-060-221)RB";
        d.name = R"RB(Tianna Crownguard)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-060/221)RB";
        d.collector_number = 60;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Elite)RB", R"RB(Demacia)RB"};
        d.energy_cost = 7;
        d.power_cost = 2;
        d.might = 4;
        d.rarity = Rarity::Epic;
        d.deflect_value = 1;
        d.keywords.set(Keyword::Deflect);
        d.ability_text = R"RB([Deflect] (Opponents must pay [A] to choose me with a spell or ability.)
While I'm at a battlefield, opponents can't gain points.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/2a1876aeedae7310409d0eaaf453507188b0c82b-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_383(CardRegistry& r) {
    r.registerCard(383, std::make_unique<TiannaCrownguard>());
}

} // namespace riftbound
