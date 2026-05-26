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

class CommanderLedros : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // ENGINE GAP: "As you play me, you may kill any number of friendly units as
    // an additional cost. Reduce my cost by [Y] for each killed this way."
    // This is an interactive sacrifice-for-cost-reduction additional cost.
    // selfCostReduction() is state-based (not interactive), and
    // OptionalAdditionalCost only models energy/power costs, not "kill N units".
    // Implementing requires an engine-side interactive additional-cost hook that
    // kills agent-chosen friendly units before paying and reduces the cost per
    // kill. [Deflect] and [Ganking] are engine keywords and work.
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 231;
        d.def_id = R"RB(ogn-231-298)RB";
        d.name = R"RB(Commander Ledros)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-231/298)RB";
        d.collector_number = 231;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Spirit)RB", R"RB(Shadow Isles)RB"};
        d.energy_cost = 6;
        d.power_cost = 4;
        d.might = 8;
        d.rarity = Rarity::Rare;
        d.deflect_value = 1;
        d.keywords.set(Keyword::Deflect);
        d.keywords.set(Keyword::Ganking);
        d.ability_text = R"RB(As you play me, you may kill any number of friendly units as an additional cost. Reduce my cost by [Y] for each killed this way.
[Deflect] (Opponents must pay [A] to choose me with a spell or ability.)
[Ganking] (I can move from battlefield to battlefield.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/1183be5effc6275f17da09b983feb36632752af4-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_231(CardRegistry& r) {
    r.registerCard(231, std::make_unique<CommanderLedros>());
}

} // namespace riftbound
