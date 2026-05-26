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

class IreliaGraceful : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "Your spells that choose me cost [1] or [A] less."
    // ESCALATE(per-target-cost-modifier): PlayerState::CostModifier has no
    // target-conditional predicate ("only spells that choose THIS object"). The
    // available filters (friendly/enemy/gear/spell-scope) cannot express "spells
    // targeting me"; a blanket [1]-off on all friendly spells would over-reach
    // the printed scope. Needs a per-target cost-modifier hook consulted at the
    // moment a spell's chosen targets are known.
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 462;
        d.def_id = R"RB(sfd-141-221)RB";
        d.name = R"RB(Irelia, Graceful)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-141/221)RB";
        d.collector_number = 141;
        d.artist = R"RB(League Splash Team)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Irelia)RB", R"RB(Ionia)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 4;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(Your spells that choose me cost [1] or [A] less.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/03b9d73a7ef02447d7f4b67381a76616249061cc-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_462(CardRegistry& r) {
    r.registerCard(462, std::make_unique<IreliaGraceful>());
}

} // namespace riftbound
