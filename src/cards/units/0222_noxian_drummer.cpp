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

class NoxianDrummer : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIMoveToFB; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // "When I move to a battlefield, play a 1 [M] Recruit unit token here."
        if (!ctx.state.objectExists(ctx.source)) return;
        auto loc = ctx.state.getObject(ctx.source).location;
        if (!loc) return;
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Recruit",
                                 1, {"Recruit"}, KeywordSet{}, *loc, false);
        ctx.events.logTrace("NOXIAN DRUMMER: move -> 1[M] Recruit here");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 222;
        d.def_id = R"RB(ogn-222-298)RB";
        d.name = R"RB(Noxian Drummer)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-222/298)RB";
        d.collector_number = 222;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Trifarian)RB", R"RB(Noxus)RB"};
        d.energy_cost = 3;
        d.might = 3;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When I move to a battlefield, play a 1 [M] Recruit unit token here. (It is also at the battlefield.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/45a69adf92b6951c8c8fa974273c22aade312068-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_222(CardRegistry& r) {
    r.registerCard(222, std::make_unique<NoxianDrummer>());
}

} // namespace riftbound
