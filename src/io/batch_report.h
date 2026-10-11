#pragma once
/// @file batch_report.h
/// What the `riftbound` binary prints after a batch (--games N): the results
/// table, and — when a game failed — the table for the games that did
/// complete followed by the error. In a header of its own so the tests can
/// check the text (main.cpp is not linked into them).

#include "engine/game_runner.h"

#include <ostream>
#include <string>

namespace riftbound {

/// The results table. `requested` is --games; when fewer completed (a game
/// failed) the heading says so and the averages are over the completed games.
inline void printBatchResults(std::ostream& out, const AggregateResults& results,
                              int requested, int threads) {
    const int completed = results.games_completed.load();
    if (completed == requested) {
        out << "\nResults (" << requested << " games):\n";
    } else {
        out << "\nResults (" << completed << " of " << requested << " games completed):\n";
    }
    out << "  P1 wins: " << results.p1_wins.load() << "\n"
        << "  P2 wins: " << results.p2_wins.load() << "\n"
        << "  Draws:   " << results.draws.load()   << "\n";
    if (requested > 1 && completed > 0) {
        out << "  Avg turns: " << (results.total_turns.load() / completed) << "\n"
            << "  Avg decisions: " << (results.total_decisions.load() / completed) << "\n"
            << "  Threads: " << threads << "\n";
    }
}

/// A batch that stopped on a failed game: the table for what completed, then
/// the error (which names the game and its seed). The caller exits non-zero.
inline void reportFailedBatch(std::ostream& out, std::ostream& err,
                              const AggregateResults& results, int requested, int threads,
                              const std::string& what) {
    printBatchResults(out, results, requested, threads);
    out.flush();
    err << "\nError: a game failed: " << what << "\n"
        << "  (" << results.games_completed.load() << " of " << requested
        << " games completed)\n";
}

} // namespace riftbound
