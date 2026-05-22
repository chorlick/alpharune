#include "batch_runner.h"

#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>
#include <iostream>

namespace riftbound {

BatchRunner::BatchRunner(const CardDB& card_db,
                         const CardRegistry& card_registry,
                         int num_threads)
    : card_db_(card_db), card_registry_(card_registry),
      num_threads_(num_threads) {}

void BatchRunner::runBatch(
    const DeckSubmission& deck1,
    const DeckSubmission& deck2,
    int num_games,
    const GameConfig& config_template,
    AggregateResults& results) {

    results.total_games = num_games;

    // Single-threaded fast path
    if (num_threads_ <= 1) {
        for (int i = 0; i < num_games; ++i) {
            GameConfig config = config_template;
            config.game_index = i;
            config.total_games = num_games;
            GameRunner runner(card_db_, card_registry_,
                            deck1, deck2, config, results);
            runner.run();
        }
        return;
    }

    // Multi-threaded via boost::asio::thread_pool
    std::cout << "Running " << num_games << " games on "
              << num_threads_ << " threads...\n";

    boost::asio::thread_pool pool(num_threads_);

    for (int i = 0; i < num_games; ++i) {
        boost::asio::post(pool, [&, i]() {
            GameConfig config = config_template;
            config.game_index = i;
            config.total_games = num_games;
            GameRunner runner(card_db_, card_registry_,
                            deck1, deck2, config, results);
            runner.run();
        });
    }

    pool.join();  // wait for all games to complete

    if (num_games > 20) {
        std::cout << "\n"; // newline after progress indicator
    }

}

} // namespace riftbound
