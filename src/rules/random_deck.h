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
    struct LegendPool {
        CardDefId legend = kInvalidId;
        std::vector<CardDefId> champions;    // legal Chosen Champions
        std::vector<CardDefId> signatures;   // Signature cards with the legend's tag
        std::vector<CardDefId> main;         // every other legal Main Deck card
        // Runes per Domain of the legend (same order as the legend's domains),
        // then one more list for runes with no Domain if there are any.
        std::vector<std::vector<CardDefId>> runes;
    };

    const CardDB& db_;
    std::vector<LegendPool> legends_;
    std::vector<CardDefId> battlefields_;
    std::vector<CardDefId> reachable_;
    std::vector<std::pair<CardDefId, std::string>> unreachable_;
};

} // namespace riftbound
