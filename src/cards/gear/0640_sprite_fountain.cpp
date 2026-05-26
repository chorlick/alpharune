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

class SpriteFountain : public GearCard {
public:
    const CardDef& def() const override { return def_; }

    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayThis, TriggerType::WhenIDie};
    }

    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // Both the play trigger and the Deathknell repeat run the same effect:
        // play a ready 3 [M] Sprite token with [Temporary] to the base.
        KeywordSet kw; kw.set(Keyword::Temporary);
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Sprite",
                                  /*might=*/3, /*tags=*/{"Fae"}, kw,
                                  BaseLocation{ctx.controller},
                                  /*enter_ready=*/true);
        ctx.events.logTrace("SPRITE FOUNTAIN: play 3M Sprite token (Temporary)");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 640;
        d.def_id = R"RB(unl-078-219)RB";
        d.name = R"RB(Sprite Fountain)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-078/219)RB";
        d.collector_number = 78;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Mind};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Deathknell);
        d.keywords.set(Keyword::Temporary);
        d.ability_text = R"RB([Temporary] (Kill this at the start of its controller's Beginning Phase, before scoring.)
When you play this, play a ready 3 [M] Sprite unit token with [Temporary] to your base.
[Deathknell][>] Repeat this gear's play effect. (When this dies, get the effect.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/fcb3ea3e8f829b8dc845fbf49080552d635bb47b-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_640(CardRegistry& r) {
    r.registerCard(640, std::make_unique<SpriteFountain>());
}

} // namespace riftbound
