#pragma once
/// @file legal_action.h
/// A legal action described by its parts (v6 interface P0.2).
///
/// The v6 policy does not pick a slot in a fixed vocabulary. It scores each
/// legal action from what the action is made of: what kind of action, which
/// object acts, which objects it is aimed at, and where it goes. Because the
/// description carries every part that distinguishes one legal intent from
/// another, no two legal actions can collide.
///
/// Rules of this interface:
///   1. An action's id is its index in the legal list of the decision it
///      belongs to. Ids mean nothing across decisions.
///   2. The conversion from `Intent` must be injective over one legal list:
///      two intents that compare unequal give descriptions that compare
///      unequal. NOT enforced yet: the conversion is not written (see below),
///      so there is no test of it.
///   3. Object ids here are runtime `GameObjectId`s. Mapping them to
///      observation tokens is the observation's job (ml/token_layout.h), which
///      is also where hidden identities stay hidden.
///
/// Types only. Nothing in the engine builds a `LegalActionDesc` from an
/// `Intent` yet; `ActionKind`, `ActionDestKind` and `ActionPlaySource` are
/// used by `ml/token_layout.h` (`ActionTokens`). openspiel/action_vocab.* is
/// the fixed-vocabulary encoding this interface is meant to replace.

#include "types.h"

#include <vector>

namespace riftbound {

/// What kind of action this is. Play / activate timing variants (main phase,
/// Reaction, Action) are one kind: the timing is part of the state, not of
/// the choice.
enum class ActionKind : uint8_t {
    EndTurn,
    PassPriority,
    PassFocus,
    Concede,
    Play,                  // source = the card; dest = where a permanent lands
    Hide,                  // source = the card; dest = the battlefield
    Activate,              // source = the object; value = ability index
    Move,                  // source + objects = the units; dest = destination
    AssignDamage,          // targets + amounts
    Mulligan,              // objects = cards set aside (may be empty)
    ChooseBattlefield,     // dest = the battlefield
    PlayFirst,             // value = 1 to go first, 0 to go second
    Choose,                // a prompted choice: objects and / or value
    PlaceOptionalTrigger,  // source = the triggering card
    DeclineOptionalTrigger,
    PayTriggeredCost,
    DeclineTriggeredCost,
    SideboardSwap,         // objects = {out, in} pairs, flattened
    Count,
};

enum class ActionDestKind : uint8_t {
    None,
    Base,          // `player`'s base
    Battlefield,   // battlefield `battlefield`
};

struct ActionDest {
    ActionDestKind kind = ActionDestKind::None;
    PlayerId player = PlayerId::None;
    BattlefieldId battlefield = kInvalidId;

    bool operator==(const ActionDest&) const = default;
};

/// Where a played card comes from, when it is not the hand.
enum class ActionPlaySource : uint8_t {
    Hand, Trash, Banishment, ChampionZone, Hidden, Chain,
};

struct LegalActionDesc {
    /// "No integer payload". Distinct from every real value, including 0.
    static constexpr int32_t kNoValue = INT32_MIN;

    ActionKind kind = ActionKind::EndTurn;

    /// The object that acts: the card played or hidden, the object whose
    /// ability is activated, the first unit moved, the card whose trigger is
    /// being answered. kInvalidId when the action has none.
    GameObjectId source = kInvalidId;

    /// Further objects the action is made of, in the engine's order: the
    /// other units moving together, the cards set aside in a mulligan, the
    /// objects picked in a prompted choice.
    std::vector<GameObjectId> objects;

    /// Objects the action is aimed at.
    std::vector<GameObjectId> targets;

    /// Parallel to `targets` when the action divides a number among them
    /// (combat damage, split damage); empty otherwise.
    std::vector<int32_t> amounts;

    ActionDest dest;

    /// Integer payload: ability index, mode index, X, yes (1) / no (0),
    /// play first (1) / second (0). kNoValue when unused.
    int32_t value = kNoValue;

    ActionPlaySource play_source = ActionPlaySource::Hand;
    /// The play uses the card's alternative cost.
    bool alt_cost = false;
    /// For Activate: the ability is one granted by another card (0 = own).
    CardDefId granted_by = 0;

    bool operator==(const LegalActionDesc&) const = default;
};

} // namespace riftbound
