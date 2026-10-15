#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include "cards/card_helpers.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class Unforgiven : public LegendCard {
public:
    const CardDef& def() const override { return def_; }
    // "[2], [E]: Move a friendly unit to or from its base." The unit is the
    // ability's target; where it moves is the move's destination (CR 355.4),
    // chosen as the ability is activated, before its cost (CR 403.2 ->
    // 405.1): a unit at a battlefield goes to its base (nothing to choose), a
    // unit in its base to a battlefield its controller chooses.
    unsigned choicesAtFinalization() const override { return kFinalizeActivate; }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        const GameObjectId u = targets.empty() ? kInvalidId : targets[0];
        int dest = -2;
        if (u != kInvalidId && ctx.state.objectExists(u) &&
            ctx.state.getObject(u).location.has_value()) {
            const auto& unit = ctx.state.getObject(u);
            // At a battlefield: to its base, nothing to choose (stored as the
            // answer, so it is not asked later). In base: a battlefield.
            if (!unit.isAtBase()) settleMode(ctx, 0);
            dest = pickMoveDest(*this, ctx, "Unforgiven: move it to which battlefield?", unit,
                                /*battlefields_only=*/true);
            if (dest == -1) return;   // waiting for the agent
        }
        if (finalizingChoices(ctx)) return;   // choices end here (CR 403.2)
        if (u == kInvalidId || !ctx.state.objectExists(u) || dest < 0) return;
        const auto& unit = ctx.state.getObject(u);
        if (!unit.isUnit() || !unit.location.has_value()) return;   // left the board
        if (unit.controller != ctx.controller) return;              // no longer friendly
        // Already there, or unable to go there now: it does not move.
        const auto now = moveDests(ctx.state, unit);
        if (std::find(now.begin(), now.end(), dest) == now.end()) return;
        moveTo(ctx, u, dest);
    }
    TriggerType triggerType() const override { return TriggerType::Activated; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true, .must_be_friendly = true};
    }
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override { return {.exhaust = true, .energy = 2}; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 256;
        d.def_id = R"RB(ogn-259-298)RB";
        d.name = R"RB(Unforgiven)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-259/298)RB";
        d.collector_number = 259;
        d.artist = R"RB(TSWCK逍)RB";
        d.card_type = CardType::Legend;
        d.domains = {Domain::Calm, Domain::Chaos};
        d.tags = {R"RB(Yasuo)RB"};
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB([2], [E]: Move a friendly unit to or from its base.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/68e4d3230b785738ae9d86f780f7f5607ef11807-744x1040.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_256(CardRegistry& r) {
    r.registerCard(256, std::make_unique<Unforgiven>());
}

} // namespace riftbound
