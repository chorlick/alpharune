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

class WielderOfWater : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "While I'm attacking or defending alone, I have +2 [M]." I'm "alone"
    // when I'm the only friendly unit sharing my combat designation
    // (Attacker / Defender) at my battlefield.
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        for (auto& [sid, self] : state.objects) {
            if (self.card_def_id != cardDefId() || self.controller != controller)
                continue;
            auto my_bf = self.battlefieldId();
            if (!my_bf) continue;
            CombatDesignation mine = self.combat_designation;
            if (mine != CombatDesignation::Attacker &&
                mine != CombatDesignation::Defender)
                continue;  // not currently attacking or defending
            bool alone = true;
            for (auto& [oid, o] : state.objects) {
                if (oid == sid) continue;
                if (!o.isUnit() || o.controller != controller) continue;
                if (o.battlefieldId() != my_bf) continue;
                if (o.combat_designation == mine) { alone = false; break; }
            }
            if (!alone) continue;
            GameObject::AuraEffect ae;
            ae.source = sid;
            ae.might_bonus = 2;
            self.aura_effects.push_back(ae);
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 55;
        d.def_id = R"RB(ogn-055-298)RB";
        d.name = R"RB(Wielder of Water)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-055/298)RB";
        d.collector_number = 55;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Ionia)RB"};
        d.energy_cost = 3;
        d.might = 2;
        d.ability_text = R"RB(While I'm attacking or defending alone, I have +2 [M].)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/5b38a7758705bd739caf07b5c0c49482a4a23015-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_55(CardRegistry& r) {
    r.registerCard(55, std::make_unique<WielderOfWater>());
}

} // namespace riftbound
