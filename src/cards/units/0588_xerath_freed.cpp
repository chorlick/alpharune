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

// "[R], [E]: Deal 3 to a unit. Use this ability only while I'm at a battlefield."

class XerathFreed : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    TriggerType triggerType() const override { return TriggerType::Activated; }
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        return ActivationCost{.exhaust = true, .power = 1, .power_domain = Domain::Fury};
    }
    bool canActivateAbility(const GameState& state, PlayerId controller) const override {
        for (auto& [id, obj] : state.objects) {
            if (obj.card_def_id != cardDefId()) continue;
            if (obj.controller != controller) continue;
            if (obj.isAtBattlefield()) return true;
        }
        return false;
    }
    std::vector<ActivatedAbility> activatedAbilities() const override {
        return {{
            .cost = {.exhaust = true, .power = 1, .power_domain = Domain::Fury},
            .targets = TargetRequirements{.count = 1, .must_be_unit = true},
            .is_action = false, .is_reaction = false,
            .needs_activation_time_target = true,
        }};
    }
    std::vector<GameObjectId> enumerateLegalTargets(
        const GameState& state, PlayerId /*controller*/,
        int /*ability_index*/) const override {
        std::vector<GameObjectId> out;
        for (auto& [id, obj] : state.objects) {
            if (obj.isUnit() && obj.location.has_value()) out.push_back(id);
        }
        return out;
    }
    void onActivate(CardContext& ctx, int /*ability_index*/,
                    const std::vector<GameObjectId>& targets) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        GameObjectId picked = kInvalidId;
        if (!targets.empty()) picked = targets[0];
        else picked = pickTarget(ctx, "Xerath: deal 3 to a unit",
                                 enumerateLegalTargets(ctx.state, ctx.controller, 0));
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        ctx.executor.dealDamage(picked, 3, ctx.source);
        if (ctx.state.objectExists(picked) &&
            ctx.state.getObject(picked).hasLethalDamage())
            ctx.executor.killObject(picked);
        ctx.events.logTrace("XERATH FREED: dealt 3 to a unit");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 588;
        d.def_id = R"RB(unl-026-219)RB";
        d.name = R"RB(Xerath, Freed)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-026/219)RB";
        d.collector_number = 26;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Shurima)RB", R"RB(Xerath)RB"};
        d.energy_cost = 5;
        d.might = 5;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB([R], [E]: Deal 3 to a unit. Use this ability only while I'm at a battlefield.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ef52e66a137a80cf5df862f31a114bfc00914a93-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_588(CardRegistry& r) {
    r.registerCard(588, std::make_unique<XerathFreed>());
}

} // namespace riftbound
