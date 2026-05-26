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

class BlueSentinel : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIHold; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        // "When I hold, [Add] [A] ..." — approximated as immediate add (the
        // "at the start of your next Main Phase" deferral has no scheduling
        // primitive; see class comment).
        ctx.executor.addFloatingUniversalPower(ctx.controller, 1);
        ctx.events.logTrace("BLUE SENTINEL: hold -> [Add] [A] "
                            "(deferral to next main phase approximated as now)");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 649;
        d.def_id = R"RB(unl-087-219)RB";
        d.name = R"RB(Blue Sentinel)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-087/219)RB";
        d.collector_number = 87;
        d.artist = R"RB(Grafit Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Mount Targon)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 4;
        d.rarity = Rarity::Epic;
        d.shield_value = 2;
        d.keywords.set(Keyword::Shield);
        d.ability_text = R"RB([Shield 2] (+2 [M] while I'm a defender.)
Your hold effects for holding here trigger an additional time.
When I hold, [Add] [A] at the start of your next Main Phase. (Abilities that add resources can't be reacted to.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/bec7e8f108fac2d9c94db01c85cf143133d13325-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_649(CardRegistry& r) {
    r.registerCard(649, std::make_unique<BlueSentinel>());
}

} // namespace riftbound
