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

class WujuMaster : public LegendCard {
public:
    const CardDef& def() const override { return def_; }
    // "[Level 6] Your units have +1 [M]."   (inline xp>=6 gate)
    // "[Level 11] Your units enter ready."  (inline xp>=11 gate)
    //
    // Legends broadcast from the Legend Zone: recalculateAuras calls this hook
    // for LegendZone objects as well as on-board ones.
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        // Confirm an on-board (LegendZone) instance of this legend.
        bool present = false;
        GameObjectId self_id = kInvalidId;
        for (auto& [sid, self] : state.objects) {
            if (self.card_def_id != cardDefId() || self.controller != controller) continue;
            present = true; self_id = sid; break;
        }
        if (!present) return;
        int xp = state.player(controller).xp;
        // [Level 6]: +1 [M] to all friendly on-board units.
        if (xp >= 6) {
            for (auto& [uid, u] : state.objects) {
                if (!u.isUnit() || u.controller != controller) continue;
                if (!u.location.has_value()) continue;
                GameObject::AuraEffect ae;
                ae.source = self_id;
                ae.might_bonus = 1;
                u.aura_effects.push_back(ae);
            }
        }
        // [Level 11]: "Your units enter ready." The legend is a source every
        // unit differs from, so each unit this player plays enters ready
        // (PlayerState::otherUnitsEnterReady, read by resolvePermanent,
        // createToken and playIgnoringCost).
        if (xp >= 11) state.player(controller).units_enter_ready_by.push_back(self_id);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 784;
        d.def_id = R"RB(unl-231-219)RB";
        d.name = R"RB(Wuju Master)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-231/219)RB";
        d.collector_number = 231;
        d.artist = R"RB(Shawn Lee)RB";
        d.card_type = CardType::Legend;
        d.domains = {Domain::Calm, Domain::Body};
        d.tags = {R"RB(Master Yi)RB"};
        d.rarity = Rarity::Showcase;
        d.keywords.set(Keyword::Level);
        d.ability_text = R"RB([Level 6][>] Your units have +1 [M].
[Level 11][>] Your units enter ready.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/557e41d84ac36ffa2bf805deda159f45e0a815f9-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_784(CardRegistry& r) {
    r.registerCard(784, std::make_unique<WujuMaster>());
}

} // namespace riftbound
