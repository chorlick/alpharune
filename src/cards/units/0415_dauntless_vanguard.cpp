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

// "You may play me to an occupied enemy battlefield."
// ENGINE-HANDLED: GameEngine::generateMainPhaseActions matches the substring
// "play me to an occupied enemy battlefield" in ability_text and emits
// PlayCard intents to every enemy-occupied battlefield. No per-card override
// is needed (getPlayLocations is never consulted by the engine).

class DauntlessVanguard : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 415;
        d.def_id = R"RB(sfd-093-221)RB";
        d.name = R"RB(Dauntless Vanguard)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-093/221)RB";
        d.collector_number = 93;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Elite)RB", R"RB(Demacia)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 4;
        d.ability_text = R"RB(You may play me to an occupied enemy battlefield.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/cb19a57b58370952cb37fff27375826d5c274129-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_415(CardRegistry& r) {
    r.registerCard(415, std::make_unique<DauntlessVanguard>());
}

} // namespace riftbound
