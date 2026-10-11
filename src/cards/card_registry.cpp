#include "cards/card_registry.h"
#include "core/card_db.h"

#include <algorithm>
#include <cassert>
#include <sstream>
#include <stdexcept>
#include <string>

namespace riftbound {

// One registration function per card, aggregated by the checked-in
// cards_init.cpp (there is no generator: a new card adds its line by hand). Each card's authoritative class lives in its own
// translation unit under src/cards/<type>/<id>_<slug>.cpp.
void registerAllCards(CardRegistry& registry);

void CardRegistry::registerCard(CardDefId id, std::unique_ptr<Card> card) {
    // Registration runs once at startup, so these are checked in every build
    // type: a slip here would otherwise silently drop or mis-key a card.
    if (!card)
        throw std::logic_error("registerCard: null card for id " + std::to_string(id));
    if (card->def().id != id)
        throw std::logic_error("registerCard: id " + std::to_string(id) +
                               " does not match def().id " +
                               std::to_string(card->def().id) + " ('" +
                               card->def().name + "')");
    auto [it, inserted] = cards_.emplace(id, std::move(card));
    if (!inserted)
        throw std::logic_error("registerCard: id " + std::to_string(id) +
                               " is already registered ('" +
                               it->second->def().name + "')");
}

Card* CardRegistry::get(CardDefId id) const {
    auto it = cards_.find(id);
    return it != cards_.end() ? it->second.get() : nullptr;
}

bool CardRegistry::has(CardDefId id) const {
    return cards_.find(id) != cards_.end();
}

const CardDef* CardRegistry::classDef(CardDefId id) const {
    auto it = cards_.find(id);
    if (it == cards_.end()) return nullptr;
    return &it->second->def();  // every card defines itself
}

std::vector<CardDefId> CardRegistry::classDefIds() const {
    std::vector<CardDefId> ids;
    ids.reserve(cards_.size());
    for (const auto& [id, card] : cards_) ids.push_back(id);
    std::sort(ids.begin(), ids.end());
    return ids;
}

void CardRegistry::loadAll() {
    // One authoritative class per card, each in its own TU; the aggregator
    // (cards_init.cpp) registers them all. There is no override layer.
    registerAllCards(*this);
}

} // namespace riftbound
