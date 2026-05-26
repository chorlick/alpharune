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

class CrimsonPigeons : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "I have +2 [M] while I'm attacking with another unit." Continuous
    // conditional → recompute each cleanup: if this Crimson Pigeons is an
    // attacker and at least one OTHER friendly unit is also attacking, grant
    // a +2 [M] aura to self.
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        for (auto& [sid, self] : state.objects) {
            if (self.card_def_id != cardDefId() || self.controller != controller) continue;
            if (self.combat_designation != CombatDesignation::Attacker) continue;
            bool another_attacker = false;
            for (auto& [oid, other] : state.objects) {
                if (oid == sid) continue;
                if (!other.isUnit() || other.controller != controller) continue;
                if (other.combat_designation == CombatDesignation::Attacker) {
                    another_attacker = true;
                    break;
                }
            }
            if (!another_attacker) continue;
            GameObject::AuraEffect ae;
            ae.source = sid;
            ae.might_bonus = 2;
            self.aura_effects.push_back(ae);  // engine sums into aura_might_bonus
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 716;
        d.def_id = R"RB(unl-154-219)RB";
        d.name = R"RB(Crimson Pigeons)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-154/219)RB";
        d.collector_number = 154;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Bird)RB", R"RB(Noxus)RB"};
        d.energy_cost = 3;
        d.might = 3;
        d.ability_text = R"RB(I have +2 [M] while I'm attacking with another unit.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/68786cd39a886a586b401d6bf818a80bf9f9e2cd-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_716(CardRegistry& r) {
    r.registerCard(716, std::make_unique<CrimsonPigeons>());
}

} // namespace riftbound
