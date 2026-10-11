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

class BlastCone : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    // Two triggered abilities:
    //   "When you play this, you may move an enemy unit."
    //   "When you move an enemy unit, you may exhaust this to [Stun] it."
    // The second is its own ability: it triggers on every enemy unit a spell
    // or ability of yours moves (this card's own play effect included), goes
    // on the chain and asks its own "may".
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayThis, TriggerType::WhenYouMoveAnEnemyUnit};
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (ctx.firing_trigger == TriggerType::WhenYouMoveAnEnemyUnit) stunMoved(ctx);
        else moveOnPlay(ctx);
    }
private:
    // "you may move an enemy unit": the controller chooses whether, which
    // unit, and where to — its base or a battlefield it is not at.
    void moveOnPlay(CardContext& ctx) {
        auto enemies = [&]() {
            auto out = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& o) {
                return o.controller != ctx.controller;
            });
            std::sort(out.begin(), out.end());
            return out;
        };
        const int go = confirmOptional(ctx, "Blast Cone: move an enemy unit?",
                                       [&]() { return !enemies().empty(); });
        GameObjectId unit = kInvalidId;
        int dest = -1;
        if (go == 1) {
            unit = pickTarget(ctx, "Blast Cone: move which enemy unit?", enemies());
            if (unit != kInvalidId && ctx.state.objectExists(unit)) {
                // Modes are moveDests codes: 0 = its base, 1 + i = battlefield i.
                const int num_modes = 1 + static_cast<int>(ctx.state.battlefields.size());
                std::vector<std::string> labels;
                for (int d = 0; d < num_modes; ++d) labels.push_back(destLabel(d));
                uint32_t legal = 0;
                for (int d : moveDests(ctx.state, ctx.state.getObject(unit)))
                    if (d < 32) legal |= 1u << d;
                dest = pickMode(ctx, "Blast Cone: move it where?", num_modes, labels, legal);
            }
        }
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (go != 1 || unit == kInvalidId || dest < 0) return;
        if (!ctx.state.objectExists(unit) || !ctx.state.getObject(unit).location.has_value())
            return;                           // left the board
        moveTo(ctx, unit, dest);
        ctx.events.logTrace("BLAST CONE: moved enemy on play");
    }
    // "you may exhaust this to [Stun] it": "it" is the unit that was moved,
    // the trigger's subject.
    void stunMoved(CardContext& ctx) {
        const GameObjectId moved = ctx.state.chain.resuming.has_value()
            ? ctx.state.chain.resuming->triggering_subject : kInvalidId;
        auto on_board_enemy = [&]() {
            if (moved == kInvalidId || !ctx.state.objectExists(moved)) return false;
            const auto& u = ctx.state.getObject(moved);
            return u.isUnit() && u.location.has_value() && u.controller != ctx.controller;
        };
        auto still_legal = [&]() {
            if (!ctx.state.objectExists(ctx.source)) return false;
            const auto& self = ctx.state.getObject(ctx.source);
            if (!self.location.has_value() || self.is_exhausted) return false;   // "exhaust this"
            return on_board_enemy() && !ctx.state.getObject(moved).is_stunned;
        };
        const int go = confirmOptional(ctx, "Blast Cone: exhaust this to stun the moved unit?",
                                       still_legal);
        if (go == 1 && costDueNow(ctx)) ctx.executor.exhaustObject(ctx.source);
        if (finalizingChoices(ctx)) return;   // choices and costs end here (CR 383.3)
        if (go != 1 || !on_board_enemy()) return;
        ctx.executor.stunUnitBy(moved, ctx.source);
        ctx.events.logTrace("BLAST CONE: exhausted to stun moved enemy");
    }
    const CardDef def_ = [] {
        CardDef d;
        d.id = 695;
        d.def_id = R"RB(unl-133-219)RB";
        d.name = R"RB(Blast Cone)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-133/219)RB";
        d.collector_number = 133;
        d.artist = R"RB(黯荧岛Dark Glow)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Chaos};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you play this, you may move an enemy unit.
When you move an enemy unit, you may exhaust this to [Stun] it. (It doesn't deal combat damage this turn.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/093799f1ad34084f05011f3b326d4d32d96849e5-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_695(CardRegistry& r) {
    r.registerCard(695, std::make_unique<BlastCone>());
}

} // namespace riftbound
