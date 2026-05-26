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

class FuryRune : public RuneCard {
public:
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 7;
        d.def_id = R"RB(ogn-007-298)RB";
        d.name = R"RB(Fury Rune)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-007/298)RB";
        d.collector_number = 7;
        d.artist = R"RB(Greg Ghielmetti & Leah Chen)RB";
        d.card_type = CardType::Rune;
        d.domains = {Domain::Fury};
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/12bcd0cde5d9ff4640e82945001e9fef863530f1-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_7(CardRegistry& r) {
    r.registerCard(7, std::make_unique<FuryRune>());
}

} // namespace riftbound
