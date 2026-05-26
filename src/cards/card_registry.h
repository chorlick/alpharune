#pragma once
/// @file card_registry.h
/// Maps CardDefId → Card object for dispatch.
///
/// Replaces EffectExecutor/EffectScript dispatch. Every card in the game
/// has a registered Card object that implements its behavior.

#include "cards/card.h"
#include "core/types.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

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

    /// The class-owned CardDef for `id`, or nullptr if that card was built with
    /// the legacy id-only constructor (data still comes from registry.json).
    const CardDef* classDef(CardDefId id) const;

    /// Sorted ids of all registered cards (each owns its data via Card::def()).
    std::vector<CardDefId> classDefIds() const;

private:
    std::unordered_map<CardDefId, std::unique_ptr<Card>> cards_;
};

/// Registers every card. One authoritative class per card lives in its own TU
/// under src/cards/<type>/<id>_<slug>.cpp; the generated cards_init.cpp defines
/// this by calling each card's register_card_<id>().
void registerAllCards(CardRegistry& registry);

} // namespace riftbound
