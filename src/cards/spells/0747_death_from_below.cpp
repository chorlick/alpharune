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

class DeathFromBelow : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Kill a unit at a battlefield. Then, if it had 3 [M] or less, do this:
    //  You may play this from your trash for [A]."
    // "do this: You may play this from your trash for [A]." is a reflexive
    // trigger — the rule book's own example (CR 388.4): it "is added to the
    // chain when the spell is resolving, and then on finalization of the
    // reflexive trigger its controller will decide if they want to perform
    // the effect". By then this spell has resolved and is in the trash; the
    // "may" and the [A] are settled at that finalization, and the item plays
    // the card from the trash when it resolves (a new play, with a new target).
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                   .must_be_at_battlefield = true};
    }
    bool needsPlayTimeTarget() const override { return true; }
    // resume_data, clear of the Card::pick* helpers' slots: the killed unit's
    // Might + 1 once the kill has happened (-1: there was nothing to kill).
    static constexpr size_t kKilledSlot = 7;

    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        auto& ri = ctx.state.chain.resuming;
        int had = -1;
        if (ri.has_value() && ri->resume_data.size() > kKilledSlot &&
            ri->resume_data[kKilledSlot] != 0) {
            had = ri->resume_data[kKilledSlot] - 1;   // re-entered after the kill
        } else {
            const auto locked = lockedPlayTarget(ctx, targets);
            const GameObjectId picked = locked ? *locked
                : pickTarget(ctx, "Death from Below",
                             enumerateLegalTargets(ctx.state, ctx.controller));
            if (picked == kInvalidId && ri.has_value() && ri->resume_point == 7)
                return;   // waiting for the agent
            if (picked != kInvalidId && ctx.state.objectExists(picked)) {
                had = ctx.state.getObject(picked).current_might;
                ctx.executor.killObject(picked);
                ctx.events.logTrace("DEATH FROM BELOW: killed unit (might=" +
                                     std::to_string(had) + ")");
            }
            if (ri.has_value()) {
                while (ri->resume_data.size() <= kKilledSlot) ri->resume_data.push_back(0);
                ri->resume_data[kKilledSlot] = had < 0 ? -1 : had + 1;
            }
        }
        if (had < 0 || had > 3) return;
        if (ri.has_value() && ri->resume_data.size() > kKilledSlot + 1 &&
            ri->resume_data[kKilledSlot + 1] != 0)
            return;                                   // already queued
        if (ri.has_value()) {
            while (ri->resume_data.size() <= kKilledSlot + 1) ri->resume_data.push_back(0);
            ri->resume_data[kKilledSlot + 1] = 1;
        }
        ctx.executor.queueReflexiveTrigger(ctx.source, ctx.controller, 0);
    }
    bool hasReflexiveTrigger() const override { return true; }
    void onReflexiveTrigger(CardContext& ctx, int /*index*/,
                            const std::vector<int32_t>& /*data*/) override {
        auto in_trash = [&]() {
            return ctx.state.objectExists(ctx.source) &&
                   ctx.state.getObject(ctx.source).zone == ZoneType::Trash;
        };
        const int again = confirmOptional(ctx, "Death from Below: play it from your trash for [A]",
            [&]() {
                return costAlreadyPaid(ctx) ||
                       (in_trash() && ctx.executor.payableAnyPower(ctx.controller) >= 1 &&
                        !enumerateLegalTargets(ctx.state, ctx.controller).empty());
            });
        if (again == 1 && costDueNow(ctx) && !ctx.executor.payAnyPower(ctx.controller, 1)) return;
        if (finalizingChoices(ctx)) return;   // the "may" and the [A] end here (CR 388.4)
        if (again < 1 || !in_trash()) return; // declined, or it left the trash in response
        ctx.events.logTrace("DEATH FROM BELOW: paid [A] -> played from the trash");
        ctx.executor.playIgnoringCost(ctx.controller, ctx.source);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 747;
        d.def_id = R"RB(unl-186-219)RB";
        d.name = R"RB(Death from Below)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-186/219)RB";
        d.collector_number = 186;
        d.artist = R"RB(莺之歌)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Fury, Domain::Chaos};
        d.tags = {R"RB(Pyke)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(Kill a unit at a battlefield. Then, if it had 3 [M] or less, do this: You may play this from your trash for [A].)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/17d93a11a252287e3b0f4bbe32722ecc9469ec66-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_747(CardRegistry& r) {
    r.registerCard(747, std::make_unique<DeathFromBelow>());
}

} // namespace riftbound
