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

class SumpworksMap : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    // "[Reaction]" + "[Temporary]" are engine-handled.
    // "When an opponent scores, draw 1." is an ENGINE GAP: there is no
    // "when an opponent scores" TriggerType, and TriggerManager::onScore only
    // dispatches score triggers to the SCORING player's own units / legends /
    // battlefields — never to the opponent's cards. Left unimplemented.
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 647;
        d.def_id = R"RB(unl-085-219)RB";
        d.name = R"RB(Sumpworks Map)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-085/219)RB";
        d.collector_number = 85;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Mind};
        d.energy_cost = 2;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Reaction);
        d.keywords.set(Keyword::Temporary);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
[Temporary] (Kill this at the start of its controller's Beginning Phase, before scoring.)
When an opponent scores, draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/87c3d5007aa3c3e2c4ff3d5bf78a92a2bee58db3-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_647(CardRegistry& r) {
    r.registerCard(647, std::make_unique<SumpworksMap>());
}

} // namespace riftbound
