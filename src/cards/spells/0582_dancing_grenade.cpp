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

class DancingGrenade : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Deal 2 to a unit. Its controller may play this spell again for [A]. If
    //  they do, this deals 1 additional Bonus Damage for each time this spell
    //  has dealt damage this turn."
    //
    // The offer goes to the damaged unit's controller as this resolves. If
    // they pay [A] the spell is played again under their control
    // (EffectExecutor::playIgnoringCost puts it back on the chain, where they
    // choose its target), and that play makes the same offer in turn.
    // This is NOT a reflexive trigger: the text has no "do this" (CR 388.1),
    // so the play is an instruction of the spell's own effect — a limited
    // play made as part of its resolution (CR 420.3) — executed while the card
    // is still on the chain; a spell goes to the trash only after its whole
    // effect has been executed (CR 359.3.d). (Contrast Death from Below 747,
    // "do this: You may play this from your trash", CR 388.4.) So the card is
    // on the chain for the new play: ChainManager does not file a spell that
    // was played again while it resolved; the new play files it.
    //
    // "Each time this spell has dealt damage this turn" is counted on the card
    // object, stamped with the turn so an old count never carries over.
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};
    }
    bool needsPlayTimeTarget() const override { return true; }

    // resume_data (clear of the Card::pick* helpers' slots):
    static constexpr size_t kPhaseSlot = 7;      // 0 = not dealt yet, 1 = offer made, 2 = done
    static constexpr size_t kPlayerSlot = 8;     // who was offered the replay
    static constexpr const char* kHitsKey = "__dancing_grenade_hits";
    static constexpr const char* kTurnKey = "__dancing_grenade_turn";

    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        auto& ri = ctx.state.chain.resuming;
        auto slot = [&](size_t i) -> int32_t& {
            while (ri->resume_data.size() <= i) ri->resume_data.push_back(0);
            return ri->resume_data[i];
        };
        const int phase = ri.has_value() ? slot(kPhaseSlot) : 0;
        if (phase >= 2) return;
        if (phase == 1) {
            // The unit's controller answered the replay offer.
            slot(kPhaseSlot) = 2;
            const PlayerId who = static_cast<PlayerId>(slot(kPlayerSlot));
            if (takenValue(ctx, 0) != 1) return;
            if (!ctx.executor.payAnyPower(who, 1)) return;
            ctx.events.logTrace(std::string("DANCING GRENADE: ") + toString(who) +
                                " pays [A] to play it again");
            ctx.executor.playIgnoringCost(who, ctx.source);
            return;
        }

        const auto locked = lockedPlayTarget(ctx, targets);
        const GameObjectId picked = locked ? *locked
            : pickTarget(ctx, "Dancing Grenade", enumerateLegalTargets(ctx.state, ctx.controller));
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;   // waiting, or no target

        int prior_hits = 0;
        if (ctx.state.objectExists(ctx.source)) {
            auto& counters = ctx.state.getObject(ctx.source).card_counters;
            if (counters[kTurnKey] != ctx.state.turn.turn_number) {
                counters[kTurnKey] = ctx.state.turn.turn_number;
                counters[kHitsKey] = 0;
            }
            prior_hits = counters[kHitsKey];
            counters[kHitsKey] = prior_hits + 1;
        }
        const PlayerId unit_controller = ctx.state.getObject(picked).controller;
        const int dmg = 2 + prior_hits;  // +1 Bonus Damage per prior hit this turn
        ctx.executor.dealDamage(picked, dmg, ctx.source);
        ctx.events.logTrace("DANCING GRENADE: deal " + std::to_string(dmg));
        if (ctx.state.objectExists(picked) && ctx.state.getObject(picked).hasLethalDamage())
            ctx.executor.killObject(picked);

        // "Its controller may play this spell again for [A]."
        if (!ri.has_value()) return;
        slot(kPhaseSlot) = 2;
        if (ctx.executor.payableAnyPower(unit_controller) < 1) return;
        if (enumerateLegalTargets(ctx.state, unit_controller).empty()) return;
        slot(kPhaseSlot) = 1;
        slot(kPlayerSlot) = static_cast<int32_t>(unit_controller);
        ctx.executor.requestChoice(unit_controller,
            {valChoice(unit_controller, 0, "No — Dancing Grenade: play it again for [A]"),
             valChoice(unit_controller, 1, "Yes — Dancing Grenade: play it again for [A]")},
            "may: Dancing Grenade: play it again for [A]");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 582;
        d.def_id = R"RB(unl-020-219)RB";
        d.name = R"RB(Dancing Grenade)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-020/219)RB";
        d.collector_number = 20;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Fury};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(Deal 2 to a unit. Its controller may play this spell again for [A]. If they do, this deals 1 additional Bonus Damage for each time this spell has dealt damage this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/8afc30b1bc7cc1841dfe11cfa29adbcc02257845-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_582(CardRegistry& r) {
    r.registerCard(582, std::make_unique<DancingGrenade>());
}

} // namespace riftbound
