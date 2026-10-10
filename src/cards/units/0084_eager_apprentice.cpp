#include "cards/card.h"
#include "cards/card_helpers.h"
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

class EagerApprentice : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "While I'm at a battlefield, the Energy costs for spells you play is
    // reduced by [1], to a minimum of [1]." A continuous spell discount
    // (PlayerState::spell_discount_energy, reset every aura pass, floor of
    // [1] applied by the cost path); each Apprentice at a battlefield adds [1].
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        forEachSelfOnce(*this, state, controller, [&](GameObjectId, GameObject& self) {
            if (self.isAtBattlefield()) state.player(controller).spell_discount_energy += 1;
        });
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 84;
        d.def_id = R"RB(ogn-084-298)RB";
        d.name = R"RB(Eager Apprentice)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-084/298)RB";
        d.collector_number = 84;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Piltover)RB"};
        d.energy_cost = 3;
        d.might = 3;
        d.ability_text = R"RB(While I'm at a battlefield, the Energy costs for spells you play is reduced by [1], to a minimum of [1].)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/cf6d5447ff3634d8c2c0216cb2e5802fb5ca0b2e-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_84(CardRegistry& r) {
    r.registerCard(84, std::make_unique<EagerApprentice>());
}

} // namespace riftbound
