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

class ShurikenFlip : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // [vendetta-E]
    // "Deal 2 to up to one enemy unit at a battlefield, then move a friendly
    //  unit."
    // All three choices are made as it is played (CR 355.4, 355.5, 355.13;
    // testing phase D16): up to one enemy unit ("None" is a choice), the
    // friendly unit to move and where. It needs a friendly unit it can move
    // (CR 355.8; D17); the enemy may be none.
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    FinalizeChoice mayChooseAtFinalization(const GameState& state, PlayerId controller,
                                           GameObjectId object) const override {
        return friendlyUnitChoice(state, controller, object, [&](const GameObject& u) {
            return !moveDests(state, u).empty();
        });
    }
    static std::vector<GameObjectId> movable(const GameState& state, PlayerId controller) {
        return chooseableUnits(state, controller, [&](const GameObject& o) {
            return o.controller == controller && !moveDests(state, o).empty();
        });
    }
    bool hasLegalTargets(const GameState& state, PlayerId controller) const override {
        return !movable(state, controller).empty();
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const auto enemies = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& o) {
            return o.controller != ctx.controller && o.isAtBattlefield();
        });
        const auto enemy = selectTargetOrNone(*this, ctx,
            "Shuriken Flip: deal 2 to up to one enemy unit", enemies);
        if (!enemy.has_value()) return;          // waiting for the agent
        const GameObjectId mover = pickTarget(ctx, "Shuriken Flip: move a friendly unit",
                                              movable(ctx.state, ctx.controller));
        if (mover == kInvalidId && ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->resume_point == 7)
            return;                              // waiting for the agent
        int dest = -2;
        if (mover != kInvalidId && ctx.state.getObject(mover).location.has_value()) {
            dest = pickMoveDest(*this, ctx, "Shuriken Flip: move it where?",
                                ctx.state.getObject(mover));
            if (dest == -1) return;
        }
        if (finalizingChoices(ctx)) return;      // choices end here (CR 355)
        if (*enemy != kInvalidId && ctx.state.getObject(*enemy).location.has_value())
            ctx.executor.dealDamage(*enemy, 2, ctx.source);
        if (mover != kInvalidId && dest >= 0 && ctx.state.getObject(mover).location.has_value())
            moveTo(ctx, mover, dest);
    }
    // "[Flow] [3][A]"
    FlowCost flowCost() const override { return flow(3, 1, Domain::Fury, /*any_domain=*/true); }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 927;
        d.def_id = R"RB(ven-140-166)RB";
        d.name = R"RB(Shuriken Flip)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-140/166)RB";
        d.collector_number = 140;
        d.artist = R"RB(莺之歌)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Fury, Domain::Calm};
        d.tags = {R"RB(Akali)RB"};
        d.energy_cost = 1;
        d.power_cost = 1;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Flow);
        d.ability_text = R"RB(Deal 2 to up to one enemy unit at a battlefield, then move a friendly unit.
[Flow] [3][A] (You may play this from your trash for its Flow cost. Then banish it.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/bac1ecff92d022e99a3696ad00d020530a1a09e7-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_927(CardRegistry& r) {
    r.registerCard(927, std::make_unique<ShurikenFlip>());
}

}  // namespace riftbound
