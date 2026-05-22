#include "action_vocab.h"

namespace riftbound::openspiel {

namespace {

/// CardDefId of a GameObject, or 0 when invalid/missing.
uint32_t defIdOf(const GameState& state, GameObjectId obj) {
    if (obj == kInvalidId) return 0;
    if (!state.objectExists(obj)) return 0;
    uint32_t def = state.getObject(obj).card_def_id;
    if (def == kInvalidId) return 0;
    return def;
}

/// Encode a card_def_id (1..kNumCardDefIds) into a 0-indexed slot offset.
/// Returns -1 if out of range.
int cardDefSlot(uint32_t card_def) {
    if (card_def < 1 || card_def > static_cast<uint32_t>(kNumCardDefIds)) return -1;
    return static_cast<int>(card_def) - 1;
}

/// Encode a destination BattlefieldId into a 0-indexed slot (0..7).
int bfSlot(BattlefieldId bf) {
    if (bf == kInvalidId) return 0;
    int v = static_cast<int>(bf);
    if (v < 0 || v >= 7) return 0;
    return v + 1;  // reserve 0 for "none/base"
}

/// Extract a destination bf id from an optional<LocationId>.
int playLocBfSlot(const std::optional<LocationId>& loc) {
    if (!loc.has_value()) return 0;
    if (auto* bf = std::get_if<BattlefieldLocation>(&*loc)) {
        return bfSlot(bf->id);
    }
    return 0;
}

int verbBase(ActionVerb v) { return verbOffset(v); }

} // namespace

int encodeAction(const Intent& intent, const GameState& state) {
    using T = IntentType;
    switch (intent.type) {
        // ── Singletons ───────────────────────────────────────────────────
        case T::EndTurn:       return verbBase(ActionVerb::EndTurn);
        case T::PassPriority:  return verbBase(ActionVerb::PassPriority);
        case T::PassFocus:     return verbBase(ActionVerb::PassFocus);
        case T::Concede:       return verbBase(ActionVerb::Concede);

        // ── PlayFirstDecision ────────────────────────────────────────────
        case T::PlayFirstDecision:
            return verbBase(ActionVerb::PlayFirstDecision)
                   + (intent.choose_first ? 1 : 0);

        // ── MulliganDecision ─────────────────────────────────────────────
        case T::MulliganDecision: {
            int count = static_cast<int>(intent.cards_to_mulligan.size());
            if (count < 0) count = 0;
            if (count > 2) count = 2;
            return verbBase(ActionVerb::MulliganDecision) + count;
        }

        // ── ChooseBattlefield ────────────────────────────────────────────
        case T::ChooseBattlefield:
            return verbBase(ActionVerb::ChooseBattlefield)
                   + bfSlot(intent.chosen_battlefield);

        // ── AssignCombatDamage ───────────────────────────────────────────
        case T::AssignCombatDamage: {
            // Hash the damage-assignment pattern into one of 16 buckets.
            // Mixes target_unit id into the hash too — two distributions
            // dealing (3, 1) to (Sett, Lux) vs (Sett, Lux) but in
            // swapped order produce different intents and SHOULD encode
            // to different slots (target order matters for Tank /
            // Backline / Deflect interactions). Bumped from 4 to 16
            // buckets (Phase 5g+) to cut collision rate without
            // ballooning the vocab — 16 buckets covers the named
            // distributions (greedy-lethal / spread-even / focus-all /
            // lethal-first) at slots 0..3 plus ad-hoc agent patterns
            // with much less aliasing than the original 4-bucket hash.
            uint32_t hash = 2166136261u;  // FNV-1a basis
            for (const auto& da : intent.damage_assignments) {
                hash ^= static_cast<uint32_t>(da.damage);
                hash *= 16777619u;
                hash ^= static_cast<uint32_t>(da.target_unit);
                hash *= 16777619u;
            }
            return verbBase(ActionVerb::AssignCombatDamage)
                   + static_cast<int>(hash & 127);  // Phase 6t: 128 buckets
        }

        // ── StandardMove ─────────────────────────────────────────────────
        case T::StandardMove:
            return verbBase(ActionVerb::StandardMove)
                   + playLocBfSlot(intent.move_destination);

        // ── Play-family (collapsed) ──────────────────────────────────────
        case T::PlayCard:
        case T::PlayActionCard:
        case T::PlayReaction: {
            int s = cardDefSlot(defIdOf(state, intent.card));
            if (s < 0) return verbBase(ActionVerb::Play);  // fallback to slot 0
            return verbBase(ActionVerb::Play) + s;
        }

        // ── HideCard ─────────────────────────────────────────────────────
        case T::HideCard: {
            int s = cardDefSlot(defIdOf(state, intent.card));
            if (s < 0) return verbBase(ActionVerb::HideCard);
            return verbBase(ActionVerb::HideCard) + s;
        }

        // ── ActivateAbility-family (per-ability, with equip-target hash) ─
        // Phase 6r — slot = verbBase + (def_id - 1) * kMaxAbilitiesPerCard
        // + intent.ability_index. Single-ability cards keep the same slot
        // they had pre-6r. Multi-ability cards (Voidreaver, Grandmaster,
        // Honeyfruit) get distinct slots per ability so the policy head
        // can express preference between them.
        //
        // Phase 6t — equip-target hash. When the source is a Gear AND the
        // intent has a target (equip intents go through this path with
        // ability_index=0), we hash the target unit's card_def_id into
        // ability_index so distinct (gear, target-unit-type) pairs land
        // in distinct vocab slots. Without this, all Seal of Strength
        // equip targets collide on one slot — the policy head can't
        // express "equip the gear on Sett vs Kai'Sa." 4 hash buckets
        // (kMaxAbilitiesPerCard) is a partial fix: 4 distinct unit
        // types per gear can be differentiated. A wider fix would
        // require a dedicated EquipUnit verb but that adds 787
        // additional slots per gear.
        case T::ActivateAbility:
        case T::ActivateReactionAbility:
        case T::ActivateActionAbility: {
            int s = cardDefSlot(defIdOf(state, intent.ability_source));
            if (s < 0) return verbBase(ActionVerb::ActivateAbility);
            int idx = intent.ability_index;

            // Equip-target hash: if source is a gear and intent has a
            // target, override idx with target-def-id hash.
            if (state.objectExists(intent.ability_source) &&
                !intent.targets.empty()) {
                const auto& src = state.getObject(intent.ability_source);
                if (src.isGear()) {
                    uint32_t target_def = defIdOf(state, intent.targets[0]);
                    if (target_def > 0) {
                        idx = static_cast<int>(target_def % kMaxAbilitiesPerCard);
                    }
                }
            }

            if (idx < 0 || idx >= kMaxAbilitiesPerCard) idx = 0;
            return verbBase(ActionVerb::ActivateAbility)
                   + s * kMaxAbilitiesPerCard + idx;
        }

        // ── MakeChoice ───────────────────────────────────────────────────
        //
        // Slot layout (must keep DISTINCT slots for each meaningfully
        // different answer or the AlphaZero policy head can't learn
        // preference between them):
        //   0                                 → empty / "skip" (no object)
        //   1..kNumCardDefIds                 → card by def_id
        //   kNumCardDefIds+1..+kNumIntChoices → int answer 0..kNumIntChoices-1
        //
        // chosen_value takes priority over chosen_objects when set, because
        // int-coded helpers (confirmOptional, pickMode, pickXAmount) use
        // chosen_value as their authoritative answer field. Card-keyed
        // helpers (discard pick, target pick) leave chosen_value unset and
        // fall through to the chosen_objects path.
        case T::MakeChoice: {
            if (intent.chosen_value.has_value()) {
                int v = *intent.chosen_value;
                if (v < 0) v = 0;
                if (v >= kNumIntChoices) v = kNumIntChoices - 1;
                return verbBase(ActionVerb::MakeChoice)
                     + kNumCardDefIds + 1 + v;
            }
            uint32_t first = 0;
            if (!intent.chosen_objects.empty()) {
                first = defIdOf(state, intent.chosen_objects[0]);
            }
            int idx = (first == 0)
                ? 0
                : (cardDefSlot(first) >= 0 ? cardDefSlot(first) + 1 : 0);
            return verbBase(ActionVerb::MakeChoice) + idx;
        }

        // ── Trigger responses (4 separate verbs, keyed by source) ────────
        case T::PlaceOptionalTrigger: {
            int s = cardDefSlot(defIdOf(state, intent.ability_source));
            if (s < 0) return verbBase(ActionVerb::PlaceOptionalTrigger);
            return verbBase(ActionVerb::PlaceOptionalTrigger) + s;
        }
        case T::DeclineOptionalTrigger: {
            int s = cardDefSlot(defIdOf(state, intent.ability_source));
            if (s < 0) return verbBase(ActionVerb::DeclineOptionalTrigger);
            return verbBase(ActionVerb::DeclineOptionalTrigger) + s;
        }
        case T::PayTriggeredCost: {
            int s = cardDefSlot(defIdOf(state, intent.ability_source));
            if (s < 0) return verbBase(ActionVerb::PayTriggeredCost);
            return verbBase(ActionVerb::PayTriggeredCost) + s;
        }
        case T::DeclineTriggeredCost: {
            int s = cardDefSlot(defIdOf(state, intent.ability_source));
            if (s < 0) return verbBase(ActionVerb::DeclineTriggeredCost);
            return verbBase(ActionVerb::DeclineTriggeredCost) + s;
        }

        // ── SideboardSwap ────────────────────────────────────────────────
        case T::SideboardSwap: {
            uint32_t out_def = 0;
            if (!intent.sideboard_swaps.empty()) {
                out_def = defIdOf(state, intent.sideboard_swaps[0].first);
            }
            int s = cardDefSlot(out_def);
            if (s < 0) return verbBase(ActionVerb::SideboardSwap);
            return verbBase(ActionVerb::SideboardSwap) + s;
        }
    }
    return -1;
}

const Intent* decodeAction(int slot,
                            const std::vector<Intent>& legal_actions,
                            const GameState& state) {
    for (const auto& intent : legal_actions) {
        if (encodeAction(intent, state) == slot) {
            return &intent;
        }
    }
    return nullptr;
}

} // namespace riftbound::openspiel
