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

class SunlitGuardian : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 54;
        d.def_id = R"RB(ogn-054-298)RB";
        d.name = R"RB(Sunlit Guardian)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-054/298)RB";
        d.collector_number = 54;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Shurima)RB"};
        d.energy_cost = 3;
        d.might = 3;
        d.shield_value = 1;
        d.keywords.set(Keyword::Shield);
        d.keywords.set(Keyword::Tank);
        d.ability_text = R"RB([Shield] (+1 [M] while I'm a defender.)
[Tank] (I must be assigned combat damage first.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/28bce7a662b9008f65565300f828d98790a641e1-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_54(CardRegistry& r) {
    r.registerCard(54, std::make_unique<SunlitGuardian>());
}

} // namespace riftbound
