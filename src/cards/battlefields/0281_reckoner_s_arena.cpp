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

class ReckonerSArena : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    // ENGINE GAP: "When you hold here, activate the conquer effects of units
    // here." This must re-fire the WhenIConquer triggers of every unit at this
    // battlefield, but a Card's onTrigger has no access to the CardRegistry to
    // dispatch other cards' triggers (CardContext/EffectExecutor expose neither
    // the registry nor a "fire trigger" primitive). Requires an engine-side
    // re-fire-conquer-effects helper.
    TriggerType triggerType() const override { return TriggerType::WhenYouHoldHere; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 281;
        d.def_id = R"RB(ogn-286-298)RB";
        d.name = R"RB(Reckoner's Arena)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-286/298)RB";
        d.collector_number = 286;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you hold here, activate the conquer effects of units here.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/6b7d867487efcbefa8c3d67043a839497ec50388-1038x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_281(CardRegistry& r) {
    r.registerCard(281, std::make_unique<ReckonerSArena>());
}

} // namespace riftbound
