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

class EagerApprentice : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        // Active only while an on-board instance is at a battlefield.
        bool at_bf = false;
        GameObjectId self_id = kInvalidId;
        for (auto& [sid, self] : state.objects) {
            if (self.card_def_id != cardDefId()) continue;
            if (self.controller != controller) continue;
            if (!self.isAtBattlefield()) continue;
            at_bf = true;
            self_id = sid;
            break;
        }
        if (!at_bf) return;
        PlayerState::CostModifier m;
        m.source = self_id;
        m.energy_reduction = 1;
        m.min_cost = 1;
        m.this_turn_only = false;
        m.affects_friendly_only = true;
        // NOTE: no spell-only filter exists on CostModifier; this reduces all
        // friendly plays by [1], a superset of the printed "spells" scope.
        state.player(controller).cost_modifiers.push_back(m);
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
