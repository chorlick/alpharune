#pragma once
/// @file random_deck.h
/// Builds a random constructed-legal deck from the whole card pool, by seed.
///
/// One seed gives one deck, on every platform (the generator draws from its
/// own SplitMix64, not from a <random> distribution). The deck is legal by
/// the rules DeckValidator enforces (Tournament Rules 402.1 / 601.1, CR 103):
///   - a Champion Legend that is not banned and not a token card;
///   - a Chosen Champion: a champion unit with the legend's champion tag,
///     inside the legend's Domain Identity;
///   - 39 more Main Deck cards (units, gear, spells) inside the Domain
///     Identity, at most 3 of a name counting the Chosen Champion (a card with
///     `unlimited_copies` is exempt, a [Unique] card is 1 of its name: CR
///     825.3.a), at most 3 Signature cards in all, each with the legend's
///     champion tag;
///   - 12 runes of the legend's Domains;
///   - 3 battlefields with different names;
///   - no banned card and no token card anywhere; no sideboard.
///
/// DeckValidator is the judge: `tests/test_deck_fuzz.cpp` validates what
/// this builds, and the fuzz driver (`tools/deck_fuzz.cpp`) does not play a seed deck the
/// validator rejects.
///
/// Every card that can legally be in some deck is in some seed's deck:
/// `reachable()` lists them, `unreachable()` the rest with the reason.

#include "core/card_db.h"
#include "rules/deck_validator.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace riftbound {

class RandomDeckBuilder {
public:
    explicit RandomDeckBuilder(const CardDB& db);

    /// The deck of `seed`. Deterministic; any 64-bit value is a seed.
    DeckSubmission build(uint64_t seed) const;

    /// Weighted building: `weight[id]` (by card id; a missing or non-positive
    /// entry counts as 1) is how much more often a card is picked than a card
    /// of weight 1, wherever the builder picks a card (legend, champion,
    /// Signature cards, Main Deck, battlefields; runes stay uniform). For
    /// steering games toward cards that are rarely exercised. **A seed means
    /// another deck under other weights**: whoever replays a game needs the
    /// same weights. An empty vector (the default) restores uniform picks and
    /// the decks every unweighted seed has always meant.
    void setWeights(const std::vector<double>& weight_by_card_id);
    bool weighted() const { return weighted_; }

    /// The cards a deck built around a legend may hold: what `build` draws
    /// from. Determinized search draws an opponent's unseen cards from it
    /// (agents/determinizer.h).
    struct LegendCards {
        /// Main Deck cards other than Signature cards, legal Chosen Champions
        /// included.
        std::vector<CardDefId> main;
        /// Signature cards with the legend's champion tag (at most 3 a deck).
        std::vector<CardDefId> signatures;
        /// Runes per Domain of the legend (in the legend's order), then the
        /// runes with no Domain if there are any; no list is empty.
        std::vector<std::vector<CardDefId>> runes;
    };
    /// Those cards for `legend`, or nullptr if no deck can be built around it.
    const LegendCards* legendCards(CardDefId legend) const;

    /// Legends a deck can be built around (one pool each).
    std::size_t legendCount() const { return legends_.size(); }

    /// Ids of every card some seed's deck contains, ascending.
    const std::vector<CardDefId>& reachable() const { return reachable_; }

    /// Registered cards no seed's deck contains, ascending, each with why
    /// ("banned", "token card", "legend without a usable champion", ...).
    const std::vector<std::pair<CardDefId, std::string>>& unreachable() const {
        return unreachable_;
    }

    /// The deck as a `.txt` deck list (the format DeckValidator::
    /// loadFromDeckList reads). For reading: a list names cards, so loading it
    /// back gives the first printing of a name, not necessarily the id the
    /// seed chose.
    std::string toDeckList(const DeckSubmission& deck) const;

private:
    /// A list of cards with running sums of their weights (empty = uniform).
    struct Weighted {
        std::vector<double> cumulative;
    };
    // main: every legal Main Deck card but the Signature cards; signatures:
    // those with the legend's tag; runes: per Domain of the legend (same
    // order as the legend's domains), then one more list for runes with no
    // Domain if there are any.
    struct LegendPool : LegendCards {
        CardDefId legend = kInvalidId;
        std::vector<CardDefId> champions;    // legal Chosen Champions
        Weighted w_champions, w_signatures, w_main;
    };

    const CardDB& db_;
    std::vector<LegendPool> legends_;
    std::vector<CardDefId> battlefields_;
    Weighted w_legends_, w_battlefields_;
    bool weighted_ = false;
    std::vector<CardDefId> reachable_;
    std::vector<std::pair<CardDefId, std::string>> unreachable_;
};

} // namespace riftbound
