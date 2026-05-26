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

class EvershadeStalker : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        discardThenAct(ctx, 1, "Evershade Stalker: discard 1 then draw 1",
            [](CardContext& c) { c.executor.drawCards(c.controller, 1); });
    }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 685;
        d.def_id = R"RB(unl-123-219)RB";
        d.name = R"RB(Evershade Stalker)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-123/219)RB";
        d.collector_number = 123;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Shadow Isles)RB"};
        d.energy_cost = 3;
        d.might = 3;
        d.ability_text = R"RB(When you play me, discard 1, then draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/d8fb687ecdb22d7497ac55a1be0db0ba33596b43-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_685(CardRegistry& r) {
    r.registerCard(685, std::make_unique<EvershadeStalker>());
}

} // namespace riftbound
