#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include "cards/card_helpers.h"

namespace riftbound {
namespace {

class RocketBarrage : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    TargetRequirements getTargetRequirements() const override {
        // Permissive target — any object. The pickMode helper filters
        // modes based on the target's type.
        return TargetRequirements{.count = 1};
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (targets.empty()) return;
        if (!ctx.state.objectExists(targets[0])) return;
        auto& target = ctx.state.getObject(targets[0]);
        // Mode 0 = Deal 4 to base unit; Mode 1 = Kill gear.
        uint32_t legal = 0;
        if (target.isUnit() && target.isAtBase()) legal |= (1u << 0);
        if (target.isGear()) legal |= (1u << 1);

        int mode = pickMode(ctx, "Rocket Barrage", 2,
                            {"Deal 4 to base unit", "Kill gear"}, legal);
        if (mode < 0) return;  // -1 pending, -2 no legal mode

        switch (mode) {
            case 0:
                ctx.events.logTrace("ROCKET BARRAGE: deal 4 to " + target.name);
                ctx.executor.dealDamage(targets[0], 4, ctx.source);
                break;
            case 1:
                ctx.events.logTrace("ROCKET BARRAGE: kill gear " + target.name);
                ctx.executor.killObject(targets[0]);
                break;
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 400;
        d.def_id = R"RB(sfd-077-221)RB";
        d.name = R"RB(Rocket Barrage)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-077/221)RB";
        d.collector_number = 77;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Mind};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Repeat);
        d.ability_text = R"RB([Repeat] [4][B] (You may pay the additional cost to repeat this spell's effect, and may make different choices.)
Choose one —
Deal 4 to a unit in a base.Kill a gear.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/0378f9adf9df08fed264fdd217ce0e94c3f611cb-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_400(CardRegistry& r) {
    r.registerCard(400, std::make_unique<RocketBarrage>());
}

} // namespace riftbound
