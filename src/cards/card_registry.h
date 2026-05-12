#pragma once
/// @file card_registry.h
/// Maps CardDefId → Card object for dispatch.
///
/// Replaces EffectExecutor/EffectScript dispatch. Every card in the game
/// has a registered Card object that implements its behavior.

#include "cards/card.h"
#include "core/types.h"

#include <memory>
#include <unordered_map>

namespace riftbound {

class CardRegistry {
public:
    /// Register a card implementation. Overwrites any existing registration.
    void registerCard(CardDefId id, std::unique_ptr<Card> card);

    /// Get the Card object for a given CardDefId. Returns nullptr if not registered.
    Card* get(CardDefId id) const;

    /// Check if a card is registered.
    bool has(CardDefId id) const;

    /// Number of registered cards.
    size_t size() const { return cards_.size(); }

    /// Register all card implementations (calls generated + manual init code).
    void loadAll();

private:
    std::unordered_map<CardDefId, std::unique_ptr<Card>> cards_;
};

/// Register all generated card implementations.
/// Defined in generated/registry_init.cpp.
void registerGeneratedCards(CardRegistry& registry);

} // namespace riftbound
