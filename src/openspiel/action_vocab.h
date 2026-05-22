#pragma once
/// @file action_vocab.h
/// Fixed dense [0, kVocabSize) action vocabulary for the OpenSpiel wrapper.
///
/// Phase B used a 52-bit structural bit-packing — the action ID was sparse
/// and could be any value in [0, 2^52). That worked for MCTS (which only
/// needs to compare IDs), but breaks AlphaZero — the policy head emits a
/// softmax over a fixed-size dense vector, so we need a small, contiguous
/// action space.
///
/// This vocab collapses semantic variants:
///   PlayCard / PlayReaction / PlayActionCard  → Play
///   ActivateAbility / *ReactionAbility / *ActionAbility → ActivateAbility
///
/// And maps each remaining bucket to a primary key:
///   Play / HideCard / ActivateAbility   → card_def_id (1..787)
///   MakeChoice                          → first chosen def_id (0 = none)
///   StandardMove / ChooseBattlefield    → battlefield slot (0..7)
///   AssignCombatDamage                  → distribution kind (0..3)
///   MulliganDecision                    → mulligan count (0..2)
///   PlayFirstDecision                   → bool
///   EndTurn / PassPriority / etc.       → singleton
///   PlaceOptionalTrigger / Pay / Decline → source card_def_id
///   SideboardSwap                        → out card_def_id
///
/// Decoding searches the current legal-action list for the first Intent whose
/// vocab slot matches the requested ID. Collisions are accepted: two Plays of
/// the same card targeting different units alias to one slot, and decode
/// picks the first one encountered. AZ-style policy heads handle this
/// gracefully — the model learns a coarser policy at aliased slots.

#include "core/intent.h"
#include "core/game_state.h"

#include <cstdint>
#include <vector>

