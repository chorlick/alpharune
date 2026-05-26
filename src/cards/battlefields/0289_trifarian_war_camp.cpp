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

class TrifarianWarCamp : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    // "Units here have +1 [M]. (This includes attackers.)" — static aura.
    void applyPassiveAura(GameState& state, PlayerId /*controller*/) const override {
        for (const auto& bf : state.battlefields) {
            if (!state.objectExists(bf.card_object_id)) continue;
            if (state.getObject(bf.card_object_id).card_def_id != cardDefId()) continue;
            for (auto& [uid, u] : state.objects) {
                if (!u.isUnit()) continue;
                auto ubf = u.battlefieldId();
                if (!ubf || *ubf != bf.id) continue;
                GameObject::AuraEffect ae;
                ae.source = bf.card_object_id;
                ae.might_bonus = 1;
                u.aura_effects.push_back(ae);
            }
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 289;
        d.def_id = R"RB(ogn-294-298)RB";
        d.name = R"RB(Trifarian War Camp)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-294/298)RB";
        d.collector_number = 294;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(Units here have +1 [M]. (This includes attackers.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/3788cf718e716e35a7fa20ec1dc56991644e6484-1038x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_289(CardRegistry& r) {
    r.registerCard(289, std::make_unique<TrifarianWarCamp>());
}

} // namespace riftbound
