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

class PakaaCub : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 135;
        d.def_id = R"RB(ogn-135-298)RB";
        d.name = R"RB(Pakaa Cub)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-135/298)RB";
        d.collector_number = 135;
        d.artist = R"RB(Bubble Cat Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Cat)RB", R"RB(Ixtal)RB"};
        d.energy_cost = 3;
        d.might = 3;
        d.keywords.set(Keyword::Hidden);
        d.ability_text = R"RB([Hidden] (Hide now for [A] to react with later for .))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/156a66d7d44165367cc5a470fb35c86f337f9429-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_135(CardRegistry& r) {
    r.registerCard(135, std::make_unique<PakaaCub>());
}

} // namespace riftbound
