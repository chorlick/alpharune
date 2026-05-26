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

class KarthusEternal : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        // Each on-board Karthus adds one extra Deathknell fire. Two
        // Karthus on board → 1 base + 2 extras = 3 total fires per
        // Deathknell death event.
        state.player(controller).deathknell_double_count++;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 236;
        d.def_id = R"RB(ogn-236-298)RB";
        d.name = R"RB(Karthus, Eternal)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-236/298)RB";
        d.collector_number = 236;
        d.artist = R"RB(League Splash Team)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Spirit)RB", R"RB(Karthus)RB", R"RB(Shadow Isles)RB"};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.might = 3;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Deathknell);
        d.ability_text = R"RB(Your [Deathknell] effects trigger an additional time.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/df3497ce6a602da554813340f572240675c7f0e2-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_236(CardRegistry& r) {
    r.registerCard(236, std::make_unique<KarthusEternal>());
}

} // namespace riftbound
