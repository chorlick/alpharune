#pragma once
/// @file weaponmaster_base.h
/// Shared base class for [Weaponmaster] units. Lives in the units/ type dir
/// (relocated from the old _shared holding pen). Derived per-card files include
/// this header.

#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "engine/effect_executor.h"

#include <algorithm>
#include <memory>
#include <vector>

namespace riftbound {

/// Base class for Weaponmaster units (CR 821): "When you play me, you may
/// choose a card you control with the Equipment tag. ... Pay the cost of its
/// Equip ability, reduced by [A], to attach it to this unit."
///
/// The "may" and the Equipment are the controller's choices. The choices are
/// made as the trigger resolves (derived cards do not opt the base into
/// choices-at-finalization).
/// ENGINE-GAP (plan 07 item 7.7): the Equip cost is not charged. A gear's
/// Equip cost lives inside its onEquip and is not exposed as data, so "its
/// cost reduced by [A]" cannot be computed here; it needs an equip-cost
/// descriptor on Card.
class WeaponmasterUnit : public UnitCard {
public:
    // Abstract: concrete weaponmaster cards supply their data via def().
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }

    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // The unit must still be on the board to be equipped (it may have
        // died or been bounced in response): objectExists alone is true for
        // every id ever created.
        auto self_on_board = [&ctx]() {
            return ctx.state.objectExists(ctx.source) &&
                   ctx.state.getObject(ctx.source).isUnit() &&
                   ctx.state.getObject(ctx.source).location.has_value();
        };
        if (!self_on_board()) return;

        // Equipment the controller has on the board, attached or not
        // ("even if it's already attached") — other than what this unit
        // already carries.
        std::vector<GameObjectId> equipment;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isGear() || obj.controller != ctx.controller) continue;
            if (!obj.location.has_value()) continue;
            if (obj.attached_to.has_value() && *obj.attached_to == ctx.source) continue;
            if (std::find(obj.tags.begin(), obj.tags.end(), "Equipment") == obj.tags.end()) continue;
            equipment.push_back(id);
        }
        std::sort(equipment.begin(), equipment.end());
        auto still_legal = [&]() { return self_on_board() && !equipment.empty(); };
        if (!still_legal()) return;

        // "you may" — optional.
        int conf = confirmOptional(ctx, "Weaponmaster: equip an Equipment to me?", still_legal);
        if (conf < 1) return;
        GameObjectId best_gear = pickTarget(ctx, "Weaponmaster: choose an Equipment", equipment);
        if (best_gear == kInvalidId || !ctx.state.objectExists(best_gear)) return;
        if (!self_on_board()) return;

        auto& gear = ctx.state.getObject(best_gear);
        if (!gear.location.has_value()) return;   // left the board since it was picked

        // "Even if it's already attached" — detach from current unit first
        if (gear.attached_to.has_value()) {
            auto old_unit = *gear.attached_to;
            if (ctx.state.objectExists(old_unit)) {
                auto& old = ctx.state.getObject(old_unit);
                old.attachment_might_bonus -= gear.might_bonus;
                auto it = std::find(old.attachments.begin(), old.attachments.end(), best_gear);
                if (it != old.attachments.end()) old.attachments.erase(it);
                old.recomputeMight();
                ctx.events.logTrace("WEAPONMASTER: detached " + gear.name +
                                     " from " + old.name);
            }
            gear.attached_to = std::nullopt;
        }

        // Attach to self (the Equip cost is not charged — see ENGINE-GAP above)
        auto& self = ctx.state.getObject(ctx.source);
        ctx.events.logTrace("WEAPONMASTER: " + self.name + " equips " + gear.name +
                             " (free, [A] less)");

        gear.attached_to = ctx.source;
        self.attachments.push_back(best_gear);
        gear.location = self.location;
        gear.zone = self.zone;
        self.attachment_might_bonus += gear.might_bonus;
        self.recomputeMight();

        ctx.events.emit(ObjectStateChangedEvent{best_gear, "attached"});
        ctx.events.emit(ObjectStateChangedEvent{ctx.source, "equipped"});
    }
};

} // namespace riftbound
