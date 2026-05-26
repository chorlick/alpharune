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

class DesertSCall : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    void onResolve(CardContext& ctx,
                   const std::vector<GameObjectId>& /*targets*/) override {
        auto loc = LocationId{BaseLocation{ctx.controller}};
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Sand Soldier",
                                 /*might=*/2, /*tags=*/{"Sand Soldier"},
                                 KeywordSet{}, loc, /*enter_ready=*/false);
        ctx.events.logTrace("DESERT'S CALL: played 2[M] Sand Soldier token "
                            "(engine repeats per [Repeat][2] paid)");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 354;
        d.def_id = R"RB(sfd-031-221)RB";
        d.name = R"RB(Desert's Call)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-031/221)RB";
        d.collector_number = 31;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Calm};
        d.energy_cost = 2;
        d.keywords.set(Keyword::Repeat);
        d.ability_text = R"RB([Repeat] [2] (You may pay the additional cost to repeat this spell's effect.)
Play a 2 [M] Sand Soldier unit token.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/b38ab6c02238b5a8e456cdad9b5108bb30e718bc-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_354(CardRegistry& r) {
    r.registerCard(354, std::make_unique<DesertSCall>());
}

} // namespace riftbound
