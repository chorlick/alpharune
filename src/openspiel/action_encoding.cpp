#include "action_encoding.h"

namespace riftbound::openspiel {

namespace {

/// CardDefId of a GameObject, or 0 when invalid/missing.
/// 0 is also the sentinel for "no object" (real CardDefIds start at 1).
uint32_t defIdOf(const GameState& state, GameObjectId obj) {
    if (obj == kInvalidId) return 0;
    if (!state.objectExists(obj)) return 0;
    uint32_t def = state.getObject(obj).card_def_id;
    if (def == kInvalidId) return 0;
    return def;
}

/// Encode a BattlefieldId as 0 (none) or BF id + 1 (1..N).
/// Bit width is 3 → up to 7 BFs encodable; Riftbound currently has ≤4.
uint32_t encodeBfSlot(BattlefieldId bf) {
    if (bf == kInvalidId) return 0;
    return (bf + 1) & static_cast<uint32_t>(kDestBfMask);
}

/// Encode the play_location's BattlefieldId, or 0 for base/none.
uint32_t encodePlayLocBf(const std::optional<LocationId>& loc) {
    if (!loc.has_value()) return 0;
    if (auto* bf = std::get_if<BattlefieldLocation>(&*loc)) {
        return encodeBfSlot(bf->id);
    }
    return 0;  // base location
}

/// Encode the move_destination's BattlefieldId, or 0 for base/none.
uint32_t encodeMoveDestBf(const std::optional<LocationId>& loc) {
    return encodePlayLocBf(loc);  // same scheme
}

int64_t pack(IntentType type,
             uint32_t card_def,
             uint32_t target1,
             uint32_t target2,
             uint32_t dest_bf,
             uint32_t ability_source) {
    int64_t out = 0;
    out |= (static_cast<int64_t>(type)    & kTypeMask)          << kTypeShift;
    out |= (static_cast<int64_t>(card_def)& kCardMask)          << kCardShift;
    out |= (static_cast<int64_t>(target1) & kTarget1Mask)       << kTarget1Shift;
    out |= (static_cast<int64_t>(target2) & kTarget2Mask)       << kTarget2Shift;
    out |= (static_cast<int64_t>(dest_bf) & kDestBfMask)        << kDestBfShift;
    out |= (static_cast<int64_t>(ability_source) & kAbilitySourceMask) << kAbilitySourceShift;
    return out;
}

} // namespace

int64_t encodeAction(const Intent& intent, const GameState& state) {
    using T = IntentType;

    // Per-type slot mapping. Each branch fills the six slots from
    // intent fields that meaningfully distinguish this action from
    // its peers in any legal-action list.

    switch (intent.type) {
        // ── Play-from-zone family ────────────────────────────────────────
        case T::PlayCard:
        case T::PlayActionCard:
        case T::PlayReaction:
        case T::HideCard: {
            uint32_t card_def = defIdOf(state, intent.card);
            uint32_t tgt1 = intent.targets.size() > 0
                ? defIdOf(state, intent.targets[0]) : 0;
            uint32_t tgt2 = intent.targets.size() > 1
                ? defIdOf(state, intent.targets[1]) : 0;
            uint32_t dest = encodePlayLocBf(intent.play_location);
            return pack(intent.type, card_def, tgt1, tgt2, dest, 0);
        }

        // ── Ability-activation family ────────────────────────────────────
        case T::ActivateAbility:
        case T::ActivateReactionAbility:
        case T::ActivateActionAbility: {
            uint32_t source_def = defIdOf(state, intent.ability_source);
            uint32_t tgt1 = intent.targets.size() > 0
                ? defIdOf(state, intent.targets[0]) : 0;
            uint32_t tgt2 = intent.targets.size() > 1
                ? defIdOf(state, intent.targets[1]) : 0;
            uint32_t ability = intent.ability == kInvalidId
                ? 0 : (intent.ability & static_cast<uint32_t>(kAbilitySourceMask));
            return pack(intent.type, source_def, tgt1, tgt2, 0, ability);
        }

        // ── Standard movement ────────────────────────────────────────────
        case T::StandardMove: {
            uint32_t u0 = intent.units_to_move.size() > 0
                ? defIdOf(state, intent.units_to_move[0]) : 0;
            uint32_t u1 = intent.units_to_move.size() > 1
                ? defIdOf(state, intent.units_to_move[1]) : 0;
            uint32_t count = static_cast<uint32_t>(intent.units_to_move.size());
            uint32_t dest = encodeMoveDestBf(intent.move_destination);
            return pack(intent.type, u0, u1, count, dest, 0);
        }

        // ── Mulligan ─────────────────────────────────────────────────────
        case T::MulliganDecision: {
            uint32_t c0 = intent.cards_to_mulligan.size() > 0
                ? defIdOf(state, intent.cards_to_mulligan[0]) : 0;
            uint32_t c1 = intent.cards_to_mulligan.size() > 1
                ? defIdOf(state, intent.cards_to_mulligan[1]) : 0;
            uint32_t count = static_cast<uint32_t>(intent.cards_to_mulligan.size());
            return pack(intent.type, c0, c1, 0, count, 0);
        }

        // ── Damage assignment ────────────────────────────────────────────
        case T::AssignCombatDamage: {
            uint32_t t0_def = intent.damage_assignments.size() > 0
                ? defIdOf(state, intent.damage_assignments[0].target_unit) : 0;
            uint32_t t0_dmg = intent.damage_assignments.size() > 0
                ? static_cast<uint32_t>(intent.damage_assignments[0].damage)
                : 0;
            uint32_t t1_def = intent.damage_assignments.size() > 1
                ? defIdOf(state, intent.damage_assignments[1].target_unit) : 0;
            uint32_t t1_dmg = intent.damage_assignments.size() > 1
                ? static_cast<uint32_t>(intent.damage_assignments[1].damage)
                : 0;
            uint32_t count = static_cast<uint32_t>(intent.damage_assignments.size());
            return pack(intent.type, t0_def,
                        (t0_dmg & static_cast<uint32_t>(kTarget1Mask)),
                        t1_def,
                        count,
                        (t1_dmg & static_cast<uint32_t>(kAbilitySourceMask)));
        }

        // ── ChooseBattlefield ────────────────────────────────────────────
        case T::ChooseBattlefield:
            return pack(intent.type, 0, 0, 0,
                        encodeBfSlot(intent.chosen_battlefield), 0);

        // ── MakeChoice (prompted) ────────────────────────────────────────
        case T::MakeChoice: {
            uint32_t c0 = intent.chosen_objects.size() > 0
                ? defIdOf(state, intent.chosen_objects[0]) : 0;
            uint32_t c1 = intent.chosen_objects.size() > 1
                ? defIdOf(state, intent.chosen_objects[1]) : 0;
            uint32_t val = intent.chosen_value.has_value()
                ? static_cast<uint32_t>(*intent.chosen_value) : 0;
            return pack(intent.type, c0, c1,
                        (val & static_cast<uint32_t>(kTarget2Mask)),
                        0, 0);
        }

        // ── PlayFirstDecision ────────────────────────────────────────────
        case T::PlayFirstDecision:
            return pack(intent.type, 0, 0, 0,
                        intent.choose_first ? 1 : 0, 0);

        // ── Triggered-ability responses ──────────────────────────────────
        case T::PlaceOptionalTrigger:
        case T::DeclineOptionalTrigger:
        case T::PayTriggeredCost:
        case T::DeclineTriggeredCost: {
            uint32_t src_def = defIdOf(state, intent.ability_source);
            uint32_t ability = intent.ability == kInvalidId
                ? 0 : (intent.ability & static_cast<uint32_t>(kAbilitySourceMask));
            return pack(intent.type, src_def, 0, 0, 0, ability);
        }

        // ── SideboardSwap ────────────────────────────────────────────────
        case T::SideboardSwap: {
            uint32_t out_def = 0, in_def = 0;
            if (!intent.sideboard_swaps.empty()) {
                out_def = defIdOf(state, intent.sideboard_swaps[0].first);
                in_def  = defIdOf(state, intent.sideboard_swaps[0].second);
            }
            return pack(intent.type, out_def, in_def, 0, 0, 0);
        }

        // ── Type-only intents ────────────────────────────────────────────
        case T::EndTurn:
        case T::PassPriority:
        case T::PassFocus:
        case T::Concede:
        default:
            return pack(intent.type, 0, 0, 0, 0, 0);
    }
}

const Intent* decodeAction(int64_t action_id,
                            const std::vector<Intent>& legal_actions,
                            const GameState& state) {
    for (const auto& intent : legal_actions) {
        if (encodeAction(intent, state) == action_id) {
            return &intent;
        }
    }
    return nullptr;
}

} // namespace riftbound::openspiel
