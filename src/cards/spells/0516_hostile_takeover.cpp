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

class HostileTakeover : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    bool needsPlayTimeTarget() const override { return true; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                   .must_be_enemy = true,
                                   .must_be_at_battlefield = true};
    }
    std::vector<GameObjectId> enumerateLegalTargets(
        const GameState& state, PlayerId controller) const override {
        // chooseableUnits honours "can't be chosen by enemy spells and
        // abilities" (GameObject::untargetable_by_enemy), which this override
        // used to skip.
        return chooseableUnits(state, controller, [&](const GameObject& obj) {
            return obj.controller != controller && obj.isAtBattlefield();
        });
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        GameObjectId picked = kInvalidId;
        if (!targets.empty()) picked = targets[0];
        else picked = pickTarget(ctx, "Hostile Takeover: seize enemy unit",
                                 enumerateLegalTargets(ctx.state, ctx.controller));
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        ctx.executor.takeControl(picked, ctx.controller, /*until_end_of_turn=*/true);
        ctx.executor.readyObject(picked);
        // "(Start a combat if other enemies are there. Otherwise, conquer.)" is
        // reminder text for what the control change does at the battlefield:
        // the next cleanup applies Contested for a unit that changed controller
        // there and stages the combat or the conquer (CR 190.3.a, 323.10.a).
        //
        // "Lose control of that unit and recall it at end of turn": a delayed
        // ability that returns control and recalls the unit. The engine's
        // end-of-turn control reversion (takeControl's until_end_of_turn) stays
        // armed as the fallback for the "lose control" half. (End-of-turn
        // delayed abilities fire for both players, so this works when played
        // from [Hidden] on an opponent's turn.)
        if (ctx.state.objectExists(ctx.source)) {
            ctx.state.getObject(ctx.source).card_counters[kSeizedKey] =
                static_cast<int>(picked.raw()) + 1;
            DelayedAbility da;
            da.source = ctx.source;
            da.card_def_id = cardDefId();
            da.controller = ctx.controller;
            da.trigger = TriggerType::AtEndOfTurn;
            da.expires_on_turn = ctx.state.turn.turn_number;
            ctx.state.delayed_abilities.push_back(da);
        }
        RIFT_TRACE(ctx.events, "HOSTILE TAKEOVER: seized enemy unit until end of turn");
    }

    // The delayed "lose control of that unit and recall it at end of turn".
    static constexpr const char* kSeizedKey = "__takeover_unit";
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& me = ctx.state.getObject(ctx.source);
        auto it = me.card_counters.find(kSeizedKey);
        if (it == me.card_counters.end() || it->second <= 0) return;
        const auto seized = GameObjectId::fromRaw(it->second - 1);
        me.card_counters.erase(it);
        if (!ctx.state.objectExists(seized)) return;
        auto& unit = ctx.state.getObject(seized);
        if (!unit.location.has_value()) return;              // it left the board
        // Only while it is still held by this spell's control change.
        if (!unit.control_reverts_eot || unit.controller != ctx.controller) return;
        const PlayerId back = unit.original_controller_eot;
        if (back == PlayerId::None) return;
        unit.controller = back;
        unit.control_reverts_eot = false;
        unit.original_controller_eot = PlayerId::None;
        ctx.events.emit(ObjectStateChangedEvent{seized, "controller_reverted"});
        // Recall (CR 457-458): relocated to its controller's base; not a Move,
        // so no UnitMovedEvent. Attached gear goes with it (CR 448.1). Damage
        // and statuses are unaffected (CR 460.1).
        ctx.executor.recallToBase(seized);
        RIFT_TRACE(ctx.events, "HOSTILE TAKEOVER: end of turn -> " + unit.name +
                            " returns to its controller and is recalled");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 516;
        d.def_id = R"RB(sfd-202-221)RB";
        d.name = R"RB(Hostile Takeover)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-202/221)RB";
        d.collector_number = 202;
        d.artist = R"RB(Grafit Studio)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Mind, Domain::Order};
        d.tags = {R"RB(Renata Glasc)RB"};
        d.energy_cost = 5;
        d.power_cost = 2;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Hidden);
        d.ability_text = R"RB([Hidden] (Hide now for [A] to react with later for .)
Take control of an enemy unit at a battlefield. Ready it. (Start a combat if other enemies are there. Otherwise, conquer.)
Lose control of that unit and recall it at end of turn. (Send it to base. This isn't a move.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/56651a87166df7108ce9f945bd71390d4926770a-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_516(CardRegistry& r) {
    r.registerCard(516, std::make_unique<HostileTakeover>());
}

} // namespace riftbound
