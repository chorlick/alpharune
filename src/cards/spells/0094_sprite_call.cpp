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

class SpriteCall : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Play a ready 3 [M] Sprite unit token with [Temporary]." — to the base
    // or a battlefield its controller controls, their choice; played from
    // facedown, at the battlefield it was hidden at and nowhere else (CR
    // 811.1.d.5 — pickTokenDest reads it off the card).
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // At resolution: an effect play's location is chosen as it plays (CR 420.3.b.2; batch M1).
        const int dest = pickTokenDest(*this, ctx, "Sprite Call: play the Sprite to");
        if (dest < 0) return;   // waiting for the choice, or nowhere to play it
        KeywordSet kw; kw.set(Keyword::Temporary);
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Sprite", 3,
                                  {"Fae"}, kw, destLocation(ctx.controller, dest),
                                  /*enter_ready=*/true,
                                  tokenPlayLocations(ctx));
        RIFT_TRACE(ctx.events, "SPRITE CALL: play ready 3 [M] Sprite [Temporary]");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 94;
        d.def_id = R"RB(ogn-094-298)RB";
        d.name = R"RB(Sprite Call)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-094/298)RB";
        d.collector_number = 94;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Mind};
        d.energy_cost = 3;
        d.keywords.set(Keyword::Action);
        d.keywords.set(Keyword::Hidden);
        d.ability_text = R"RB([Hidden] (Hide now for [A] to react with later for .)
[Action] (Play on your turn or in showdowns.)
Play a ready 3 [M] Sprite unit token with [Temporary]. (Kill it at the start of its controller's Beginning Phase, before scoring.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ab3db48dcbb12b01151d3a08c2412020380b7ca5-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_94(CardRegistry& r) {
    r.registerCard(94, std::make_unique<SpriteCall>());
}

} // namespace riftbound
