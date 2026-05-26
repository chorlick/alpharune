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

class TargonianVisionary : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    bool requiresLevel() const override { return true; }
    int levelThreshold() const override { return 11; }
    // "[Level 11] I have +4 [M]." Level hooks are not consumed by the engine,
    // so implement the tier inline by checking controller XP during aura recalc.
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        if (state.player(controller).xp < 11) return;
        for (auto& [id, obj] : state.objects) {
            if (obj.card_def_id != cardDefId() || obj.controller != controller) continue;
            if (!obj.location.has_value()) continue;
            GameObject::AuraEffect ae;
            ae.source = id;
            ae.might_bonus = 4;
            obj.aura_effects.push_back(ae);
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 660;
        d.def_id = R"RB(unl-098-219)RB";
        d.name = R"RB(Targonian Visionary)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-098/219)RB";
        d.collector_number = 98;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Mount Targon)RB"};
        d.energy_cost = 6;
        d.might = 6;
        d.keywords.set(Keyword::Level);
        d.ability_text = R"RB([Level 11][>] I have +4 [M]. (While you have 11+ XP, get the effect.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/fe645cecdd1f251218f5bf33225cde6c1e12b67e-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_660(CardRegistry& r) {
    r.registerCard(660, std::make_unique<TargonianVisionary>());
}

} // namespace riftbound
