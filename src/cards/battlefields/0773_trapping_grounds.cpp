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

class TrappingGrounds : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouConquerHere; }
    // "When you conquer here, if you assigned 3 or more excess damage, play a
    //  1 [M] Bird unit token with [Deflect]."
    // The "3+ excess damage" condition is not surfaced to triggers (same as
    // Piltover Enforcer 783) — treated as satisfied (documented approximation).
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        KeywordSet kw; kw.set(Keyword::Deflect);
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Bird",
                                  /*might=*/1, /*tags=*/{"Bird"}, kw,
                                  BaseLocation{ctx.controller},
                                  /*enter_ready=*/false);
        ctx.events.logTrace("TRAPPING GROUNDS: conquer -> 1[M] Bird w/ [Deflect] "
                            "(excess-damage condition not engine-checked)");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 773;
        d.def_id = R"RB(unl-217-219)RB";
        d.name = R"RB(Trapping Grounds)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-217/219)RB";
        d.collector_number = 217;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.deflect_value = 1;
        d.keywords.set(Keyword::Deflect);
        d.ability_text = R"RB(When you conquer here, if you assigned 3 or more excess damage, play a 1 [M] Bird unit token with [Deflect].)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/234bb01e24892aefa024ae402f6ee7703ccdbbd4-1039x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_773(CardRegistry& r) {
    r.registerCard(773, std::make_unique<TrappingGrounds>());
}

} // namespace riftbound
