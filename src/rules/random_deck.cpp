#include "random_deck.h"

#include <algorithm>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace riftbound {

namespace {

/// SplitMix64: the whole generator, so a seed means the same deck everywhere.
struct SplitMix {
    uint64_t s;
    uint64_t next() {
        uint64_t z = (s += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }
    /// 0 .. n-1. The modulo bias is below 2^-50 for the sizes used here.
    std::size_t below(std::size_t n) { return static_cast<std::size_t>(next() % n); }
};

bool withinIdentity(const CardDef& card, const CardDef& legend) {
    for (Domain d : card.domains)
        if (!legend.hasDomain(d)) return false;
    return true;
}

bool isMainDeckType(const CardDef& d) {
    return d.card_type == CardType::Unit || d.card_type == CardType::Spell ||
           d.card_type == CardType::Gear;
}

constexpr int kMainDeckCards = 39;   // 40 with the Chosen Champion (TR 402.1)
constexpr int kRunes = 12;
constexpr int kBattlefields = 3;
constexpr int kCopiesPerName = 3;    // CR 103.3.b
constexpr int kSignatureCards = 3;   // CR 103.3.d

} // namespace

RandomDeckBuilder::RandomDeckBuilder(const CardDB& db) : db_(db) {
    std::vector<const CardDef*> defs;
    for (const auto& [id, def] : db.all()) defs.push_back(&def);
    std::sort(defs.begin(), defs.end(),
              [](const CardDef* a, const CardDef* b) { return a->id < b->id; });

    std::map<CardDefId, std::string> why_not;   // provisional reasons
    std::set<CardDefId> used;

    std::set<std::string> battlefield_names;
    for (const CardDef* d : defs) {
        if (d->banned) { why_not[d->id] = "banned"; continue; }
        if (d->isTokenCard()) { why_not[d->id] = "token card"; continue; }
        if (d->card_type == CardType::Battlefield) {
            battlefields_.push_back(d->id);
            battlefield_names.insert(d->name);
        }
    }
    if (static_cast<int>(battlefield_names.size()) < kBattlefields)
        throw std::runtime_error("RandomDeckBuilder: fewer than 3 battlefield names in the pool");
    for (CardDefId id : battlefields_) used.insert(id);

    for (const CardDef* legend : defs) {
        if (legend->card_type != CardType::Legend) continue;
        if (legend->banned || legend->isTokenCard()) continue;
        const std::string& tag = legend->legendChampionTag();
        if (tag.empty()) { why_not[legend->id] = "legend without a single champion tag"; continue; }

        LegendPool pool;
        pool.legend = legend->id;
        pool.runes.resize(legend->domains.size() + 1);
        std::set<std::string> main_names;
        int unlimited = 0;
        for (const CardDef* d : defs) {
            if (d->banned || d->isTokenCard()) continue;
            if (!withinIdentity(*d, *legend)) continue;
            if (d->card_type == CardType::Rune) {
                if (d->domains.empty()) {
                    pool.runes.back().push_back(d->id);
                } else {
                    for (std::size_t k = 0; k < legend->domains.size(); ++k)
                        if (d->domains.front() == legend->domains[k])
                            pool.runes[k].push_back(d->id);
                }
                continue;
            }
            if (!isMainDeckType(*d)) continue;
            if (d->super_type == SuperType::Signature) {
                if (d->hasTag(tag)) pool.signatures.push_back(d->id);
                continue;
            }
            if (d->card_type == CardType::Unit && d->super_type == SuperType::Champion &&
                d->hasTag(tag))
                pool.champions.push_back(d->id);
            pool.main.push_back(d->id);
            main_names.insert(d->name);
            if (d->unlimited_copies) ++unlimited;
        }
        // Drop the empty rune lists; a pool needs at least one rune.
        pool.runes.erase(std::remove_if(pool.runes.begin(), pool.runes.end(),
                                        [](const auto& v) { return v.empty(); }),
                         pool.runes.end());
        if (pool.champions.empty()) { why_not[legend->id] = "legend without a legal Chosen Champion"; continue; }
        if (pool.runes.empty()) { why_not[legend->id] = "legend without a rune of its Domains"; continue; }
        if (unlimited == 0 &&
            static_cast<int>(main_names.size()) * kCopiesPerName < kMainDeckCards + 1) {
            why_not[legend->id] = "legend whose Domains hold too few cards for a Main Deck";
            continue;
        }
        used.insert(pool.legend);
        for (CardDefId id : pool.champions) used.insert(id);
        for (CardDefId id : pool.signatures) used.insert(id);
        for (CardDefId id : pool.main) used.insert(id);
        for (const auto& list : pool.runes)
            for (CardDefId id : list) used.insert(id);
        legends_.push_back(std::move(pool));
    }
    if (legends_.empty())
        throw std::runtime_error("RandomDeckBuilder: no legend a deck can be built around");

    for (const CardDef* d : defs) {
        if (used.count(d->id)) {
            reachable_.push_back(d->id);
            continue;
        }
        auto it = why_not.find(d->id);
        std::string why;
        if (it != why_not.end()) why = it->second;
        else if (d->super_type == SuperType::Signature) why = "Signature card of no usable legend";
        else why = "in no usable legend's Domain Identity";
        unreachable_.emplace_back(d->id, std::move(why));
    }
}

DeckSubmission RandomDeckBuilder::build(uint64_t seed) const {
    SplitMix rng{seed};
    const LegendPool& pool = legends_[rng.below(legends_.size())];

    DeckSubmission deck;
    deck.legend = pool.legend;
    deck.chosen_champion = pool.champions[rng.below(pool.champions.size())];

    std::unordered_map<std::string, int> copies;   // by name (CR 103.3.b)
    auto room = [&](const CardDef& d) {
        if (d.unlimited_copies) return kCopiesPerName;   // no cap; add at most 3 per pick
        // [Unique] (CR 825.3.a): one card of that name. DeckValidator does not
        // check it (2026-10); the builder keeps to the rule.
        const int cap = d.keywords.has(Keyword::Unique) ? 1 : kCopiesPerName;
        return cap - copies[d.name];
    };
    auto add = [&](const CardDef& d, int n) {
        for (int i = 0; i < n; ++i) deck.main_deck.push_back(d.id);
        if (!d.unlimited_copies) copies[d.name] += n;
    };
    copies[db_.get(deck.chosen_champion).name] = 1;   // CR 103.3.b.1

    // Signature cards: 0 to 3 in all, each a uniform pick of the legend's.
    if (!pool.signatures.empty()) {
        const int wanted = static_cast<int>(rng.below(kSignatureCards + 1));
        for (int i = 0; i < wanted; ++i) {
            const CardDef& d = db_.get(pool.signatures[rng.below(pool.signatures.size())]);
            if (room(d) > 0) add(d, 1);
        }
    }

    // The rest: uniform picks of the pool, 1 to 3 copies each (3 : 2 : 1), so
    // a deck has both many different cards and duplicates that meet.
    static const int kCopyTable[6] = {1, 1, 1, 2, 2, 3};
    while (static_cast<int>(deck.main_deck.size()) < kMainDeckCards) {
        const CardDef& d = db_.get(pool.main[rng.below(pool.main.size())]);
        int n = kCopyTable[rng.below(6)];
        n = std::min(n, room(d));
        n = std::min(n, kMainDeckCards - static_cast<int>(deck.main_deck.size()));
        if (n > 0) add(d, n);
    }

    // Runes: a Domain of the legend's (uniform), then a printing of it.
    for (int i = 0; i < kRunes; ++i) {
        const auto& list = pool.runes[rng.below(pool.runes.size())];
        deck.rune_deck.push_back(list[rng.below(list.size())]);
    }

    // Battlefields: three different names (CR 103.5.c).
    std::set<std::string> names;
    while (static_cast<int>(deck.battlefields.size()) < kBattlefields) {
        const CardDefId id = battlefields_[rng.below(battlefields_.size())];
        if (names.insert(db_.get(id).name).second) deck.battlefields.push_back(id);
    }
    return deck;
}

std::string RandomDeckBuilder::toDeckList(const DeckSubmission& deck) const {
    std::ostringstream out;
    auto section = [&](const char* title, const std::vector<CardDefId>& ids) {
        out << title << ":\n";
        std::vector<std::pair<std::string, int>> lines;   // in first-seen order
        for (CardDefId id : ids) {
            const std::string& name = db_.get(id).name;
            auto it = std::find_if(lines.begin(), lines.end(),
                                   [&](const auto& l) { return l.first == name; });
            if (it == lines.end()) lines.emplace_back(name, 1);
            else ++it->second;
        }
        for (const auto& [name, n] : lines) out << n << " " << name << "\n";
        out << "\n";
    };
    section("Legend", {deck.legend});
    section("Champion", {deck.chosen_champion});
    section("MainDeck", deck.main_deck);
    section("Battlefields", deck.battlefields);
    section("Runes", deck.rune_deck);
    return out.str();
}

} // namespace riftbound
