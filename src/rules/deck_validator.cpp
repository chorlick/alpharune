#include "deck_validator.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace riftbound {

ValidationResult DeckValidator::validate(const DeckSubmission& deck) const {
    ValidationResult result;

    checkLegend(deck, result);
    if (!result.is_legal) return result; // can't validate domain identity without legend

    const auto& legend = db_.get(deck.legend);

    checkChampion(deck, result, legend);
    checkMainDeckSize(deck, result);
    checkMainDeckCardTypes(deck, result);
    checkRuneDeckSize(deck, result);
    checkBattlefields(deck, result);
    checkSideboardSize(deck, result);
    checkDomainIdentity(deck, result, legend);
    checkRuneDomains(deck, result, legend);
    checkCopyLimits(deck, result);
    checkSignatureLimits(deck, result, legend);
    checkBanList(deck, result);

    return result;
}

void DeckValidator::checkLegend(const DeckSubmission& deck,
                                 ValidationResult& result) const {
    if (deck.legend == kInvalidId) {
        result.addError("Missing legend");
        return;
    }
    const auto& legend = db_.get(deck.legend);
    if (legend.card_type != CardType::Legend) {
        result.addError("Legend slot contains non-legend card: " + legend.name);
    }
}

void DeckValidator::checkChampion(const DeckSubmission& deck,
                                   ValidationResult& result,
                                   const CardDef& legend) const {
    if (deck.chosen_champion == kInvalidId) {
        result.addError("Missing chosen champion");
        return;
    }
    const auto& champ = db_.get(deck.chosen_champion);
    if (champ.card_type != CardType::Unit) {
        result.addError("Champion slot contains non-unit: " + champ.name);
        return;
    }
    if (champ.super_type != SuperType::Champion) {
        result.addError("Champion slot contains non-champion unit: " + champ.name);
        return;
    }

    // The champion must carry the legend's champion tag (CR 103.3.a.2). Any
    // other tag the two share (Yordle, a region) does not count.
    const std::string& champion_tag = legend.legendChampionTag();
    if (champion_tag.empty()) {
        result.addError("Legend '" + legend.name + "' has no single champion tag");
        return;
    }
    if (!champ.hasTag(champion_tag)) {
        result.addError("Chosen champion '" + champ.name +
                        "' doesn't share a champion tag with legend '" +
                        legend.name + "'");
    }
}

void DeckValidator::checkMainDeckCardTypes(const DeckSubmission& deck,
                                            ValidationResult& result) const {
    // The Main Deck holds units, gear and spells (CR 103.3); a sideboard holds
    // only valid Main Deck cards (Tournament Rules 601.1.c.2). Token reference
    // cards are not deck cards at all.
    auto checkCard = [&](CardDefId id, const char* source) {
        const auto& card = db_.get(id);
        const bool main_deck_type = card.card_type == CardType::Unit ||
                                    card.card_type == CardType::Spell ||
                                    card.card_type == CardType::Gear;
        if (card.isTokenCard()) {
            result.addError(std::string(source) + " contains token card: " + card.name);
        } else if (!main_deck_type) {
            result.addError(std::string(source) + " contains " +
                            toString(card.card_type) + " card: " + card.name);
        }
    };
    for (auto id : deck.main_deck) checkCard(id, "Main deck");
    for (auto id : deck.sideboard) checkCard(id, "Sideboard");
}

void DeckValidator::checkMainDeckSize(const DeckSubmission& deck,
                                       ValidationResult& result) const {
    // Main deck (39) + champion (1) = 40 total (CR 402.1)
    int total = static_cast<int>(deck.main_deck.size()) +
                (deck.chosen_champion != kInvalidId ? 1 : 0);
    if (total != 40) {
        result.addError("Main deck must be exactly 40 cards (including champion), got " +
                        std::to_string(total));
    }
}

void DeckValidator::checkRuneDeckSize(const DeckSubmission& deck,
                                       ValidationResult& result) const {
    if (deck.rune_deck.size() != 12) {
        result.addError("Rune deck must be exactly 12 runes, got " +
                        std::to_string(deck.rune_deck.size()));
    }
}

void DeckValidator::checkBattlefields(const DeckSubmission& deck,
                                       ValidationResult& result) const {
    if (deck.battlefields.size() != 3) {
        result.addError("Must have exactly 3 battlefields, got " +
                        std::to_string(deck.battlefields.size()));
    }

    // Check unique names (CR 103.4.c)
    std::set<std::string> bf_names;
    for (auto id : deck.battlefields) {
        const auto& bf = db_.get(id);
        if (bf.card_type != CardType::Battlefield) {
            result.addError("Battlefield slot contains non-battlefield: " + bf.name);
        } else if (bf.isTokenCard()) {
            result.addError("Battlefield slot contains token battlefield: " + bf.name);
        }
        if (!bf_names.insert(bf.name).second) {
            result.addError("Duplicate battlefield name: " + bf.name);
        }
    }
}

