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

class YiMeditative : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "While you have 8+ runes, I have +4 [M]." Conditional self might aura.
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        int runes = 0;
        for (auto& [id, obj] : state.objects) {
            if (!obj.isRune() || obj.controller != controller) continue;
            if (!obj.location.has_value()) continue;
            ++runes;
        }
        if (runes < 8) return;
        for (auto& [id, obj] : state.objects) {
            if (obj.card_def_id != cardDefId()) continue;
            if (obj.controller != controller || !obj.location.has_value()) continue;
            GameObject::AuraEffect ae;
            ae.source = id;
            ae.might_bonus = 4;
            obj.aura_effects.push_back(ae);
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 302;
        d.def_id = R"RB(ogs-004-024)RB";
        d.name = R"RB(Yi, Meditative)RB";
        d.set_code = R"RB(OGS)RB";
        d.set_name = R"RB(Proving Grounds)RB";
        d.public_code = R"RB(OGS-004/024)RB";
        d.collector_number = 4;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Master Yi)RB", R"RB(Ionia)RB"};
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 4;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(While you have 8+ runes, I have +4 [M].)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/5508566c8f05f09492148faa803332a731095eb7-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_302(CardRegistry& r) {
    r.registerCard(302, std::make_unique<YiMeditative>());
}

} // namespace riftbound
