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

class Sprite : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 269;
        d.def_id = R"RB(ogn-274-298)RB";
        d.name = R"RB(Sprite)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-274/298)RB";
        d.collector_number = 274;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Unit;
        d.tags = {R"RB(Fae)RB"};
        d.might = 3;
        d.keywords.set(Keyword::Temporary);
        d.ability_text = R"RB([Temporary] (Kill me at the start of your Beginning Phase, before scoring.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/055892752559d2d3d32e76f491a7a0b540e1a669-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_269(CardRegistry& r) {
    r.registerCard(269, std::make_unique<Sprite>());
}

} // namespace riftbound
