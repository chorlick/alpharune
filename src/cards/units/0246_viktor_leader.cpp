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

class ViktorLeader : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "When another non-Recruit unit you control dies, play a 1 [M] Recruit
    // unit token into your base."
    // NOTE: the engine fires WhenAFriendlyUnitDies generically and does not
    // surface the dead unit's tags at resolution time, so the "non-Recruit"
    // filter can't be enforced here. Core effect (token into base) implemented.
    TriggerType triggerType() const override { return TriggerType::WhenAFriendlyUnitDies; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        LocationId loc{BaseLocation{ctx.controller}};
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Recruit",
                                 1, {"Recruit"}, KeywordSet{}, loc, false);
        ctx.events.logTrace("VIKTOR LEADER: friendly unit died -> 1[M] Recruit to base");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 246;
        d.def_id = R"RB(ogn-246-298)RB";
        d.name = R"RB(Viktor, Leader)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-246/298)RB";
        d.collector_number = 246;
        d.artist = R"RB(League Splash Team)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Viktor)RB", R"RB(Zaun)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 4;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(When another non-Recruit unit you control dies, play a 1 [M] Recruit unit token into your base.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ab390d6d074c3f07abba000cc166faa1796ec464-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_246(CardRegistry& r) {
    r.registerCard(246, std::make_unique<ViktorLeader>());
}

} // namespace riftbound
