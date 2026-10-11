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
    bool isActionAbility() const override { return true; }
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
        for (const auto& [id, obj] : state.objects) {
            if (obj.controller != who || !isEquipment(obj) || !obj.location.has_value()) continue;
            if (obj.attached_to.has_value() && *obj.attached_to == unit) continue;
            out.push_back(id);
        }
        return out;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (targets.empty() || !ctx.state.objectExists(targets[0])) return;
        const GameObjectId unit = targets[0];

        // Every choice is made before anything happens (this callback is
        // re-entered from the top with each answer): where the unit moves,
        // whether to attach an Equipment, and which one.
        const auto dests = moveDests(ctx.state, ctx.state.getObject(unit));
        const int n = 1 + static_cast<int>(ctx.state.battlefields.size());
        std::vector<std::string> labels;
        uint32_t mask = 0;
        for (int d = 0; d < n; ++d) labels.push_back(destLabel(d));
        for (int d : dests) if (d >= 0 && d < 32) mask |= (1u << d);
        const int dest = pickMode(ctx, "Relentless Pursuit: move it where?", n, labels, mask);
        if (dest == -1) return;   // waiting for the agent (-2: nowhere to move)

        // "up to one Equipment": "No" is none (CR 355.13); the move and the
        // granted ability do not depend on it. The unit itself is mandatory.
        const int attach = confirmOptional(ctx, "Relentless Pursuit: attach an Equipment to it",
            [&]() { return !attachable(ctx.state, unit).empty(); });
        if (attach == -1) return;
        GameObjectId gear = kInvalidId;
        if (attach == 1) {
            gear = pickTarget(ctx, "Relentless Pursuit: Equipment to attach",
                              attachable(ctx.state, unit));
            if (gear == kInvalidId) return;   // waiting
        }

        if (dest >= 0) {
            moveTo(ctx, unit, dest);
            RIFT_TRACE(ctx.events, "RELENTLESS PURSUIT: moved a friendly unit");
        }
        if (!ctx.state.getObject(unit).location.has_value()) return;   // it left the board
        if (gear != kInvalidId && ctx.state.objectExists(gear) &&
            ctx.state.getObject(gear).location.has_value()) {
            attachFree(ctx, gear, unit);
            RIFT_TRACE(ctx.events, "RELENTLESS PURSUIT: attached " + ctx.state.getObject(gear).name);
        }
        // This turn, that unit has "When I conquer, you may move me to my
        // base.": a delayed ability scoped to the unit, gone at end of turn.
        DelayedAbility da;
        da.source = ctx.source;
        da.card_def_id = cardDefId();
        da.controller = ctx.state.getObject(unit).controller;
        da.trigger = TriggerType::WhenIConquer;
        da.target_filter = unit;
        da.expires_on_turn = ctx.state.turn.turn_number;
        ctx.state.delayed_abilities.push_back(da);
    }
    // The granted "When I conquer, you may move me to my base."
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.chain.resuming.has_value()) return;
        const GameObjectId unit = ctx.state.chain.resuming->triggering_subject;
        auto can_move = [&]() {
            if (unit == kInvalidId || !ctx.state.objectExists(unit)) return false;
            const auto& u = ctx.state.getObject(unit);
            return u.isUnit() && u.location.has_value() && !u.isAtBase();
        };
        if (confirmOptional(ctx, "Relentless Pursuit: move it to its base", can_move) <= 0) return;
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
