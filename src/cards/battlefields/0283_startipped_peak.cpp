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

class StartippedPeak : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouHoldHere; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto still_legal = []() { return true; };
        int conf = confirmOptional(ctx, "Startipped Peak: channel 1 rune exhausted?",
                                   still_legal);
        if (conf == -1) return;  // waiting on agent
        if (conf == 0) return;   // declined
        ctx.executor.channelRunes(ctx.controller, 1, /*enter_exhausted=*/true);
        ctx.events.logTrace("STARTIPPED PEAK: channel 1 rune exhausted");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 283;
        d.def_id = R"RB(ogn-288-298)RB";
        d.name = R"RB(Startipped Peak)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-288/298)RB";
        d.collector_number = 288;
        d.artist = R"RB(Polar Engine Studio)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you hold here, you may channel 1 rune exhausted.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/05adde1d8ab40e2a2f832a89ac5c9174ee78796f-1038x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_283(CardRegistry& r) {
    r.registerCard(283, std::make_unique<StartippedPeak>());
}

} // namespace riftbound
