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

class ForbiddingWaste : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    void applyPassiveAura(GameState& state, PlayerId /*controller*/) const override {
        // Locate the BattlefieldState whose card object is THIS card.
        for (const auto& bf : state.battlefields) {
            if (!state.objectExists(bf.card_object_id)) continue;
            if (state.getObject(bf.card_object_id).card_def_id != cardDefId()) continue;

            // For each unit at this battlefield that is defending: check if
            // it's alone (no other friendly units here). If so, -2 M.
            for (auto& [uid, u] : state.objects) {
                if (!u.isUnit()) continue;
                auto ubf = u.battlefieldId();
                if (!ubf || *ubf != bf.id) continue;
                if (u.combat_designation != CombatDesignation::Defender) continue;
                int friendly_here = 0;
                for (auto& [oid, o] : state.objects) {
                    if (!o.isUnit() || o.controller != u.controller) continue;
                    auto obf = o.battlefieldId();
                    if (obf && *obf == bf.id) friendly_here++;
                }
                if (friendly_here == 1) {  // alone
                    GameObject::AuraEffect ae;
                    ae.source = bf.card_object_id;
                    ae.might_bonus = -2;
                    u.aura_effects.push_back(ae);
                }
            }
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 766;
        d.def_id = R"RB(unl-210-219)RB";
        d.name = R"RB(Forbidding Waste)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-210/219)RB";
        d.collector_number = 210;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(While a unit here is defending alone, it has -2 [M]. (It's alone if there are no other friendly units here.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/83dbc88d462da85be9398c790e88ff13da8637d4-1039x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_766(CardRegistry& r) {
    r.registerCard(766, std::make_unique<ForbiddingWaste>());
}

} // namespace riftbound
