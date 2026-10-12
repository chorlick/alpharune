#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include "cards/card_helpers.h"
#include "core/intent.h"
#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace riftbound {
namespace {

class AlphaStrike : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                  .must_be_friendly = true};
    }
    // The units the damage is split among are targets chosen as the spell is
    // finalized (CR 355.14.a-b); splitDamageAmong asks for them then and ends
    // the finalize pass.
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    // It can choose any unit its controller has on the board (the play gate).
    FinalizeChoice mayChooseAtFinalization(const GameState& state, PlayerId controller,
                                           GameObjectId object) const override {
        return friendlyUnitChoice(state, controller, object);
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        // "Choose a friendly unit." From hand it is locked in by the play
        // intent. A play by an effect (Blind Fury, Promising Future) and new
        // choices after a control change (Mystic Reversal) come with none:
        // the unit is then chosen here, in the finalize pass, like any other
        // choice of the spell (finding F2 — the pass used to end at an early
        // return, which the finalize contract forbids).
        const bool chosen_at_play = ctx.state.chain.resuming.has_value() &&
                                    !ctx.state.chain.resuming->targets.empty();
        GameObjectId src = kInvalidId;
        if (chosen_at_play || !ctx.state.chain.resuming.has_value()) {
            src = chosenTarget(ctx, targets, 0);
            if (!ctx.state.chain.resuming.has_value() && !targets.empty()) src = targets[0];
        } else {
            src = pickTarget(ctx, "Alpha Strike: choose a friendly unit",
                chooseableUnits(ctx.state, ctx.controller,
                    [&](const GameObject& o) { return o.controller == ctx.controller; }));
            if (src == kInvalidId) return;   // waiting, or no friendly unit to choose
        }
        std::vector<GameObjectId> damaged;
        if (src == kInvalidId || !ctx.state.objectExists(src) ||
            !ctx.state.getObject(src).location.has_value()) {
            // The chosen unit is no longer legal (a mistarget): nothing deals
            // damage. Said through the helper, so the finalize pass and the
            // resolution agree that there is nothing to choose.
            splitDamageAmong(ctx, "Alpha Strike", 0, kInvalidId, {}, damaged,
                             /*may_choose_none=*/true);
            return;
        }
        // "It deals damage equal to its Might": its Might now — as the spell
        // is finalized that caps the number of targets (CR 355.14.c), as it
        // resolves it is the damage divided.
        int total = ctx.state.getObject(src).current_might;

        // "split among enemy units at battlefields" (CR 355.14): the
        // controller picks the units, then the division. Units that can't be
        // chosen by enemy spells and abilities are not legal picks.
        auto legal = chooseableUnits(ctx.state, ctx.controller,
            [&](const GameObject& o) {
                return o.controller != ctx.controller && o.isAtBattlefield();
            });
        if (!splitDamageAmong(ctx, "Alpha Strike", total, src, legal, damaged)) return;

        // Kill lethally-damaged units (after all damage is marked); gain 1 XP
        // per unit this kills.
        int kills = 0;
        {   // the batch is for these kills only
            kills += static_cast<int>(ctx.executor.killLethallyDamagedTogether(damaged).size());   // they die at the same time
        }
        // "Then for each unit this kills, do this: Gain 1 XP." — one
        // reflexive trigger per unit killed (CR 388.1.a, 389.2), each a chain
        // item of its own.
        for (int i = 0; i < kills; ++i)
            ctx.executor.queueReflexiveTrigger(ctx.source, ctx.controller, 0);
        if (kills > 0)
            RIFT_TRACE(ctx.events, "ALPHA STRIKE: " + std::to_string(kills) + " kill(s)");
    }
    bool hasReflexiveTrigger() const override { return true; }
    void onReflexiveTrigger(CardContext& ctx, int /*index*/,
                            const std::vector<int32_t>& /*data*/) override {
        if (finalizingChoices(ctx)) return;   // nothing to choose
        gainXp(ctx, ctx.controller, 1);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 751;
        d.def_id = R"RB(unl-192-219)RB";
        d.name = R"RB(Alpha Strike)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-192/219)RB";
        d.collector_number = 192;
        d.artist = R"RB(莺之歌)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Calm, Domain::Body};
        d.tags = {R"RB(Master Yi)RB"};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Choose a friendly unit. It deals damage equal to its Might split among enemy units at battlefields. Then for each unit this kills, do this: Gain 1 XP.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/154940caa774fd8219625cf051ddcb230ffc7f02-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_751(CardRegistry& r) {
    r.registerCard(751, std::make_unique<AlphaStrike>());
}

} // namespace riftbound
