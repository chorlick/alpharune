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

class MagmaWurm : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "Other friendly units enter ready."
    // ENGINE GAP: the engine consults only the ENTERING card's own
    // entersReadyOnPlay() (resolvePermanent), so a passive that makes OTHER
    // units enter ready cannot be expressed per-card — it needs an engine-side
    // check (e.g. "any friendly Magma Wurm in play?") at unit-resolution time.
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 11;
        d.def_id = R"RB(ogn-011-298)RB";
        d.name = R"RB(Magma Wurm)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-011/298)RB";
        d.collector_number = 11;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Freljord)RB"};
        d.energy_cost = 8;
        d.power_cost = 1;
        d.might = 8;
        d.ability_text = R"RB(Other friendly units enter ready.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/f6565b15f65f538804e6a56623c8aa2eedeffc22-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_11(CardRegistry& r) {
    r.registerCard(11, std::make_unique<MagmaWurm>());
}

} // namespace riftbound
