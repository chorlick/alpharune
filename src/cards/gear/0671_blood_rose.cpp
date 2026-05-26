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

class BloodRose : public GearCard {
public:
    const CardDef& def() const override { return def_; }

    // ── Trigger: WhenYouPlayAUnit, optionally pay 1 for 1 XP ──
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayAUnit; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& ps = ctx.state.player(ctx.controller);
        // confirmOptional pattern (Phase 5f). still_legal closure validates
        // BOTH at prompt-time and after the agent says yes.
        auto still_legal = [&ps]() { return ps.rune_pool.energy >= 1; };
        if (!still_legal()) return;
        auto conf = confirmOptional(ctx,
            "Blood Rose: pay [1] to gain 1 XP?", still_legal);
        if (conf < 1) return;
        ps.rune_pool.energy -= 1;
        ps.xp += 1;
        ctx.events.logTrace("BLOOD ROSE: paid [1] -> +1 XP (now " +
                             std::to_string(ps.xp) + ")");
    }

    // ── Activated: Spend 3 XP, [E]: Ready a unit (Phase 6r migrated) ──
    std::vector<ActivatedAbility> activatedAbilities() const override {
        return {{
            .cost = {.exhaust = true, .xp_cost = 3},
            .targets = TargetRequirements{.count = 1, .must_be_unit = true,
                                           .must_be_friendly = true},
            .is_action = false, .is_reaction = false,
            .needs_activation_time_target = true,
        }};
    }
    void onActivate(CardContext& ctx, int /*ability_idx*/,
                    const std::vector<GameObjectId>& targets) override {
        GameObjectId picked = kInvalidId;
        if (!targets.empty()) {
            picked = targets[0];
        } else {
            auto legal = enumerateLegalTargets(ctx.state, ctx.controller, 0);
            picked = pickTarget(ctx, "Blood Rose", legal);
        }
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        ctx.executor.readyObject(picked);
        ctx.events.logTrace("BLOOD ROSE: spent 3 XP -> ready " +
                             ctx.state.getObject(picked).name);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 671;
        d.def_id = R"RB(unl-109-219)RB";
        d.name = R"RB(Blood Rose)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-109/219)RB";
        d.collector_number = 109;
        d.artist = R"RB(黯荧岛Dark Glow)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Body};
        d.energy_cost = 1;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When you play a unit, you may pay [1] to gain 1 XP.
Spend 3 XP, [E]: Ready a unit.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/64067ebdbe3140e9a458da7ecf7253a5fae9a31b-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_671(CardRegistry& r) {
    r.registerCard(671, std::make_unique<BloodRose>());
}

} // namespace riftbound
