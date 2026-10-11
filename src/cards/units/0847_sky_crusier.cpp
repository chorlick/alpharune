#include "cards/card.h"
#include "cards/card_helpers.h"
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

class SkyCrusier : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "Discard a gear, [1], [E]: Deal 4 to a unit at a battlefield."
    std::vector<ActivatedAbility> activatedAbilities() const override {
        ActivatedAbility ab;
        ab.cost.exhaust = true;
        ab.cost.energy = 1;
        ab.cost.discard = true;
        ab.cost.discard_count = 1;
        ab.cost.discard_type_filter = 1 + static_cast<int>(CardType::Gear);
        ab.targets = TargetRequirements{.count = 1, .must_be_unit = true,
                                        .must_be_at_battlefield = true};
        ab.needs_activation_time_target = true;
        return {ab};
    }
    std::vector<GameObjectId> enumerateLegalTargets(
        const GameState& state, PlayerId controller,
        int /*ability_index*/) const override {
        auto out = chooseableUnits(state, controller,
            [](const GameObject& o) { return o.isAtBattlefield(); });
        std::sort(out.begin(), out.end());
        return out;
    }
    void onActivate(CardContext& ctx, int /*ability_index*/,
                    const std::vector<GameObjectId>& targets) override {
        GameObjectId picked = kInvalidId;
        if (!targets.empty()) picked = targets[0];
        else picked = pickTarget(ctx, "Sky Crusier: deal 4 to a unit at a battlefield",
                                 enumerateLegalTargets(ctx.state, ctx.controller, 0));
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        if (!ctx.state.getObject(picked).isAtBattlefield()) return;  // fizzle
        ctx.executor.dealDamage(picked, 4, ctx.source);
        if (ctx.state.objectExists(picked) &&
            ctx.state.getObject(picked).hasLethalDamage())
            ctx.executor.killObject(picked);
        RIFT_TRACE(ctx.events, "SKY CRUSIER: deal 4 to a unit at a battlefield");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 847;
        d.def_id = R"RB(ven-060-166)RB";
        d.name = R"RB(Sky Crusier)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-060/166)RB";
        d.collector_number = 60;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Piltover)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 3;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(Discard a gear, [1], [E]: Deal 4 to a unit at a battlefield.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/4a99fbe995f6d5d71c1c46e1c38f64a9f9d7c3e9-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_847(CardRegistry& r) {
    r.registerCard(847, std::make_unique<SkyCrusier>());
}

}  // namespace riftbound
