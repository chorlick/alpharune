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

class LeonaZealot : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "If an opponent's score is within 3 points of the Victory Score, I
    // enter ready." (Stun -8M aura handled by the generic aura parser.)
    bool entersReadyOnPlay(const GameState& state,
                           PlayerId controller) const override {
        int vs = state.mode.victory_score;
        return state.player(opponent(controller)).score >= vs - 3;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 79;
        d.def_id = R"RB(ogn-079-298)RB";
        d.name = R"RB(Leona, Zealot)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-079/298)RB";
        d.collector_number = 79;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Leona)RB", R"RB(Mount Targon)RB"};
        d.energy_cost = 6;
        d.power_cost = 1;
        d.might = 6;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(If an opponent's score is within 3 points of the Victory Score, I enter ready.
Stunned enemy units here have -8 [M], to a minimum of 1 [M].)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/3bd1e924bec41b0f733fc8b93dc8918ce5a53ba4-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_79(CardRegistry& r) {
    r.registerCard(79, std::make_unique<LeonaZealot>());
}

} // namespace riftbound
