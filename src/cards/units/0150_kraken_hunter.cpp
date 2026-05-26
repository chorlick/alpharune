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

class KrakenHunter : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // [Accelerate] / [Assault] are engine-handled keywords.
    // "As you play me, you may spend any number of buffs as an additional cost.
    // Reduce my cost by [O] for each buff you spend."
    // ENGINE LIMITATION: there is no per-card interactive additional-cost hook
    // that lets the agent choose to spend a VARIABLE number of buffs at play
    // time and translate that into a cost reduction. optionalAdditionalCost()
    // only pays a fixed energy/power amount (no waiver/scaling), and
    // selfCostReduction() has no agent decision point. The optional
    // buff-spend cost reduction is left unimplemented (needs engine support).
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 150;
        d.def_id = R"RB(ogn-150-298)RB";
        d.name = R"RB(Kraken Hunter)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-150/298)RB";
        d.collector_number = 150;
        d.artist = R"RB(JiHun Lee)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Pirate)RB", R"RB(Bilgewater)RB"};
        d.energy_cost = 3;
        d.power_cost = 2;
        d.might = 5;
        d.rarity = Rarity::Rare;
        d.assault_value = 1;
        d.keywords.set(Keyword::Accelerate);
        d.keywords.set(Keyword::Assault);
        d.ability_text = R"RB([Accelerate] (You may pay [1][O] as an additional cost to have me enter ready.)
[Assault] (+1 [M] while I'm an attacker.)
As you play me, you may spend any number of buffs as an additional cost. Reduce my cost by [O] for each buff you spend.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/51aab4710d000a9c1e665a37ef8c919ff11b0282-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_150(CardRegistry& r) {
    r.registerCard(150, std::make_unique<KrakenHunter>());
}

} // namespace riftbound
