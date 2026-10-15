#pragma once
/// @file knowledge_state.h
/// What each player has learned about hidden cards (v6 task P1.2).
///
/// Plain data, stored in `GameState` so that a search clone carries it. It is
/// written only by the hooks in engine/knowledge_tracker.h and read only
/// through `KnowledgeView` (core/knowledge_view.h).
///
/// Only hidden zones are recorded here: hands, Main Decks, facedown cards and
/// cards a player is holding while an effect looks at them. A card in a public
/// zone is known to everyone and is read from the state itself.

#include "types.h"

#include <array>
#include <map>
#include <vector>

namespace riftbound {

/// The end of a deck a known position is counted from.
enum class DeckEnd : uint8_t { None, Top, Bottom };

/// One observer's knowledge of one card that is in a hidden zone.
struct HiddenCardFact {
    /// Where the observer last knew the card to be.
    ZoneType zone = ZoneType::MainDeck;
    /// False once the card could have left `zone` without the observer being
    /// able to tell that it was this card that moved.
    bool zone_confirmed = true;
    /// The card was taken out of its zone by a "look at" and has not been put
    /// anywhere yet.
    bool held = false;
    /// Set when the observer knows the card's slot in a deck. The rank from
    /// that end is `deck seq - anchor` (see DeckCounters), or the group's rank
    /// when `group` is not 0.
    DeckEnd end = DeckEnd::None;
    int anchor = 0;
    uint32_t group = 0;
};

/// Cards put on one end of a deck together, in an order nobody knows
/// (CR 417.5). They occupy `members.size()` consecutive slots.
struct DeckGroup {
    PlayerId owner = PlayerId::None;
    DeckEnd end = DeckEnd::Bottom;
    int anchor = 0;
    std::vector<GameObjectId> members;
};

/// Running count of cards put on, minus cards taken from, each end of one
/// Main Deck. A fact stores the count at the moment the card's slot became
/// known, so later pushes and pops at that end move its rank without touching
/// the fact. A missed hook shows up as a rank that disagrees with the deck.
struct DeckCounters {
    int top_seq = 0;
    int bottom_seq = 0;
};

struct KnowledgeState {
    /// Indexed by playerIndex(observer).
    std::array<std::map<GameObjectId, HiddenCardFact>, 2> facts;
    std::map<uint32_t, DeckGroup> groups;
    uint32_t next_group = 1;
    /// Indexed by playerIndex(deck owner).
    std::array<DeckCounters, 2> deck;
    /// Rune Decks (testing phase T4): the runes each observer (indexed by
    /// playerIndex) knows by identity while they are in a Rune Deck, and the
    /// end they were seen at. A Rune Deck is only added to at its bottom and
    /// taken from at its top, and how many runes go each way is public, so a
    /// known rune's slot is read from the deck itself (GameKnowledgeView);
    /// a shuffle forgets them (knowledge::runeDeckShuffled).
    std::array<std::map<GameObjectId, DeckEnd>, 2> runes;
};

} // namespace riftbound
