#pragma once
/// @file config_driver.h
/// Bridges a parsed `riftbound::config::TrainingConfig` to the V2
/// self-play + trainer pipeline. Owns the iteration loop, samples
/// (self_deck, opp_deck) pairs from the configured deck pools per
/// game, drives self-play, runs trainer steps, and writes checkpoints.
///
/// Per-iteration deck sampling:
///   With probability `mirror_ratio`, sample BOTH decks from `self`
///   (true mirror match — the model plays itself with the same
///   deck on both sides). Otherwise sample one deck from `self` and
///   one from `opponent`. The trained model always controls Player1;
///   side variance is handled by OpenSpiel's seed shuffling on
///   subsequent iterations.
///
/// Per-legend training is enforced upstream by `parseConfig` —
/// every deck in `self` must declare the same legend as
/// `training.legend`. This driver trusts that invariant and does
/// not re-validate.
///
/// Output:
///   - Checkpoints: `<checkpoint_dir>/iter_<n>.pt`. Frequency
///     controlled by `checkpoint_every_iter`. Old checkpoints pruned
///     to `keep_last_n` (0 = keep all).
///   - Per-iter stdout log line (always).
///   - Per-decision JSONL log if `training.log_jsonl` set (future).
///
/// Returns the number of iterations completed (== cfg.iterations on
/// success). Throws on irrecoverable error (model load failure,
/// device unavailable, etc).

#include "config/config.h"

namespace riftbound::training {

/// Run a full training session per the TrainingConfig. Returns the
/// number of iterations completed.
int runConfigTraining(const ::riftbound::config::Config& cfg);

}  // namespace riftbound::training
