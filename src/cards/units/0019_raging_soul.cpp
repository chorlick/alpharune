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

class RagingSoul : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "If you've discarded a card this turn, I have [Assault] and [Ganking]."
    //  Conditional static ability: grant the Assault + Ganking keywords (and
    //  the +1 [M]-while-attacking that Assault confers) only while the
    //  controller has discarded a card this turn. Auras recalc each cleanup,
    //  so this tracks the live discard flag and drops off at end of turn.
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        if (!state.player(controller).has_discarded_this_turn) return;
        for (auto& [sid, self] : state.objects) {
            if (self.card_def_id != cardDefId() || self.controller != controller) continue;
            if (!self.location.has_value()) continue;
            // Ganking (movement keyword — presence only).
            {
                GameObject::AuraEffect ae;
                ae.source = sid;
                ae.keyword = Keyword::Ganking;
                self.aura_effects.push_back(ae);
            }
            // Assault: presence + the +1 [M] bonus while I'm an attacker.
            {
                GameObject::AuraEffect ae;
                ae.source = sid;
                ae.keyword = Keyword::Assault;
                ae.keyword_value = 1;
                if (self.combat_designation == CombatDesignation::Attacker)
                    ae.might_bonus = 1;
                self.aura_effects.push_back(ae);
            }
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 19;
        d.def_id = R"RB(ogn-019-298)RB";
        d.name = R"RB(Raging Soul)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-019/298)RB";
        d.collector_number = 19;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Spirit)RB", R"RB(Shadow Isles)RB"};
        d.energy_cost = 4;
        d.might = 4;
        d.rarity = Rarity::Uncommon;
        // [Assault] and [Ganking] are CONDITIONAL ("if you've discarded a card
        // this turn") — granted at runtime via applyPassiveAura, NOT printed
        // base keywords. assault_value stays 0 so the bonus only applies under
        // the condition (handled by the aura's might_bonus while attacking).
        d.ability_text = R"RB(If you've discarded a card this turn, I have [Assault] and [Ganking]. (+1 [M] while I'm an attacker. I can move from battlefield to battlefield.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/037647d0decc94ff4a5d53b11cf36afe9d849533-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_19(CardRegistry& r) {
    r.registerCard(19, std::make_unique<RagingSoul>());
}

} // namespace riftbound
