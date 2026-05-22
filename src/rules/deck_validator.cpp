#include "deck_validator.h"

#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>
#include <set>
#include <unordered_map>

using json = nlohmann::json;

namespace riftbound {

ValidationResult DeckValidator::validate(const DeckSubmission& deck) const {
    ValidationResult result;

    checkLegend(deck, result);
    if (!result.is_legal) return result; // can't validate domain identity without legend

    const auto& legend = db_.get(deck.legend);

    checkChampion(deck, result, legend);
    checkMainDeckSize(deck, result);
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
    if (deck.legend == 0) {
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
    if (deck.chosen_champion == 0) {
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

    // Champion must share a tag with the legend (CR 103.2.a.2)
    bool has_matching_tag = false;
    for (auto& champ_tag : champ.tags) {
        for (auto& legend_tag : legend.tags) {
            if (champ_tag == legend_tag) {
                has_matching_tag = true;
                break;
            }
        }
        if (has_matching_tag) break;
    }
    if (!has_matching_tag) {
        result.addError("Chosen champion '" + champ.name +
                        "' doesn't share a champion tag with legend '" +
                        legend.name + "'");
    }
}

void DeckValidator::checkMainDeckSize(const DeckSubmission& deck,
                                       ValidationResult& result) const {
    // Main deck (39) + champion (1) = 40 total (CR 402.1)
    int total = static_cast<int>(deck.main_deck.size()) +
                (deck.chosen_champion != 0 ? 1 : 0);
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
        }
        if (!bf_names.insert(bf.name).second) {
            result.addError("Duplicate battlefield name: " + bf.name);
        }
    }
}

void DeckValidator::checkSideboardSize(const DeckSubmission& deck,
                                        ValidationResult& result) const {
    if (deck.sideboard.size() > 8) {
        result.addError("Sideboard can have at most 8 cards, got " +
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
    if (deck.chosen_champion != 0) {
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
    // Max 3 copies of any named card across main + sideboard (CR 103.2.b)
    std::unordered_map<std::string, int> name_counts;

    auto countCard = [&](CardDefId id) {
        const auto& card = db_.get(id);
        name_counts[card.name]++;
    };

    for (auto id : deck.main_deck) countCard(id);
    if (deck.chosen_champion != 0) countCard(deck.chosen_champion);
    for (auto id : deck.sideboard) countCard(id);

    for (auto& [name, count] : name_counts) {
        if (count > 3) {
            result.addError("Too many copies of '" + name + "': " +
                            std::to_string(count) + " (max 3 across main + sideboard)");
        }
    }
}

void DeckValidator::checkSignatureLimits(const DeckSubmission& deck,
                                          ValidationResult& result,
                                          const CardDef& legend) const {
    // Max 3 total signature cards, all must share legend's champion tag (CR 103.2.d)
    int sig_count = 0;

    auto checkSig = [&](CardDefId id) {
        const auto& card = db_.get(id);
        if (card.super_type != SuperType::Signature) return;
        sig_count++;

        bool tag_match = false;
        for (auto& ct : card.tags) {
            for (auto& lt : legend.tags) {
                if (ct == lt) { tag_match = true; break; }
            }
            if (tag_match) break;
        }
        if (!tag_match) {
            result.addError("Signature card '" + card.name +
                            "' doesn't match legend's champion tag");
        }
    };

    for (auto id : deck.main_deck) checkSig(id);
    if (deck.chosen_champion != 0) checkSig(deck.chosen_champion);
    for (auto id : deck.sideboard) checkSig(id);

    if (sig_count > 3) {
        result.addError("Too many signature cards: " + std::to_string(sig_count) +
                        " (max 3)");
    }
}

DeckSubmission DeckValidator::loadFromJson(const std::string& path,
                                            const CardDB& db) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open deck file: " + path);
    }

    json deck_json = json::parse(file);
    DeckSubmission deck;

    deck.legend = deck_json["legend"]["card_id"].get<CardDefId>();
    deck.chosen_champion = deck_json["chosen_champion"]["card_id"].get<CardDefId>();

    for (auto& card : deck_json["main_deck"]) {
        deck.main_deck.push_back(card["card_id"].get<CardDefId>());
    }
    for (auto& card : deck_json["rune_deck"]) {
        deck.rune_deck.push_back(card["card_id"].get<CardDefId>());
    }
    for (auto& card : deck_json["battlefields"]) {
        deck.battlefields.push_back(card["card_id"].get<CardDefId>());
    }
    for (auto& card : deck_json["sideboard"]) {
        deck.sideboard.push_back(card["card_id"].get<CardDefId>());
    }

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
    if (banned_ids_.empty()) return;

    auto checkCard = [&](CardDefId id, const std::string& zone) {
        if (banned_ids_.count(id)) {
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
