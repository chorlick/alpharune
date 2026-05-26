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

class ToweringPairofant : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "[Assault] If a unit died this turn, I enter ready." [Assault] is an engine
    // keyword; the enter-ready clause reads TurnState::any_unit_died_this_turn
    // (reset each runTurn, set when any unit dies).
    bool entersReadyOnPlay(const GameState& state, PlayerId) const override {
        return state.turn.any_unit_died_this_turn;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 570;
        d.def_id = R"RB(unl-008-219)RB";
        d.name = R"RB(Towering Pairofant)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-008/219)RB";
        d.collector_number = 8;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Shurima)RB"};
        d.energy_cost = 6;
        d.might = 6;
        d.assault_value = 1;
        d.keywords.set(Keyword::Assault);
        d.ability_text = R"RB([Assault] (+1 [M] while I'm an attacker.)
If a unit died this turn, I enter ready.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/47ac3971efe67f7910771709b8bb9a5df5b63952-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_570(CardRegistry& r) {
    r.registerCard(570, std::make_unique<ToweringPairofant>());
}

} // namespace riftbound
