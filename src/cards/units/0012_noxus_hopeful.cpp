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

class NoxusHopeful : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    int selfCostReduction(const GameState& state, PlayerId player) const override {
        return state.player(player).cards_played_this_turn >= 1 ? 2 : 0;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 12;
        d.def_id = R"RB(ogn-012-298)RB";
        d.name = R"RB(Noxus Hopeful)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-012/298)RB";
        d.collector_number = 12;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Trifarian)RB", R"RB(Noxus)RB"};
        d.energy_cost = 4;
        d.might = 4;
        d.keywords.set(Keyword::Legion);
        d.ability_text = R"RB([Legion] — I cost [2] less. (Get the effect if you've played another card this turn.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/c3bb6f4cb58feeb50e396d12ec9865c5434025af-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_12(CardRegistry& r) {
    r.registerCard(12, std::make_unique<NoxusHopeful>());
}

} // namespace riftbound
