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

class Thwonk : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "[Repeat] [2]" (CR 820): the Repeat cost, as data the engine charges.
    std::vector<RepeatCost> repeatCosts() const override {
        return {RepeatCost{.energy = 2}};
    }
    // "Stun an attacking unit." The unit is asked for as the effect executes,
    // so a paid [Repeat] chooses again (CR 820.2.a) instead of re-stunning the
    // unit the first execution already stunned.
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};
    }
    bool needsPlayTimeTarget() const override { return true; }
    std::vector<GameObjectId> enumerateLegalTargets(
        const GameState& state, PlayerId controller) const override {
        std::vector<GameObjectId> out;
        for (auto id : Card::enumerateLegalTargets(state, controller))
            if (state.getObject(id).combat_designation == CombatDesignation::Attacker)
                out.push_back(id);
        std::sort(out.begin(), out.end());
        return out;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const GameObjectId picked =
            pickTarget(ctx, "Thwonk!: stun an attacking unit",
                       enumerateLegalTargets(ctx.state, ctx.controller));
        if (picked == kInvalidId) return;   // suspended, or no attacker to choose
        ctx.executor.stunUnitBy(picked, ctx.source);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 363;
        d.def_id = R"RB(sfd-040-221)RB";
        d.name = R"RB(Thwonk!)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-040/221)RB";
        d.collector_number = 40;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Calm};
        d.energy_cost = 2;
        d.keywords.set(Keyword::Action);
        d.keywords.set(Keyword::Repeat);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
[Repeat] [2] (You may pay the additional cost to repeat this spell's effect.)
Stun an attacking unit. (It doesn't deal combat damage this turn.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/c414fdba5e47db294b8364a07f1258bc3e204c51-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_363(CardRegistry& r) {
    r.registerCard(363, std::make_unique<Thwonk>());
}

} // namespace riftbound
