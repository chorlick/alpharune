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

class MinotaurReckoner : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "Units can't move to base." A continuous effect on every unit (both
    // players) while I'm on the board: GameObject::aura_cant_move_to_base is
    // cleared at the start of every aura pass and re-asserted here, so it ends
    // when I leave. (cant_move_to_base is the lasting flag effects set.)
    // Both the standard-move generator and EffectExecutor::moveToBase refuse
    // ; a recall is not a move and still happens (CR 458.3).
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        bool present = false;
        for (auto& [id, obj] : state.objects) {
            if (obj.card_def_id != cardDefId()) continue;
            if (obj.controller != controller || !obj.location.has_value()) continue;
            present = true;
            break;
        }
        if (!present) return;
        for (auto& [id, obj] : state.objects) {
            if (!obj.isUnit() || !obj.location.has_value()) continue;
            obj.aura_cant_move_to_base = true;
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 337;
        d.def_id = R"RB(sfd-014-221)RB";
        d.name = R"RB(Minotaur Reckoner)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-014/221)RB";
        d.collector_number = 14;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Noxus)RB"};
        d.energy_cost = 5;
        d.might = 5;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(Units can't move to base.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/77bb46cd667e59f26310797ac99686f3a4d19af5-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_337(CardRegistry& r) {
    r.registerCard(337, std::make_unique<MinotaurReckoner>());
}

} // namespace riftbound
