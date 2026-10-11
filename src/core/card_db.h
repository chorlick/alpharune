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
    // Legends only: which of `tags` is the champion tag (CR 103.3.a.2,
    // 103.3.d.2). Needed only when a legend prints more than one tag (Heart of
    // the Tempest: Yordle + Kennen); a single-tag legend's tag is its champion
    // tag. See legendChampionTag().
    std::string champion_tag;

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

    // Deck construction: exempt from the 3-copies-per-name cap ("Your deck can
    // have any number of cards named Spiderling").
    bool unlimited_copies = false;

    bool hasTag(const std::string& tag) const {
        for (auto& t : tags)
            if (t == tag) return true;
        return false;
    }

    /// A legend's champion tag: `champion_tag` when authored, else its only
    /// tag. Empty when it cannot be told (not a legend's concern otherwise) —
    /// deck validation treats that as an error rather than guessing.
    const std::string& legendChampionTag() const {
        static const std::string kNone;
        if (!champion_tag.empty()) return champion_tag;
        return tags.size() == 1 ? tags.front() : kNone;
    }

    /// True for a token's reference card (Gold, Sprite, Baron Pit, …): never a
    /// legal deck card (CR 103.3). Tokens are numbered "<set>-t<nn>" in
    /// `def_id`; most of them do not carry SuperType::Token.
    bool isTokenCard() const {
        if (super_type == SuperType::Token) return true;
        auto dash = def_id.find('-');
        return dash != std::string::npos && dash + 2 < def_id.size() &&
               def_id[dash + 1] == 't' &&
               def_id[dash + 2] >= '0' && def_id[dash + 2] <= '9';
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

    /// How many times buildFromClasses has run in this process. A program
    /// needs one card table (openspiel::sharedCards() in the `riftbound`
    /// binary); `riftbound --debug` prints this so a second one is noticed.
    static int tablesBuilt();

private:
    std::unordered_map<CardDefId, CardDef> cards_;
    std::unordered_map<std::string, std::vector<CardDefId>> name_index_; // lowercase

    static std::string toLower(const std::string& s);
};

} // namespace riftbound
