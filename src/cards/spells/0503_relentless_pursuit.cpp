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

class RelentlessPursuit : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Move a friendly unit. You may attach up to one Equipment with the same
    //  controller to it. This turn, that unit has "When I conquer, you may move
    //  me to my base.""
    // As it is played: the unit (its target), where it moves (CR 355.4) and the
    // Equipment, up to one (a target: CR 355.12, 355.13 — zero allowed). As it
    // resolves: whether to attach the chosen Equipment ("you may"). The granted
    // trigger's "may" is decided as that trigger is finalized (CR 403.1).
    bool isActionAbility() const override { return true; }
    unsigned choicesAtFinalization() const override {
        return kFinalizeResolve | kFinalizeTrigger;
    }
    FinalizeChoice mayChooseAtFinalization(const GameState& state, PlayerId controller,
                                           GameObjectId object) const override {
        return friendlyUnitChoice(state, controller, object);   // its target
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                  .must_be_friendly = true};
    }
    /// Equipment `unit`'s controller controls that is not already on it (an
    /// Equipment attached elsewhere moves over, CR 435.1.f).
    static std::vector<GameObjectId> attachable(const GameState& state, GameObjectId unit) {
        std::vector<GameObjectId> out;
        if (!state.objectExists(unit)) return out;
        const PlayerId who = state.getObject(unit).controller;
        for (const auto& [id, obj] : state.objects.onBoard()) {
            if (obj.controller != who || !isEquipment(obj) || !obj.location.has_value()) continue;
            if (obj.attached_to.has_value() && *obj.attached_to == unit) continue;
            out.push_back(id);
        }
        return out;
    }
    bool needsPlayTimeTarget() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const GameObjectId unit = pickTarget(ctx, "Relentless Pursuit: move a friendly unit",
                                             enumerateLegalTargets(ctx.state, ctx.controller));
        if (unit == kInvalidId && ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->resume_point == 7)
            return;                           // waiting for the agent
        int dest = -2;
        std::optional<GameObjectId> gear = GameObjectId{kInvalidId};
        if (unit != kInvalidId && ctx.state.getObject(unit).location.has_value()) {
            dest = pickMoveDest(*this, ctx, "Relentless Pursuit: move it to",
                                ctx.state.getObject(unit));
            if (dest == -1) return;   // waiting for the agent (-2: nowhere to move)
            gear = selectTargetOrNone(*this, ctx, "Relentless Pursuit: Equipment to attach",
                                      attachable(ctx.state, unit));
            if (!gear.has_value()) return;   // waiting
        }
        if (finalizingChoices(ctx)) return;  // choices end here (CR 355)
        if (unit == kInvalidId || !ctx.state.objectExists(unit)) return;
        // "You may attach": decided as it resolves, for the Equipment chosen.
        const GameObjectId g = *gear;
        int attach = 0;
        if (g != kInvalidId) {
            attach = confirmOptional(ctx, "Relentless Pursuit: attach the Equipment to it", [&]() {
                return ctx.state.objectExists(g) && ctx.state.getObject(g).location.has_value();
            });
            if (attach == -1) return;
        }
        if (dest >= 0 && ctx.state.getObject(unit).location.has_value()) {
            moveTo(ctx, unit, dest);
            RIFT_TRACE(ctx.events, "RELENTLESS PURSUIT: moved a friendly unit");
        }
        if (!ctx.state.getObject(unit).location.has_value()) return;   // it left the board
        if (attach == 1 && ctx.state.objectExists(g) && ctx.state.getObject(g).location.has_value()) {
            attachFree(ctx, g, unit);
            RIFT_TRACE(ctx.events, "RELENTLESS PURSUIT: attached " + ctx.state.getObject(g).name);
        }
        // This turn, that unit has "When I conquer, you may move me to my base.":
        // a delayed ability scoped to the unit, gone at end of turn.
        DelayedAbility da;
        da.source = ctx.source;
        da.card_def_id = cardDefId();
        da.controller = ctx.state.getObject(unit).controller;
        da.trigger = TriggerType::WhenIConquer;
        da.target_filter = unit;
        da.expires_on_turn = ctx.state.turn.turn_number;
        ctx.state.delayed_abilities.push_back(da);
    }
    // The granted "When I conquer, you may move me to my base." Its "may" is
    // decided as it is finalized (CR 403.1).
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.chain.resuming.has_value()) return;
        const GameObjectId unit = ctx.state.chain.resuming->triggering_subject;
        auto can_move = [&]() {
            if (unit == kInvalidId || !ctx.state.objectExists(unit)) return false;
            const auto& u = ctx.state.getObject(unit);
            return u.isUnit() && u.location.has_value() && !u.isAtBase();
        };
        const int go = confirmOptional(ctx, "Relentless Pursuit: move it to its base", can_move);
        if (go == -1) return;
        if (finalizingChoices(ctx)) return;  // choices end here (CR 403)
        if (go < 1) return;
        ctx.executor.moveToBase(unit);
        RIFT_TRACE(ctx.events, "RELENTLESS PURSUIT: the unit moved to its base after conquering");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 503;
        d.def_id = R"RB(sfd-184-221)RB";
        d.name = R"RB(Relentless Pursuit)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-184/221)RB";
        d.collector_number = 184;
        d.artist = R"RB(Max Grecke)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Fury, Domain::Body};
        d.tags = {R"RB(Lucian)RB"};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Move a friendly unit. You may attach up to one Equipment with the same controller to it. This turn, that unit has "When I conquer, you may move me to my base.")RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/48754730486abefdb56221d0760b9c963aae7c09-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_503(CardRegistry& r) {
    r.registerCard(503, std::make_unique<RelentlessPursuit>());
}

} // namespace riftbound
