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

class VanguardAttendant : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "I enter ready." Unconditional — override entersReadyOnPlay so the
    // engine skips the default "units enter exhausted" (CR 143.4) rule.
    bool entersReadyOnPlay() const override { return true; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 314;
        d.def_id = R"RB(ogs-016-024)RB";
        d.name = R"RB(Vanguard Attendant)RB";
        d.set_code = R"RB(OGS)RB";
        d.set_name = R"RB(Proving Grounds)RB";
        d.public_code = R"RB(OGS-016/024)RB";
        d.collector_number = 16;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Elite)RB", R"RB(Demacia)RB"};
        d.energy_cost = 6;
        d.power_cost = 1;
        d.might = 5;
        d.ability_text = R"RB(I enter ready.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/4ce467c1d51a65ab4fbae918dca38ae90b510844-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_314(CardRegistry& r) {
    r.registerCard(314, std::make_unique<VanguardAttendant>());
}

} // namespace riftbound
