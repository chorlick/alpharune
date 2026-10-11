#include "batch_runner.h"

#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>
#include <atomic>
#include <exception>
#include <iostream>
#include <mutex>

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

    // An exception escaping a pool task would terminate the process. Keep
    // the first one, skip the games not yet started, and rethrow it on the
    // calling thread — the same thing the single-threaded path does.
    std::mutex failure_mu;
    std::exception_ptr failure;
    std::atomic<bool> failed{false};

    for (int i = 0; i < num_games; ++i) {
        boost::asio::post(pool, [&, i]() {
            if (failed.load()) return;
            try {
                GameConfig config = config_template;
                config.game_index = i;
                config.total_games = num_games;
                GameRunner runner(card_db_, card_registry_,
                                deck1, deck2, config, results);
                runner.run();
            } catch (...) {
                std::lock_guard<std::mutex> lock(failure_mu);
                if (!failure) failure = std::current_exception();
                failed.store(true);
            }
        });
    }

    pool.join();  // wait for all games to complete

    if (failure) std::rethrow_exception(failure);

    if (num_games > 20) {
        std::cout << "\n"; // newline after progress indicator
    }

}

} // namespace riftbound
