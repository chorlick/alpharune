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

class AltarToUnity : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouHoldHere; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // "When you hold here, play a 1 [M] Recruit unit token in your base."
        LocationId loc{BaseLocation{ctx.controller}};
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Recruit",
                                 1, {"Recruit"}, KeywordSet{}, loc, false);
        ctx.events.logTrace("ALTAR TO UNITY: hold -> 1[M] Recruit to base");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 270;
        d.def_id = R"RB(ogn-275-298)RB";
        d.name = R"RB(Altar to Unity)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-275/298)RB";
        d.collector_number = 275;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you hold here, play a 1 [M] Recruit unit token in your base.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/2392529560dc9af72596c6fc65b4c0356bbc44d1-1038x744.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_270(CardRegistry& r) {
    r.registerCard(270, std::make_unique<AltarToUnity>());
}

} // namespace riftbound
