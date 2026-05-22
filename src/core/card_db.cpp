#include "card_db.h"

#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>

using json = nlohmann::json;

namespace riftbound {

void CardDB::loadFromRegistry(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open registry: " + path);
    }

    json registry = json::parse(file);

    feature_vector_size_ = registry.value("feature_vector_size", 0);

    for (auto& card_json : registry["cards"]) {
        CardDef def;
        def.id = card_json["card_id"].get<CardDefId>();
        def.def_id = card_json.value("card_def_id", "");
        def.name = card_json.value("name", "");
        def.set_code = card_json.value("set", "");
        def.public_code = card_json.value("public_code", "");

        def.card_type = parseCardType(card_json.value("card_type", "unit"));
        std::string super_str;
        if (card_json.contains("super_type") && !card_json["super_type"].is_null()) {
            super_str = card_json["super_type"].get<std::string>();
        }
        def.super_type = super_str.empty() ? SuperType::None
                                            : parseSuperType(super_str);

        if (card_json.contains("domains") && card_json["domains"].is_array()) {
            for (auto& d : card_json["domains"]) {
                def.domains.push_back(parseDomain(d.get<std::string>()));
            }
        }

        if (card_json.contains("tags") && card_json["tags"].is_array()) {
            for (auto& t : card_json["tags"]) {
                def.tags.push_back(t.get<std::string>());
            }
        }

        auto safeInt = [](const json& j, const char* key, int def) -> int {
            if (!j.contains(key) || j[key].is_null()) return def;
            return j[key].get<int>();
        };
        auto safeStr = [](const json& j, const char* key,
                          const std::string& def = "") -> std::string {
            if (!j.contains(key) || j[key].is_null()) return def;
            return j[key].get<std::string>();
        };

        def.energy_cost = safeInt(card_json, "energy_cost", 0);
        def.power_cost = safeInt(card_json, "power_cost", 0);
        def.might = safeInt(card_json, "might", 0);
        def.might_bonus = safeInt(card_json, "might_bonus", 0);
        def.rarity = parseRarity(safeStr(card_json, "rarity", "common"));
        def.ability_text = safeStr(card_json, "ability_text");
        def.effect_text = safeStr(card_json, "effect_text");

        // Effect parsing removed — Card objects handle effects (Phase 4)

        // Parse keyword values from features
        if (card_json.contains("features")) {
            auto& feat = card_json["features"];
            def.assault_value = feat.value("assault_value", 0);
            def.shield_value = feat.value("shield_value", 0);
            def.deflect_value = feat.value("deflect_value", 0);

            // Build keyword bitset from features
            if (feat.contains("keywords") && feat["keywords"].is_array()) {
                auto& kw_flags = feat["keywords"];
                // Order matches KEYWORDS list in card_registry.py
                const Keyword kw_order[] = {
                    Keyword::Accelerate, Keyword::Action, Keyword::Ambush,
                    Keyword::Assault, Keyword::Backline, Keyword::Deathknell,
                    Keyword::Deflect, Keyword::Equip, Keyword::Ganking,
                    Keyword::Hidden, Keyword::Hunt, Keyword::Legion,
                    Keyword::Level, Keyword::Predict, Keyword::QuickDraw,
                    Keyword::Reaction, Keyword::Repeat, Keyword::Shield,
                    Keyword::Tank, Keyword::Temporary, Keyword::Unique,
                    Keyword::Vision, Keyword::Weaponmaster,
                };
                for (size_t i = 0; i < kw_flags.size() &&
                     i < sizeof(kw_order)/sizeof(kw_order[0]); ++i) {
                    if (kw_flags[i].get<int>() != 0) {
                        def.keywords.set(kw_order[i]);
                    }
                }
            }
        }

        // Feature vector
        if (card_json.contains("feature_vector") &&
            card_json["feature_vector"].is_array()) {
            for (auto& v : card_json["feature_vector"]) {
                def.feature_vector.push_back(v.get<float>());
            }
        }

        // Index by name (case-insensitive)
        std::string lower_name = toLower(def.name);
        name_index_[lower_name].push_back(def.id);

        cards_[def.id] = std::move(def);
    }

    // Also index legend aliases from name_to_id
    if (registry.contains("name_to_id")) {
        for (auto& [name, entries] : registry["name_to_id"].items()) {
            std::string lower = toLower(name);
            if (name_index_.find(lower) == name_index_.end()) {
                for (auto& entry : entries) {
                    name_index_[lower].push_back(
                        entry["card_id"].get<CardDefId>());
                }
            }
        }
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

CardType CardDB::parseCardType(const std::string& s) {
    if (s == "unit")        return CardType::Unit;
    if (s == "spell")       return CardType::Spell;
    if (s == "gear")        return CardType::Gear;
    if (s == "rune")        return CardType::Rune;
    if (s == "battlefield") return CardType::Battlefield;
    if (s == "legend")      return CardType::Legend;
    return CardType::Unit;
}

SuperType CardDB::parseSuperType(const std::string& s) {
    if (s == "champion")  return SuperType::Champion;
    if (s == "signature") return SuperType::Signature;
    if (s == "token")     return SuperType::Token;
    return SuperType::None;
}

Domain CardDB::parseDomain(const std::string& s) {
    if (s == "fury")  return Domain::Fury;
    if (s == "calm")  return Domain::Calm;
    if (s == "mind")  return Domain::Mind;
    if (s == "body")  return Domain::Body;
    if (s == "chaos") return Domain::Chaos;
    if (s == "order") return Domain::Order;
    return Domain::Fury;
}

Rarity CardDB::parseRarity(const std::string& s) {
    if (s == "common")   return Rarity::Common;
    if (s == "uncommon") return Rarity::Uncommon;
    if (s == "rare")     return Rarity::Rare;
    if (s == "epic")     return Rarity::Epic;
    if (s == "showcase") return Rarity::Showcase;
    return Rarity::Common;
}

} // namespace riftbound
