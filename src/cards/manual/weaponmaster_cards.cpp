// Manual Weaponmaster card implementations.
// Weaponmaster: "When you play me, you may [Equip] one of your Equipment
// to me for [A] less, even if it's already attached."

#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "engine/effect_executor.h"

#include <memory>

namespace riftbound {

/// Base class for Weaponmaster units.
/// On play trigger: find Equipment on board, detach if needed, attach to self.
/// Cost reduced by [A] (not implemented yet — simplified to free equip).
class WeaponmasterUnit : public UnitCard {
public:
    using UnitCard::UnitCard;

    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }

    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        // Find any Equipment gear controlled by this player
        GameObjectId best_gear = kInvalidId;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isGear() || obj.controller != ctx.controller) continue;
            if (!obj.location.has_value()) continue;
            // Check if it's an Equipment (has Equip keyword or tag)
            const auto& def_check = obj.tags;
            bool is_equipment = false;
            for (auto& tag : def_check) {
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

// [324] Armed Assailant — Weaponmaster + Accelerate
class UArmedAssailant : public WeaponmasterUnit {
public:
    UArmedAssailant() : WeaponmasterUnit(324) {}
};

// [331] Sentinel Adept — Weaponmaster
class USentinelAdept : public WeaponmasterUnit {
public:
    USentinelAdept() : WeaponmasterUnit(331) {}
};

// [407] Ornn, Forge God — Weaponmaster + Deflect 2
class UOrnn : public WeaponmasterUnit {
public:
    UOrnn() : WeaponmasterUnit(407) {}
};

// [414] Combat Chef — Weaponmaster
class UCombatChef : public WeaponmasterUnit {
public:
    UCombatChef() : WeaponmasterUnit(414) {}
};

// [421] Veteran Poro — Weaponmaster
class UVeteranPoro : public WeaponmasterUnit {
public:
    UVeteranPoro() : WeaponmasterUnit(421) {}
};

// [435] Lucian, Merciless — Weaponmaster + "first time I conquer each turn, ready me"
class ULucianMerciless : public WeaponmasterUnit {
public:
    ULucianMerciless() : WeaponmasterUnit(435) {}
};

// [440] Jax, Unrelenting — Weaponmaster + "When you attach Equipment, may pay [1] to draw 1"
class UJaxUnrelenting : public WeaponmasterUnit {
public:
    UJaxUnrelenting() : WeaponmasterUnit(440) {}
};

// [448] Master Bingwen — Weaponmaster
class UMasterBingwen : public WeaponmasterUnit {
public:
    UMasterBingwen() : WeaponmasterUnit(448) {}
};

// [544] Yone, Blademaster — Weaponmaster
class UYoneBlademaster : public WeaponmasterUnit {
public:
    UYoneBlademaster() : WeaponmasterUnit(544) {}
};

// ─── Registration ───────────────────────────────────────────────────────────

void registerManualWeaponmasterCards(CardRegistry& registry) {
    registry.registerCard(324, std::make_unique<UArmedAssailant>());
    registry.registerCard(331, std::make_unique<USentinelAdept>());
    registry.registerCard(407, std::make_unique<UOrnn>());
    registry.registerCard(414, std::make_unique<UCombatChef>());
    registry.registerCard(421, std::make_unique<UVeteranPoro>());
    registry.registerCard(435, std::make_unique<ULucianMerciless>());
    registry.registerCard(440, std::make_unique<UJaxUnrelenting>());
    registry.registerCard(448, std::make_unique<UMasterBingwen>());
    registry.registerCard(544, std::make_unique<UYoneBlademaster>());
}

} // namespace riftbound
