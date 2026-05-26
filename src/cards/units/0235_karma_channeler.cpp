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

class KarmaChanneler : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // Clause 1: "[Vision]" — engine-handled (TriggerManager peeks the top card on
    //   play for any unit with the Vision keyword; set in def below). Works.
    // Clause 2: "When you recycle one or more cards to your Main Deck, buff a
    //   friendly unit."
    // ESCALATE(WhenYouRecycle): the TriggerType enum value WhenYouRecycle exists
    // but has NO emit site / dispatch (no recycle-to-main-deck event), so this
    // trigger cannot be wired from the card layer. Needs the recycle path to emit
    // a recycle event and TriggerManager to dispatch WhenYouRecycle.
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 235;
        d.def_id = R"RB(ogn-235-298)RB";
        d.name = R"RB(Karma, Channeler)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-235/298)RB";
        d.collector_number = 235;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Vi)RB", R"RB(Ionia)RB"};
        d.energy_cost = 6;
        d.power_cost = 1;
        d.might = 6;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Vision);
        d.ability_text = R"RB([Vision] (When you play me, look at the top card of your Main Deck. You may recycle it.)
When you recycle one or more cards to your Main Deck, buff a friendly unit. (If it doesn't have a buff, it gets a +1 [M] buff. Runes aren't cards.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/aca8664b334f2fa40cc7bae2811dd4e1b4f06388-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_235(CardRegistry& r) {
    r.registerCard(235, std::make_unique<KarmaChanneler>());
}

} // namespace riftbound
