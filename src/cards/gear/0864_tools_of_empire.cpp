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

class ToolsOfEmpire : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    // [vendetta-E]
    // "[Empower] [2]"
    EmpowerSpec empowerSpec() const override { return empowerCost(2); }
    // "[E]: Give a unit +2 [M] this turn. If this is [Empowered], give that
    //  unit +4 [M] this turn instead." (ability 0; Empower is appended as 1)
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        ActivationCost c;
        c.exhaust = true;
        return c;
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (targets.empty() || !ctx.state.objectExists(targets[0])) return;
        const auto& unit = ctx.state.getObject(targets[0]);
        if (!unit.isUnit() || !unit.location.has_value()) return;   // left the board
        const bool empowered = ctx.state.objectExists(ctx.source) &&
                               ctx.state.getObject(ctx.source).isEmpowered();
        ctx.executor.giveTemporaryMight(targets[0], empowered ? 4 : 2);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 864;
        d.def_id = R"RB(ven-077-166)RB";
        d.name = R"RB(Tools of Empire)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-077/166)RB";
        d.collector_number = 77;
        d.artist = R"RB(Wild Blue Studios)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Body};
        d.energy_cost = 4;
        d.rarity = Rarity::Common;
        d.keywords.set(Keyword::Empower);
        d.ability_text = R"RB([Empower] [2] ([2]: Empower this. Use only if not Empowered.)
[E]: Give a unit +2 [M] this turn. If this is [Empowered], give that unit +4 [M] this turn instead.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/c4f511ca650143e3f5267e272513e4dcf9cba04f-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_864(CardRegistry& r) {
    r.registerCard(864, std::make_unique<ToolsOfEmpire>());
}

}  // namespace riftbound