void DeckValidator::checkSideboardSize(const DeckSubmission& deck,
                                        ValidationResult& result) const {
    // Tournament Rules 601.1.c.1 (July 2026 update: raised from 8 to 10).
    if (deck.sideboard.size() > kMaxSideboardSize) {
        result.addError("Sideboard can have at most " +
                        std::to_string(kMaxSideboardSize) + " cards, got " +
                        std::to_string(deck.sideboard.size()));
    }
}

void DeckValidator::checkDomainIdentity(const DeckSubmission& deck,
                                         ValidationResult& result,
                                         const CardDef& legend) const {
    // Build legend's domain set (CR 103.1.b)
    std::set<Domain> legend_domains(legend.domains.begin(), legend.domains.end());

    auto checkCard = [&](CardDefId id, const char* source) {
        const auto& card = db_.get(id);
        // Domainless cards are always allowed
        if (card.domains.empty()) return;
        // All of the card's domains must be in the legend's domain identity
        for (auto d : card.domains) {
            if (legend_domains.find(d) == legend_domains.end()) {
                result.addError(std::string(source) + " card '" + card.name +
                                "' has domain " + toString(d) +
                                " not in legend's domain identity");
                return;
            }
        }
    };

    // Check main deck
    for (auto id : deck.main_deck) {
        checkCard(id, "Main deck");
    }

    // Check champion
    if (deck.chosen_champion != kInvalidId) {
        checkCard(deck.chosen_champion, "Champion");
    }

    // Check sideboard
    for (auto id : deck.sideboard) {
        checkCard(id, "Sideboard");
    }
}

void DeckValidator::checkRuneDomains(const DeckSubmission& deck,
                                      ValidationResult& result,
                                      const CardDef& legend) const {
    std::set<Domain> legend_domains(legend.domains.begin(), legend.domains.end());

    for (auto id : deck.rune_deck) {
        const auto& rune = db_.get(id);
        if (rune.card_type != CardType::Rune) {
            result.addError("Rune deck contains non-rune: " + rune.name);
            continue;
        }
        for (auto d : rune.domains) {
            if (legend_domains.find(d) == legend_domains.end()) {
                result.addError("Rune '" + rune.name + "' has domain " +
                                toString(d) + " not in legend's domain identity");
            }
        }
    }
}

void DeckValidator::checkCopyLimits(const DeckSubmission& deck,
                                     ValidationResult& result) const {
    // Max 3 copies of any named card, and one of a [Unique] card (CR 825.3.a),
    // across Main Deck (the Chosen Champion included) + sideboard: limits on
    // copies of named cards apply to the combination (Tournament Rules 403.3).
    std::unordered_map<std::string, int> name_counts;
    std::unordered_set<std::string> unique_names;

    auto countCard = [&](CardDefId id) {
        const auto& card = db_.get(id);
        if (card.unlimited_copies) return;   // "any number of cards named ..."
        name_counts[card.name]++;
        if (card.keywords.has(Keyword::Unique)) unique_names.insert(card.name);
    };

    for (auto id : deck.main_deck) countCard(id);
    if (deck.chosen_champion != kInvalidId) countCard(deck.chosen_champion);
    for (auto id : deck.sideboard) countCard(id);

    for (auto& [name, count] : name_counts) {
        if (unique_names.count(name)) {
            // CR 825.3.a: "A deck can contain only one card of a given name if
            // the card has Unique". 825.3.b: still one each for a Signature.
            if (count > 1) {
                result.addError("Too many copies of [Unique] card '" + name + "': " +
                                std::to_string(count) + " (max 1 across main + sideboard)");
            }
            continue;
        }
        if (count > 3) {
            result.addError("Too many copies of '" + name + "': " +
                            std::to_string(count) + " (max 3 across main + sideboard)");
        }
    }
}

void DeckValidator::checkSignatureLimits(const DeckSubmission& deck,
                                          ValidationResult& result,
                                          const CardDef& legend) const {
    // Max 3 total signature cards, all must carry the legend's champion tag
    // (CR 103.3.d.2) — not merely share some tag with it.
    int sig_count = 0;
    const std::string& champion_tag = legend.legendChampionTag();

    auto checkSig = [&](CardDefId id) {
        const auto& card = db_.get(id);
        if (card.super_type != SuperType::Signature) return;
        sig_count++;

        if (champion_tag.empty() || !card.hasTag(champion_tag)) {
            result.addError("Signature card '" + card.name +
                            "' doesn't match legend's champion tag");
        }
    };

    for (auto id : deck.main_deck) checkSig(id);
    if (deck.chosen_champion != kInvalidId) checkSig(deck.chosen_champion);
    for (auto id : deck.sideboard) checkSig(id);

    if (sig_count > 3) {
        result.addError("Too many signature cards: " + std::to_string(sig_count) +
                        " (max 3)");
    }
}

