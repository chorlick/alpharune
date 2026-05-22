#pragma once
/// @file batch_runner.h
/// Parallel game execution via boost::asio::thread_pool.
/// Posts GameRunner work units to the pool, collects aggregate results.

#include "core/card_db.h"
#include "cards/card_registry.h"
#include "engine/game_runner.h"
#include "rules/deck_validator.h"

namespace riftbound {

class BatchRunner {
public:
    BatchRunner(const CardDB& card_db,
                const CardRegistry& card_registry,
                int num_threads);

    /// Run N games with the given decks and config template.
    /// Results are written to the provided AggregateResults object.
    void runBatch(
        const DeckSubmission& deck1,
        const DeckSubmission& deck2,
        int num_games,
        const GameConfig& config_template,
        AggregateResults& results);

private:
    const CardDB& card_db_;
    const CardRegistry& card_registry_;
    int num_threads_;
};

} // namespace riftbound
