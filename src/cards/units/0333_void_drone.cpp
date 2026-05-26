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

class VoidDrone : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "I cost [2] less to play from anywhere other than your hand."
    // PlayerState::current_play_source is set by the engine to the play's
    // source zone before payCardCost consults this hook (see Rek'Sai,
    // Breacher's identical use of current_play_source).
    int selfCostReduction(const GameState& state, PlayerId player) const override {
        return state.player(player).current_play_source != Intent::PlaySource::Hand
                   ? 2 : 0;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 333;
        d.def_id = R"RB(sfd-010-221)RB";
        d.name = R"RB(Void Drone)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-010/221)RB";
        d.collector_number = 10;
        d.artist = R"RB(Wild Blue Studios)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(The Void)RB"};
        d.energy_cost = 3;
        d.might = 3;
        d.ability_text = R"RB(I cost [2] less to play from anywhere other than your hand.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/589dde45bc610eb83bc808803e4becf8a6d2da84-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_333(CardRegistry& r) {
    r.registerCard(333, std::make_unique<VoidDrone>());
}

} // namespace riftbound