namespace riftbound::openspiel {

/// Card def ids are 1-indexed in the registry. Highest registered id today
/// is 787; bump this if the registry grows.
constexpr int kNumCardDefIds = 787;

/// Reserved slot count for integer-coded MakeChoice answers
/// (Intent::chosen_value). Used by card.cpp helpers — confirmOptional
/// (yes/no = 0/1), pickMode (mode index), pickXAmount (X value). Sized
/// large enough for any modal spell + small-N X pickers; 16 covers every
/// current card. Bump if a future card has > 15 modes or X > 15.
///
/// Important for the AlphaZero policy head: each integer-coded answer
/// gets a DISTINCT slot, so the model can learn preferences across modes
/// (e.g. Curtain Call "Draw 1 vs Deal 2 vs -4M") and yes/no triggers
/// (e.g. Virtuoso "may banish 4-cost spell"). Previously the codebase
/// stuffed the integer into chosen_objects as a fake GameObjectId, which
/// collided in the encoder — confirmOptional yes_choice and no_choice
/// both mapped to slot 0 and the agent never saw the yes option.
constexpr int kNumIntChoices = 16;

/// Phase 6r — multi-ability per Card. ActivateAbility slots are keyed by
/// (card_def_id, ability_index). Most cards have 0 or 1 activated abilities;
/// the maximum observed in the registry is 2 (Voidreaver, Grandmaster at
/// Arms, Honeyfruit). We reserve 4 slots per card for headroom — cards
/// with fewer abilities just leave the higher indices unused. Bump if a
/// future card ever has > 4 activated abilities.
constexpr int kMaxAbilitiesPerCard = 4;

/// Verb categories. Each verb owns a contiguous range of slot ids.
///
/// Phase 6t — Slot 0 is reserved (Reserved verb, arity 1, never
/// emitted by encodeAction). This removes the structural bias where
/// any source of "low-index slot preference" (initialization noise,
/// regularization, position-implicit biases) would privilege the
/// action at slot 0. Prior layout had EndTurn at slot 0; the model
/// learned to over-predict EndTurn (68-80% of all model decisions in
/// iter_6 of the rengar bootstrap). Reserving slot 0 means no real
/// action sits there — any slot-0 bias is absorbed by an action that
/// the legal-action mask never selects.
enum class ActionVerb : uint8_t {
    Reserved = 0,                   // arity 1, NEVER emitted
    EndTurn,
    PassPriority,
    PassFocus,
    Concede,
    PlayFirstDecision,         // arity 2 (false/true)
    MulliganDecision,          // arity 3 (count 0/1/2)
    ChooseBattlefield,         // arity 8 (bf id, 0..7)
    AssignCombatDamage,        // arity 128 (distribution hash bucket 0..127)
                               // Phase 6t — bumped from 16 to 128 to
                               // cut collision rate on combat-damage
                               // distributions. With 2-5 attackers vs
                               // 2-5 defenders distributing 5-20
                               // damage, the realistic distribution
                               // space is hundreds of distinct
                               // patterns; 16 buckets had high
                               // collision rate (lossy training signal
                               // for combat decisions). 128 covers
                               // most realistic distributions with
                               // <10% collision rate. Slots 0..3
                               // remain reserved for the named
                               // distribution kinds (greedy-lethal /
                               // spread-even / focus-all /
                               // lethal-first) at natural hash points.
    StandardMove,              // arity 8 (destination bf slot)
    Play,                      // arity kNumCardDefIds
    HideCard,                  // arity kNumCardDefIds
    ActivateAbility,           // arity kNumCardDefIds * kMaxAbilitiesPerCard
                               // Phase 6r — multi-ability per Card. Slot =
                               // verbBase + (def_id - 1) * kMaxAbilitiesPerCard
                               //   + intent.ability_index. Single-ability
                               // cards default to ability_index=0, identical
                               // to the pre-6r layout's first card_def_id
                               // slot. Vocab grew by
                               // kNumCardDefIds * (kMaxAbilitiesPerCard - 1).
    MakeChoice,                // arity kNumCardDefIds + 1 + kNumIntChoices
                               //   slot 0                                 → no object
                               //   slots 1..kNumCardDefIds                → card by def id
                               //   slots kNumCardDefIds+1..+kNumIntChoices → int answer 0..15
    PlaceOptionalTrigger,      // arity kNumCardDefIds
    DeclineOptionalTrigger,    // arity kNumCardDefIds
    PayTriggeredCost,          // arity kNumCardDefIds
    DeclineTriggeredCost,      // arity kNumCardDefIds
    SideboardSwap,             // arity kNumCardDefIds (out def id)
    Count,
};

/// Total number of vocab slots (computed at compile time below).
constexpr int verbArity(ActionVerb v);
constexpr int verbOffset(ActionVerb v);

constexpr int verbArity(ActionVerb v) {
    switch (v) {
        case ActionVerb::Reserved:                return 1;  // slot 0, never used
        case ActionVerb::EndTurn:                 return 1;
        case ActionVerb::PassPriority:            return 1;
        case ActionVerb::PassFocus:               return 1;
        case ActionVerb::Concede:                 return 1;
        case ActionVerb::PlayFirstDecision:       return 2;
        case ActionVerb::MulliganDecision:        return 3;
        case ActionVerb::ChooseBattlefield:       return 8;
        case ActionVerb::AssignCombatDamage:      return 128;
        case ActionVerb::StandardMove:            return 8;
        case ActionVerb::Play:                    return kNumCardDefIds;
        case ActionVerb::HideCard:                return kNumCardDefIds;
        case ActionVerb::ActivateAbility:         return kNumCardDefIds * kMaxAbilitiesPerCard;
        case ActionVerb::MakeChoice:              return kNumCardDefIds + 1 + kNumIntChoices;
        case ActionVerb::PlaceOptionalTrigger:    return kNumCardDefIds;
        case ActionVerb::DeclineOptionalTrigger:  return kNumCardDefIds;
        case ActionVerb::PayTriggeredCost:        return kNumCardDefIds;
        case ActionVerb::DeclineTriggeredCost:    return kNumCardDefIds;
        case ActionVerb::SideboardSwap:           return kNumCardDefIds;
        case ActionVerb::Count:                   return 0;
    }
    return 0;
}

constexpr int verbOffset(ActionVerb v) {
    int sum = 0;
    for (int i = 0; i < static_cast<int>(v); ++i) {
        sum += verbArity(static_cast<ActionVerb>(i));
    }
    return sum;
}

constexpr int kVocabSize = verbOffset(ActionVerb::Count);

// ─── Encoding ───────────────────────────────────────────────────────────────

/// Compute the dense vocab slot for an Intent. Returns -1 if the Intent is
/// unknown / cannot be represented (should not happen for legal actions).
int encodeAction(const Intent& intent, const GameState& state);

/// Find the first Intent in `legal_actions` whose dense slot equals `slot`.
/// Returns nullptr if none match.
const Intent* decodeAction(int slot,
                            const std::vector<Intent>& legal_actions,
                            const GameState& state);

} // namespace riftbound::openspiel
