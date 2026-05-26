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

// "Increase the points needed to win the game by 1."
// ESCALATE(victory_score_modifier): The points needed to win are a fixed
// global value (GameState::mode.victory_score, default 8) read directly in
// scoring/win checks. There is no hook for a battlefield (or any card) to
// modify victory_score while it is in play. The only structured BattlefieldCard
// hook is minTurnToScore(); nothing adjusts victory_score. Implementing this
// needs the engine to apply a per-battlefield (and generally per-permanent)
// victory_score delta — ideally an aura-recalc-style "effective victory score"
// that sums contributions from cards in play, consulted everywhere
// mode.victory_score is read today.

class AspirantSClimb : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 271;
        d.def_id = R"RB(ogn-276-298)RB";
        d.name = R"RB(Aspirant's Climb)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-276/298)RB";
        d.collector_number = 276;
        d.artist = R"RB(Polar Engine Studio)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(Increase the points needed to win the game by 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/9301593f3800e68427469d38181b578a672473c3-1038x744.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_271(CardRegistry& r) {
    r.registerCard(271, std::make_unique<AspirantSClimb>());
}

} // namespace riftbound
