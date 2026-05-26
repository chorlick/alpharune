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

// "Your Mechs have [Shield]." The engine's generic aura text-matcher only
// catches "your mechs EACH have", not "your mechs have", so grant Shield to
// friendly Mech units here.

class MechanizedMenace : public LegendCard {
public:
    const CardDef& def() const override { return def_; }
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        for (auto& [id, obj] : state.objects) {
            if (!obj.isUnit() || obj.controller != controller) continue;
            if (!obj.location.has_value()) continue;
            if (!hasTag(obj, "Mech")) continue;
            GameObject::AuraEffect ae;
            ae.source = id;
            ae.keyword = Keyword::Shield;
            obj.aura_effects.push_back(ae);
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 551;
        d.def_id = R"RB(sfd-240-221)RB";
        d.name = R"RB(Mechanized Menace)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-240/221)RB";
        d.collector_number = 240;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Legend;
        d.domains = {Domain::Fury, Domain::Mind};
        d.tags = {R"RB(Rumble)RB"};
        d.rarity = Rarity::Showcase;
        d.shield_value = 1;
        d.keywords.set(Keyword::Shield);
        d.ability_text = R"RB(Your Mechs have [Shield]. (+1 [M] while they're defenders.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/0234cad3663d52d3120e78f2adbe8a253102a7a3-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_551(CardRegistry& r) {
    r.registerCard(551, std::make_unique<MechanizedMenace>());
}

} // namespace riftbound
