#pragma once
/// @file action_vocab.h
/// Action ids for the OpenSpiel wrapper: a fixed range [0, kNumActions) in
/// which no two distinct legal intents of one decision share an id, and every
/// legal intent has one.
///
/// An id is made of two parts:
///
///     id = slot + kNumSlots * twin
///
///   * `slot` in [0, kNumSlots) says WHAT THE ACTION IS MADE OF: its verb and
///     the verb's key (which card is played and where, which unit moves where,
///     which of the player's or the opponent's cards a prompt picks, which
///     number it answers). `actionSlot(intent, state)`; it needs no legal list
///     and means the same in every position.
///   * `twin` in [0, kTwinPlanes) tells apart the legal intents of one
///     decision that have the same slot: the two copies of one card in hand,
///     a spell offered once per target, a prompt offered once per rune. The
///     intents of a decision that share a slot are put in a canonical order
///     and numbered 0, 1, 2, ...; the first keeps the plain slot as its id.
///
/// The twin order (`compareIntentsAsSeen`) compares what the deciding
/// player sees of the intents (objects by whose they are, where they are,
/// their card if the decider knows it, their Might and statuses), then the
/// public order of the objects named, and object ids only between intents
/// the decider cannot tell apart. It does not depend on the order the
/// generator listed them in. The slot reads the card of an object only if
/// the decider knows which card it is (key 0 otherwise). So the ids of a
/// decision are the same in every position the decider cannot tell apart
/// (batch 5b item T2, `HiddenOrderLeak.*`): other hidden cards, another
/// deck order, other object ids. The order of `StepResult::legal` is the
/// generator's and may depend on ids: a consumer orders options by action
/// id, never by their index in the engine's list.
///
/// Verbs collapse timing variants (PlayCard / PlayReaction / PlayActionCard
/// are Play; the three Activate types are one): the timing is part of the
/// state, not of the choice. Verb names follow core/legal_action.h's
/// `ActionKind` where both exist.
///
/// Before the 2026-10 checkpoint break an id was the slot alone, slots were
/// coarser, and of the intents sharing one only the first could be chosen
/// (plans/01 item 1.12: 39% of the decisions of random games lost an option
/// that way). docs/checkpoint-breaking-2026-10.md lists every range.

#include "core/intent.h"
#include "core/game_state.h"

namespace riftbound { class CardDB; }

#include <cstdint>
#include <vector>

