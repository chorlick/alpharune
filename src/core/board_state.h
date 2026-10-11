#pragma once
/// @file board_state.h
/// What an object carries only while it is on the board.
///
/// CR 124: "A Game Object that changes zones to or from a Non-Board Zone
/// becomes a new object ... all Temporary Modifications of all kinds cease to
/// be tracked on it in all capacities. Damage is cleared. Counters are
/// removed. Granted Keywords are no longer granted. Statuses are cleared."
/// (CR 124.2 lists the statuses: Attached, Attacking, Buffed, Controlled,
/// Defending, Empowered, Equipped, Exhausted, Stunned, ...).
///
/// `forgetBoardState` is that rule for the engine's GameObject. Every path
/// that takes an object off the board calls it at the move — through
/// EffectExecutor::leftBoard, which also sheds attachments, or directly where
/// there is no executor and nothing can be attached (a rune recycled for
/// Power). Before, each path reset the fields it knew of (damage here,
/// exhaustion there) and none reset them all; Stunned was reset by none
/// (plans/09 items 9.LL1, 9.LL4).
///
/// What is NOT forgotten, and why:
///  - `last_known`, `last_location`: the snapshot of the object as it was on
///    the board, taken by every leave path before the move
///    (EffectExecutor::snapshotLastKnown) for exactly the readers that come
///    after it: "When I die" abilities (CR 383.2.c.3.b) and effects that
///    move a card and then speak of it (CR 359.3.e.13) — its controller
///    then (LastKnown::controller), its Might, where it was.
///  - `card_counters`, `string_state`, `tracked_objects`: the engine's and the
///    cards' own memory (a per-play payment flag, which spell killed it, what
///    a card named), not counters in the sense of the rules. Abilities that
///    resolve after the move read them.
///  - identity (`card_def_id`, name, type, tags, domains, owner) and the
///    token bit.
///  - facedown state (`is_hidden`, `hidden_at`): a facedown card is not on the
///    board to begin with; the hide code owns those two.

#include "card_db.h"
#include "game_object.h"

namespace riftbound {

/// The object has just left the board: it is a new object (CR 124.1).
/// `def` is its card (nullptr for an object with no card behind it, e.g. a
/// plain token): the printed keywords and values are what it has again.
/// The caller has already shed its attachments (EffectExecutor::leftBoard).
inline void forgetBoardState(GameObject& obj, const CardDef* def) {
    // Statuses.
    obj.is_exhausted = false;
    obj.is_stunned = false;
    obj.combat_designation = CombatDesignation::None;
    obj.empowered = 0;
    obj.empower_flip_eot = 0;
    obj.empower_cost_reduction = 0;
    obj.is_rules_text_inactive = false;
    // Controlled: in a non-board zone a card is its owner's (CR 191.5.a.1).
    if (obj.owner != PlayerId::None) obj.controller = obj.owner;
    obj.control_reverts_eot = false;
    obj.original_controller_eot = PlayerId::None;
    obj.control_reverts_on_source_leave = false;
    obj.control_source_obj = kInvalidId;
    obj.control_revert_to = PlayerId::None;
    // Damage.
    obj.damage_marked = 0;
    obj.damage_marked_by[0] = obj.damage_marked_by[1] = 0;
    obj.damage_taken_this_turn = 0;
    obj.damage_doubled_this_turn = false;
    obj.prevent_next_damage_this_turn = false;
    obj.prevent_damage_remaining = 0;
    // Buffs and "this turn" Might.
    obj.buff_count = 0;
    obj.temp_buff_count = 0;
    obj.temp_might_bonus = 0;
    obj.moves_this_turn = 0;
    // Keywords: the printed ones again.
    if (def) {
        obj.keywords = def->keywords;
        obj.assault_value = def->assault_value;
        obj.shield_value = def->shield_value;
        obj.deflect_value = def->deflect_value;
    } else {
        obj.keywords.bits &= ~(obj.temp_keywords.bits | obj.granted_keywords.bits);
        obj.assault_value = std::max(0, obj.assault_value - obj.temp_assault_value);
        obj.shield_value = std::max(0, obj.shield_value - obj.temp_shield_value);
    }
    obj.temp_keywords.reset();
    obj.granted_keywords.reset();
    obj.temp_assault_value = 0;
    obj.temp_shield_value = 0;
    obj.temp_deflect_value = 0;
    // Other "this turn" and "next time" modifications.
    obj.untargetable_by_enemy = false;
    obj.untargetable_by_enemy_this_turn = false;
    obj.cant_move_this_turn = false;
    obj.cant_move_to_base = false;
    obj.death_replacement_recall_pending = false;
    obj.banish_if_would_die_this_turn = false;
    obj.pending_death_replacements.clear();
    obj.temp_granted_abilities.clear();
    // Continuous effects (the aura pass recomputes them for what is in play;
    // nothing in play gives them to this object unless a hook says so again).
    obj.granted_abilities.clear();
    obj.aura_effects.clear();
    obj.aura_might_bonus = 0;
    obj.aura_keywords.reset();
    obj.aura_no_combat_damage = false;
    obj.aura_bonus_damage_taken = 0;
    obj.immune_to_damage = false;
    obj.spells_targeting_me_cost_reduction = 0;
    obj.prevent_enemy_spell_ability_damage = false;
    obj.cant_be_moved_by_enemy = false;
    obj.aura_cant_move_to_base = false;
    // Attachments were shed by the caller; what they added goes with them.
    obj.attachment_might_bonus = 0;
    // Off the board a unit has its printed Might (CR 711).
    obj.current_might = obj.base_might;
}

/// Whether an object that is not on the board still carries something
/// forgetBoardState drops. For checks: an off-board object for which this is
/// true was taken off the board by a path that did not go through the rule.
/// (Continuous-effect fields are left out: a hook may legitimately give an
/// effect to a card in a hand — Equipment with [Quick-Draw] — and the aura
/// pass owns those.)
inline bool carriesBoardState(const GameObject& obj) {
    return obj.is_exhausted || obj.is_stunned ||
           obj.combat_designation != CombatDesignation::None || obj.empowered != 0 ||
           obj.empower_flip_eot != 0 || obj.damage_marked != 0 || obj.buff_count != 0 ||
           obj.temp_buff_count != 0 || obj.temp_might_bonus != 0 ||
           obj.temp_keywords.bits != 0 || obj.granted_keywords.bits != 0 ||
           obj.temp_assault_value != 0 || obj.temp_shield_value != 0 ||
           obj.temp_deflect_value != 0 || obj.control_reverts_eot ||
           obj.control_reverts_on_source_leave || obj.attached_to.has_value() ||
           !obj.attachments.empty() || obj.cant_move_this_turn ||
           obj.death_replacement_recall_pending || obj.banish_if_would_die_this_turn ||
           !obj.pending_death_replacements.empty() || !obj.temp_granted_abilities.empty();
}

}  // namespace riftbound
