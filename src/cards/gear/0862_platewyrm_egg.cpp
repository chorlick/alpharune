#include "cards/card.h"
#include "cards/card_helpers.h"
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

class PlatewyrmEgg : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    // [vendetta-E]
    // "This enters exhausted."
    bool entersExhausted() const override { return true; }
    // "[Empower] - [1], [E]"
    EmpowerSpec empowerSpec() const override {
        EmpowerSpec s = empowerCost(1);
        s.cost.exhaust = true;
        return s;
    }
    // "[Reaction][>] [E]: [Add] [1]. If this is [Empowered], [Add] [2]
    //  instead." (ability 0; Empower is appended as 1)
    bool hasActivatedAbility() const override { return true; }
    // [Add]: it resolves as it is activated — no chain item, no priority,
    // Focus stays (CR 430.2, 401.2; testing phase D2).
    bool abilityAddsResources() const override { return true; }
    bool isReactionAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        ActivationCost c;
        c.exhaust = true;
        return c;
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const bool empowered = ctx.state.objectExists(ctx.source) &&
                               ctx.state.getObject(ctx.source).isEmpowered();
        ctx.executor.addFloatingEnergy(ctx.controller, empowered ? 2 : 1);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 862;
        d.def_id = R"RB(ven-075-166)RB";
        d.name = R"RB(Platewyrm Egg)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-075/166)RB";
        d.collector_number = 75;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Body};
        d.energy_cost = 3;
        d.rarity = Rarity::Common;
        d.keywords.set(Keyword::Empower);
        d.ability_text = R"RB(This enters exhausted.
[Empower] — [1], [E] (Pay the cost: Empower this. Use only if not Empowered.)
[Reaction][>] [E]: [Add] [1]. If this is [Empowered], [Add] [2] instead.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/f6a3940440a38e34fe306947d663018dd2df7ca8-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_862(CardRegistry& r) {
    r.registerCard(862, std::make_unique<PlatewyrmEgg>());
}

}  // namespace riftbound
