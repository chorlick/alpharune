#pragma once
/// @file deck_ops.h
/// The only functions that may add a card to, or remove one from, a Main Deck
/// (v6 task P1.3). Each one makes the edit and tells the knowledge tracker, so
/// the two cannot drift apart. Engine and card code call these instead of
/// editing `PlayerState::main_deck`.
///
/// Decks are stored top = back.

#include "core/game_state.h"
#include "engine/knowledge_tracker.h"

#include <algorithm>
#include <vector>

namespace riftbound::deck {

using knowledge::Place;
using knowledge::Seers;

/// Where `object` is for hidden-information purposes, from the zone lists.
/// A card that an effect has taken out of the deck and not yet placed is
/// `Held`.
Place whereIs(const GameState& state, GameObjectId object);

/// Remove the top card of `owner`'s Main Deck and return it (kInvalidId if
/// the deck is empty). The caller puts it in `to`. `seers` are the players
/// shown the card as it is taken; pass `Place::Held` when it is being looked
/// at and will be placed afterwards.
GameObjectId takeTop(GameState& state, PlayerId owner, Place to,
                     Seers seers = knowledge::nobody());

/// Remove `object` from wherever it is in its owner's Main Deck. Pass
/// `slot_public = false` when it is picked out of cards one player was
/// looking at, so the other cannot tell which slot it came from.
/// Returns false if it was not in the deck.
bool take(GameState& state, GameObjectId object, Place to,
          Seers seers = knowledge::nobody(), bool slot_public = true);

/// Put `object` on the top / bottom of `owner`'s Main Deck. `from` is where
/// it was; the caller has already removed it from there.
void putOnTop(GameState& state, PlayerId owner, GameObjectId object, Place from,
              Seers seers = knowledge::nobody());
void putOnBottom(GameState& state, PlayerId owner, GameObjectId object, Place from,
                 Seers seers = knowledge::nobody());

/// Put several cards on the bottom of `owner`'s Main Deck together. `objects` is the
/// order they are physically placed in (already randomised by the caller);
/// nobody is told that order.
struct Placed {
    GameObjectId object = kInvalidId;
    Place from = Place::Public;
    Seers seers;
};
void putOnBottomTogether(GameState& state, PlayerId owner,
                         const std::vector<Placed>& cards);

/// Put `object` into `owner`'s Main Deck to be shuffled in. Must be
/// followed by `shuffle` (or `shuffled`) before anything else uses the deck.
void putToShuffle(GameState& state, PlayerId owner, GameObjectId object, Place from,
                  Seers seers = knowledge::nobody());

/// Shuffle `owner`'s Main Deck.
template <class Rng>
void shuffle(GameState& state, PlayerId owner, Rng& rng) {
    auto& d = state.player(owner).main_deck;
    std::shuffle(d.begin(), d.end(), rng);
    knowledge::shuffled(state, owner);
}

} // namespace riftbound::deck
