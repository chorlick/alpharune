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

// Baron Pit is a battlefield TOKEN spawned by Baron Nashor (id 709). Both
// printed clauses are realized outside this Card subclass:
//   - "Units can move here from anywhere." — Baron Nashor calls
//     addBattlefieldToken("Baron Pit", accepts_any_inbound=true); the engine's
//     move-legality check (game_engine.cpp ~2007) honors
//     BattlefieldState::accepts_any_inbound. Nothing for the Card to do.
//   - "(You can't start the game with a token battlefield.)" — a deck/setup
//     constraint: token BFs are never in a starting configuration. No runtime
//     Card hook applies.
// This class therefore only supplies def(); there is no per-card behavior to
// override.
class BaronPit : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 558;
        d.def_id = R"RB(unl-t01)RB";
        d.name = R"RB(Baron Pit)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-T01)RB";
        d.collector_number = 1;
        d.artist = R"RB(Fish Art Studio)RB";
        d.card_type = CardType::Battlefield;
        d.ability_text = R"RB((You can't start the game with a token battlefield.)
Units can move here from anywhere.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/e44f173629322a4e0c32d3f8902c294d4482ef42-1039x744.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_558(CardRegistry& r) {
    r.registerCard(558, std::make_unique<BaronPit>());
}

} // namespace riftbound
