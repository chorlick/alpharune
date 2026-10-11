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
    /// Register a card implementation under `id`, which must be the card's own
    /// `def().id` and not registered yet. Throws std::logic_error otherwise.
    void registerCard(CardDefId id, std::unique_ptr<Card> card);

    /// Get the Card object for a given CardDefId. Returns nullptr if not registered.
    /// A table lookup: the engine asks this for every object it looks at in
    /// every trigger scan, aura pass and legal-action pass (14.9 million times
    /// per 1000 random games when it was a hash lookup — plans/06 item 6.3).
    Card* get(CardDefId id) const {
        const uint32_t n = id.raw();
        return n < by_id_.size() ? by_id_[n] : (n < kTableLimit ? nullptr : find(id));
    }

    /// Check if a card is registered.
    bool has(CardDefId id) const;

    /// Number of registered cards.
    size_t size() const { return cards_.size(); }

    /// Register all card implementations (calls registerAllCards).
    void loadAll();

    /// The class-owned CardDef for `id` (Card::def()), or nullptr if no card
    /// is registered under that id.
    const CardDef* classDef(CardDefId id) const;

    /// Sorted ids of all registered cards (each owns its data via Card::def()).
    std::vector<CardDefId> classDefIds() const;

private:
    Card* find(CardDefId id) const;   // the map: ids too large for the table
    std::unordered_map<CardDefId, std::unique_ptr<Card>> cards_;
    // cards_ by id, for get(). Card ids are small (the vocabulary is 1100);
    // an id of kTableLimit or more stays in the map only.
    static constexpr uint32_t kTableLimit = 1u << 16;
    std::vector<Card*> by_id_;
};

/// Registers every card. One authoritative class per card lives in its own TU
/// under src/cards/<type>/<id>_<slug>.cpp; the checked-in cards_init.cpp defines
/// this by calling each card's register_card_<id>().
void registerAllCards(CardRegistry& registry);

} // namespace riftbound
