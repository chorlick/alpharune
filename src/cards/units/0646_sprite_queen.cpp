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

class SpriteQueen : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "When you play me or at the start of your Beginning Phase, play a ready
    //  3 [M] Sprite unit token with [Temporary] to your base."
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayMe, TriggerType::AtStartOfBeginning};
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        KeywordSet kw; kw.set(Keyword::Temporary);
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Sprite",
                                  /*might=*/3, /*tags=*/{"Fae"}, kw,
                                  LocationId{BaseLocation{ctx.controller}},
                                  /*enter_ready=*/true);
        ctx.events.logTrace("SPRITE QUEEN: play a ready 3 [M] Sprite token with [Temporary]");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 646;
        d.def_id = R"RB(unl-084-219)RB";
        d.name = R"RB(Sprite Queen)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-084/219)RB";
        d.collector_number = 84;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Fae)RB", R"RB(Ionia)RB"};
        d.energy_cost = 7;
        d.power_cost = 1;
        d.might = 6;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Temporary);
        d.ability_text = R"RB(When you play me or at the start of your Beginning Phase, play a ready 3 [M] Sprite unit token with [Temporary] to your base. (Kill them at the start of their controller's next Beginning Phase, before scoring.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/64b7debe0dd3d7b644f627eba562187bc45dd313-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_646(CardRegistry& r) {
    r.registerCard(646, std::make_unique<SpriteQueen>());
}

} // namespace riftbound
