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

class SpriteMother : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        KeywordSet kw; kw.set(Keyword::Temporary);
        auto loc = ctx.state.getObject(ctx.source).location.value_or(BaseLocation{ctx.controller});
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Sprite", 3,
                                  {"Fae"}, kw, loc, true);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 106;
        d.def_id = R"RB(ogn-106-298)RB";
        d.name = R"RB(Sprite Mother)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-106/298)RB";
        d.collector_number = 106;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Fae)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 3;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Temporary);
        d.ability_text = R"RB(When you play me, play a ready 3 [M] Sprite unit token with [Temporary] here. (Kill it at the start of its controller's Beginning Phase, before scoring.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/65c06528f88de1ac207b382d4830ccfdd08a2d12-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_106(CardRegistry& r) {
    r.registerCard(106, std::make_unique<SpriteMother>());
}

} // namespace riftbound
