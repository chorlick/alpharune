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

class AltarOfBlood : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    // "If a unit here would die during combat, its controller may pay [A][A][A]
    //  to heal it, exhaust it, and recall it instead."
    // ENGINE GAP: the structured replacement-effect dispatch (GameEngine::
    // killUnit) only consults Card::hasReplacementEffect on objects with
    // `controller == dying-unit's controller` AND a board `location`.
    // Battlefield cards have no `location` (and shared BFs have no single
    // controller), so they are never offered to applyReplacement. There is
    // also no "during combat" gate nor a [A][A][A]-payment prompt path at the
    // replacement site. Wiring this requires engine changes (iterate BF cards
    // in the replacement loop + combat-phase gate + cost prompt), out of scope
    // for per-card edits. Left unimplemented.
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 762;
        d.def_id = R"RB(unl-206-219)RB";
        d.name = R"RB(Altar of Blood)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-206/219)RB";
        d.collector_number = 206;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(If a unit here would die during combat, its controller may pay [A][A][A] to heal it, exhaust it, and recall it instead.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/a17e5bb639866671772dc514a6096a5863712f71-1039x744.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_762(CardRegistry& r) {
    r.registerCard(762, std::make_unique<AltarOfBlood>());
}

} // namespace riftbound
