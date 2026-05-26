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

class HeimerdingerInventor : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "I have all [E] abilities of all friendly legends, units, and gear."
    // ESCALATE(proxy-granted-activated-abilities): the action generator and
    // onActivate dispatch key activated abilities off each object's OWN Card (via
    // ctx.source). There is no mechanism to PROXY another object's activated
    // ability onto Heimerdinger — that needs engine-level ability-granting
    // (enumerate + dispatch another card's [E] ability as though it were mine).
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 111;
        d.def_id = R"RB(ogn-111-298)RB";
        d.name = R"RB(Heimerdinger, Inventor)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-111/298)RB";
        d.collector_number = 111;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Yordle)RB", R"RB(Heimerdinger)RB", R"RB(Piltover)RB"};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.might = 3;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(I have all [E] abilities of all friendly legends, units, and gear.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/5b14a5f9d567c90329c151a8cc72d870b47b1434-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_111(CardRegistry& r) {
    r.registerCard(111, std::make_unique<HeimerdingerInventor>());
}

} // namespace riftbound
