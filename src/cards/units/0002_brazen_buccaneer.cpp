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

class BrazenBuccaneer : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "As you play me, you may discard 1 as an additional cost. If you do,
    //  reduce my cost by [2]."
    // ENGINE GAP: the optional-additional-cost play hook
    // (Card::optionalAdditionalCost) only supports energy/power costs, not a
    // discard, and cost reduction must be decided BEFORE payment. There is no
    // discard-as-additional-cost-to-reduce path, so this cannot be modeled
    // from the card layer.
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 2;
        d.def_id = R"RB(ogn-002-298)RB";
        d.name = R"RB(Brazen Buccaneer)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-002/298)RB";
        d.collector_number = 2;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Pirate)RB", R"RB(Bilgewater)RB"};
        d.energy_cost = 6;
        d.might = 5;
        d.ability_text = R"RB(As you play me, you may discard 1 as an additional cost. If you do, reduce my cost by [2].)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/d481971f7560e7235e7d6934767da18daa019eff-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_2(CardRegistry& r) {
    r.registerCard(2, std::make_unique<BrazenBuccaneer>());
}

} // namespace riftbound
