#pragma once
/// @file deck_model.h
/// What a deck around a legend may hold, and what an observer has seen of a
/// player's deck: the deck model honest search draws an opponent's unseen
/// cards from (agents/determinizer.h, batch 5 T3) and the opponent's legal
/// card pool an observer may be shown (engine/observer_view.h, testing phase
/// T6). One model, so the two cannot drift.
///
/// The pool of a legend is `RandomDeckBuilder::legendCards` (its Domain
/// Identity, Signature cards of its champion tag, no banned card, no token
/// card; released sets only — what the registry holds); the copy limits are
/// CR 103.3.b / 103.3.d / 825.3.a (3 of a name, a [Unique] card 1, a card with
/// `unlimited_copies` exempt, at most 3 Signature cards), counted over every
/// card of the player's the observer can identify.

#include "core/card_db.h"
#include "core/game_state.h"
#include "rules/random_deck.h"

#include <map>
#include <string>
#include <vector>

namespace riftbound {

/// A unit, a spell or a gear: a card a Main Deck holds.
bool isMainDeckType(const CardDef& d);

/// Copy-limit bookkeeping for one deck (CR 103.3.b / .d, 825.3.a).
struct CopyCounts {
    static constexpr int kCopiesPerName = 3;    // CR 103.3.b
    static constexpr int kSignatureCards = 3;   // CR 103.3.d
    std::map<std::string, int> by_name;
    int signatures = 0;

    /// The copies of `d` a deck may hold besides those counted (-1: no limit).
    int remaining(const CardDef& d) const;
    bool room(const CardDef& d) const { return remaining(d) != 0; }
    void add(const CardDef& d);
};

class DeckModel {
public:
    explicit DeckModel(const CardDB& db);

    struct LegendPool {
        std::vector<CardDefId> cards;    // main + signatures, by id
        std::vector<CardDefId> hidden;   // the [Hidden] ones
        std::vector<std::vector<CardDefId>> runes;
        bool found = false;              // the legend has a RandomDeckBuilder pool
    };
    /// The pool of decks around `legend`; the fallback (every legal Main Deck
    /// card and rune) for a legend with none.
    const LegendPool& poolFor(CardDefId legend) const;
    /// The fallback pool.
    const LegendPool& any() const { return any_; }
    const CardDef* def(CardDefId id) const;
    /// `owner`'s legend, or kInvalidId.
    static CardDefId legendOf(const GameState& state, PlayerId owner);

    /// The copies counted over every Main Deck card of `owner`'s that
    /// `observer` can identify (knowledge::GameKnowledgeView::about), in a
    /// fixed order.
    CopyCounts seenCounts(const GameState& state, PlayerId owner, PlayerId observer) const;

    /// One card of a legal pool: `copies` more of it the deck may hold beyond
    /// what the observer has seen (-1: no limit).
    struct PoolCard {
        CardDefId card = kInvalidId;
        int copies = 0;
        bool operator==(const PoolCard&) const = default;
    };
    /// What `owner`'s unseen cards may be, as far as `observer` can tell
    /// (testing phase T6): the cards of their legend's pool that their deck
    /// may still hold given what `observer` has seen of it. Sorted by card id;
    /// a card with no copy left is not listed. Signature cards are listed only
    /// while fewer than 3 have been seen.
    std::vector<PoolCard> unseenPool(const GameState& state, PlayerId owner,
                                     PlayerId observer) const;

private:
    const CardDB& db_;
    RandomDeckBuilder builder_;
    std::map<CardDefId, LegendPool> pools_;   // one per legend
    LegendPool any_;
};

}  // namespace riftbound
