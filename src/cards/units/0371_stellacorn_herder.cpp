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

class StellacornHerder : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIMove; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        ctx.executor.drawCards(ctx.controller, 1);
        ctx.events.logTrace("STELLACORN HERDER: drew 1 on move");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 371;
        d.def_id = R"RB(sfd-048-221)RB";
        d.name = R"RB(Stellacorn Herder)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-048/221)RB";
        d.collector_number = 48;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Mount Targon)RB"};
        d.energy_cost = 4;
        d.might = 3;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When I move, draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/1e7960a35d45dd55e934260ea23307853aa5afd4-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_371(CardRegistry& r) {
    r.registerCard(371, std::make_unique<StellacornHerder>());
}

} // namespace riftbound
