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

class MageseekerWarden : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "While I'm at a battlefield, opponents can only play units to their base."
    // "While I'm at a battlefield, spells and abilities can't ready enemy units
    //  and gear."
    // ESCALATE(play-location-narrowing + ready-suppression): the first clause
    // needs the play action generator to forbid enemy unit-plays to
    // battlefields (no opponent-scoped play-location restriction surface); the
    // second needs readyObject() to refuse enemy targets while an effect is the
    // source (no ready-suppression hook). Both engine-side. Whole card blocked.
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 70;
        d.def_id = R"RB(ogn-070-298)RB";
        d.name = R"RB(Mageseeker Warden)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-070/298)RB";
        d.collector_number = 70;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Demacia)RB"};
        d.energy_cost = 6;
        d.power_cost = 1;
        d.might = 5;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(While I'm at a battlefield, opponents can only play units to their base.
While I'm at a battlefield, spells and abilities can't ready enemy units and gear.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ae844b929f817cdf76fe40c7bf5d5fc02062bdac-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_70(CardRegistry& r) {
    r.registerCard(70, std::make_unique<MageseekerWarden>());
}

} // namespace riftbound
