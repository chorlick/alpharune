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

class SimianAncestor : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 370;
        d.def_id = R"RB(sfd-047-221)RB";
        d.name = R"RB(Simian Ancestor)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-047/221)RB";
        d.collector_number = 47;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Ionia)RB"};
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 5;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you buff me, ready me.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/901a77a01e869f1f38f7a0b1ba5ae28f06b54a08-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_370(CardRegistry& r) {
    r.registerCard(370, std::make_unique<SimianAncestor>());
}

} // namespace riftbound
