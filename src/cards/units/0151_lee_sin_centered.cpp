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

class LeeSinCentered : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // [Accelerate] is engine-handled.
    // "Other buffed friendly units at my battlefield have +2 [M]."
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        // Find each on-board copy of me and apply to other buffed friendlies
        // sharing my battlefield.
        for (auto& [sid, self] : state.objects) {
            if (self.card_def_id != cardDefId()) continue;
            if (self.controller != controller || !self.location.has_value()) continue;
            auto my_bf = self.battlefieldId();
            if (!my_bf) continue;  // must be at a battlefield
            for (auto& [uid, u] : state.objects) {
                if (uid == sid) continue;                       // "other"
                if (!u.isUnit() || u.controller != controller) continue;
                if (u.buff_count <= 0) continue;                // "buffed"
                if (u.battlefieldId() != my_bf) continue;       // "at my battlefield"
                GameObject::AuraEffect ae;
                ae.source = sid;
                ae.might_bonus = 2;
                u.aura_effects.push_back(ae);
            }
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 151;
        d.def_id = R"RB(ogn-151-298)RB";
        d.name = R"RB(Lee Sin, Centered)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-151/298)RB";
        d.collector_number = 151;
        d.artist = R"RB(League Splash Team)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Lee Sin)RB", R"RB(Ionia)RB"};
        d.energy_cost = 6;
        d.might = 6;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Accelerate);
        d.ability_text = R"RB([Accelerate] (You may pay [1][O] as an additional cost to have me enter ready.)
Other buffed friendly units at my battlefield have +2 [M].)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/eca1aec1304de9c237751eb0aeed620b9ad0408e-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_151(CardRegistry& r) {
    r.registerCard(151, std::make_unique<LeeSinCentered>());
}

} // namespace riftbound
