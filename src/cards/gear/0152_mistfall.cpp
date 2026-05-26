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

class Mistfall : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    // "When you buff a friendly unit, you may pay [O] and exhaust this to ready it."
    // TriggerType::WhenYouBuffAFriendlyUnit now exists (fires on this gear when
    // the controller buffs any friendly unit). REMAINING: the trigger does not
    // yet carry the *buffed unit's* id, so "ready IT" can't identify the subject.
    // Pending a small extension to pass the buffed object through the trigger
    // (then: confirmOptional pay [O] + exhaust this -> readyObject(subject)).
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 152;
        d.def_id = R"RB(ogn-152-298)RB";
        d.name = R"RB(Mistfall)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-152/298)RB";
        d.collector_number = 152;
        d.artist = R"RB(Polar Engine Studio)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Body};
        d.energy_cost = 3;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When you buff a friendly unit, you may pay [O] and exhaust this to ready it.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/1cedf0dcd6c4a44ae867e9f04f9f179d9b91c357-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_152(CardRegistry& r) {
    r.registerCard(152, std::make_unique<Mistfall>());
}

} // namespace riftbound
