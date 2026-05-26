#include "cards/card_registry.h"
#include "core/card_db.h"

#include <algorithm>
#include <cassert>
#include <sstream>

namespace riftbound {

// Post-refactor: one registration function per card, aggregated by the
// generated cards_init.cpp. Each card's authoritative class lives in its own
// translation unit under src/cards/<type>/<id>_<slug>.cpp.
void registerAllCards(CardRegistry& registry);

void CardRegistry::registerCard(CardDefId id, std::unique_ptr<Card> card) {
    cards_[id] = std::move(card);
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
    // One authoritative class per card, each in its own TU; the generated
    // aggregator registers them all. Precedence (manual-over-generated) was
    // resolved at split time, so there is no longer an override layer.
    registerAllCards(*this);
}

} // namespace riftbound
