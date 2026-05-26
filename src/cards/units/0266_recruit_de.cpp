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

class RecruitDE : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 266;
        d.def_id = R"RB(ogn-271-298)RB";
        d.name = R"RB(Recruit (DE))RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-271/298)RB";
        d.collector_number = 271;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.tags = {R"RB(Recruit)RB"};
        d.might = 1;
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/7c3362dc6e6e6fb724ef31b061a44d2976f5b01f-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_266(CardRegistry& r) {
    r.registerCard(266, std::make_unique<RecruitDE>());
}

} // namespace riftbound
