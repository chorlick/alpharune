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

class OrderRune : public RuneCard {
public:
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 214;
        d.def_id = R"RB(ogn-214-298)RB";
        d.name = R"RB(Order Rune)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-214/298)RB";
        d.collector_number = 214;
        d.artist = R"RB(Greg Ghielmetti & Leah Chen)RB";
        d.card_type = CardType::Rune;
        d.domains = {Domain::Order};
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/35ec6fdd2124324bb7052cba31c8c44f2e98f3ae-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_214(CardRegistry& r) {
    r.registerCard(214, std::make_unique<OrderRune>());
}

} // namespace riftbound
