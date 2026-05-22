#pragma once
/// @file riftbound_game.h
/// OpenSpiel Game subclass for Riftbound (Phase A, fully-observable v1).
///
/// Holds the heavy shared resources (CardDB, CardRegistry, loaded decks)
/// constructed once per game-instance. Every State spawned from this Game
/// reuses these via const references.
///
/// Game parameters (settable via LoadGame("riftbound(deck1=..., deck2=...)")):
///   - deck1     : path to player-1 deck JSON (default: decks/leblanc_test.json)
///   - deck2     : path to player-2 deck JSON (default: decks/leblanc_test.json)
///   - registry  : path to cards/registry.json    (default: cards/registry.json)
///   - seed      : RNG seed for the engine; 0 = nondeterministic (default: 0)

#include "open_spiel/spiel.h"

#include "cards/card_registry.h"
#include "core/card_db.h"
#include "rules/deck_validator.h"

#include <memory>

namespace riftbound::openspiel {

class RiftboundGame : public ::open_spiel::Game {
public:
    explicit RiftboundGame(const ::open_spiel::GameParameters& params);

    std::unique_ptr<::open_spiel::State> NewInitialState() const override;

    int NumDistinctActions() const override;
    int NumPlayers() const override { return 2; }
    double MinUtility() const override { return -1.0; }
    double MaxUtility() const override { return  1.0; }
    absl::optional<double> UtilitySum() const override { return  0.0; }
    int MaxGameLength() const override { return 10000; }
    int MaxChanceOutcomes() const override { return 0; }

    std::vector<int> ObservationTensorShape() const override;

    const ::riftbound::CardDB&        cardDb()       const { return card_db_; }
    const ::riftbound::CardRegistry&  cardRegistry() const { return registry_; }
    const ::riftbound::DeckSubmission& deck1()       const { return deck1_; }
    const ::riftbound::DeckSubmission& deck2()       const { return deck2_; }
    uint64_t seed() const { return seed_; }

private:
    ::riftbound::CardDB card_db_;
    ::riftbound::CardRegistry registry_;
    ::riftbound::DeckSubmission deck1_;
    ::riftbound::DeckSubmission deck2_;
    uint64_t seed_;
};

} // namespace riftbound::openspiel
