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
        d.id = 569;
        d.def_id = R"RB(unl-t07)RB";
        d.name = R"RB(Sprite)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-T07)RB";
        d.collector_number = 7;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Unit;
        d.tags = {R"RB(Fae)RB"};
        d.might = 3;
        d.keywords.set(Keyword::Temporary);
        d.ability_text = R"RB([Temporary] (Kill me at the start of your Beginning Phase, before scoring.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/1a90578f055e01515ebfb069dc3dbdba08d24da0-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_569(CardRegistry& r) {
    r.registerCard(569, std::make_unique<Sprite>());
}

} // namespace riftbound
