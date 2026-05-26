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

class RengarTrophyHunter : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // Base "[Ambush]" is engine-handled.
    // "I can [Ambush] to a battlefield where there are enemy units, even if you
    // don't have units there." is an ENGINE GAP: GameEngine's Ambush action
    // generator hard-codes the placement to battlefields where the controller
    // already has units (bf.hasUnitsFrom(player)) with no per-card override
    // hook for extended placement. Left unimplemented (would require an engine
    // change, which is out of scope here).
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 682;
        d.def_id = R"RB(unl-120-219)RB";
        d.name = R"RB(Rengar, Trophy Hunter)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-120/219)RB";
        d.collector_number = 120;
        d.artist = R"RB(TSWCK逍)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Cat)RB", R"RB(Rengar)RB", R"RB(Ixtal)RB"};
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 6;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Ambush);
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Ambush] (You may play me as a [Reaction] to a battlefield where you have units.)
I can [Ambush] to a battlefield where there are enemy units, even if you don't have units there.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/8e159fd8aa9bb349e32bda6a10b0178cc28ca8dd-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_682(CardRegistry& r) {
    r.registerCard(682, std::make_unique<RengarTrophyHunter>());
}

} // namespace riftbound
