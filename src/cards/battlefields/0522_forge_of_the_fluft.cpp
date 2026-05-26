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

class ForgeOfTheFluft : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    // "While you control this battlefield, friendly legends have
    //  '[E]: Attach an Equipment you control to a unit you control.'"
    // ESCALATE(aura-granted-activated-ability): the aura system (AuraEffect)
    // grants only Might/keyword/combat-damage suppression. There is no surface
    // for an aura source to inject an ACTIVATED ability onto another card
    // (activatedAbilities() is per-class, not aura-driven), and the action
    // generator does not enumerate aura-granted abilities. Whole card blocked.
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 522;
        d.def_id = R"RB(sfd-208-221)RB";
        d.name = R"RB(Forge of the Fluft)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-208/221)RB";
        d.collector_number = 208;
        d.artist = R"RB(Dao Trong Le)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(While you control this battlefield, friendly legends have "[E]: Attach an Equipment you control to a unit you control.")RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/6ad10353e7fc1337d8cc79086f8eaac1c75f5598-1039x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_522(CardRegistry& r) {
    r.registerCard(522, std::make_unique<ForgeOfTheFluft>());
}

} // namespace riftbound
