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

class GalioIndefatigable : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "I don't deal combat damage." Continuous self-property → suppress
    // combat damage via a self aura recomputed each cleanup.
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        for (auto& [sid, self] : state.objects) {
            if (self.card_def_id != cardDefId() || self.controller != controller) continue;
            if (!self.location.has_value()) continue;
            GameObject::AuraEffect ae;
            ae.source = sid;
            ae.suppress_combat_damage = true;
            self.aura_effects.push_back(ae);
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 733;
        d.def_id = R"RB(unl-171-219)RB";
        d.name = R"RB(Galio, Indefatigable)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-171/219)RB";
        d.collector_number = 171;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Demacia)RB", R"RB(Galio)RB"};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.might = 6;
        d.rarity = Rarity::Rare;
        d.deflect_value = 1;
        d.keywords.set(Keyword::Deflect);
        d.keywords.set(Keyword::Tank);
        d.ability_text = R"RB([Deflect] (Opponents must pay [A] to choose me with a spell or ability.)
[Tank] (I must be assigned combat damage first.)
I don't deal combat damage.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/fc3f1248dc4a02ce90ce172c3b3d5ec35e5f14c3-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_733(CardRegistry& r) {
    r.registerCard(733, std::make_unique<GalioIndefatigable>());
}

} // namespace riftbound
