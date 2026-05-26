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

class VolibearImposing : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // [Shield 3] / [Tank] are engine-handled keywords.
    // "When an opponent moves to a battlefield other than mine, draw 1."
    // ENGINE LIMITATION: TriggerManager::onUnitMoved only fans
    // WhenAFriendlyUnitMovesToFB out to watchers controlled by the SAME player
    // as the mover. There is no trigger/dispatch that fires on the OPPONENT's
    // watchers when an enemy unit moves. Wiring an "enemy moved" watcher
    // trigger requires an engine edit (out of scope). The draw is left
    // unimplemented.
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 158;
        d.def_id = R"RB(ogn-158-298)RB";
        d.name = R"RB(Volibear, Imposing)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-158/298)RB";
        d.collector_number = 158;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Volibear)RB", R"RB(Freljord)RB"};
        d.energy_cost = 12;
        d.power_cost = 2;
        d.might = 10;
        d.rarity = Rarity::Rare;
        d.shield_value = 3;
        d.keywords.set(Keyword::Shield);
        d.keywords.set(Keyword::Tank);
        d.ability_text = R"RB([Shield 3] (+3 [M] while I'm a defender.)
[Tank] (I must be assigned combat damage first.)
When an opponent moves to a battlefield other than mine, draw 1. (Bases are not battlefield.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/bcb15f95f4a72f8b070a3b1cd54e6482fe1a4b3e-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_158(CardRegistry& r) {
    r.registerCard(158, std::make_unique<VolibearImposing>());
}

} // namespace riftbound
