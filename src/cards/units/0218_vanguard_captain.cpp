#include "cards/card.h"
#include "cards/card_helpers.h"
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

class VanguardCaptain : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    // [Legion] — the ability is Active only if another card was played this
    // turn, and only an Active ability triggers (CR 727.1.c.1, 812.1.c):
    // judged as I am played. Off, nothing goes on the chain (finding G9).
    bool triggerCondition(const GameState& state, GameObjectId /*source*/, PlayerId controller,
                          TriggerType /*trigger*/, GameObjectId /*subject*/) const override {
        return legionActive(state, controller);
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // "[Legion] — play two 1 [M] Recruit unit tokens here." "Here" is my
        // location.
        auto loc = ctx.state.getObject(ctx.source).location;
        if (!loc) loc = LocationId{BaseLocation{ctx.controller}};
        for (int i = 0; i < 2; ++i)
            ctx.executor.createToken(ctx.controller, CardType::Unit, "Recruit",
                                     1, {"Recruit"}, KeywordSet{}, *loc, false);
        RIFT_TRACE(ctx.events, "VANGUARD CAPTAIN: Legion -> two 1[M] Recruits here");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 218;
        d.def_id = R"RB(ogn-218-298)RB";
        d.name = R"RB(Vanguard Captain)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-218/298)RB";
        d.collector_number = 218;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Elite)RB", R"RB(Demacia)RB"};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.might = 3;
        d.keywords.set(Keyword::Legion);
        d.ability_text = R"RB([Legion] — When you play me, play two 1 [M] Recruit unit tokens here. (Get the effect if you've played another card this turn.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/f0c9ddb2af7a0d4991938cf1e3058eb0f5d2e357-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_218(CardRegistry& r) {
    r.registerCard(218, std::make_unique<VanguardCaptain>());
}

} // namespace riftbound
