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

class JhinMurderousArtist : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIMove; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        // [Add] [1][A] — 1 floating energy + 1 universal-domain power.
        ctx.executor.addFloatingEnergy(ctx.controller, 1);
        ctx.executor.addFloatingUniversalPower(ctx.controller, 1);
        ctx.events.logTrace("JHIN: [Add] [1][A] on move");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 584;
        d.def_id = R"RB(unl-022-219)RB";
        d.name = R"RB(Jhin, Murderous Artist)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-022/219)RB";
        d.collector_number = 22;
        d.artist = R"RB(Alessandro Poli)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Jhin)RB", R"RB(Ionia)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 4;
        d.rarity = Rarity::Rare;
        d.deflect_value = 1;
        d.keywords.set(Keyword::Deflect);
        d.keywords.set(Keyword::Ganking);
        d.ability_text = R"RB([Deflect] (Opponents must pay [A] to choose me with a spell or ability.)
[Ganking] (I can move from battlefield to battlefield.)
When I move, [Add] [1][A]. (Abilities that add resources can't be reacted to.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/8da724a842cf61cb24c3c02dd99adc93a9222e06-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_584(CardRegistry& r) {
    r.registerCard(584, std::make_unique<JhinMurderousArtist>());
}

} // namespace riftbound
