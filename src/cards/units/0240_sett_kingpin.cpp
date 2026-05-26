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

class SettKingpin : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "I get +1 [M] for each buffed friendly unit at my battlefield."
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        for (auto& [sid, self] : state.objects) {
            if (self.card_def_id != cardDefId() || self.controller != controller) continue;
            auto my_bf = self.battlefieldId();
            if (!my_bf) continue;
            int buffed = 0;
            for (auto& [oid, o] : state.objects) {
                if (oid == sid) continue;
                if (!o.isUnit() || o.controller != controller) continue;
                if (o.battlefieldId() != my_bf) continue;
                if (o.buff_count > 0) buffed++;
            }
            if (buffed > 0) {
                GameObject::AuraEffect ae;
                ae.source = sid;
                ae.might_bonus = buffed;
                self.aura_effects.push_back(ae);
            }
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 240;
        d.def_id = R"RB(ogn-240-298)RB";
        d.name = R"RB(Sett, Kingpin)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-240/298)RB";
        d.collector_number = 240;
        d.artist = R"RB(League Splash Team)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Sett)RB", R"RB(Ionia)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 5;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Tank);
        d.ability_text = R"RB([Tank] (I must be assigned combat damage first.)
I get +1 [M] for each buffed friendly unit at my battlefield.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/bd43f1dcef824b66f1af994b59b961fc3705f9c3-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_240(CardRegistry& r) {
    r.registerCard(240, std::make_unique<SettKingpin>());
}

} // namespace riftbound
