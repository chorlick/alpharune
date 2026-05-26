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

class RecruitZN : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 268;
        d.def_id = R"RB(ogn-273-298)RB";
        d.name = R"RB(Recruit (ZN))RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-273/298)RB";
        d.collector_number = 273;
        d.artist = R"RB(Fortiche Production)RB";
        d.card_type = CardType::Unit;
        d.tags = {R"RB(Recruit)RB"};
        d.might = 1;
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/d93ec9a524a2989b9d7ef23c6fc02e8ce39959c2-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_268(CardRegistry& r) {
    r.registerCard(268, std::make_unique<RecruitZN>());
}

} // namespace riftbound
