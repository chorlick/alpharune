#include "cards/card_registry.h"

#include <cassert>

namespace riftbound {

// Manual card registrations
void registerManualEquipCards(CardRegistry& registry);
void registerManualWeaponmasterCards(CardRegistry& registry);
void registerManualDeckCards(CardRegistry& registry);

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

void CardRegistry::loadAll() {
    registerGeneratedCards(*this);
    // Manual overrides (registered AFTER generated — overwrites generated stubs)
    registerManualEquipCards(*this);
    registerManualWeaponmasterCards(*this);
    registerManualDeckCards(*this);
}

} // namespace riftbound
