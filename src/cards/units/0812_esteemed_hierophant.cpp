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

class EsteemedHierophant : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "While you control 7 or more runes, prevent all damage that enemy spells
    //  and abilities would deal to me."
    // (GameObject::prevent_enemy_spell_ability_damage is reset every aura pass
    // and honoured by EffectExecutor::dealDamage for enemy sources only.)
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        if (runesControlled(state, controller) < 7) return;
        for (auto& [id, obj] : state.objects.onBoard()) {
            if (obj.card_def_id != cardDefId() || obj.controller != controller) continue;
            if (!obj.location.has_value()) continue;
            obj.prevent_enemy_spell_ability_damage = true;
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 812;
        d.def_id = R"RB(ven-025-166)RB";
        d.name = R"RB(Esteemed Hierophant)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-025/166)RB";
        d.collector_number = 25;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Shurima)RB"};
        d.energy_cost = 5;
        d.might = 5;
        d.rarity = Rarity::Common;
        d.ability_text = R"RB(While you control 7 or more runes, prevent all damage that enemy spells and abilities would deal to me.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/b47621d2b8efcee4d17de40b16c61fa06e404a2a-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_812(CardRegistry& r) {
    r.registerCard(812, std::make_unique<EsteemedHierophant>());
}

}  // namespace riftbound
