#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include "cards/card_helpers.h"

namespace riftbound {
namespace {

class JaxUnmatched : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "[Deflect]" — engine-handled. "Your Equipment everywhere have
    // [Quick-Draw]." Grant QuickDraw to friendly Equipment in any zone.
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        GameObjectId self_id = kInvalidId;
        for (auto& [id, obj] : state.objects) {
            if (obj.card_def_id != cardDefId()) continue;
            if (obj.controller != controller || !obj.location.has_value()) continue;
            self_id = id;
            break;
        }
        if (self_id == kInvalidId) return;
        for (auto& [gid, g] : state.objects) {
            if (g.owner != controller) continue;
            if (!isEquipment(g)) continue;  // gear with "Equipment" tag
            GameObject::AuraEffect ae;
            ae.source = self_id;
            ae.keyword = Keyword::QuickDraw;
            g.aura_effects.push_back(ae);
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 377;
        d.def_id = R"RB(sfd-054-221)RB";
        d.name = R"RB(Jax, Unmatched)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-054/221)RB";
        d.collector_number = 54;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Jax)RB", R"RB(Icathia)RB"};
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 5;
        d.rarity = Rarity::Rare;
        d.deflect_value = 1;
        d.keywords.set(Keyword::Deflect);
        d.keywords.set(Keyword::QuickDraw);
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Deflect] (Opponents must pay [A] to choose me with a spell or ability.)
Your Equipment everywhere have [Quick-Draw]. (Each gains [Reaction]. When you play it, attach it to a unit you control.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/3d425589c74dbff818fa09db134fdd8bb7136420-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_377(CardRegistry& r) {
    r.registerCard(377, std::make_unique<JaxUnmatched>());
}

} // namespace riftbound
