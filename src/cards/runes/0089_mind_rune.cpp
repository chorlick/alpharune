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

class MindRune : public RuneCard {
public:
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 89;
        d.def_id = R"RB(ogn-089-298)RB";
        d.name = R"RB(Mind Rune)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-089/298)RB";
        d.collector_number = 89;
        d.artist = R"RB(Greg Ghielmetti & Leah Chen)RB";
        d.card_type = CardType::Rune;
        d.domains = {Domain::Mind};
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/f99aa4874baaebd2e81798c8a3aa01c5900f6d30-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_89(CardRegistry& r) {
    r.registerCard(89, std::make_unique<MindRune>());
}

} // namespace riftbound
