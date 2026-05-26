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

class YiHoned : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 307;
        d.def_id = R"RB(ogs-009-024)RB";
        d.name = R"RB(Yi, Honed)RB";
        d.set_code = R"RB(OGS)RB";
        d.set_name = R"RB(Proving Grounds)RB";
        d.public_code = R"RB(OGS-009/024)RB";
        d.collector_number = 9;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Master Yi)RB", R"RB(Ionia)RB"};
        d.energy_cost = 7;
        d.power_cost = 1;
        d.might = 6;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Ganking);
        d.ability_text = R"RB([Ganking] (I can move from battlefield to battlefield.)
I enter ready.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/0e16976cd6d7ee5a874be9351b428671990fbd25-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_307(CardRegistry& r) {
    r.registerCard(307, std::make_unique<YiHoned>());
}

} // namespace riftbound
