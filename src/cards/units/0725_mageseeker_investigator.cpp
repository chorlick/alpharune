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

class MageseekerInvestigator : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "Opponents must pay [A] for each unit beyond the first to move multiple
    //  units to my battlefield at the same time."
    // ENGINE GAP: there is no move-cost-modifier infrastructure. Unit movement
    // is not gated by a per-target/per-destination cost, and there is no hook
    // for a continuous effect to surcharge multi-unit moves to a specific
    // battlefield. Modeling this requires engine support (a move-cost modifier
    // consulted by the move-action generator/payment path). Left unimplemented.
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 725;
        d.def_id = R"RB(unl-163-219)RB";
        d.name = R"RB(Mageseeker Investigator)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-163/219)RB";
        d.collector_number = 163;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Demacia)RB"};
        d.energy_cost = 4;
        d.might = 4;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(Opponents must pay [A] for each unit beyond the first to move multiple units to my battlefield at the same time.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/004c77c98bc5a6f679191a0d289b26ac13e84728-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_725(CardRegistry& r) {
    r.registerCard(725, std::make_unique<MageseekerInvestigator>());
}

} // namespace riftbound