namespace riftbound::openspiel {

/// Card def ids are 1-indexed in the registry: 1..955 registered (through
/// Vendetta), 956..1165 reserved for Radiance, 1166..1565 for Legacy,
/// 1566..2048 spare (see ml::kCardVocabSize). Must equal ml::kCardVocabSize
/// (static_assert in riftbound_state.cpp).
constexpr int kNumCardDefIds = 2048;

/// Width of a range keyed by a card: key 0 is "an object with no card id" (a
/// token that has no card class) and keys 1..kNumCardDefIds are the card ids.
/// No verb falls back to another card's key for such an object.
constexpr int kCardKeys = kNumCardDefIds + 1;

/// Integer answers of a prompt (Intent::chosen_value: yes / no, a mode, an X)
/// that have a slot of their own: 0..kNumIntChoices-1. Any other value shares
/// one more slot (and is told apart by its twin number).
constexpr int kNumIntChoices = 16;

/// Activated abilities of one card that have a slot of their own, by
/// `Intent::ability_index`; a higher index shares the last one.
constexpr int kMaxAbilitiesPerCard = 4;

/// Places an action can name: 0 = none or a base, 1..7 = battlefield slot + 1.
constexpr int kPlaceKeys = 8;

/// Hand positions a mulligan slot can name.
constexpr int kMulliganHand = 8;

/// How a card is played, where it changes what the play is.
enum class PlayVariant : uint8_t {
    Normal = 0,       // from hand, or the Chosen Champion from its zone
    AlternativeCost,  // Intent::use_alt_play_cost
    LocationCost,     // Intent::pay_location_cost
    FromElsewhere,    // from the trash, banishment, facedown or the chain
    Count,
};

/// Verb categories. Each verb owns a contiguous range of slots.
/// Slot 0 is reserved and never an action.
enum class ActionVerb : uint8_t {
    Reserved = 0,              // 1, never emitted
    EndTurn,                   // 1
    PassPriority,              // 1
    PassFocus,                 // 1
    Concede,                   // 1
    PlayFirstDecision,         // 2: second, first
    MulliganDecision,          // 38: which hand positions are set aside —
                               //   keep (1), one position (8), a pair (28),
                               //   anything else (1)
    ChooseBattlefield,         // 8: position in the player's battlefield list
    AssignCombatDamage,        // 16: number of units given damage (15 = 15+)
    StandardMove,              // kCardKeys * kPlaceKeys: (unit card, destination)
    Play,                      // kCardKeys * kPlaceKeys * PlayVariant::Count:
                               //   (card, where it lands, how it is played)
    HideCard,                  // kCardKeys * kPlaceKeys: (card, battlefield)
    ActivateAbility,           // kCardKeys * kMaxAbilitiesPerCard:
                               //   (source card, ability index), the source's
                               //   own ability with no target in the intent
    ActivateAtTarget,          // kCardKeys: (source card), an ability whose
                               //   intent names its target — Equip
    ActivateGranted,           // kCardKeys: (card that grants the ability)
    ChooseOwnObject,           // kCardKeys: a prompt answered with an object
                               //   the deciding player controls (first object)
    ChooseOpposingObject,      // kCardKeys: ... the opponent controls
    ChooseNamedCard,           // kCardKeys: a prompt answered by naming a card
    ChooseValue,               // kNumIntChoices + 1: an integer answer
    PlaceOptionalTrigger,      // kCardKeys (source card)
    DeclineOptionalTrigger,    // kCardKeys
    PayTriggeredCost,          // kCardKeys
    DeclineTriggeredCost,      // kCardKeys
    SideboardSwap,             // kCardKeys (card taken out)
    Count,
};

constexpr int verbArity(ActionVerb v) {
    switch (v) {
        case ActionVerb::Reserved:                return 1;
        case ActionVerb::EndTurn:                 return 1;
        case ActionVerb::PassPriority:            return 1;
        case ActionVerb::PassFocus:               return 1;
        case ActionVerb::Concede:                 return 1;
        case ActionVerb::PlayFirstDecision:       return 2;
        case ActionVerb::MulliganDecision:
            return 1 + kMulliganHand + kMulliganHand * (kMulliganHand - 1) / 2 + 1;
        case ActionVerb::ChooseBattlefield:       return 8;
        case ActionVerb::AssignCombatDamage:      return 16;
        case ActionVerb::StandardMove:            return kCardKeys * kPlaceKeys;
        case ActionVerb::Play:
            return kCardKeys * kPlaceKeys * static_cast<int>(PlayVariant::Count);
        case ActionVerb::HideCard:                return kCardKeys * kPlaceKeys;
        case ActionVerb::ActivateAbility:         return kCardKeys * kMaxAbilitiesPerCard;
        case ActionVerb::ActivateAtTarget:        return kCardKeys;
        case ActionVerb::ActivateGranted:         return kCardKeys;
        case ActionVerb::ChooseOwnObject:         return kCardKeys;
        case ActionVerb::ChooseOpposingObject:    return kCardKeys;
        case ActionVerb::ChooseNamedCard:         return kCardKeys;
        case ActionVerb::ChooseValue:             return kNumIntChoices + 1;
        case ActionVerb::PlaceOptionalTrigger:    return kCardKeys;
        case ActionVerb::DeclineOptionalTrigger:  return kCardKeys;
        case ActionVerb::PayTriggeredCost:        return kCardKeys;
        case ActionVerb::DeclineTriggeredCost:    return kCardKeys;
        case ActionVerb::SideboardSwap:           return kCardKeys;
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

/// Number of slots (descriptions).
constexpr int kNumSlots = verbOffset(ActionVerb::Count);

/// Legal intents of one decision that may share a slot. More than this many
/// and the ones beyond have no id (see `actionIds`; RiftboundState then
/// throws rather than leave options out). The most the walk of
/// tests/test_action_ids.cpp has met is 64 (a spell offered once per pair of
/// targets); the bound is a constant so that the id range is.
constexpr int kTwinPlanes = 1024;

/// Number of distinct action ids: RiftboundGame::NumDistinctActions().
constexpr int kNumActions = kNumSlots * kTwinPlanes;

/// The two parts of an id.
constexpr int actionIdSlot(int id) { return id % kNumSlots; }
constexpr int actionIdTwin(int id) { return id / kNumSlots; }
constexpr int makeActionId(int slot, int twin) { return slot + kNumSlots * twin; }

/// The verb whose range `slot` is in.
constexpr ActionVerb slotVerb(int slot) {
    int v = 0;
    while (v + 1 < static_cast<int>(ActionVerb::Count) &&
           slot >= verbOffset(static_cast<ActionVerb>(v + 1))) {
        ++v;
    }
    return static_cast<ActionVerb>(v);
}

// ─── Encoding ───────────────────────────────────────────────────────────────

/// "This intent has no id": its slot already has kTwinPlanes legal intents
/// before it.
constexpr int kNoActionId = -1;

/// What the intent is made of: its slot, in [0, kNumSlots). Total: every
/// intent has one. `state` is read only for the card id and the controller of
/// the objects the intent names (and, for a mulligan, the player's hand).
int actionSlot(const Intent& intent, const GameState& state);

/// The action id of every intent of one legal list, parallel to it. Distinct
/// intents get distinct ids; an intent listed twice gets its id twice. An
/// intent past the twin planes gets kNoActionId.
std::vector<int> actionIds(const std::vector<Intent>& legal_actions,
                           const GameState& state);

/// The id `intent` has in `legal_actions` (which need not list it: a concede
/// is accepted at any decision), or kNoActionId.
int actionIdOf(const Intent& intent,
               const std::vector<Intent>& legal_actions,
               const GameState& state);

/// The intent of `legal_actions` whose id is `action_id`, or nullptr.
const Intent* decodeAction(int action_id,
                            const std::vector<Intent>& legal_actions,
                            const GameState& state);

/// A total order of intents, field by field with objects by id: negative,
/// zero or positive as `a` comes before, is, or comes after `b`. Zero exactly
/// when `a == b`. The last tie-break of the twin order only.
int compareIntents(const Intent& a, const Intent& b);

/// What the deciding player (`intent.player`) sees of an intent, as numbers:
/// every field `Intent::operator==` compares, each object named replaced by
/// what the decider sees of it (whose it is, its zone and place, its card
/// only if the decider knows it, for a permanent its Might, damage,
/// statuses, buffs, keywords and gear, and which chain items name it as
/// source, subject or target). Never an object id. Two intents of
/// one decision with equal descriptions are the same choice to the decider.
std::vector<int64_t> visibleDescription(const Intent& intent, const GameState& state);

/// What `viewer` sees of one object, as numbers (the per-object part of
/// `visibleDescription`): whose it is, its zone and place, its card only if
/// the viewer knows it, and for a permanent its Might, damage, statuses,
/// buffs, keywords and gear. Never its id. For putting objects in an order
/// that does not follow ids (an "up to N" choice of targets, listed as a set).
std::vector<int64_t> visibleObjectDescription(const GameState& state, GameObjectId object,
                                              PlayerId viewer);

/// The twin order (batch 5b item T2): by `visibleDescription`, then by the
/// public order of the objects named (the order cards entered a hand, a
/// trash, a banishment), then by `compareIntents` — which decides only
/// between intents the decider sees as the same, and object ids are dealt
/// in a random order (GameEngine::setIdLabelSalt). Zero exactly when a == b.
int compareIntentsAsSeen(const Intent& a, const Intent& b, const GameState& state);

// ─── What a player is shown of a decision ───────────────────────────────────

/// The info-set id of `player` at a decision (RiftboundState::InfoSetId): a
/// hash of `player`'s observation (ml::extractStateFeatures), `player`'s
/// seat, who decides and, for the decider, the action ids on offer
/// (`legal`, ignored for any other player). `decider` is PlayerId::None at
/// the end of a game. Reads nothing a player may not see (batch 5b item T2,
/// `HiddenOrderLeak.*`).
uint64_t infoSetIdFor(const GameState& state, const std::vector<Intent>& legal,
                      PlayerId decider, PlayerId player, const CardDB& card_db);

// ─── History tokens ─────────────────────────────────────────────────────────
//
// `GameState::action_history` holds one token per applied decision: the slot
// of the chosen intent and a fingerprint of the whole intent (every field
// `Intent::operator==` compares, object ids included). The fingerprint is
// what identifies the intent within its legal list, so replaying tokens from
// the same decks and seed reproduces the game exactly; and because it names
// the objects, a replay on the wrong game (another seed deals other cards and
// offers the same ids) is noticed at the first decision whose objects differ.
//
// A token does not hold the action id. The twin number of an id depends on
// the other options of the decision, and the engine records a token for
// every decision of every game, also when nobody uses the vocabulary: working
// the twin out there cost 4% of random play. The id of a recorded choice is
// `actionIdOf(intent, legal, state)` at the decision, or OpenSpiel's own
// `State::History()`.
//
//   bits  0..19  slot
//   bits 20..62  intent fingerprint (43 bits)

constexpr int     kHistorySlotBits = 20;
constexpr int64_t kHistorySlotMask = (int64_t{1} << kHistorySlotBits) - 1;
static_assert(kNumSlots < kHistorySlotMask, "a slot must fit a token");

/// The token for applying `intent` in `state`.
int64_t historyToken(const Intent& intent, const GameState& state);

/// The slot of the intent a token was made from.
constexpr int historyTokenSlot(int64_t token) {
    return static_cast<int>(token & kHistorySlotMask);
}

/// Index of the intent in `legal_actions` that `token` was made from, or -1.
int findHistoryToken(int64_t token,
                     const std::vector<Intent>& legal_actions,
                     const GameState& state);

} // namespace riftbound::openspiel