DeckSubmission DeckValidator::loadFromDeckList(const std::string& path,
                                               const CardDB& db) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open deck list: " + path);
    }

    auto trim = [](std::string s) {
        size_t a = s.find_first_not_of(" \t\r\n");
        size_t b = s.find_last_not_of(" \t\r\n");
        return (a == std::string::npos) ? std::string{} : s.substr(a, b - a + 1);
    };
    auto lower = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return s;
    };

    DeckSubmission deck;
    std::string raw, section;
    int lineno = 0;
    auto fail = [&](const std::string& msg) {
        throw std::runtime_error("Deck list " + path + " line " +
                                 std::to_string(lineno) + ": " + msg);
    };
    auto resolve = [&](const std::string& name) -> CardDefId {
        const CardDef* c = db.findByName(name);
        if (!c) fail("unknown card '" + name + "'");
        return c->id;
    };

    while (std::getline(file, raw)) {
        ++lineno;
        std::string line = trim(raw);
        if (line.empty() || line[0] == '#') continue;

        // Section header: a line ending in ':'.
        if (line.back() == ':') {
            section = lower(trim(line.substr(0, line.size() - 1)));
            continue;
        }
        if (section.empty()) fail("card line before any section header");

        // "N CardName"
        std::istringstream iss(line);
        int count = 0;
        if (!(iss >> count) || count <= 0) fail("expected '<count> <card name>'");
        // The rest of the line is the name. (Not substr(tellg()): on a line
        // that is only a count the stream is at EOF and tellg() is -1.)
        std::string name;
        std::getline(iss, name);
        name = trim(name);
        if (name.empty()) fail("missing card name");
        CardDefId id = resolve(name);

        if (section == "legend") {
            if (deck.legend != kInvalidId) fail("multiple legends");
            deck.legend = id;
        } else if (section == "champion") {
            if (deck.chosen_champion != kInvalidId) fail("multiple champions");
            deck.chosen_champion = id;
        } else if (section == "maindeck") {
            for (int i = 0; i < count; ++i) deck.main_deck.push_back(id);
        } else if (section == "battlefields") {
            for (int i = 0; i < count; ++i) deck.battlefields.push_back(id);
        } else if (section == "runes") {
            for (int i = 0; i < count; ++i) deck.rune_deck.push_back(id);
        } else if (section == "sideboard") {
            for (int i = 0; i < count; ++i) deck.sideboard.push_back(id);
        } else {
            fail("unknown section '" + section + "'");
        }
    }

    if (deck.legend == kInvalidId) throw std::runtime_error("Deck list " + path +
                                                   ": no Legend section");
    if (deck.chosen_champion == kInvalidId) throw std::runtime_error("Deck list " + path +
                                                            ": no Champion section");
    return deck;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Ban list
// ═══════════════════════════════════════════════════════════════════════════════

void DeckValidator::loadBanList(const std::string& csv_path) {
    std::ifstream file(csv_path);
    if (!file.is_open()) return; // ban list is optional

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;

        // Format: SET,ID,'DISPLAY NAME'
        // Note: display names are single-quoted because some contain commas
        auto first_comma = line.find(',');
        if (first_comma == std::string::npos) continue;

        std::string set_code = line.substr(0, first_comma);

        // Find the ID (between first and second comma)
        auto rest = line.substr(first_comma + 1);
        auto second_comma = rest.find(',');
        if (second_comma == std::string::npos) continue;

        std::string id_str = rest.substr(0, second_comma);

        // Display name is after second comma, wrapped in single quotes
        std::string display_name = rest.substr(second_comma + 1);
        // Strip single quotes
        if (!display_name.empty() && display_name.front() == '\'')
            display_name = display_name.substr(1);
        if (!display_name.empty() && display_name.back() == '\'')
            display_name.pop_back();

        // Build the def_id string: "set-id-total" pattern (e.g., "ogn-168-298")
        // Look up by name in CardDB instead (more robust)
        auto* card = db_.findByName(display_name);
        if (card) {
            banned_ids_.insert(card->id);
            banned_names_.push_back(card->name);
        }
    }
}

void DeckValidator::checkBanList(const DeckSubmission& deck,
                                  ValidationResult& result) const {
    if (wild_) return;  // --wild: banned cards are allowed
    // No early-out on an empty CSV ban list: a card may still be banned via its
    // own CardDef::banned flag (class-owned, post-refactor). isBanned() honors
    // both sources.
    auto checkCard = [&](CardDefId id, const std::string& zone) {
        if (id != kInvalidId && isBanned(id)) {
            auto& def = db_.get(id);
            result.addError("Banned card in " + zone + ": " + def.name);
        }
    };

    checkCard(deck.legend, "legend");
    checkCard(deck.chosen_champion, "champion");
    for (auto id : deck.main_deck) checkCard(id, "main deck");
    for (auto id : deck.rune_deck) checkCard(id, "rune deck");
    for (auto id : deck.battlefields) checkCard(id, "battlefields");
    for (auto id : deck.sideboard) checkCard(id, "sideboard");
}

} // namespace riftbound
