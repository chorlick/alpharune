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
/// The "may" and the Equipment are the controller's choices, made when the
/// ability is finalized (CR 383.3, kFinalizeTrigger); the cost is paid and the
/// Equipment attached as it resolves. A derived card with further triggers
/// marks the end of the choices in each of its own branches
/// (finalizingChoices).
/// The Equip cost is the gear's own declaration (Card::equipCost), charged
/// through Card::payEquipAndAttach with one Power taken off (a cost with no
/// Power is not reduced — CR 821.1.c.3) and "even if it's already attached".
/// The Equip ability is not activated this way (CR 821.1.c.6), so effects on
/// activated abilities do not apply. An Equipment whose cost cannot be paid
/// is not offered: choosing it would change nothing (CR 821.1.c.5).
class WeaponmasterUnit : public UnitCard {
public:
    // Abstract: concrete weaponmaster cards supply their data via def().
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }

    /// The terms Weaponmaster pays an Equip cost on.
    static EquipTerms weaponmasterTerms() {
        EquipTerms t;
        t.power_discount = 1;
        t.even_if_attached = true;
        return t;
    }
    /// Equipment `controller` could attach to `unit` through Weaponmaster now.
    static std::vector<GameObjectId> weaponmasterChoices(const CardContext& ctx,
                                                         GameObjectId unit) {
        std::vector<GameObjectId> equipment;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isGear() || obj.controller != ctx.controller) continue;
            if (!obj.location.has_value()) continue;
            if (std::find(obj.tags.begin(), obj.tags.end(), "Equipment") == obj.tags.end()) continue;
            // A card with no Equip cost can't be equipped this way (CR
            // 821.1.c.4) — that includes an object with no card behind it.
            if (obj.card_def_id == kInvalidId) continue;
            const Card* gc = ctx.registry ? ctx.registry->get(obj.card_def_id) : nullptr;
            if (!gc || !gc->canPayEquip(ctx.state, ctx.controller, id, unit, weaponmasterTerms()))
                continue;
            equipment.push_back(id);
        }
        std::sort(equipment.begin(), equipment.end());
        return equipment;
    }
    /// Pay `gear`'s Equip cost on Weaponmaster's terms and attach it to `unit`.
    static bool weaponmasterEquip(CardContext& ctx, GameObjectId gear, GameObjectId unit) {
        if (!ctx.state.objectExists(gear)) return false;
        if (ctx.state.getObject(gear).card_def_id == kInvalidId) return false;   // CR 821.1.c.4
        Card* gc = ctx.registry ? ctx.registry->get(ctx.state.getObject(gear).card_def_id)
                                : nullptr;
        if (!gc) return false;
        CardContext gear_ctx{ctx.state, ctx.events, ctx.executor, ctx.controller, gear};
        gear_ctx.registry = ctx.registry;
        if (!gc->payEquipAndAttach(gear_ctx, unit, weaponmasterTerms())) return false;
        RIFT_TRACE(ctx.events, "WEAPONMASTER: " + ctx.state.getObject(unit).name + " equips " +
                            ctx.state.getObject(gear).name + " (Equip cost, [A] less)");
        return true;
    }

    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // The unit must still be on the board to be equipped (it may have
        // died or been bounced in response): objectExists alone is true for
        // every id ever created.
        auto self_on_board = [&ctx]() {
            return ctx.state.objectExists(ctx.source) &&
                   ctx.state.getObject(ctx.source).isUnit() &&
                   ctx.state.getObject(ctx.source).location.has_value();
        };
        // Equipment the controller has on the board, attached or not ("even
        // if it's already attached") — other than what this unit already
        // carries — whose reduced Equip cost can be paid.
        const std::vector<GameObjectId> equipment =
            self_on_board() ? weaponmasterChoices(ctx, ctx.source) : std::vector<GameObjectId>{};
        auto still_legal = [&]() { return self_on_board() && !equipment.empty(); };

        // "you may" — optional; then which Equipment.
        const int conf = still_legal()
            ? confirmOptional(ctx, "Weaponmaster: equip an Equipment to me?", still_legal) : 0;
        GameObjectId best_gear = kInvalidId;
        if (conf == 1)
            best_gear = pickTarget(ctx, "Weaponmaster: choose an Equipment", equipment);
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (conf < 1) return;
        if (best_gear == kInvalidId || !ctx.state.objectExists(best_gear)) return;
        if (!self_on_board()) return;
        // Paid and attached as the ability resolves; an Equipment already on
        // another unit is taken off it by the executor (unattachGear, inside
        // Card::payEquipAndAttach).
        weaponmasterEquip(ctx, best_gear, ctx.source);
    }
};

} // namespace riftbound
