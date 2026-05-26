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

// "If you've spent [4] or more to play a spell this turn, I have +4 [M]."

class PreparedNeophyte : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        // last_spell_energy_spent holds the most-recently-played spell's spend
        // this turn (reset at end of turn). Approximation: any single spell of
        // cost [4]+ this turn satisfies the gate.
        if (state.player(controller).last_spell_energy_spent < 4) return;
        for (auto& [sid, self] : state.objects) {
            if (self.card_def_id != cardDefId()) continue;
            if (self.controller != controller || !self.location.has_value()) continue;
            GameObject::AuraEffect ae;
            ae.source = sid;
            ae.might_bonus = 4;
            self.aura_effects.push_back(ae);
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 563;
        d.def_id = R"RB(unl-004-219)RB";
        d.name = R"RB(Prepared Neophyte)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-004/219)RB";
        d.collector_number = 4;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Shadow Isles)RB"};
        d.energy_cost = 3;
        d.might = 1;
        d.ability_text = R"RB(If you've spent [4] or more to play a spell this turn, I have +4 [M].)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ad993e451437950e56935a7ecff44fea09e522b2-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_563(CardRegistry& r) {
    r.registerCard(563, std::make_unique<PreparedNeophyte>());
}

} // namespace riftbound
