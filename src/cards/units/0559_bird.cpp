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

class Bird : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 559;
        d.def_id = R"RB(unl-t02)RB";
        d.name = R"RB(Bird)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-T02)RB";
        d.collector_number = 2;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.tags = {R"RB(Bird)RB"};
        d.might = 1;
        d.deflect_value = 1;
        d.keywords.set(Keyword::Deflect);
        d.ability_text = R"RB([Deflect] (Opponents must pay [A] to choose me with a spell or ability.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/949a2e43263a9fe0d957595325c7e2ebe06bf85f-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_559(CardRegistry& r) {
    r.registerCard(559, std::make_unique<Bird>());
}

} // namespace riftbound
