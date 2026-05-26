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

class MachineEvangel : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIDie; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // "[Deathknell] — Play three 1 [M] Recruit unit tokens into your base."
        LocationId loc{BaseLocation{ctx.controller}};
        for (int i = 0; i < 3; ++i)
            ctx.executor.createToken(ctx.controller, CardType::Unit, "Recruit",
                                     1, {"Recruit"}, KeywordSet{}, loc, false);
        ctx.events.logTrace("MACHINE EVANGEL: Deathknell -> three 1[M] Recruits to base");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 239;
        d.def_id = R"RB(ogn-239-298)RB";
        d.name = R"RB(Machine Evangel)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-239/298)RB";
        d.collector_number = 239;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Piltover)RB"};
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 4;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Deathknell);
        d.ability_text = R"RB([Deathknell] — Play three 1 [M] Recruit unit tokens into your base. (When I die, get the effect.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ad15d4ff89548e83dcede9b209b12233652cf3a1-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_239(CardRegistry& r) {
    r.registerCard(239, std::make_unique<MachineEvangel>());
}

} // namespace riftbound
