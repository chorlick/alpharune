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

class SkywardStrike : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                   .must_be_enemy = true};
    }
    bool needsPlayTimeTarget() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        GameObjectId picked = kInvalidId;
        if (!targets.empty()) {
            picked = targets[0];
        } else {
            auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
            picked = pickTarget(ctx, "Skyward Strike", legal);
        }
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        if (ctx.state.player(ctx.controller).xp >= 6) {
            ctx.executor.stunUnitBy(picked, ctx.source);
            ctx.events.logTrace("SKYWARD STRIKE: Level 6 -> stun");
        } else {
            ctx.executor.moveToBase(picked);
            ctx.events.logTrace("SKYWARD STRIKE: move to base");
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 600;
        d.def_id = R"RB(unl-038-219)RB";
        d.name = R"RB(Skyward Strike)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-038/219)RB";
        d.collector_number = 38;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Calm};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.keywords.set(Keyword::Level);
        d.ability_text = R"RB(Move an enemy unit.
[Level 6][>] [Stun] an enemy unit. (While you have 6+ XP, get the effect. A stunned unit doesn't deal combat damage this turn.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/998740e590fec024f204ad26ed7e2dad2a0c8d66-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_600(CardRegistry& r) {
    r.registerCard(600, std::make_unique<SkywardStrike>());
}

} // namespace riftbound
