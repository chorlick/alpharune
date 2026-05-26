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

class BodyRune : public RuneCard {
public:
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 126;
        d.def_id = R"RB(ogn-126-298)RB";
        d.name = R"RB(Body Rune)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-126/298)RB";
        d.collector_number = 126;
        d.artist = R"RB(Greg Ghielmetti & Leah Chen)RB";
        d.card_type = CardType::Rune;
        d.domains = {Domain::Body};
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/3b3c3c07626d6180457c849047e0228dc0d19539-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_126(CardRegistry& r) {
    r.registerCard(126, std::make_unique<BodyRune>());
}

} // namespace riftbound
