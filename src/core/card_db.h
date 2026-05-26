#pragma once
/// @file card_db.h
/// Static card database materialized from the Card classes.
///
/// CardDef is immutable data describing a card template. Each Card class owns its
/// CardDef (the source of truth); CardDB::buildFromClasses copies them in and
/// provides lookups by ID and name. registry.json has been retired.

#include "types.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace riftbound {

class CardRegistry;  // fwd-decl: buildFromClasses sources data from registered cards

struct CardDef {
    CardDefId id = 0;           // registry card_id (1-based, 0 = PAD)
    std::string def_id;          // gallery ID like "ogn-001-298"
    std::string name;
    std::string set_code;
    std::string set_name;        // e.g. "Origins"
    std::string public_code;
    int collector_number = 0;    // printing number within the set
    std::string artist;          // illustrator credit

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

    // Official Riot CDN art URL. Class-owned (baked into each card).
    std::string image_url;

    // Tournament legality — class-owned. Banned cards set this in their own
    // CardDef; DeckValidator rejects them unless --wild is set.
    bool banned = false;

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

/// Stores all card definitions. The data is OWNED by the Card classes (each card
/// carries an inline CardDef); CardDB is materialized from them via
/// buildFromClasses. registry.json has been retired — the C++ sources are the
/// single source of truth for card data.
class CardDB {
public:
    /// Materialize the card database from the authored data the Card classes own
    /// (CardRegistry::classDef). This is the runtime source of truth: C++ classes
    /// own all card data, including tournament legality (CardDef::banned).
    void buildFromClasses(const CardRegistry& registry);

    /// Get card by registry ID (1-based).
    const CardDef& get(CardDefId id) const;

    /// Find card by name (case-insensitive, returns first match).
    const CardDef* findByName(const std::string& name) const;

    /// Find all cards matching a name (for cards with same name in different sets).
    std::vector<const CardDef*> findAllByName(const std::string& name) const;

    /// Total number of loaded cards.
    size_t size() const { return cards_.size(); }

    /// Iterate all cards.
    const std::unordered_map<CardDefId, CardDef>& all() const { return cards_; }

private:
    std::unordered_map<CardDefId, CardDef> cards_;
    std::unordered_map<std::string, std::vector<CardDefId>> name_index_; // lowercase

    static std::string toLower(const std::string& s);
};

} // namespace riftbound
