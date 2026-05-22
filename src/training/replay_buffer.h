#pragma once
/// @file replay_buffer.h
/// AlphaZero-style replay buffer for self-play tuples — V2 entity-token
/// flavor.
///
/// Each sample captures one decision point during self-play. The state
/// is stored as a structured entity-token snapshot (per-token card_def,
/// zone, domain, stance, perspective, chain_index, spatial_node, stats,
/// + validity mask) rather than the V1 flat 4623-dim observation vector.
/// The trainer stacks these into V2Model's batched tensor inputs.
///
/// Per-sample memory (kMaxEntityTokens=256, kEntityStatsDim=5):
///   8 categorical fields × 256 × int32   = 8192 B
///   stats: 256 × 5 × float32              = 5120 B
///   legal_actions (~80) + policy (~80)    ≈  640 B
///   value                                  =    4 B
///   total                                  ≈ 14 KB / tuple
/// At capacity=100k that's ~1.4 GB.
///
/// Thread-safe via a single mutex — fine for sequential self-play +
/// trainer. Convert to a sharded structure if we ever need >1
/// concurrent producer.

#include "ml/entity_tokens.h"

#include <cstdint>
#include <mutex>
#include <random>
#include <utility>
#include <vector>

namespace riftbound::training {

struct ReplaySample {
    // ── V2 entity-token snapshot (information-set masked for the
    // ── acting player). Fields mirror ml::EntityTokens 1:1.
    // Populated when training V2 (cfg.model.kind == "v2"); left empty
    // for V3 to save memory (~14 KB/tuple).
    ::riftbound::ml::EntityTokens tokens;

    // Phase 6v — flat state-feature snapshot (kStateFeatureDim floats,
    // information-set masked for the acting player). Populated when
    // training V3 (cfg.model.kind == "v3"); left empty for V2.
    // Mirrors what RiftboundState::ObservationTensor exposes.
    std::vector<float> flat_state;

    // Legal action ids at this decision point. Used at training time
    // to mask illegal logits to -inf before log_softmax.
    std::vector<int32_t> legal_actions;

    // MCTS visit-count distribution over the legal_actions list (same
    // length, indices aligned). Sums to 1.0 (or 0 if MCTS produced no
    // visits — e.g. a forced-pass decision).
    std::vector<float> policy_over_legal;

    // Game outcome from the acting player's perspective: +1 win,
    // -1 loss, 0 draw. Filled in when the game finishes.
    float value = 0.0f;
};

class ReplayBuffer {
public:
    explicit ReplayBuffer(size_t capacity);

    /// Append one sample. When at capacity, overwrites the oldest slot
    /// (FIFO ring). Thread-safe.
    void push(ReplaySample sample);

    /// Sample `k` tuples uniformly with replacement. Returns copies so
    /// the caller can use them after the lock is released without
    /// blocking producers. Thread-safe.
    std::vector<ReplaySample> sample(size_t k, std::mt19937& rng) const;

    /// Current number of stored samples (≤ capacity).
    size_t size() const;

    /// Total number of `push()` calls since construction. Unlike
    /// `size()`, this keeps incrementing once the ring fills — useful
    /// for per-iter "how much new data was generated" stats that
    /// would otherwise read 0 once at capacity.
    int64_t total_pushes() const;

    /// Capacity of the ring buffer.
    size_t capacity() const { return capacity_; }

private:
    mutable std::mutex mu_;
    std::vector<ReplaySample> buf_;
    size_t capacity_;
    size_t next_idx_ = 0;
    int64_t total_pushes_ = 0;
};

} // namespace riftbound::training
