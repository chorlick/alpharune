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

class UndercoverAgent : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        discardThenAct(ctx, 2, "Undercover Agent: discard 2 then draw 2",
            [](CardContext& c) { c.executor.drawCards(c.controller, 2); });
    }
    TriggerType triggerType() const override { return TriggerType::WhenIDie; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 178;
        d.def_id = R"RB(ogn-178-298)RB";
        d.name = R"RB(Undercover Agent)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-178/298)RB";
        d.collector_number = 178;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Chaos};
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 5;
        d.keywords.set(Keyword::Deathknell);
        d.ability_text = R"RB([Deathknell] — Discard 2, then draw 2. (When I die, get the effect.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/3e4e318c0cc97b13646ee454da71046a09236e47-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_178(CardRegistry& r) {
    r.registerCard(178, std::make_unique<UndercoverAgent>());
}

} // namespace riftbound
