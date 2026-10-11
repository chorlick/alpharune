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

class SandstoneChimera : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "While I'm at a battlefield, players only channel 1 rune at the start of
    //  their Channel Phase." TurnState::channel_phase_cap is reset every aura
    // pass and re-asserted here.
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        for (const auto& [id, obj] : state.objects.onBoard()) {
            if (obj.card_def_id != cardDefId() || obj.controller != controller) continue;
            if (!obj.isUnit() || !obj.isAtBattlefield()) continue;
            state.turn.channel_phase_cap = 1;
            return;
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 823;
        d.def_id = R"RB(ven-036-166)RB";
        d.name = R"RB(Sandstone Chimera)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-036/166)RB";
        d.collector_number = 36;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Shurima)RB"};
        d.energy_cost = 7;
        d.power_cost = 2;
        d.might = 8;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(While I'm at a battlefield, players only channel 1 rune at the start of their Channel Phase.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/7a483e91e2bbb81bc8f66697bd610427b080b45c-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_823(CardRegistry& r) {
    r.registerCard(823, std::make_unique<SandstoneChimera>());
}

}  // namespace riftbound
