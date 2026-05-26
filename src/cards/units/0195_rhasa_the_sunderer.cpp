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

class RhasaTheSunderer : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "I cost [1] less for each card in your trash."
    int selfCostReduction(const GameState& state, PlayerId player) const override {
        return static_cast<int>(state.player(player).trash.size());
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 195;
        d.def_id = R"RB(ogn-195-298)RB";
        d.name = R"RB(Rhasa the Sunderer)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-195/298)RB";
        d.collector_number = 195;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Spirit)RB", R"RB(Shadow Isles)RB"};
        d.energy_cost = 10;
        d.power_cost = 1;
        d.might = 6;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(I cost [1] less for each card in your trash.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/e34fe8a9ee533dee99f96b4e6677d1edbd6a262d-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_195(CardRegistry& r) {
    r.registerCard(195, std::make_unique<RhasaTheSunderer>());
}

} // namespace riftbound
