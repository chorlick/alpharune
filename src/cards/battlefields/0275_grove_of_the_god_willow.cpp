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

class GroveOfTheGodWillow : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        ctx.executor.drawCards(ctx.controller, 1);
    }
    TriggerType triggerType() const override { return TriggerType::WhenYouHoldHere; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 275;
        d.def_id = R"RB(ogn-280-298)RB";
        d.name = R"RB(Grove of the God-Willow)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-280/298)RB";
        d.collector_number = 280;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you hold here, draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/1574863000ab23d69cbd388a32b1a09f29f78d5f-1038x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_275(CardRegistry& r) {
    r.registerCard(275, std::make_unique<GroveOfTheGodWillow>());
}

} // namespace riftbound
