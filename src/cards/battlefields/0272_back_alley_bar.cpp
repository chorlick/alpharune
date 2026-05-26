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

// "When a unit moves FROM here, give it +1 [M] this turn."
// ESCALATE(TriggerType + dispatch): there is no "when a unit moves from here"
// battlefield trigger. The available move triggers (WhenIMove on the mover,
// WhenAFriendlyUnitMovesToFB on watchers) key off the DESTINATION battlefield,
// not the ORIGIN. TriggerManager::onUnitMoved fires nothing for the
// battlefield a unit departs from, so a battlefield card cannot observe a
// unit leaving it. Wiring this needs a new TriggerType (e.g.
// WhenAUnitMovesFromHere) emitted against UnitMovedEvent.from in
// TriggerManager, which is an engine change outside the card layer.
class BackAlleyBar : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 272;
        d.def_id = R"RB(ogn-277-298)RB";
        d.name = R"RB(Back-Alley Bar)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-277/298)RB";
        d.collector_number = 277;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When a unit moves from here, give it +1 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/3e9f659a32e390b45bc87a01bdd6af4a8a3565f7-1038x744.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_272(CardRegistry& r) {
    r.registerCard(272, std::make_unique<BackAlleyBar>());
}

} // namespace riftbound
