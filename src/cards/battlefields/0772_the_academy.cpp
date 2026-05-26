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

class TheAcademy : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouHoldHere; }
    // "When you hold here, give your next spell this turn [Repeat] equal to its
    //  base cost."
    // ESCALATE(grant-repeat-to-next-spell): [Repeat] (CR 820) is an OPTIONAL
    // ADDITIONAL COST the player pays at spell-play time (ChainItem::repeats_paid).
    // There is no surface for an external effect to GRANT free/eligible [Repeat]
    // to a player's next spell — no per-player "next spell has Repeat N" field
    // exists and the play-action generator does not consult one. Needs a
    // granted-repeat field + payment-path honoring it. (Same gap as Syndra,
    // Transcendent #708 "your spells have [Repeat]".)
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 772;
        d.def_id = R"RB(unl-216-219)RB";
        d.name = R"RB(The Academy)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-216/219)RB";
        d.collector_number = 216;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Repeat);
        d.ability_text = R"RB(When you hold here, give your next spell this turn [Repeat] equal to its base cost. (You may pay the additional cost to repeat the spell's effect.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/82646c995f3d0b897ce97f7b68c35f5fd9384b8e-1039x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_772(CardRegistry& r) {
    r.registerCard(772, std::make_unique<TheAcademy>());
}

} // namespace riftbound
