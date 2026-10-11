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

class JaggedCutlass : public SimpleEquipGear {
public:
    const CardDef& def() const override { return def_; }
    JaggedCutlass() : SimpleEquipGear(Domain::Body) {}
    // [vendetta-E]
    // "Equip [O]" - SimpleEquipGear(Body); the +2 [M] comes from might_bonus.
    // Equipped: "I can't be moved by enemy spells and abilities."
    // (GameObject::cant_be_moved_by_enemy is reset every aura pass and
    // honoured by EffectExecutor::moveToBase / moveToBattlefield.)
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        // (Every object: finds its own instances by card id, wherever they are.)
        for (auto& [gid, gear] : state.objects) {
            if (gear.card_def_id != cardDefId() || gear.controller != controller) continue;
            if (!gear.attached_to.has_value() || !state.objectExists(*gear.attached_to)) continue;
            state.getObject(*gear.attached_to).cant_be_moved_by_enemy = true;
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 860;
        d.def_id = R"RB(ven-073-166)RB";
        d.name = R"RB(Jagged Cutlass)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-073/166)RB";
        d.collector_number = 73;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Equipment)RB"};
        d.energy_cost = 3;
        d.might_bonus = 2;
        d.rarity = Rarity::Common;
        d.keywords.set(Keyword::Equip);
        d.ability_text = R"RB(Equip [O] ([O]: Attach this to a unit you control.))RB";
        d.effect_text = R"RB(I can't be moved by enemy spells and abilities.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/22aae23e1ac1797beb50f3f735becfe65b51ee02-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_860(CardRegistry& r) {
    r.registerCard(860, std::make_unique<JaggedCutlass>());
}

}  // namespace riftbound
