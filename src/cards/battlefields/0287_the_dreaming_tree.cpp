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

class TheDreamingTree : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    // ENGINE GAP: "When a player chooses a friendly unit here with a spell for
    // the first time each turn, they draw 1." The WhenYouChooseAFriendlyUnit
    // trigger is dispatched only to the choosing player's ON-BOARD cards (the
    // dispatcher requires obj.location and never iterates battlefield cards),
    // and it carries no "unit is HERE" location filter. Wiring this requires an
    // engine-side battlefield-scoped choose trigger (+ once-per-turn gating).
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 287;
        d.def_id = R"RB(ogn-292-298)RB";
        d.name = R"RB(The Dreaming Tree)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-292/298)RB";
        d.collector_number = 292;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When a player chooses a friendly unit here with a spell for the first time each turn, they draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/f382c2c2442959f2a8905de2be7d202458918a04-1040x744.png)RB";
        d.banned = true;  // tournament ban (formerly cards/ban-list.csv)
        return d;
    }();
};

}  // anonymous namespace

void register_card_287(CardRegistry& r) {
    r.registerCard(287, std::make_unique<TheDreamingTree>());
}

} // namespace riftbound
