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

class SaiScout : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // [Vision] is engine-handled.
    // "You may play me to an open battlefield."
    // ENGINE-HANDLED: GameEngine::generateMainPhaseActions matches the substring
    // "play me to an open battlefield" in ability_text (registry.json) and emits
    // PlayCard intents to open battlefields. getPlayLocations() is never
    // consulted, so no per-card override is needed.
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 174;
        d.def_id = R"RB(ogn-174-298)RB";
        d.name = R"RB(Sai Scout)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-174/298)RB";
        d.collector_number = 174;
        d.artist = R"RB(Dao Trong Le)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Shurima)RB"};
        d.energy_cost = 6;
        d.might = 5;
        d.keywords.set(Keyword::Vision);
        d.ability_text = R"RB([Vision] (When you play me, look at the top card of your Main Deck. You may recycle it.)
You may play me to an open battlefield.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/eab1fc8c95931519319c4c80681a261d75214c48-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_174(CardRegistry& r) {
    r.registerCard(174, std::make_unique<SaiScout>());
}

} // namespace riftbound
