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

class PetalPixie : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "I have +1 [M] for each of your units with [Temporary] at my battlefield."
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        for (auto& [sid, self] : state.objects) {
            if (self.card_def_id != cardDefId() || self.controller != controller) continue;
            auto my_bf = self.battlefieldId();
            if (!my_bf) continue;
            int temp_count = 0;
            for (auto& [oid, o] : state.objects) {
                if (!o.isUnit() || o.controller != controller) continue;
                if (o.battlefieldId() != my_bf) continue;
                if (o.hasKeyword(Keyword::Temporary)) temp_count++;
            }
            if (temp_count > 0) {
                GameObject::AuraEffect ae;
                ae.source = sid;
                ae.might_bonus = temp_count;
                self.aura_effects.push_back(ae);
            }
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 638;
        d.def_id = R"RB(unl-076-219)RB";
        d.name = R"RB(Petal Pixie)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-076/219)RB";
        d.collector_number = 76;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Fae)RB", R"RB(Ionia)RB"};
        d.energy_cost = 2;
        d.might = 2;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Temporary);
        d.ability_text = R"RB(I have +1 [M] for each of your units with [Temporary] at my battlefield.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/68189fb5a6b3193e8925969ac0c545bc78b210c9-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_638(CardRegistry& r) {
    r.registerCard(638, std::make_unique<PetalPixie>());
}

} // namespace riftbound
