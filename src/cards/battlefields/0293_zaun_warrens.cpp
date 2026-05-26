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

class ZaunWarrens : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        discardThenAct(ctx, 1, "Zaun Warrens: discard 1 then draw 1",
            [](CardContext& c) { c.executor.drawCards(c.controller, 1); });
    }
    TriggerType triggerType() const override { return TriggerType::WhenYouConquerHere; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 293;
        d.def_id = R"RB(ogn-298-298)RB";
        d.name = R"RB(Zaun Warrens)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-298/298)RB";
        d.collector_number = 298;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you conquer here, discard 1, then draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/458ee40086c77b43b98c2decbaf33a4aa2359bb9-1038x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_293(CardRegistry& r) {
    r.registerCard(293, std::make_unique<ZaunWarrens>());
}

} // namespace riftbound
