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

class MaskOfForesight : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    // "When a friendly unit attacks or defends alone, give it +1 [M] this turn."
    // ESCALATE(WhenAUnitAttacksOrDefendsAlone): the enum value exists but has
    // NO dispatch — TriggerManager never emits it, and there is no "alone"
    // predicate surfaced to triggers. Cannot be wired from the card layer.
    // Whole card blocked.
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 60;
        d.def_id = R"RB(ogn-060-298)RB";
        d.name = R"RB(Mask of Foresight)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-060/298)RB";
        d.collector_number = 60;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Calm};
        d.energy_cost = 2;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When a friendly unit attacks or defends alone, give it +1 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/03824709acbb4151d13b083a842c4702a3e61221-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_60(CardRegistry& r) {
    r.registerCard(60, std::make_unique<MaskOfForesight>());
}

} // namespace riftbound
