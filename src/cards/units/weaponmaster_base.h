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

/// Base class for Weaponmaster units.
/// On play trigger: find Equipment on board, detach if needed, attach to self.
/// Cost reduced by [A] (not implemented yet — simplified to free equip).
class WeaponmasterUnit : public UnitCard {
public:
    // Abstract: concrete weaponmaster cards supply their data via def().
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }

    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // Find any Equipment gear controlled by this player
        GameObjectId best_gear = kInvalidId;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isGear() || obj.controller != ctx.controller) continue;
            if (!obj.location.has_value()) continue;
            bool is_equipment = false;
            for (auto& tag : obj.tags) {
                if (tag == "Equipment") { is_equipment = true; break; }
            }
            if (!is_equipment) continue;
            best_gear = id;
            break; // take first available (agent choice in future)
        }

        if (best_gear == kInvalidId) return;

        auto& gear = ctx.state.getObject(best_gear);

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

        // Attach to self (free — [A] less cost means no cost for simple equip)
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
