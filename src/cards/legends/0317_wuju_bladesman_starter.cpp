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

class WujuBladesmanStarter : public LegendCard {
public:
    const CardDef& def() const override { return def_; }
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        // Confirm an on-board instance of this legend controlled by `controller`.
        GameObjectId self_id = kInvalidId;
        for (auto& [sid, self] : state.objects) {
            if (self.card_def_id != cardDefId()) continue;
            if (self.controller != controller) continue;
            self_id = sid;
            break;
        }
        if (self_id == kInvalidId) return;
        // Collect this controller's defenders.
        std::vector<GameObjectId> defenders;
        for (auto& [uid, u] : state.objects) {
            if (!u.isUnit() || u.controller != controller) continue;
            if (u.combat_designation != CombatDesignation::Defender) continue;
            defenders.push_back(uid);
        }
        if (defenders.size() != 1) return;  // "defends alone"
        auto& lone = state.getObject(defenders.front());
        GameObject::AuraEffect ae;
        ae.source = self_id;
        ae.might_bonus = 2;
        lone.aura_effects.push_back(ae);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 317;
        d.def_id = R"RB(ogs-019-024)RB";
        d.name = R"RB(Wuju Bladesman - Starter)RB";
        d.set_code = R"RB(OGS)RB";
        d.set_name = R"RB(Proving Grounds)RB";
        d.public_code = R"RB(OGS-019/024)RB";
        d.collector_number = 19;
        d.artist = R"RB(Grafit Studio/Quy Ho)RB";
        d.card_type = CardType::Legend;
        d.domains = {Domain::Calm, Domain::Body};
        d.tags = {R"RB(Master Yi)RB"};
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(While a friendly unit defends alone, it gets +2 [M].)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/8231ced23eaf22ca3bf62ec8cb86b83a3e222da6-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_317(CardRegistry& r) {
    r.registerCard(317, std::make_unique<WujuBladesmanStarter>());
}

} // namespace riftbound
