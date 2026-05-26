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

class ForgottenMonument : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    int minTurnToScore() const override { return 3; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 523;
        d.def_id = R"RB(sfd-209-221)RB";
        d.name = R"RB(Forgotten Monument)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-209/221)RB";
        d.collector_number = 209;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(Players can't score here until their third turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/b0c98558fccd68c9ac93cd5b3412c31c68b5ace9-1039x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_523(CardRegistry& r) {
    r.registerCard(523, std::make_unique<ForgottenMonument>());
}

} // namespace riftbound
