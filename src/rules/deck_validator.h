#pragma once
/// @file deck_validator.h
/// Validates deck submissions against tournament rules.
///
/// Rules enforced (from tournament-rules.md and core-rules.md):
///   - Main deck exactly 40 cards (including chosen champion)
///   - Rune deck exactly 12 runes
///   - Exactly 3 battlefields with unique names
///   - Sideboard 0-8 cards
///   - All main deck + sideboard cards match legend's domain identity
///   - All runes match legend's domain identity
///   - Max 3 copies of any named card across main deck + sideboard
///   - Max 3 total signature cards, all matching legend's champion tag
///   - Chosen champion must be a champion unit with matching tag
///   - No banned cards (future: ban list support)

#include "core/card_db.h"
#include "core/types.h"

#include <set>
#include <string>
#include <vector>

namespace riftbound {

/// A deck submission to be validated before a game.
struct DeckSubmission {
    CardDefId legend = 0;
    CardDefId chosen_champion = 0;
    std::vector<CardDefId> main_deck;     // 39 cards (champion is separate)
    std::vector<CardDefId> rune_deck;     // 12 runes
    std::vector<CardDefId> battlefields;  // 3 battlefields
    std::vector<CardDefId> sideboard;     // 0-8 cards
};

struct ValidationResult {
    bool is_legal = true;
    std::vector<std::string> errors;

    void addError(const std::string& msg) {
        is_legal = false;
        errors.push_back(msg);
    }
};

class DeckValidator {
public:
    explicit DeckValidator(const CardDB& db) : db_(db) {}

    /// Load ban list from CSV file. Format: SET,ID,'DISPLAY NAME'
    /// Note: display names are single-quoted because some contain commas.
    void loadBanList(const std::string& csv_path);

    /// Wild format (--wild): allow banned cards in decks. When set, the ban
    /// check is skipped entirely regardless of the loaded list or CardDef::banned.
    void setWild(bool wild) { wild_ = wild; }
    bool isWild() const { return wild_; }

    /// Check if a card_def_id is banned. Honors BOTH the CSV-loaded ban list
    /// AND the card's own CardDef::banned flag (which, post-refactor, the card
    /// class owns) — either source bans the card.
    bool isBanned(CardDefId id) const {
        if (wild_) return false;  // wild format: nothing is banned
        return banned_ids_.count(id) > 0 || db_.get(id).banned;
    }

    ValidationResult validate(const DeckSubmission& deck) const;

    /// Load a deck submission directly from a `.txt` deck list (the
    /// human-authored format: `Legend:` / `Champion:` / `MainDeck:` /
    /// `Battlefields:` / `Runes:` / `Sideboard:` sections of `N CardName`
    /// lines). Resolves names via CardDB::findByName. THROWS
    /// std::runtime_error on any parse failure — unreadable file, malformed
    /// line, unknown card name, or unknown/missing section.
    static DeckSubmission loadFromDeckList(const std::string& path, const CardDB& db);

private:
    const CardDB& db_;

    void checkLegend(const DeckSubmission& deck, ValidationResult& result) const;
    void checkChampion(const DeckSubmission& deck, ValidationResult& result,
                       const CardDef& legend) const;
    void checkMainDeckSize(const DeckSubmission& deck, ValidationResult& result) const;
    void checkRuneDeckSize(const DeckSubmission& deck, ValidationResult& result) const;
    void checkBattlefields(const DeckSubmission& deck, ValidationResult& result) const;
    void checkSideboardSize(const DeckSubmission& deck, ValidationResult& result) const;
    void checkDomainIdentity(const DeckSubmission& deck, ValidationResult& result,
                             const CardDef& legend) const;
    void checkCopyLimits(const DeckSubmission& deck, ValidationResult& result) const;
    void checkSignatureLimits(const DeckSubmission& deck, ValidationResult& result,
                              const CardDef& legend) const;
    void checkRuneDomains(const DeckSubmission& deck, ValidationResult& result,
                          const CardDef& legend) const;
    void checkBanList(const DeckSubmission& deck, ValidationResult& result) const;

    std::set<CardDefId> banned_ids_;
    std::vector<std::string> banned_names_;  // for error messages
    bool wild_ = false;                      // --wild: allow banned cards
};

} // namespace riftbound
