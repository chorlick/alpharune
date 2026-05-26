#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include "cards/card_helpers.h"

namespace riftbound {
namespace {

class VexCheerless : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        PlayerState::CostModifier friendly_mod;
        friendly_mod.energy_reduction = 1;
        friendly_mod.min_cost = 1;            // "to a minimum of [1]"
        friendly_mod.this_turn_only = false;
        friendly_mod.combat_active_only = true;
        friendly_mod.affects_friendly_only = true;
        state.player(controller).cost_modifiers.push_back(friendly_mod);

        PlayerState::CostModifier enemy_mod;
        enemy_mod.energy_increase = 1;
        enemy_mod.this_turn_only = false;
        enemy_mod.combat_active_only = true;
        enemy_mod.affects_enemy_only = true;
        state.player(controller).cost_modifiers.push_back(enemy_mod);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 467;
        d.def_id = R"RB(sfd-146-221)RB";
        d.name = R"RB(Vex, Cheerless)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-146/221)RB";
        d.collector_number = 146;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Yordle)RB", R"RB(Vex)RB", R"RB(Shadow Isles)RB"};
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 5;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(While I'm in combat, friendly spells cost [1][A] less to a minimum of [1], and enemy spells cost [1][A] more.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/e9025578c7b5845a1a9c9a83e045bcbe71a76e71-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_467(CardRegistry& r) {
    r.registerCard(467, std::make_unique<VexCheerless>());
}

} // namespace riftbound
