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

class CorinaVeraza : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIMoveToFB; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // "When I move to a battlefield, play three 1 [M] Recruit unit tokens here."
        if (!ctx.state.objectExists(ctx.source)) return;
        auto loc = ctx.state.getObject(ctx.source).location;
        if (!loc) return;  // must be at a battlefield ("here")
        for (int i = 0; i < 3; ++i)
            ctx.executor.createToken(ctx.controller, CardType::Unit, "Recruit",
                                     1, {"Recruit"}, KeywordSet{}, *loc, false);
        ctx.events.logTrace("CORINA VERAZA: move -> three 1[M] Recruit tokens here");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 499;
        d.def_id = R"RB(sfd-179-221)RB";
        d.name = R"RB(Corina Veraza)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-179/221)RB";
        d.collector_number = 179;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Zaun)RB"};
        d.energy_cost = 7;
        d.power_cost = 1;
        d.might = 6;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Accelerate);
        d.ability_text = R"RB([Accelerate] (You may pay [1][Y] as an additional cost to have me enter ready.)
When I move to a battlefield, play three 1 [M] Recruit unit tokens here.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/6b0f55aee4b4f9d0074c808e0fff00dcb7e35377-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_499(CardRegistry& r) {
    r.registerCard(499, std::make_unique<CorinaVeraza>());
}

} // namespace riftbound
