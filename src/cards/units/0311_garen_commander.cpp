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

class GarenCommander : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "Other friendly units have +1 [M] here." Static buff to every OTHER
    // friendly unit at the same battlefield as me.
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        for (auto& [sid, self] : state.objects) {
            if (self.card_def_id != cardDefId() || self.controller != controller)
                continue;
            auto my_bf = self.battlefieldId();
            if (!my_bf) continue;  // "here" — only while I'm at a battlefield
            for (auto& [uid, u] : state.objects) {
                if (uid == sid) continue;  // "other"
                if (!u.isUnit() || u.controller != controller) continue;
                if (u.battlefieldId() != my_bf) continue;  // same battlefield
                GameObject::AuraEffect ae;
                ae.source = sid;
                ae.might_bonus = 1;
                u.aura_effects.push_back(ae);
            }
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 311;
        d.def_id = R"RB(ogs-013-024)RB";
        d.name = R"RB(Garen, Commander)RB";
        d.set_code = R"RB(OGS)RB";
        d.set_name = R"RB(Proving Grounds)RB";
        d.public_code = R"RB(OGS-013/024)RB";
        d.collector_number = 13;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Elite)RB", R"RB(Garen)RB", R"RB(Demacia)RB"};
        d.energy_cost = 6;
        d.power_cost = 1;
        d.might = 5;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(Other friendly units have +1 [M] here.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/cbf2c12d69a86566e4cda07050b2d4495e40187e-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_311(CardRegistry& r) {
    r.registerCard(311, std::make_unique<GarenCommander>());
}

} // namespace riftbound
