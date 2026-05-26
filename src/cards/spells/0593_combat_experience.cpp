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

class CombatExperience : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};
    }
    bool needsPlayTimeTarget() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        GameObjectId picked = kInvalidId;
        if (!targets.empty()) {
            picked = targets[0];
        } else {
            auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
            picked = pickTarget(ctx, "Combat Experience", legal);
        }
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        int bonus = (ctx.state.player(ctx.controller).xp >= 6) ? 3 : 1;
        ctx.executor.giveTemporaryMight(picked, bonus);
        ctx.events.logTrace("COMBAT EXPERIENCE: +" + std::to_string(bonus) +
                             "[M] this turn");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 593;
        d.def_id = R"RB(unl-031-219)RB";
        d.name = R"RB(Combat Experience)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-031/219)RB";
        d.collector_number = 31;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Calm};
        d.energy_cost = 1;
        d.keywords.set(Keyword::Level);
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Give a unit +1 [M] this turn.
[Level 6][>] Give it +3 [M] this turn instead. (While you have 6+ XP, get the effect.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/537b66b5f0259f80bf25b1aafb78558f4db6886a-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_593(CardRegistry& r) {
    r.registerCard(593, std::make_unique<CombatExperience>());
}

} // namespace riftbound
