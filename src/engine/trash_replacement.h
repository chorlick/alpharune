#pragma once
/// @file trash_replacement.h
/// "If a card would go to your trash from anywhere other than your Main Deck,
/// banish it instead." (Endless Riches — PlayerState::trash_goes_to_banish,
/// set by the card's applyPassiveAura.)
///
/// Every place that puts a card into a trash from somewhere other than a Main
/// Deck (a kill, a discard, a resolved or countered spell, a facedown card
/// that is removed) calls settleInTrash right after the push. Cards moved from
/// a Main Deck (Burn, "put the rest into your trash") do not.

#include "core/events.h"
#include "core/game_state.h"

#include <algorithm>

namespace riftbound {

/// `card` has just been added to its owner's trash. If that player's cards
/// are banished instead, move it on to Banishment and return true.
inline bool settleInTrash(GameState& state, EventBus& events, GameObjectId card) {
    if (!state.objectExists(card)) return false;
    auto& obj = state.getObject(card);
    if (obj.owner == PlayerId::None) return false;
    auto& ps = state.player(obj.owner);
    if (!ps.trash_goes_to_banish) return false;
    const auto it = std::find(ps.trash.begin(), ps.trash.end(), card);
    if (it == ps.trash.end()) return false;
    ps.trash.erase(it);
    obj.zone = ZoneType::Banishment;
    obj.location = std::nullopt;
    ps.banishment.push_back(card);
    ps.cards_banished_this_turn++;
    RIFT_TRACE(events, "  REPLACED: " + obj.name + " is banished instead of going to the trash");
    events.emit(ObjectStateChangedEvent{card, "banished_owned"});
    return true;
}

}  // namespace riftbound
