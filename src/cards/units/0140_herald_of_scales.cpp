#include "cards/card.h"
#include "cards/card_helpers.h"
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

class HeraldOfScales : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "Your Dragons' Energy costs are reduced by [2], to a minimum of [1]."
    // One aura cost modifier per Herald on board, scoped to Dragon cards. The
    // engine rebuilds aura-sourced modifiers on every aura pass.
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        forEachSelfOnce(*this, state, controller, [&](GameObjectId sid, GameObject& self) {
            if (!self.location.has_value()) return;
            PlayerState::CostModifier m;
            m.source = sid;
            m.energy_reduction = 2;
            m.min_cost = 1;
            m.this_turn_only = false;
            m.affects_friendly_only = true;
            m.required_tag = "Dragon";
            state.player(controller).cost_modifiers.push_back(m);
        });
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 140;
        d.def_id = R"RB(ogn-140-298)RB";
        d.name = R"RB(Herald of Scales)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-140/298)RB";
        d.collector_number = 140;
        d.artist = R"RB(Polar Engine Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Mount Targon)RB"};
        d.energy_cost = 4;
        d.might = 3;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(Your Dragons' Energy costs are reduced by [2], to a minimum of [1].)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/be49bceea1d328769774fb4daac4732861f6e4fd-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_140(CardRegistry& r) {
    r.registerCard(140, std::make_unique<HeraldOfScales>());
}

} // namespace riftbound
