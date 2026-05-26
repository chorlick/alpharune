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

class Voidreaver : public LegendCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIWinCombat; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        ctx.state.player(ctx.controller).xp += 1;
        ctx.events.logTrace("VOIDREAVER: win combat -> +1 XP");
    }
    std::vector<ActivatedAbility> activatedAbilities() const override {
        return {
            // Ability 0 — Spend 1 XP, [E]: Buff a unit.
            {
                .cost = {.exhaust = true, .xp_cost = 1},
                .targets = TargetRequirements{.count = 1, .must_be_unit = true},
                .is_action = false, .is_reaction = false,
                .needs_activation_time_target = true,
            },
            // Ability 1 — Spend 2 XP, [E]: Move an exhausted friendly unit
            //              at a battlefield to its base.
            {
                .cost = {.exhaust = true, .xp_cost = 2},
                .targets = TargetRequirements{.count = 1, .must_be_unit = true,
                                               .must_be_friendly = true,
                                               .must_be_at_battlefield = true},
                .is_action = false, .is_reaction = false,
                .needs_activation_time_target = true,
            },
        };
    }
    std::vector<GameObjectId> enumerateLegalTargets(
        const GameState& state, PlayerId controller,
        int ability_index) const override {
        std::vector<GameObjectId> out;
        if (ability_index == 0) {
            // Any unit (buff target).
            for (auto& [id, obj] : state.objects) {
                if (!obj.isUnit() || !obj.location.has_value()) continue;
                out.push_back(id);
            }
        } else {
            // Exhausted friendly unit at a battlefield.
            for (auto& [id, obj] : state.objects) {
                if (!obj.isUnit() || obj.controller != controller) continue;
                if (!obj.location.has_value()) continue;
                if (!std::holds_alternative<BattlefieldLocation>(*obj.location)) continue;
                if (!obj.is_exhausted) continue;
                out.push_back(id);
            }
        }
        return out;
    }
    void onActivate(CardContext& ctx, int ability_index,
                    const std::vector<GameObjectId>& targets) override {
        GameObjectId picked = kInvalidId;
        if (!targets.empty()) {
            picked = targets[0];
        } else {
            auto legal = enumerateLegalTargets(ctx.state, ctx.controller, ability_index);
            picked = pickTarget(ctx,
                ability_index == 0 ? "Voidreaver: buff" : "Voidreaver: recall",
                legal);
        }
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;

        if (ability_index == 0) {
            ctx.executor.buffUnit(picked);
            ctx.events.logTrace("VOIDREAVER: spent 1 XP to buff " +
                                 ctx.state.getObject(picked).name);
        } else {
            ctx.executor.moveToBase(picked);
            ctx.events.logTrace("VOIDREAVER: spent 2 XP to recall " +
                                 ctx.state.getObject(picked).name + " to base");
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 787;
        d.def_id = R"RB(unl-236-219)RB";
        d.name = R"RB(Voidreaver)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-236/219)RB";
        d.collector_number = 236;
        d.artist = R"RB(Jeffrey Chen)RB";
        d.card_type = CardType::Legend;
        d.domains = {Domain::Body, Domain::Chaos};
        d.tags = {R"RB(Kha'Zix)RB"};
        d.rarity = Rarity::Showcase;
        d.ability_text = R"RB(When you win a combat, gain 1 XP.
Spend 1 XP, [E]: [Buff] a unit.
Spend 2 XP, [E]: Move an exhausted friendly unit from a battlefield to its base.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ffed0102adcae6fe01d173042487ea85ebe899bc-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_787(CardRegistry& r) {
    r.registerCard(787, std::make_unique<Voidreaver>());
}

} // namespace riftbound
