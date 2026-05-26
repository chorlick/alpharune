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

// "When a friendly unit is played this turn, buff it. (If it doesn't have a
//  buff, it gets a +1 [M] buff.)  Draw 1."

class RallyTheTroops : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // ENGINE GAP: the delayed-ability mechanism is one-shot (fires once
        // then is removed) and does NOT surface the played-unit id to the
        // delayed onTrigger. So the rest-of-turn "buff EACH friendly unit
        // played" rider can't be expressed (neither the per-unit repeat nor
        // the buff target are reachable). Only the unconditional "Draw 1"
        // half is implemented; the delayed buff is left for a per-event
        // delayed-ability hook that passes the subject object.
        ctx.executor.drawCards(ctx.controller, 1);
        ctx.events.logTrace("RALLY THE TROOPS: draw 1 (delayed buff-on-play "
                            "rider not engine-expressible)");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 487;
        d.def_id = R"RB(sfd-166-221)RB";
        d.name = R"RB(Rally the Troops)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-166/221)RB";
        d.collector_number = 166;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Order};
        d.energy_cost = 2;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
When a friendly unit is played this turn, buff it. (If it doesn't have a buff, it gets a +1 [M] buff.)
Draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/952476dd51338ff97774946ded134a2072b2e6c9-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_487(CardRegistry& r) {
    r.registerCard(487, std::make_unique<RallyTheTroops>());
}

} // namespace riftbound
