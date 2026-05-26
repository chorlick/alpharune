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

class Forecaster : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "Your Mechs have [Vision]." The engine's aura text-matcher only handles
    // "your mechs EACH have", not "your mechs have", so grant Vision here.
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        GameObjectId self_id = kInvalidId;
        for (auto& [id, obj] : state.objects) {
            if (obj.card_def_id != cardDefId()) continue;
            if (obj.controller != controller || !obj.location.has_value()) continue;
            self_id = id;
            break;
        }
        if (self_id == kInvalidId) return;
        for (auto& [uid, u] : state.objects) {
            if (!u.isUnit() || u.controller != controller || !u.location.has_value())
                continue;
            bool is_mech = false;
            for (auto& tag : u.tags) if (tag == "Mech") { is_mech = true; break; }
            if (!is_mech) continue;
            GameObject::AuraEffect ae;
            ae.source = self_id;
            ae.keyword = Keyword::Vision;
            u.aura_effects.push_back(ae);
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 388;
        d.def_id = R"RB(sfd-065-221)RB";
        d.name = R"RB(Forecaster)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-065/221)RB";
        d.collector_number = 65;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Mech)RB", R"RB(Yordle)RB", R"RB(Bandle City)RB"};
        d.energy_cost = 2;
        d.might = 2;
        d.keywords.set(Keyword::Vision);
        d.ability_text = R"RB(Your Mechs have [Vision]. (When you play us, look at the top card of your Main Deck. You may recycle it.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/3f58f0c58503e4f8caecb7ea7f77b478a1e92961-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_388(CardRegistry& r) {
    r.registerCard(388, std::make_unique<Forecaster>());
}

} // namespace riftbound
