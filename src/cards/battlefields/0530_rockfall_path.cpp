#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include "cards/card_helpers.h"

namespace riftbound {
namespace {
// COVERAGE-OK: engine-handled: "can't be played here" -> BattlefieldState::blocks_unit_play

// "Units can't be played here."
// ENGINE-HANDLED: GameEngine::setupBattlefields lowercases ability_text and
// matches the substring "can't be played here" to set
// BattlefieldState::blocks_unit_play, which the play-from-hand action
// generator (generateMainPhaseActions) consults to skip this BF. BF->BF moves
// are NOT restricted (per the comment in the move generator). No per-card
// override is needed.

class RockfallPath : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 530;
        d.def_id = R"RB(sfd-216-221)RB";
        d.name = R"RB(Rockfall Path)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-216/221)RB";
        d.collector_number = 216;
        d.artist = R"RB(Polar Engine Studio)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(Units can't be played here.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/a1bde66bccf2786e7c0b9d44fcdcdaa6b59f0328-1039x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_530(CardRegistry& r) {
    r.registerCard(530, std::make_unique<RockfallPath>());
}

} // namespace riftbound
