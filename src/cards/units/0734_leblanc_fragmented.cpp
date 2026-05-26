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

class LeBlancFragmented : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIDie; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        int draw_count = (ctx.state.turn.phase == TurnPhase::BeginningStep) ? 2 : 1;
        ctx.executor.drawCards(ctx.controller, draw_count);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 734;
        d.def_id = R"RB(unl-172-219)RB";
        d.name = R"RB(LeBlanc, Fragmented)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-172/219)RB";
        d.collector_number = 172;
        d.artist = R"RB(League Splash Team)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Noxus)RB", R"RB(LeBlanc)RB"};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.might = 3;
        d.rarity = Rarity::Rare;
        d.assault_value = 1;
        d.keywords.set(Keyword::Assault);
        d.keywords.set(Keyword::Deathknell);
        d.ability_text = R"RB([Assault] (+1 [M] while I'm an attacker.)
[Deathknell][>] Draw 1. If it's your Beginning Phase, draw 2 instead. (When I die, get the effect.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ee9f8d7a65e57c1d907edd4e5df2a3ea6966bff9-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_734(CardRegistry& r) {
    r.registerCard(734, std::make_unique<LeBlancFragmented>());
}

} // namespace riftbound
