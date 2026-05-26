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
