#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include "cards/card_helpers.h"

namespace riftbound {
namespace {
// COVERAGE-OK: engine-handled: "play me to an open battlefield" matched by generateMainPhaseActions

// "You may play me to an open battlefield."
// ENGINE-HANDLED: GameEngine::generateMainPhaseActions matches the substring
// "play me to an open battlefield" in ability_text (registry.json) and emits
// PlayCard intents to every open battlefield. getPlayLocations() is never
// consulted by the engine, so no per-card override is needed.

class SneakyDeckhand : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 176;
        d.def_id = R"RB(ogn-176-298)RB";
        d.name = R"RB(Sneaky Deckhand)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-176/298)RB";
        d.collector_number = 176;
        d.artist = R"RB(Dao Trong Le)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Pirate)RB", R"RB(Bilgewater)RB"};
        d.energy_cost = 3;
        d.might = 2;
        d.ability_text = R"RB(You may play me to an open battlefield.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/6f95b6aac293d8e477192281df47d8466da502bf-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_176(CardRegistry& r) {
    r.registerCard(176, std::make_unique<SneakyDeckhand>());
}

} // namespace riftbound
