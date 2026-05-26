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

class NeedlesslyLargeYordle : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // [Shield 5] and [Tank] are engine-handled via the keyword set.
    //
    // "I cost [2][G] less for each point you scored from holding this turn."
    // PlayerState::hold_points_this_turn now exists (incremented in
    // GameEngine::scoreHold, reset in resetTurnTracking). The ENERGY portion of
    // the discount — [2] per hold point — is applied here via selfCostReduction
    // (engine clamps the net energy cost to >= 0).
    //
    // ESCALATE(power_cost_reduction): the [G] (Order power) portion of the
    // discount — 1 Order power per hold point — cannot be expressed. The engine
    // has NO per-card POWER reduction hook (selfCostReduction reduces ENERGY
    // only). Same gap as Master Yi, Unstoppable's tiered "[2][G] less". Only the
    // energy half of the discount is applied.
    int selfCostReduction(const GameState& state, PlayerId player) const override {
        return 2 * state.player(player).hold_points_this_turn;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 378;
        d.def_id = R"RB(sfd-055-221)RB";
        d.name = R"RB(Needlessly Large Yordle)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-055/221)RB";
        d.collector_number = 55;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Yordle)RB", R"RB(Bandle City)RB"};
        d.energy_cost = 10;
        d.power_cost = 3;
        d.might = 5;
        d.rarity = Rarity::Rare;
        d.shield_value = 5;
        d.keywords.set(Keyword::Shield);
        d.keywords.set(Keyword::Tank);
        d.ability_text = R"RB([Shield 5] (+5 [M] while I'm a defender.)
[Tank] (I must be assigned combat damage first.)
I cost [2][G] less for each point you scored from holding this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/952dcaf9fd4b5f3e8592d140c11d792175597da1-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_378(CardRegistry& r) {
    r.registerCard(378, std::make_unique<NeedlesslyLargeYordle>());
}

} // namespace riftbound
