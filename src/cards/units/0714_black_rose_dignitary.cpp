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

class BlackRoseDignitary : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIDie; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        ctx.executor.channelRunes(ctx.controller, 1, true);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 714;
        d.def_id = R"RB(unl-152-219)RB";
        d.name = R"RB(Black Rose Dignitary)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-152/219)RB";
        d.collector_number = 152;
        d.artist = R"RB(Grafit Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Noxus)RB"};
        d.energy_cost = 3;
        d.might = 2;
        d.assault_value = 1;
        d.keywords.set(Keyword::Assault);
        d.keywords.set(Keyword::Deathknell);
        d.ability_text = R"RB([Assault] (+1 [M] while I'm an attacker.)
[Deathknell][>] Channel 1 rune exhausted. (When I die, get the effect.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/b63912e07914c3ef7e2bcdb95d5521fc26c880f2-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_714(CardRegistry& r) {
    r.registerCard(714, std::make_unique<BlackRoseDignitary>());
}

} // namespace riftbound
