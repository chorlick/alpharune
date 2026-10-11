#pragma once
/// @file equip_base.h
/// Shared equip infrastructure for gear cards — the canonical standardEquip
/// helper plus the SimpleEquipGear / UniversalEquipGear base classes. Lives in
/// the gear/ type dir (relocated from the old _shared holding pen). This is the
/// ONE canonical standardEquip; the old per-audit-file copies redirect here.

#include "cards/card_helpers.h"   // standardEquip, payEnergy, payOnePower

#include <memory>
#include <optional>
#include <vector>

namespace riftbound {

// ─── Canonical standard equip: pay [energy] + one [domain] power, then attach ──
// Thin overload of card_helpers.h's standardEquip for a fixed Domain. The cost
// goes through payEnergy / payOnePower, so the Power is accounted the way the
// engine accounts it (power_spent_this_turn, the rune's LeftBoardEvent) and
// floating Energy / Power can pay it.
inline bool standardEquip(CardContext& ctx, GameObjectId gear_id, GameObjectId unit_id,
                          int energy_cost, Domain domain) {
    return standardEquip(ctx, gear_id, unit_id, energy_cost, std::optional<Domain>{domain});
}

// ─── Simple equip gear: "[Equip] [N][DOMAIN]" ───────────────────────────────
// The cost is declared (Card::equipCost); the engine charges it and attaches.
class SimpleEquipGear : public GearCard {
public:
    // Behavior config only (equip domain + energy). The concrete card supplies
    // its data via def().
    explicit SimpleEquipGear(Domain equip_domain, int energy_cost = 0)
        : equip_domain_(equip_domain), energy_cost_(energy_cost) {}

    bool hasEquipAbility() const override { return true; }
    EquipCost equipCost(const GameState&, PlayerId, GameObjectId, GameObjectId) const override {
        return {.valid = true, .energy = energy_cost_, .power = 1, .power_domain = equip_domain_};
    }

protected:
    Domain equip_domain_;
    int energy_cost_;
};

// ─── Universal equip gear: pay [A] (recycle any rune), then attach ──────────
class UniversalEquipGear : public GearCard {
public:
    explicit UniversalEquipGear(int energy_cost = 0)
        : energy_cost_(energy_cost) {}

    bool hasEquipAbility() const override { return true; }
    EquipCost equipCost(const GameState&, PlayerId, GameObjectId, GameObjectId) const override {
        return {.valid = true, .energy = energy_cost_, .power = 1, .power_any_domain = true};
    }

protected:
    int energy_cost_;
};

} // namespace riftbound
