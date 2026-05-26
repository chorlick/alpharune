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

class ImmortalPhoenix : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "When you kill a unit with a spell, you may pay [1][R] to play me from
    //  your trash."
    // ESCALATE(kill_with_spell_trigger + alt_play_from_trash): (1) there is no
    // "when you kill a unit with a spell" trigger event, and (2) triggers are
    // only dispatched to on-board cards — a card in the trash receives none, and
    // there is no action-generator hook to play a card from the trash. Both
    // require engine support. ([Assault 2] is engine-handled via keywords.)
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 37;
        d.def_id = R"RB(ogn-037-298)RB";
        d.name = R"RB(Immortal Phoenix)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-037/298)RB";
        d.collector_number = 37;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Spirit)RB"};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.might = 3;
        d.rarity = Rarity::Epic;
        d.assault_value = 2;
        d.keywords.set(Keyword::Assault);
        d.ability_text = R"RB([Assault 2] (+2 [M] while I'm an attacker.)
When you kill a unit with a spell, you may pay [1][R] to play me from your trash.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/7b623ae985bf5f362b6d8d4a17e9b8146aeae3c3-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_37(CardRegistry& r) {
    r.registerCard(37, std::make_unique<ImmortalPhoenix>());
}

} // namespace riftbound
