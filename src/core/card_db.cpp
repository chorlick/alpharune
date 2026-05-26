#include "card_db.h"

#include "cards/card_registry.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <stdexcept>

namespace riftbound {

void CardDB::buildFromClasses(const CardRegistry& registry) {
    cards_.clear();
    name_index_.clear();
    for (CardDefId id : registry.classDefIds()) {
        const CardDef* cd = registry.classDef(id);
        if (!cd) continue;  // legacy id-only card (none expected post-bake)
        CardDef def = *cd;  // copy the class-owned authored data
        name_index_[toLower(def.name)].push_back(def.id);
        // Legends: index the "<ChampionTag>, <PrintedName>" alias too (e.g.,
        // legend "Gloomist" with tag "Vex" -> "Vex, Gloomist"). Decks and humans
        // refer to legends by this full form even though the printed name is just
        // the title. Mirrors the alias the old registry.json carried.
        if (def.card_type == CardType::Legend) {
            for (const auto& tag : def.tags) {
                std::string alias = toLower(tag + ", " + def.name);
                if (name_index_.find(alias) == name_index_.end())
                    name_index_[alias].push_back(def.id);
            }
        }
        cards_[def.id] = std::move(def);
    }
}

const CardDef& CardDB::get(CardDefId id) const {
    auto it = cards_.find(id);
    if (it == cards_.end()) {
        throw std::out_of_range(
            "Card ID not found: " + std::to_string(id));
    }
    return it->second;
}

const CardDef* CardDB::findByName(const std::string& name) const {
    auto it = name_index_.find(toLower(name));
    if (it == name_index_.end() || it->second.empty()) {
        return nullptr;
    }
    return &get(it->second.front());
}

std::vector<const CardDef*> CardDB::findAllByName(
    const std::string& name) const {
    std::vector<const CardDef*> result;
    auto it = name_index_.find(toLower(name));
    if (it != name_index_.end()) {
        for (auto id : it->second) {
            result.push_back(&get(id));
        }
    }
    return result;
}

std::string CardDB::toLower(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

} // namespace riftbound
