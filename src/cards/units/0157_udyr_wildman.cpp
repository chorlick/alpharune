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

class UdyrWildman : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "Spend my buff: Choose one you've not chosen this turn — Deal 2 to a unit
    // at a battlefield / Stun a unit at a battlefield / Ready me / Give me
    // [Ganking] this turn." Spending the buff is the activation cost: the
    // mode, its unit and the payment are settled as the ability is activated
    // (CR 355), by the Udyr that activates it.
    unsigned choicesAtFinalization() const override { return kFinalizeActivate; }
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override { return {}; }
    // Modes this Udyr has already chosen this turn (a per-turn bitmask).
    static uint32_t chosenModes(const GameState& state, const GameObject& self) {
        auto turn = self.card_counters.find("__udyr_modes_turn");
        auto mask = self.card_counters.find("__udyr_modes_mask");
        if (turn == self.card_counters.end() || mask == self.card_counters.end()) return 0;
        return turn->second == state.turn.turn_number ? static_cast<uint32_t>(mask->second) : 0;
    }
    bool canActivateAbilityAt(const GameState& state, PlayerId controller,
                              GameObjectId source, int ability_index) const override {
        if (!Card::canActivateAbilityAt(state, controller, source, ability_index)) return false;
        const auto& self = state.getObject(source);
        // A buff to spend, and a mode not yet chosen this turn.
        return self.buff_count > 0 && (chosenModes(state, self) & 0xF) != 0xF;
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& self = ctx.state.getObject(ctx.source);
        const uint32_t chosen_mask = chosenModes(ctx.state, self);
        int mode = pickMode(ctx, "Udyr (spend buff)", 4,
                            {"Deal 2", "Stun", "Ready me", "Give Ganking"},
                            (~chosen_mask) & 0xF);
        if (mode < 0) return;   // waiting / no mode left this turn
        // "a unit at a battlefield", minus enemy units that can't be chosen
        // by enemy spells and abilities.
        GameObjectId t = kInvalidId;
        if (mode == 0 || mode == 1) {
            auto bf_units = chooseableUnits(ctx.state, ctx.controller,
                                            [](const GameObject& o) { return o.isAtBattlefield(); });
            std::sort(bf_units.begin(), bf_units.end());
            t = pickTarget(ctx, mode == 0 ? "Udyr deal 2" : "Udyr stun", bf_units);
            if (t == kInvalidId) return;   // waiting / no unit / mistarget
        }
        // The cost, once: my buff, and this mode is used up for the turn.
        if (costDueNow(ctx)) {
            if (!self.location.has_value() || self.buff_count <= 0) return;
            self.buff_count -= 1;
            self.recomputeMight();
            self.card_counters["__udyr_modes_turn"] = ctx.state.turn.turn_number;
            self.card_counters["__udyr_modes_mask"] =
                static_cast<int>(chosen_mask | (1u << mode));
        }
        if (finalizingChoices(ctx)) return;   // choices and costs end here (CR 355)
        switch (mode) {
        case 0:  // Deal 2 to a unit at a battlefield
            ctx.executor.dealDamage(t, 2, ctx.source);
            if (ctx.state.getObject(t).location.has_value() &&
                ctx.state.getObject(t).hasLethalDamage())
                ctx.executor.killObject(t);
            break;
        case 1:  // Stun a unit at a battlefield
            ctx.executor.stunUnitBy(t, ctx.source);
            break;
        case 2:  // Ready me
            ctx.executor.readyObject(ctx.source);
            break;
        case 3:  // Give me Ganking this turn
            if (self.location.has_value())
                ctx.executor.giveTemporaryKeyword(ctx.source, Keyword::Ganking, 1);
            break;
        }
        ctx.events.logTrace("UDYR: spent a buff, mode=" + std::to_string(mode));
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 157;
        d.def_id = R"RB(ogn-157-298)RB";
        d.name = R"RB(Udyr, Wildman)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-157/298)RB";
        d.collector_number = 157;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Udyr)RB", R"RB(Freljord)RB"};
        d.energy_cost = 6;
        d.power_cost = 1;
        d.might = 6;
        d.rarity = Rarity::Rare;
        // No printed [Ganking]: it is only what the fourth mode gives this turn.
        d.ability_text = R"RB(Spend my buff: Choose one you've not chosen this turn —
Deal 2 to a unit at a battlefield.Stun a unit at a battlefield.Ready me.Give me [Ganking] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/2e461e5d05b5ee86e47a425c42c6f3b3c12bc4cf-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_157(CardRegistry& r) {
    r.registerCard(157, std::make_unique<UdyrWildman>());
}

} // namespace riftbound
