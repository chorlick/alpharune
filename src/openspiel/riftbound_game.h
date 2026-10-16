#pragma once
/// @file riftbound_game.h
/// OpenSpiel Game subclass for Riftbound (Phase A, fully-observable v1).
///
/// Holds the two loaded decks, and refers to the card table (CardDB,
/// CardRegistry). The card table is built once per process and shared by
/// every RiftboundGame (it is compiled-in, const data: see `sharedCards`);
/// every State spawned from this Game reuses all of it via const references.
///
/// Game parameters (settable via LoadGame("riftbound(deck1=..., deck2=...)")):
///   - deck1     : path to player-1 deck list .txt (default: decks/yi_rebuild.txt)
///   - deck2     : path to player-2 deck list .txt (default: decks/yi_rebuild.txt)
///   - registry  : ignored (default: empty). Card data and ban status are
///                 compiled into the engine; the parameter is accepted only so
///                 older game strings still parse
///   - seed      : RNG seed for the engine; 0 = nondeterministic (default: 0)
///   - game_mode : "duel" (default: each player's setup battlefield drawn at
///                 random, CR 487.5) or "match" (each player chooses it, CR
///                 488.5 — what the trainer plays); testing phase V1-G4

#include "open_spiel/spiel.h"

#include "cards/card_registry.h"
#include "core/card_db.h"
#include "rules/deck_validator.h"

#include <memory>

namespace riftbound::openspiel {

/// Longest game OpenSpiel will see, in applied actions. RiftboundState is
/// terminal (a draw) once MoveNumber() reaches it — the bound that keeps
/// random rollouts finite (see RiftboundState::IsTerminal).
inline constexpr int kMaxGameLength = 600;

/// The card table every RiftboundGame uses: registry and database, built from
/// the card classes on first use (thread-safe) and never changed afterwards.
/// Card objects are stateless, so concurrent games read it freely.
///
/// LoadGame used to rebuild both per call — once per search agent per game —
/// at 1.25 ms of its 1.39 ms; what is left is reading the two deck files
/// (plans/06-performance.md item 6.8).
struct SharedCards {
    ::riftbound::CardRegistry registry;
    ::riftbound::CardDB db;
};
const SharedCards& sharedCards();

class RiftboundGame : public ::open_spiel::Game {
public:
    explicit RiftboundGame(const ::open_spiel::GameParameters& params);

    std::unique_ptr<::open_spiel::State> NewInitialState() const override;

    int NumDistinctActions() const override;
    int NumPlayers() const override { return 2; }
    double MinUtility() const override { return -1.0; }
    double MaxUtility() const override { return  1.0; }
    absl::optional<double> UtilitySum() const override { return  0.0; }
    int MaxGameLength() const override { return kMaxGameLength; }
    int MaxChanceOutcomes() const override { return 0; }

    std::vector<int> ObservationTensorShape() const override;

    const ::riftbound::CardDB&        cardDb()       const { return card_db_; }
    const ::riftbound::CardRegistry&  cardRegistry() const { return registry_; }
    const ::riftbound::DeckSubmission& deck1()       const { return deck1_; }
    const ::riftbound::DeckSubmission& deck2()       const { return deck2_; }
    uint64_t seed() const { return seed_; }
    ::riftbound::MatchFormat matchFormat() const { return match_format_; }

private:
    const ::riftbound::CardDB& card_db_;
    const ::riftbound::CardRegistry& registry_;
    ::riftbound::DeckSubmission deck1_;
    ::riftbound::DeckSubmission deck2_;
    uint64_t seed_;
    ::riftbound::MatchFormat match_format_;
};

} // namespace riftbound::openspiel
