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

class ExistentialDread : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                   .must_be_enemy = true,
                                   .must_be_at_battlefield = true};
    }
    // Phase 6q — defer target selection so the policy head gets
    // distinct vocab slots per target choice.
    bool needsPlayTimeTarget() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
        GameObjectId picked = pickTarget(ctx, "Existential Dread", legal);
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        auto& tgt = ctx.state.getObject(picked);
        if (tgt.combat_designation != CombatDesignation::Attacker) return;
        if (tgt.is_stunned) {
            ctx.executor.bounceToHand(picked);
        } else {
            ctx.executor.stunUnit(picked);
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 696;
        d.def_id = R"RB(unl-134-219)RB";
        d.name = R"RB(Existential Dread)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-134/219)RB";
        d.collector_number = 134;
        d.artist = R"RB(Caravan Studio)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Chaos};
        d.energy_cost = 1;
        d.power_cost = 1;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Action);
        d.keywords.set(Keyword::Repeat);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
[Repeat] [2] (You may pay the additional cost to repeat this spell's effect.)
[Stun] an attacking enemy unit. If it's already stunned, return it to its owner's hand instead. (A stunned unit doesn't deal combat damage this turn.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/0a197ec3bf96e10307859e2a152c1742288b2b09-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_696(CardRegistry& r) {
    r.registerCard(696, std::make_unique<ExistentialDread>());
}

} // namespace riftbound
