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

class TrustyRamhound : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "While you have another unit here, I have +1 [M]." Conditional self-aura.
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        for (auto& [sid, self] : state.objects) {
            if (self.card_def_id != cardDefId() || self.controller != controller) continue;
            if (!self.location.has_value()) continue;
            // "here" — must be at a battlefield with another friendly unit.
            auto my_bf = self.battlefieldId();
            if (!my_bf) continue;
            bool another = false;
            for (auto& [oid, other] : state.objects) {
                if (oid == sid) continue;
                if (!other.isUnit() || other.controller != controller) continue;
                if (other.battlefieldId() == my_bf) { another = true; break; }
            }
            if (!another) continue;
            GameObject::AuraEffect ae;
            ae.source = sid;
            ae.might_bonus = 1;
            self.aura_effects.push_back(ae);
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 480;
        d.def_id = R"RB(sfd-159-221)RB";
        d.name = R"RB(Trusty Ramhound)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-159/221)RB";
        d.collector_number = 159;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Dog)RB", R"RB(Elite)RB", R"RB(Demacia)RB"};
        d.energy_cost = 2;
        d.might = 2;
        d.ability_text = R"RB(While you have another unit here, I have +1 [M].)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/0bd3f82ebe45a4dc09204582d06900916e6c0480-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_480(CardRegistry& r) {
    r.registerCard(480, std::make_unique<TrustyRamhound>());
}

} // namespace riftbound
