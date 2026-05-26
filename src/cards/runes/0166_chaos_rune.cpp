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

class ChaosRune : public RuneCard {
public:
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 166;
        d.def_id = R"RB(ogn-166-298)RB";
        d.name = R"RB(Chaos Rune)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-166/298)RB";
        d.collector_number = 166;
        d.artist = R"RB(Greg Ghielmetti & Leah Chen)RB";
        d.card_type = CardType::Rune;
        d.domains = {Domain::Chaos};
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/daf23b0deaa5e1a5a5d310b59e9ad25d1bd70363-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_166(CardRegistry& r) {
    r.registerCard(166, std::make_unique<ChaosRune>());
}

} // namespace riftbound
