#pragma once
/// @file equip_base.h
/// Shared equip infrastructure for gear cards — the canonical standardEquip
/// helper plus the SimpleEquipGear / UniversalEquipGear base classes. Lives in
/// the gear/ type dir (relocated from the old _shared holding pen). This is the
/// ONE canonical standardEquip; the old per-audit-file copies redirect here.

#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "engine/effect_executor.h"

#include <algorithm>
#include <memory>
#include <vector>

namespace riftbound {

// ─── Canonical standard equip: pay [energy] + one [domain] power, then attach ──
inline bool standardEquip(CardContext& ctx, GameObjectId gear_id, GameObjectId unit_id,
                          int energy_cost, Domain domain) {
    auto& state = ctx.state;
    auto player = ctx.controller;
    auto& ps = state.player(player);
    auto base_loc = BaseLocation{player};

    // PRE-CHECK: bail (no state change) unless BOTH the energy and the
    // domain-power can be paid — otherwise the loops below would partially
    // pay and then "equip for free".
    int ready_count = 0;
    GameObjectId domain_rune = kInvalidId;
    for (auto& [id, obj] : state.objects) {
        if (!obj.isRune() || obj.controller != player) continue;
        if (!obj.location.has_value() || *obj.location != LocationId{base_loc}) continue;
        if (!obj.is_exhausted) ready_count++;
        if (domain_rune == kInvalidId) {
            for (auto d : obj.domains) {
                if (d == domain) { domain_rune = id; break; }
            }
        }
    }
    if (ready_count < energy_cost) return false;
    if (domain_rune == kInvalidId)  return false;

    // Pay energy: exhaust ready runes.
    if (energy_cost > 0) {
        int e_remaining = energy_cost;
        for (auto& [id, obj] : state.objects) {
            if (e_remaining <= 0) break;
            if (!obj.isRune() || obj.controller != player || obj.is_exhausted) continue;
            if (!obj.location.has_value() || *obj.location != LocationId{base_loc}) continue;
            obj.is_exhausted = true;
            e_remaining--;
        }
    }

    // Recycle the pre-located matching-domain rune for power.
    {
        auto& dr = state.getObject(domain_rune);
        ctx.events.logTrace("  EQUIP_COST: recycled " + dr.name + " for power");
        dr.location = std::nullopt;
        dr.zone = ZoneType::RuneDeck;
        ps.rune_deck.insert(ps.rune_deck.begin(), domain_rune);
    }

    // Attach.
    auto& gear = state.getObject(gear_id);
    auto& unit = state.getObject(unit_id);
    ctx.events.logTrace("EQUIP: " + gear.name + " -> " + unit.name +
                         " (might_bonus=" + std::to_string(gear.might_bonus) + ")");
    gear.attached_to = unit_id;
    unit.attachments.push_back(gear_id);
    gear.location = unit.location;
    gear.zone = unit.zone;
    unit.attachment_might_bonus += gear.might_bonus;
    unit.recomputeMight();
    ctx.events.emit(ObjectStateChangedEvent{gear_id, "attached"});
    ctx.events.emit(ObjectStateChangedEvent{unit_id, "equipped"});
    return true;
}

// ─── Simple equip gear: pay [DOMAIN] power, then attach ─────────────────────
class SimpleEquipGear : public GearCard {
public:
    // Behavior config only (equip domain + energy). The concrete card supplies
    // its data via def().
    explicit SimpleEquipGear(Domain equip_domain, int energy_cost = 0)
        : equip_domain_(equip_domain), energy_cost_(energy_cost) {}

    bool hasEquipAbility() const override { return true; }
    bool onEquip(CardContext& ctx, GameObjectId unit) override {
        return standardEquip(ctx, ctx.source, unit, energy_cost_, equip_domain_);
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
    bool onEquip(CardContext& ctx, GameObjectId unit) override {
        auto& state = ctx.state;
        auto player = ctx.controller;
        auto& ps = state.player(player);
        auto base_loc = BaseLocation{player};

        for (int i = 0; i < energy_cost_; ++i) {
            for (auto& [id, obj] : state.objects) {
                if (!obj.isRune() || obj.controller != player || obj.is_exhausted) continue;
                if (!obj.location.has_value() || *obj.location != LocationId{base_loc}) continue;
                obj.is_exhausted = true;
                break;
            }
        }
        for (auto& [id, obj] : state.objects) {
            if (!obj.isRune() || obj.controller != player) continue;
            if (!obj.location.has_value() || *obj.location != LocationId{base_loc}) continue;
            ctx.events.logTrace("  EQUIP_COST: recycled " + obj.name + " for [A]");
            obj.location = std::nullopt;
            obj.zone = ZoneType::RuneDeck;
            ps.rune_deck.insert(ps.rune_deck.begin(), id);
            break;
        }
        auto& gear = state.getObject(ctx.source);
        auto& unit_obj = state.getObject(unit);
        ctx.events.logTrace("EQUIP: " + gear.name + " -> " + unit_obj.name);
        gear.attached_to = unit;
        unit_obj.attachments.push_back(ctx.source);
        gear.location = unit_obj.location;
        gear.zone = unit_obj.zone;
        unit_obj.attachment_might_bonus += gear.might_bonus;
        unit_obj.recomputeMight();
        ctx.events.emit(ObjectStateChangedEvent{ctx.source, "attached"});
        ctx.events.emit(ObjectStateChangedEvent{unit, "equipped"});
        return true;
    }

protected:
    int energy_cost_;
};

} // namespace riftbound
