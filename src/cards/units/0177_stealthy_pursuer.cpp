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

class StealthyPursuer : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "When a friendly unit moves from my location, I may be moved with it."
    // ENGINE LIMITATION: WhenAFriendlyUnitMovesToFB fires the watcher's
    // onTrigger with EMPTY context — it does not pass the moving unit's id, its
    // FROM-location, or its destination. This card needs all three ("from my
    // location" gate + "moved WITH it" destination). TriggerManager::onUnitMoved
    // would need to capture move context for watchers (engine edit, out of
    // scope). Left as a documented no-op.
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 177;
        d.def_id = R"RB(ogn-177-298)RB";
        d.name = R"RB(Stealthy Pursuer)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-177/298)RB";
        d.collector_number = 177;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Shurima)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 4;
        d.ability_text = R"RB(When a friendly unit moves from my location, I may be moved with it.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ab23fd84badd67e9981512acbe6c3e9a8f97f42d-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_177(CardRegistry& r) {
    r.registerCard(177, std::make_unique<StealthyPursuer>());
}

} // namespace riftbound
