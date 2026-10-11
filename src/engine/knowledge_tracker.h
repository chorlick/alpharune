#pragma once
/// @file knowledge_tracker.h
/// Hooks that keep `GameState::knowledge` up to date, and the `KnowledgeView`
/// that reads it (v6 task P1.2).
///
/// A hook is needed where a card enters a hidden zone, moves between hidden
/// zones, is looked at, leaves or joins a Main Deck, or a deck is shuffled. A
/// card going from a hand or a facedown zone to a public zone needs none:
/// public zones are read from the state.
///
/// Every hook describes one physical move and must be called exactly once for
/// it. The deck hooks also count pushes and pops at each end, which is what
/// keeps known positions correct, so a Main Deck edit that bypasses them shows
/// up as a rank that disagrees with the deck (`check` reports it).

#include "core/events.h"
#include "core/game_state.h"
#include "core/knowledge_view.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

namespace riftbound::knowledge {

/// Where a card is, as far as hidden information goes.
enum class Place : uint8_t {
    Public,    // board, trash, banishment, chain, champion and legend zones
    Hand,
    Deck,      // Main Deck
    Facedown,
    Held,      // taken out by a "look at", not yet put anywhere
};

Place placeOf(ZoneType zone);

/// Which players can identify the card during a move or a look.
struct Seers {
    bool p1 = false;
    bool p2 = false;
    bool includes(PlayerId p) const {
        return p == PlayerId::Player1 ? p1 : p == PlayerId::Player2 ? p2 : false;
    }
};
inline Seers nobody() { return {}; }
inline Seers everyone() { return {true, true}; }
inline Seers only(PlayerId p) {
    return {p == PlayerId::Player1, p == PlayerId::Player2};
}

/// A slot in a Main Deck, counted from one end.
struct DeckSlot {
    DeckEnd end = DeckEnd::None;
    int rank = 0;
};

/// `seers` are shown `object` where it is, in `where` (Hand, Facedown, Held,
/// or Deck when the slot is not learned, as in a search of the whole deck).
void seen(GameState& state, GameObjectId object, Place where, Seers seers);

/// `seers` are shown `object` WHERE IT IS NOW, whatever that is: a card in a
/// hand or facedown, a card held by a "look at", or a card in a Main Deck, in
/// which case they also learn its slot (counted from the top: what is looked
/// at or revealed in a deck is its top cards). A card in a public zone needs
/// nothing. This is the one function a look or a reveal reaches the tracker
/// through: `recordReveal` calls it for every CardRevealedEvent, and the few
/// looks that announce no event (Void Hatchling's look, "look at the top N")
/// call it directly. Nothing else should call `seen` / `seenInDeck` for a
/// card that is being shown.
void shown(GameState& state, GameObjectId object, Seers seers);

/// The handler of CardRevealedEvent (GameEngine subscribes it): the players
/// the event names have seen the card — `shown` for exactly them — and their
/// PlayerState::observed_cards count of that card goes up
/// (GameState::recordReveal). The tracker says what a player knows NOW; the
/// count says how often a card was shown and is never taken back.
void recordReveal(GameState& state, const CardRevealedEvent& e);

/// `seers` look at the card `rank` slots from `end` of its owner's Main Deck
/// without moving it.
void seenInDeck(GameState& state, GameObjectId object, Seers seers,
                DeckEnd end, int rank);

/// One card moves from `from` to `to`.
///
/// `seers` are the players who can identify the card because of how the move
/// happens (it is revealed, or they are the one looking). These are added
/// automatically: both players when either end is public; the owner when
/// either end is the owner's hand or facedown zone; and any player who knew
/// the card's exact slot, when it is taken from an end of a deck in the open.
///
/// `from_slot` is the deck slot it is taken from: an end and the number of
/// cards between it and that end (0 for the top or bottom card itself). An
/// end of `None` means a slot nobody can count (a search), and voids every
/// known position in that deck.
/// `to_end` is the end of the deck it is put on. `None` means it is shuffled
/// in; call `shuffled` as well.
/// `slot_public` is false when the other player cannot tell which slot the
/// card came from (the looker rearranging cards they looked at).
void moved(GameState& state, GameObjectId object, Place from, Place to,
           Seers seers, DeckSlot from_slot = {},
           DeckEnd to_end = DeckEnd::None, bool slot_public = true);

struct GroupCard {
    GameObjectId object = kInvalidId;
    Place from = Place::Public;
    Seers seers;
};

/// Several cards of one owner go to one end of that owner's Main Deck
/// together, in a random order (a recycle of several cards, a mulligan).
/// None of them may come from the deck itself.
void movedTogether(GameState& state, const std::vector<GroupCard>& cards,
                   DeckEnd to_end);

/// `owner`'s Main Deck was shuffled: nobody knows a position in it any more.
void shuffled(GameState& state, PlayerId owner);

/// Drop facts about cards that are now public or gone. Cheap; call it before
/// handing a decision to an agent.
void prune(GameState& state);

/// Compare what is recorded with the true state. Returns one line per claim
/// that is false: a card confirmed in a zone it is not in, or a known deck
/// slot that does not hold the card. Empty means consistent. For tests.
std::vector<std::string> check(const GameState& state);

/// `KnowledgeView` over a `GameState`. Holds a reference: build one per
/// observation and do not keep it across a state change.
class GameKnowledgeView final : public KnowledgeView {
public:
    GameKnowledgeView(const GameState& state, PlayerId observer);

    PlayerId observer() const override { return observer_; }
    CardKnowledge about(GameObjectId object) const override;
    std::vector<CardKnowledge> knownHiddenCards(PlayerId owner) const override;
    int unknownCount(PlayerId owner, ZoneType zone) const override;
    std::vector<std::pair<CardDefId, int>> ownDeckComposition() const override;
    std::vector<CardKnowledge> knownDeckTop(PlayerId owner) const override;
    std::vector<CardKnowledge> knownDeckBottom(PlayerId owner) const override;

private:
    struct Where {
        Place place = Place::Public;
        PlayerId owner = PlayerId::None;
    };
    const HiddenCardFact* factFor(GameObjectId object) const;
    /// Fills the deck fields of `k` from `fact`; false if the fact holds no
    /// usable position.
    bool position(const HiddenCardFact& fact, PlayerId owner,
                  CardKnowledge& k) const;
    std::vector<CardKnowledge> knownDeckEnd(PlayerId owner, DeckEnd end) const;

    const GameState& state_;
    PlayerId observer_;
    std::map<GameObjectId, Where> hidden_;
    /// The observer's facts, with held cards that have since been placed
    /// read as where they now are.
    std::map<GameObjectId, HiddenCardFact> facts_;
};

} // namespace riftbound::knowledge
