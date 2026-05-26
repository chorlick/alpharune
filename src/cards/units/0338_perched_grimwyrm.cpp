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

// "Play me only to a battlefield you conquered this turn. (You can't play me
// anywhere else.)"
// This is a RESTRICTING play-location rule: the unit may be played ONLY to a
// battlefield the controller conquered this turn, and NOT to base or other
// battlefields. The action generator (generateMainPhaseActions) only EXPANDS
// play locations via positive substring matches ("play me to a battlefield",
// "play me to an open battlefield", etc.) and always emits a base-play intent
// (CR 355.2.a). Card::getPlayLocations() is defined but NEVER consulted by the
// engine. There is no hook to (a) forbid the default base play or (b) restrict
// plays to "battlefields you conquered this turn" (the engine does track
// __conquered_turn per unit, but not a per-battlefield/per-player conquered set
// the play generator consults).
// ESCALATE(restricted_play_location): the play-action generator must consult a
// per-card "legal play locations" hook that can NARROW (not just widen) the
// candidate set and suppress the default base play.

class PerchedGrimwyrm : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 338;
        d.def_id = R"RB(sfd-015-221)RB";
        d.name = R"RB(Perched Grimwyrm)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-015/221)RB";
        d.collector_number = 15;
        d.artist = R"RB(Kudos Productions & 黯荧岛Dark Glow)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Dragon)RB", R"RB(Shadow Isles)RB"};
        d.energy_cost = 4;
        d.might = 5;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(Play me only to a battlefield you conquered this turn. (You can't play me anywhere else.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/71af1587378c1c3feabd1b148fb776e42c7d27e8-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_338(CardRegistry& r) {
    r.registerCard(338, std::make_unique<PerchedGrimwyrm>());
}

} // namespace riftbound
