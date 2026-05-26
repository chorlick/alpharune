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

class VoidGate : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    // "Spells and abilities deal 1 Bonus Damage to units here." Pushes a
    // bonus_damage_taken aura onto units at this battlefield; dealDamage adds it
    // to spell/ability damage.
    void applyPassiveAura(GameState& state, PlayerId /*controller*/) const override {
        for (auto& bf : state.battlefields) {
            if (!state.objectExists(bf.card_object_id)) continue;
            if (state.getObject(bf.card_object_id).card_def_id != cardDefId()) continue;
            for (auto& [id, obj] : state.objects) {
                if (!obj.isUnit() || !obj.location.has_value()) continue;
                if (!std::holds_alternative<BattlefieldLocation>(*obj.location)) continue;
                if (std::get<BattlefieldLocation>(*obj.location).id != bf.id) continue;
                GameObject::AuraEffect ae;
                ae.source = bf.card_object_id;
                ae.bonus_damage_taken = 1;
                obj.aura_effects.push_back(ae);
            }
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 291;
        d.def_id = R"RB(ogn-296-298)RB";
        d.name = R"RB(Void Gate)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-296/298)RB";
        d.collector_number = 296;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(Spells and abilities deal 1 Bonus Damage to units here. (Each instance of damage the spell deals to a unit here is increased by 1.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/14a52a367fd41fd84745e050e62d1f281f733467-1038x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_291(CardRegistry& r) {
    r.registerCard(291, std::make_unique<VoidGate>());
}

} // namespace riftbound
