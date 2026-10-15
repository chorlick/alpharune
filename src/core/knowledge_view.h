#pragma once
/// @file knowledge_view.h
/// What one player knows about the cards in the game (v6 interface P0.1).
///
/// The rule this encodes: an agent knows what a human at the table knows, and
/// nothing else. Its own hand, the composition of its own deck (never the
/// order), every public zone, and any card it was shown by an effect. A card
/// it has seen is remembered as it moves: "somewhere in the deck" after a
/// shuffle, "at the bottom" after a recycle. One card recycled alone has an
/// exact place; cards recycled together land in a random order (CR 417.5),
/// so each is known only to be among the slots the group occupies.
///
/// This header is types only. The tracker that implements it lives behind
/// `GameState` (engine/knowledge_tracker.*); the observation extractor and
/// determinized search read hidden information through this interface and
/// never from `GameState` directly.

#include "types.h"

#include <utility>
#include <vector>

namespace riftbound {

/// Where in a deck the observer knows a card to be.
enum class DeckPosition : uint8_t {
    Unknown,   // somewhere in the deck
    Top,       // counted from the top (rank 0 = the next card drawn)
    Bottom,    // counted from the bottom (rank 0 = the last card)
};

/// Everything one observer knows about one game object.
///
/// Identity and place are separate: a facedown card has a known place and an
/// unknown identity; a card shuffled away after being revealed has a known
/// identity and only a known zone.
struct CardKnowledge {
    GameObjectId object = kInvalidId;
    PlayerId owner = PlayerId::None;

    /// The observer knows which card this is. Always true for objects in a
    /// public zone and for the observer's own hand.
    bool identity_known = false;
    /// Valid only when `identity_known`.
    CardDefId card_def_id = kInvalidId;

    /// The zone the observer last knew the object to be in.
    ZoneType last_known_zone = ZoneType::MainDeck;
    /// False once the object could have left `last_known_zone` unseen: a card
    /// from a hidden zone moved to another hidden zone and the observer could
    /// not tell which one it was (for example an opponent whose revealed hand
    /// card may since have been recycled face down).
    bool zone_confirmed = true;

    /// Meaningful only when `last_known_zone` is a deck. Any shuffle of that
    /// deck resets it to Unknown.
    DeckPosition deck_position = DeckPosition::Unknown;
    /// The card is in one of the `deck_rank_span` consecutive slots starting
    /// `deck_rank` from that end. A span of 1 is an exact place; a larger span
    /// is a group whose internal order the observer does not know (cards
    /// recycled together, the cards set aside in a mulligan). Later cards put
    /// on the same end push `deck_rank` up.
    int deck_rank = 0;
    int deck_rank_span = 1;
};

/// Read-only view of one player's knowledge. All answers are from the point
/// of view of `observer()`.
class KnowledgeView {
public:
    virtual ~KnowledgeView() = default;

    virtual PlayerId observer() const = 0;

    /// What the observer knows about `object`. For an object the observer
    /// knows nothing about, `identity_known` is false and `last_known_zone`
    /// is the hidden zone it is counted in.
    virtual CardKnowledge about(GameObjectId object) const = 0;

    /// Cards of `owner` in hidden zones (hand, main deck, facedown) whose
    /// identity the observer knows. For owner == observer() this excludes
    /// the observer's own hand, which is simply visible.
    virtual std::vector<CardKnowledge> knownHiddenCards(PlayerId owner) const = 0;

    /// Number of `owner`'s cards in `zone` whose identity the observer does
    /// not know. Zone sizes are public, so this is always available.
    virtual int unknownCount(PlayerId owner, ZoneType zone) const = 0;

    /// The observer's own Main Deck as (card, copies) for the cards whose
    /// position is unknown: the deck list minus everything seen elsewhere and
    /// minus cards with a known top or bottom position. Sorted by card id;
    /// never in deck order.
    virtual std::vector<std::pair<CardDefId, int>> ownDeckComposition() const = 0;

    /// Cards the observer knows to be at the top / bottom of `owner`'s Main
    /// Deck, nearest the end first, stopping at the first slot that could
    /// hold an unknown card. Members of one unordered group are adjacent and
    /// sorted by card id, never in their true order.
    virtual std::vector<CardKnowledge> knownDeckTop(PlayerId owner) const = 0;
    virtual std::vector<CardKnowledge> knownDeckBottom(PlayerId owner) const = 0;

    /// The same for `owner`'s Rune Deck (testing phase T4): runes the observer
    /// knows to be on its top / bottom, nearest the end first, stopping at the
    /// first slot it does not know. A recycled rune is known to both players
    /// at the bottom (CR 417.1.b, 417.5.a), a rune looked at on top to the
    /// player who looked.
    virtual std::vector<CardKnowledge> knownRuneDeckTop(PlayerId owner) const = 0;
    virtual std::vector<CardKnowledge> knownRuneDeckBottom(PlayerId owner) const = 0;
    /// The observer's own Rune Deck as (card, copies) for the runes whose slot
    /// it does not know: the runes it brought minus those elsewhere and those
    /// at a known slot. Sorted by card id; never in deck order.
    virtual std::vector<std::pair<CardDefId, int>> ownRuneDeckComposition() const = 0;
};

} // namespace riftbound
