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

class Conscription : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // Broad play-time requirement; the 3M cap / "any" widening is applied at
    // resolve based on whether 5 XP was paid.
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                   .must_be_enemy = true,
                                   .must_be_at_battlefield = true};
    }
    bool needsPlayTimeTarget() const override { return true; }
    bool hasLegalTargets(const GameState& state, PlayerId controller) const override {
        // Legal if any enemy unit sits at a battlefield (might cap is checked
        // at resolve; if 5 XP would be paid the cap is irrelevant).
        auto opp = opponent(controller);
        for (auto& [id, obj] : state.objects) {
            if (!obj.isUnit() || obj.controller != opp) continue;
            if (!obj.isAtBattlefield()) continue;
            if (obj.untargetable_by_enemy) continue;
            return true;
        }
        return false;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ps = ctx.state.player(ctx.controller);
        // Optional additional cost: spend 5 XP (only offered if affordable).
        int paid = confirmOptional(ctx, "Conscription: spend 5 XP (any enemy unit)",
            [&]() { return ps.xp >= 5; });
        if (paid == -1) return;  // waiting on agent

        bool widened = (paid == 1);
        if (widened) ps.xp -= 5;

        // Build the legal enemy-unit set under the current restriction.
        auto opp = opponent(ctx.controller);
        std::vector<GameObjectId> legal;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller != opp) continue;
            if (!obj.isAtBattlefield()) continue;
            if (obj.untargetable_by_enemy) continue;
            if (!widened && obj.current_might > 3) continue;
            legal.push_back(id);
        }
        GameObjectId picked = pickTarget(ctx, "Conscription target", legal);
        if (picked == kInvalidId && ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->resume_point == 7) {
            return;  // suspended for agent decision
        }
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;

        // Take control of it, exhaust it, and recall it (to its base).
        ctx.executor.takeControl(picked, ctx.controller, /*until_end_of_turn=*/false);
        ctx.executor.exhaustObject(picked);
        ctx.executor.moveToBase(picked);
        ctx.events.logTrace("CONSCRIPTION: take control + exhaust + recall (paid="
                            + std::to_string(widened) + ")");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 702;
        d.def_id = R"RB(unl-140-219)RB";
        d.name = R"RB(Conscription)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-140/219)RB";
        d.collector_number = 140;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Chaos};
        d.energy_cost = 5;
        d.power_cost = 2;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(You may spend 5 XP as an additional cost to play this.
Choose an enemy unit at a battlefield with 3 [M] or less. If you paid the additional cost, choose any enemy unit at a battlefield instead. Take control of it, exhaust it, and recall it.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/1c05bff48666586a2b3552b5638deaf20d9006f7-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_702(CardRegistry& r) {
    r.registerCard(702, std::make_unique<Conscription>());
}

} // namespace riftbound
