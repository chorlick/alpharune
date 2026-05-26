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

class UdyrWildman : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "Spend my buff: Choose one you've not chosen this turn — Deal 2 to a unit
    // at a battlefield / Stun a unit at a battlefield / Ready me / Give me
    // [Ganking] this turn." Cost = spend a buff (gated below).
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override { return {}; }
    bool canActivateAbility(const GameState& state, PlayerId controller) const override {
        // Need a buff to spend.
        for (auto& [id, obj] : state.objects) {
            if (obj.card_def_id == cardDefId() && obj.controller == controller &&
                obj.location.has_value() && obj.buff_count > 0)
                return true;
        }
        return false;
    }

    void onActivate(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& self = ctx.state.getObject(ctx.source);
        // Modes already chosen this turn live in a per-turn bitmask.
        int turn = ctx.state.turn.turn_number;
        if (self.card_counters["__udyr_modes_turn"] != turn) {
            self.card_counters["__udyr_modes_turn"] = turn;
            self.card_counters["__udyr_modes_mask"] = 0;
        }
        uint32_t chosen_mask =
            static_cast<uint32_t>(self.card_counters["__udyr_modes_mask"]);
        uint32_t legal = (~chosen_mask) & 0xF;  // 4 modes
        int mode = pickMode(ctx, "Udyr (spend buff)", 4,
                            {"Deal 2", "Stun", "Ready me", "Give Ganking"}, legal);
        if (mode == -1) return;   // suspended
        if (mode == -2) return;   // no legal mode
        // Spend the buff (the cost) once per activation. On re-entry (after a
        // pickTarget suspend) this mode's bit is already set, so don't double-
        // spend.
        bool first_commit = !(chosen_mask & (1u << mode));
        if (first_commit) {
            if (self.buff_count <= 0) return;
            self.buff_count -= 1;
            self.recomputeMight();
            self.card_counters["__udyr_modes_mask"] =
                static_cast<int>(chosen_mask | (1u << mode));
        }

        auto bf_units = [&]() {
            std::vector<GameObjectId> out;
            for (auto& [id, obj] : ctx.state.objects)
                if (obj.isUnit() && obj.isAtBattlefield()) out.push_back(id);
            return out;
        };
        switch (mode) {
        case 0: {  // Deal 2 to a unit at a battlefield
            GameObjectId t = pickTarget(ctx, "Udyr deal 2", bf_units());
            if (t == kInvalidId) return;
            ctx.executor.dealDamage(t, 2, ctx.source);
            if (ctx.state.objectExists(t) && ctx.state.getObject(t).hasLethalDamage())
                ctx.executor.killObject(t);
            break;
        }
        case 1: {  // Stun a unit at a battlefield
            GameObjectId t = pickTarget(ctx, "Udyr stun", bf_units());
            if (t == kInvalidId) return;
            ctx.executor.stunUnitBy(t, ctx.source);
            break;
        }
        case 2:  // Ready me
            ctx.executor.readyObject(ctx.source);
            break;
        case 3:  // Give me Ganking this turn
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
        d.keywords.set(Keyword::Ganking);
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
