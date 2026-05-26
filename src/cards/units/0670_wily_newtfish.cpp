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

class WilyNewtfish : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "If you've gained XP this turn, I have +1 [M] and [Ganking]."
    // Continuous conditional self-buff. PlayerState::xp_gained_this_turn tracks
    // the per-turn XP gain (reset by resetTurnTracking). While that is > 0 for my
    // controller, grant myself a +1 [M] aura and the [Ganking] keyword. Auras
    // recalc each cleanup, so this turns off automatically next turn (when the
    // counter resets) or if it somehow drops to 0. [Ganking] is granted via the
    // aura (not a base keyword) so it is correctly absent when no XP was gained.
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        if (state.player(controller).xp_gained_this_turn <= 0) return;
        for (auto& [sid, self] : state.objects) {
            if (self.card_def_id != cardDefId() || self.controller != controller) continue;
            if (!self.location.has_value()) continue;  // must be on board
            GameObject::AuraEffect ae;
            ae.source = sid;
            ae.might_bonus = 1;
            ae.keyword = Keyword::Ganking;
            self.aura_effects.push_back(ae);
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 670;
        d.def_id = R"RB(unl-108-219)RB";
        d.name = R"RB(Wily Newtfish)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-108/219)RB";
        d.collector_number = 108;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Bilgewater)RB"};
        d.energy_cost = 4;
        d.might = 4;
        d.rarity = Rarity::Uncommon;
        // [Ganking] is conditional (only "if you've gained XP this turn"), so
        // it is NOT a base keyword — it is granted via applyPassiveAura below.
        d.ability_text = R"RB(If you've gained XP this turn, I have +1 [M] and [Ganking]. (I can move from battlefield to battlefield.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/4a0b0ebe9f47dadeb3b5f0520ec566a528df9c94-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_670(CardRegistry& r) {
    r.registerCard(670, std::make_unique<WilyNewtfish>());
}

} // namespace riftbound
