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

class LilliaFaeFawn : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIMove; }
    void onTrigger(CardContext& ctx,
                   const std::vector<GameObjectId>& /*targets*/) override {
        LocationId loc = BaseLocation{ctx.controller};  // fallback
        if (ctx.state.objectExists(ctx.source)) {
            const auto& self = ctx.state.getObject(ctx.source);
            if (self.location.has_value()) loc = *self.location;
        }
        KeywordSet kw; kw.set(Keyword::Temporary);
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Sprite",
                                 /*might=*/3, /*tags=*/{"Fae"}, kw, loc,
                                 /*enter_ready=*/true);
        ctx.events.logTrace("LILLIA: creates 3M Sprite token (Temporary) at "
                            "move location");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 644;
        d.def_id = R"RB(unl-082-219)RB";
        d.name = R"RB(Lillia, Fae Fawn)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-082/219)RB";
        d.collector_number = 82;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Fae)RB", R"RB(Lillia)RB", R"RB(Ionia)RB"};
        d.energy_cost = 3;
        d.might = 3;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Accelerate);
        d.keywords.set(Keyword::Temporary);
        d.ability_text = R"RB([Accelerate] (You may pay [1][B] as an additional cost to have me enter ready.)
When I move from a location, play a 3 [M] Sprite unit token with [Temporary] there. (Kill it at the start of its controller's Beginning Phase, before scoring.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/65d0af5361b335a7e65c7aa03a099fc0de3431e6-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_644(CardRegistry& r) {
    r.registerCard(644, std::make_unique<LilliaFaeFawn>());
}

} // namespace riftbound
