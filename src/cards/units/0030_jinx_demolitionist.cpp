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

class JinxDemolitionist : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        discardThenAct(ctx, 2, "Jinx, Demolitionist: discard 2", [](CardContext&){});
    }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 30;
        d.def_id = R"RB(ogn-030-298)RB";
        d.name = R"RB(Jinx, Demolitionist)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-030/298)RB";
        d.collector_number = 30;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Jinx)RB", R"RB(Zaun)RB"};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.might = 4;
        d.rarity = Rarity::Rare;
        d.assault_value = 2;
        d.keywords.set(Keyword::Accelerate);
        d.keywords.set(Keyword::Assault);
        d.ability_text = R"RB([Accelerate] (You may pay [1][R] as an additional cost to have me enter ready.)
[Assault 2] (+2 [M] while I'm an attacker.)
When you play me, discard 2.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/d6cac988aa7798945e550eba6841d3993868c4a4-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_30(CardRegistry& r) {
    r.registerCard(30, std::make_unique<JinxDemolitionist>());
}

} // namespace riftbound
