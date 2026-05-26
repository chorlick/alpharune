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
// COVERAGE-OK: engine-handled: "play me to an occupied enemy battlefield" matched by generateMainPhaseActions

class DeadbloomPredator : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // [Deflect] is engine-handled.
    // "You may play me to an occupied enemy battlefield."
    // ENGINE-HANDLED: GameEngine::generateMainPhaseActions matches the substring
    // "play me to an occupied enemy battlefield" in ability_text (registry.json)
    // and emits PlayCard intents to enemy-occupied battlefields.
    // getPlayLocations() is never consulted, so no per-card override is needed.
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 161;
        d.def_id = R"RB(ogn-161-298)RB";
        d.name = R"RB(Deadbloom Predator)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-161/298)RB";
        d.collector_number = 161;
        d.artist = R"RB(Slawomir Maniak)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Shadow Isles)RB"};
        d.energy_cost = 8;
        d.power_cost = 2;
        d.might = 8;
        d.rarity = Rarity::Epic;
        d.deflect_value = 1;
        d.keywords.set(Keyword::Deflect);
        d.ability_text = R"RB([Deflect] (Opponents must pay [A] to choose me with a spell or ability.)
You may play me to an occupied enemy battlefield.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/112b4b3847b26ea2d245ac0976717d109be7b032-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_161(CardRegistry& r) {
    r.registerCard(161, std::make_unique<DeadbloomPredator>());
}

} // namespace riftbound
