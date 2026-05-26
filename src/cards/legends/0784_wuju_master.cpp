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
    // "[Level 11] Your units enter ready."  (inline xp>=11 gate — see note)
    //
    // NOTE: this matches the Purifier (#502) legend-aura pattern, BUT
    // GameEngine::recalculateAuras only calls applyPassiveAura on objects with
    // a board `location` (Step 1b skips LegendZone objects). Legends have no
    // location, so this hook does not currently fire for legends — the Level-6
    // buff will not take effect until the engine extends the aura pass to
    // legend-zone cards. Code is left in place so it activates for free once
    // that engine gap is closed.
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
        // [Level 11]: "Your units enter ready."
        // ENGINE GAP: whether a unit enters ready is decided per-Card via
        // Card::entersReadyOnPlay (consulted in GameEngine::resolvePermanent).
        // There is no per-player "your units enter ready" flag a legend can
        // raise, so this tier can't be granted from the legend's file alone.
        // Left unimplemented (xp>=11 gate would go here once a player-level
        // enter-ready hook exists).
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
