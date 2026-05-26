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

class MissFortuneBuccaneer : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "You may play me to an open battlefield." — ENGINE-HANDLED via the
    // "play me to an open battlefield" ability_text substring match in
    // GameEngine::generateMainPhaseActions.
    //
    // "Friendly units may be played to open battlefields." — board-wide grant.
    // Wired via applyPassiveAura: while Miss Fortune is on board, set the
    // controller's grant_friendly_units_open_bf flag (reset+recomputed each
    // cleanup). The play-from-hand generator ORs this into every friendly
    // unit's open-BF allowance.
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        state.player(controller).grant_friendly_units_open_bf = true;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 193;
        d.def_id = R"RB(ogn-193-298)RB";
        d.name = R"RB(Miss Fortune, Buccaneer)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-193/298)RB";
        d.collector_number = 193;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Pirate)RB", R"RB(Miss Fortune)RB", R"RB(Bilgewater)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 4;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(You may play me to an open battlefield.
Friendly units may be played to open battlefields.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/e6ff9ad3d6e2b96ff76cda4c7bc75415c1cdcced-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_193(CardRegistry& r) {
    r.registerCard(193, std::make_unique<MissFortuneBuccaneer>());
}

} // namespace riftbound
