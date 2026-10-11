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

class Shadow : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "If you play me to a battlefield, I enter ready." A condition on where I
    // was played, not a permission to be played to any battlefield (no
    // extraPlayLocations(): base or a battlefield my controller controls, CR 355.2).
    bool entersReadyOnPlay(const GameState&, PlayerId, const GameObject& self) const override {
        return self.isAtBattlefield();
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true, .must_be_enemy = true,
                                  .must_be_at_battlefield = true};
    }
    // "[Stun] an attacking unit here": only attackers at my battlefield.
    std::vector<GameObjectId> enumerateLegalTargets(const GameState& state, PlayerId controller,
                                                    int ability_index,
                                                    GameObjectId source) const override {
        std::vector<GameObjectId> out;
        if (!state.objectExists(source)) return out;
        const auto my_bf = state.getObject(source).battlefieldId();
        if (!my_bf) return out;
        for (auto t : Card::enumerateLegalTargets(state, controller, ability_index)) {
            const auto& u = state.getObject(t);
            if (u.combat_designation == CombatDesignation::Attacker && u.battlefieldId() == my_bf)
                out.push_back(t);
        }
        return out;
    }
    std::vector<ActivatedAbility> activatedAbilities() const override {
        return {{
            // [1][A]: the power is of ANY Domain (CR 135.2.e), so it is flagged
            // power_any_domain — power_domain alone would demand Fury.
            .cost = {.exhaust = true, .energy = 1, .power = 1,
                     .power_any_domain = true},
            .targets = TargetRequirements{.count = 1, .must_be_unit = true,
                                           .must_be_enemy = true,
                                           .must_be_at_battlefield = true},
            .is_action = true, .is_reaction = false,
            .needs_activation_time_target = true,
        }};
    }
    void onActivate(CardContext& ctx, int /*ability_idx*/,
                    const std::vector<GameObjectId>& targets) override {
        GameObjectId picked = kInvalidId;
        if (!targets.empty()) {
            picked = targets[0];
        } else {
            auto legal = enumerateLegalTargets(ctx.state, ctx.controller, 0, ctx.source);
            picked = pickTarget(ctx, "Shadow", legal);
        }
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        // CR 5h: combat_designation is cleared between trigger-fire and
        // chain resolution. For actively-attacking-here gating we
        // capture the attacker id via card_counters set elsewhere; for
        // Shadow's specific case (you pick an attacker at activation
        // time), targets[0] IS the attacker the player chose. We still
        // verify combat is in progress at THIS BF.
        auto& tgt = ctx.state.getObject(picked);
        if (tgt.combat_designation != CombatDesignation::Attacker) return;
        // "attacking here": the attacker must (still) be at my battlefield.
        if (!ctx.state.objectExists(ctx.source)) return;
        auto my_bf = ctx.state.getObject(ctx.source).battlefieldId();
        if (!my_bf || tgt.battlefieldId() != my_bf) return;
        ctx.executor.stunUnitBy(picked, ctx.source);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 752;
        d.def_id = R"RB(unl-194-219)RB";
        d.name = R"RB(Shadow)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-194/219)RB";
        d.collector_number = 194;
        d.artist = R"RB(莺之歌)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Calm, Domain::Chaos};
        d.tags = {R"RB(Vex)RB", R"RB(Shadow Isles)RB"};
        d.energy_cost = 3;
        d.might = 3;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(If you play me to a battlefield, I enter ready.
[Action][>] [1][A], [E]: [Stun] an enemy unit attacking here. (It doesn't deal combat damage this turn.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/690ee4937b3926810d8ed814afd14d4d9e98b13e-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_752(CardRegistry& r) {
    r.registerCard(752, std::make_unique<Shadow>());
}

} // namespace riftbound
