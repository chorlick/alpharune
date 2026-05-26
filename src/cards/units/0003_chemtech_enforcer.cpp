#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include "cards/card_helpers.h"

namespace riftbound {
namespace {

class ChemtechEnforcer : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        discardThenAct(ctx, 1, "Chemtech Enforcer: discard 1", [](CardContext&){});
    }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 3;
        d.def_id = R"RB(ogn-003-298)RB";
        d.name = R"RB(Chemtech Enforcer)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-003/298)RB";
        d.collector_number = 3;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Zaun)RB"};
        d.energy_cost = 2;
        d.might = 2;
        d.assault_value = 2;
        d.keywords.set(Keyword::Assault);
        d.ability_text = R"RB([Assault 2] (+2 [M] while I'm an attacker.)
When you play me, discard 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/19dcf211457d9c9c6e9ea0cd32af76c2c92a3160-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_3(CardRegistry& r) {
    r.registerCard(3, std::make_unique<ChemtechEnforcer>());
}

} // namespace riftbound
