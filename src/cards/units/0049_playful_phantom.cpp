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

class PlayfulPhantom : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 49;
        d.def_id = R"RB(ogn-049-298)RB";
        d.name = R"RB(Playful Phantom)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-049/298)RB";
        d.collector_number = 49;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Spirit)RB", R"RB(Shadow Isles)RB"};
        d.energy_cost = 5;
        d.might = 5;
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/3171ef8c7968b0dfe088725b9721b19d175bdb1e-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_49(CardRegistry& r) {
    r.registerCard(49, std::make_unique<PlayfulPhantom>());
}

} // namespace riftbound
