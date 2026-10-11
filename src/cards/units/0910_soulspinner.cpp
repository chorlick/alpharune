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

class Soulspinner : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 910;
        d.def_id = R"RB(ven-123-166)RB";
        d.name = R"RB(Soulspinner)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-123/166)RB";
        d.collector_number = 123;
        d.artist = R"RB(Wild Blue Studios)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Shadow Isles)RB", R"RB(Spider)RB"};
        d.energy_cost = 3;
        d.might = 3;
        d.rarity = Rarity::Common;
        d.keywords.set(Keyword::Ambush);
        d.ability_text = R"RB([Ambush] (You may play me as a [Reaction] to a battlefield where you have units.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/9a23a1f2c83751c2a715049be32c2482ea10454d-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_910(CardRegistry& r) {
    r.registerCard(910, std::make_unique<Soulspinner>());
}

}  // namespace riftbound
