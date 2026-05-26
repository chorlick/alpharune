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

class SpriteBurst : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        KeywordSet kw; kw.set(Keyword::Temporary);
        auto loc = BaseLocation{ctx.controller};
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Sprite", 3,
                                  {"Fae"}, kw, loc, true);
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Sprite", 3,
                                  {"Fae"}, kw, loc, true);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 631;
        d.def_id = R"RB(unl-069-219)RB";
        d.name = R"RB(Sprite Burst)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-069/219)RB";
        d.collector_number = 69;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Mind};
        d.energy_cost = 5;
        d.keywords.set(Keyword::Temporary);
        d.ability_text = R"RB(Play two ready 3 [M] Sprite unit tokens with [Temporary]. (Kill each at the start of its controller's Beginning Phase, before scoring.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/4427ceffae1c3e7b012167461ce7c080bb2c1fe4-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_631(CardRegistry& r) {
    r.registerCard(631, std::make_unique<SpriteBurst>());
}

} // namespace riftbound
