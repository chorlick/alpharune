#pragma once
/// @file card_db.h
/// Static card database loaded from registry.json.
///
/// CardDef is immutable data describing a card template.
/// CardDB loads all definitions and provides lookups by ID and name.

#include "types.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace riftbound {

struct CardDef {
    CardDefId id = 0;           // registry card_id (1-based, 0 = PAD)
    std::string def_id;          // gallery ID like "ogn-001-298"
    std::string name;
    std::string set_code;
    std::string public_code;

    CardType card_type = CardType::Unit;
    SuperType super_type = SuperType::None;
    std::vector<Domain> domains;
    std::vector<std::string> tags;

    int energy_cost = 0;
    int power_cost = 0;
    int might = 0;
    int might_bonus = 0;
    Rarity rarity = Rarity::Common;

    KeywordSet keywords;
    int assault_value = 0;
    int shield_value = 0;
    int deflect_value = 0;

    std::string ability_text;
    std::string effect_text;

    std::vector<float> feature_vector;

    bool hasTag(const std::string& tag) const {
        for (auto& t : tags)
            if (t == tag) return true;
        return false;
    }

    bool hasDomain(Domain d) const {
        for (auto& dom : domains)
            if (dom == d) return true;
        return false;
    }
};

/// Loads and stores all card definitions from the registry JSON.
class CardDB {
public:
    /// Load from registry.json path.
    void loadFromRegistry(const std::string& path);

    /// Get card by registry ID (1-based).
    const CardDef& get(CardDefId id) const;

    /// Find card by name (case-insensitive, returns first match).
    const CardDef* findByName(const std::string& name) const;

    /// Find all cards matching a name (for cards with same name in different sets).
    std::vector<const CardDef*> findAllByName(const std::string& name) const;

    /// Total number of loaded cards.
    size_t size() const { return cards_.size(); }

    /// Feature vector dimensionality.
    int featureVectorSize() const { return feature_vector_size_; }

    /// Iterate all cards.
    const std::unordered_map<CardDefId, CardDef>& all() const { return cards_; }

private:
    std::unordered_map<CardDefId, CardDef> cards_;
    std::unordered_map<std::string, std::vector<CardDefId>> name_index_; // lowercase
    int feature_vector_size_ = 0;

    static std::string toLower(const std::string& s);
    static CardType parseCardType(const std::string& s);
    static SuperType parseSuperType(const std::string& s);
    static Domain parseDomain(const std::string& s);
    static Rarity parseRarity(const std::string& s);
};

} // namespace riftbound
