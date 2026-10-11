#pragma once
/// @file equip_base.h
/// Shared equip infrastructure for gear cards: the SimpleEquipGear /
/// UniversalEquipGear base classes, which declare an Equip cost
/// (Card::equipCost) for the engine to charge.

#include "cards/card_helpers.h"

#include <memory>
#include <optional>
#include <vector>

namespace riftbound {

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
