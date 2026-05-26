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

class VanguardArmory : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        return ActivationCost{.exhaust = true};
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        LocationId loc{BaseLocation{ctx.controller}};
        for (int i = 0; i < 3; ++i) {
            ctx.executor.createToken(ctx.controller, CardType::Unit, "Recruit",
                                     /*might=*/1, /*tags=*/{"Recruit"}, KeywordSet{},
                                     loc, /*enter_ready=*/false);
        }
        ctx.events.logTrace("VANGUARD ARMORY: [E] -> three 1[M] Recruit tokens");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 489;
        d.def_id = R"RB(sfd-168-221)RB";
        d.name = R"RB(Vanguard Armory)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-168/221)RB";
        d.collector_number = 168;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Order};
        d.energy_cost = 7;
        d.power_cost = 1;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB([E]: Play three 1 [M] Recruit unit tokens. (You may play them to different locations.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/4f4d735ff77ea599307e142196338d438fcedc05-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_489(CardRegistry& r) {
    r.registerCard(489, std::make_unique<VanguardArmory>());
}

} // namespace riftbound
