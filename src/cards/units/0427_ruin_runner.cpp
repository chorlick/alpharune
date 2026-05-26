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

class RuinRunner : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    bool canBeChosenByEnemy() const override { return false; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 427;
        d.def_id = R"RB(sfd-105-221)RB";
        d.name = R"RB(Ruin Runner)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-105/221)RB";
        d.collector_number = 105;
        d.artist = R"RB(Grafit Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Shurima)RB"};
        d.energy_cost = 6;
        d.might = 5;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(I can't be chosen by enemy spells and abilities.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/08abcf17d3356325b0c1b83a78a87be855e5a71a-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_427(CardRegistry& r) {
    r.registerCard(427, std::make_unique<RuinRunner>());
}

} // namespace riftbound
