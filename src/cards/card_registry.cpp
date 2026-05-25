#include "cards/card_registry.h"

#include <cassert>

namespace riftbound {

// Manual card registrations
void registerManualEquipCards(CardRegistry& registry);
void registerManualWeaponmasterCards(CardRegistry& registry);
void registerManualDeckCards(CardRegistry& registry);
// Pre-release audit fixes — registered LAST so they override prior impls.
void registerAuditFixes0(CardRegistry& registry);
void registerAuditFixes1(CardRegistry& registry);
void registerAuditFixes2(CardRegistry& registry);
void registerAuditFixes3(CardRegistry& registry);
void registerAuditFixes4(CardRegistry& registry);
void registerAuditFixes5(CardRegistry& registry);
void registerAuditFixes6(CardRegistry& registry);
void registerAuditFixes7(CardRegistry& registry);

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
    registerAuditFixes0(*this);
    registerAuditFixes1(*this);
    registerAuditFixes2(*this);
    registerAuditFixes3(*this);
    registerAuditFixes4(*this);
    registerAuditFixes5(*this);
    registerAuditFixes6(*this);
    registerAuditFixes7(*this);
}

} // namespace riftbound
