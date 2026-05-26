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

class DownstageDramatics : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        ctx.executor.drawCards(ctx.controller, 1);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 623;
        d.def_id = R"RB(unl-061-219)RB";
        d.name = R"RB(Downstage Dramatics)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-061/219)RB";
        d.collector_number = 61;
        d.artist = R"RB(莺之歌)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Mind};
        d.energy_cost = 2;
        d.keywords.set(Keyword::Reaction);
        d.keywords.set(Keyword::Repeat);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
[Repeat] [2] (You may pay the additional cost to repeat this spell's effect.)
Draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/d76418eabcddbea148f3331913223b79a39aabda-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_623(CardRegistry& r) {
    r.registerCard(623, std::make_unique<DownstageDramatics>());
}

} // namespace riftbound
