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

class SoulShepherd : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "Your token units have +1 [M]." Called once per on-board Soul Shepherd
    // instance during aura recalc, so two copies correctly stack to +2.
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        for (auto& [id, obj] : state.objects) {
            if (!obj.isUnit() || obj.controller != controller) continue;
            if (!obj.isToken() || !obj.location.has_value()) continue;
            GameObject::AuraEffect ae;
            ae.might_bonus = 1;
            obj.aura_effects.push_back(ae);
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 639;
        d.def_id = R"RB(unl-077-219)RB";
        d.name = R"RB(Soul Shepherd)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-077/219)RB";
        d.collector_number = 77;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Shadow Isles)RB"};
        d.energy_cost = 5;
        d.might = 3;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(Your token units have +1 [M].)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/cb2a157e715ef103e688e094c21f772003775e45-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_639(CardRegistry& r) {
    r.registerCard(639, std::make_unique<SoulShepherd>());
}

} // namespace riftbound
