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
// COVERAGE-OK: engine-handled: "can't move from here to base" -> BattlefieldState::blocks_move_to_base (game_engine.cpp:416,1989)

class VilemawSLair : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 290;
        d.def_id = R"RB(ogn-295-298)RB";
        d.name = R"RB(Vilemaw's Lair)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-295/298)RB";
        d.collector_number = 295;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(Units can't move from here to base.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/f374b90a0bec11a5f423e7d4ebd869ab1a146471-1038x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_290(CardRegistry& r) {
    r.registerCard(290, std::make_unique<VilemawSLair>());
}

} // namespace riftbound
