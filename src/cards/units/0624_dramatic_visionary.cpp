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

class DramaticVisionary : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "[Deathknell] [Predict 2]." When I die, look at the top two cards of your
    // Main Deck; recycle any of them and put the rest back in any order.
    TriggerType triggerType() const override { return TriggerType::WhenIDie; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        ctx.executor.predict(ctx.controller, 2);
        ctx.events.logTrace("DRAMATIC VISIONARY: Deathknell -> Predict 2");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 624;
        d.def_id = R"RB(unl-062-219)RB";
        d.name = R"RB(Dramatic Visionary)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-062/219)RB";
        d.collector_number = 62;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Ionia)RB"};
        d.energy_cost = 4;
        d.might = 4;
        d.keywords.set(Keyword::Deathknell);
        d.keywords.set(Keyword::Predict);
        d.ability_text = R"RB([Deathknell][>] [Predict 2]. (When I die, look at the top two cards of your Main Deck. Recycle any of them and put the rest back in any order.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/3153c05a8c3fb4d8f945a95fad54ea25e087ad47-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_624(CardRegistry& r) {
    r.registerCard(624, std::make_unique<DramaticVisionary>());
}

} // namespace riftbound
