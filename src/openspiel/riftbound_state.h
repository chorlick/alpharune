#pragma once
/// @file riftbound_state.h
/// OpenSpiel State subclass wrapping a live Riftbound game.
///
/// All threading + halting machinery lives inside `GameEngine`
/// (`beginGame` / `applyChoice` / `currentStep` — see game_engine.h).
/// This wrapper holds the engine + its EventBus, plus the seed it was
/// constructed with so `Clone()` can replay deterministically.
///
/// ── Lazy-clone mode (Phase C-1 mini-commit 9 / lazy-thread) ──
///
/// `Clone()` does NOT spawn a worker thread. Instead, the clone snapshots
/// the original's `GameState`, `StepResult`, and `GameResult` into private
/// fields and defers engine construction. Read-only methods
/// (LegalActions / CurrentPlayer / IsTerminal / Returns /
/// ObservationTensor / engineState) serve answers from the snapshot
/// directly. The first `DoApplyAction` call on the clone — or anything
/// that hands out the EventBus / sets a decision callback — triggers
/// `ensureLive()`, which builds the engine, calls `beginGame`, and
/// replays `action_history` so the engine reaches the snapshotted
/// decision point. From there normal `applyChoice` takes over.
///
/// MCTS in particular produces many clones that are never advanced
/// (search-tree leaves, rollout heads that terminate early). Each such
/// clone now costs a GameState copy (~tens of μs) instead of the full
/// thread spawn + history replay (~5 ms in mid-game).
///
/// Action IDs are dense vocab slots in [0, kVocabSize) (see action_vocab.h)
/// keyed by CardDefId — stable across clones, sized for AlphaZero policy
/// heads.

#include "open_spiel/spiel.h"

#include "core/events.h"
#include "core/game_state.h"
#include "engine/game_engine.h"

#include <memory>

namespace riftbound::openspiel {

class RiftboundGame;

class RiftboundState : public ::open_spiel::State {
public:
    explicit RiftboundState(std::shared_ptr<const ::open_spiel::Game> game);
    /// Internal: construct with an explicit engine seed. Used by Clone()
    /// so the replayed state plays the same game as the original (the
    /// public constructor draws random_device seed when the Game's seed
    /// param is 0).
    RiftboundState(std::shared_ptr<const ::open_spiel::Game> game,
                    uint64_t engine_seed);
    RiftboundState(const RiftboundState&) = delete;
    RiftboundState& operator=(const RiftboundState&) = delete;
    ~RiftboundState() override = default;

    /// Inspect the underlying engine's GameResult after the game is terminal.
    /// Defined only when IsTerminal() — values are zero/None before then.
    const ::riftbound::GameResult& engineResult() const {
        return is_lazy_ ? lazy_result_ : engine_->stepResult();
    }

    ::open_spiel::Player CurrentPlayer() const override;
    std::vector<::open_spiel::Action> LegalActions() const override;
    std::string ActionToString(::open_spiel::Player p,
                                ::open_spiel::Action a) const override;
    std::string ToString() const override;
    bool IsTerminal() const override;
    std::vector<double> Returns() const override;
    std::unique_ptr<::open_spiel::State> Clone() const override;

    /// Hidden-info-masked observation tensor for `player`. Zeroes out
    /// opponent hand identities, opponent main_deck identities, and
    /// facedown card identities; preserves what `player` would actually
    /// observe (own zones, opponent's public zones, observed reveals).
    void ObservationTensor(::open_spiel::Player player,
                            absl::Span<float> values) const override;

    /// Compact deterministic string key for an information set, used by
    /// ISMCTSBot to identify nodes in its search tree. Derived from the
    /// hidden-info-masked ObservationTensor — same masking rules apply,
    /// so two states `player` cannot distinguish hash to the same key.
    /// Format: hex-encoded SHA-style rolling hash of the masked tensor
    /// bytes. Not human-readable; that's not the contract OpenSpiel
    /// needs (it just keys node lookups).
    std::string ObservationString(::open_spiel::Player player) const override;

    /// Phase 6w — raw 64-bit info-set ID for CFR. Same masked FNV-1a
    /// hash as ObservationString but skips the hex encoding so CFR's
    /// unordered_map<uint64_t, ...> can key directly.
    ///
    /// The hash incorporates `player` (perspective) so the SAME
    /// engine state queried from P1's view vs P2's view produces
    /// DIFFERENT IDs — required for CFR which models per-player
    /// info sets.
    ///
    /// Collision properties: identical to ObservationString since
    /// they share the underlying byte stream. 64 bits over a 4623-
    /// float input is overkill for ISMCTS-scale trees; CFR's regret
    /// tables may grow larger but the birthday-paradox floor at
    /// 2^32 entries still gives <1% collision probability with 64-bit
    /// FNV-1a on uniform-ish inputs.
    uint64_t InfoSetId(::open_spiel::Player player) const;

    /// Sequence of action_ids applied so far, in order. Lives on the
    /// engine's GameState — Clone() preserves it via deep copy.
    const std::vector<int64_t>& actionHistory() const {
        return engineState().action_history;
    }

    /// Underlying engine GameState. Used by future Intent-aware agents
    /// (LibTorch when C++ AlphaZero training lands) to score the current
    /// decision point. Caller must NOT mutate.
    const ::riftbound::GameState& engineState() const {
        return is_lazy_ ? lazy_state_ : engine_->state();
    }

    /// Mutable access to the per-game EventBus. Lets the OpenSpiel match
    /// runner subscribe to `on_log` (for HTML replay trace capture) or any
    /// other debug observer. Forces ensureLive() — a snapshot clone has
    /// no live bus, and event-fire ordering is engine-internal.
    ::riftbound::EventBus& eventBus() {
        ensureLive();
        return *event_bus_;
    }

    /// Install a decision callback fired by the underlying GameEngine at
    /// each decision point. Forces ensureLive(); callbacks installed on
    /// snapshot clones would not fire on subsequent replay.
    void setOnDecisionCallback(::riftbound::GameEngine::DecisionCallback cb) {
        ensureLive();
        engine_->on_decision = std::move(cb);
    }

    /// Legal Intents at the current decision point in their rich form.
    /// LegalActions() returns the bit-packed int64 ActionID variant for
    /// OpenSpiel's tree algorithms; this method returns the matching
    /// Intent objects so an Intent-aware agent can score them directly.
    /// Aligned 1:1 with LegalActions() before the dedup-by-encoding
    /// pass — so use this for scoring, then map the chosen Intent's
    /// encoded id back to LegalActions() to apply.
    std::vector<::riftbound::Intent> legalIntents() const {
        if (is_lazy_) return lazy_step_.legal;
        return engine_->currentStep().legal;
    }

    /// Read the resolved engine seed (whether set explicitly or chosen
    /// randomly when the Game's seed param is 0). Lets the replay
    /// header record the actual seed used so a "nondeterministic" run
    /// is still reproducible by re-running with --seed <value>.
    uint64_t engineSeed() const { return engine_seed_; }

protected:
    void DoApplyAction(::open_spiel::Action action) override;

private:
    /// Private tag-dispatch constructor used by Clone() to build a
    /// snapshot-only state with no live engine. The clone's read-only
    /// methods read from `snap_state` / `snap_step` / `snap_result`
    /// until the first mutating call lazy-spawns the engine.
    struct SnapshotTag {};
    RiftboundState(std::shared_ptr<const ::open_spiel::Game> game,
                    uint64_t engine_seed,
                    ::riftbound::GameState snap_state,
                    ::riftbound::StepResult snap_step,
                    ::riftbound::GameResult snap_result,
                    SnapshotTag);

    /// Build engine + EventBus, run beginGame(), replay action_history
    /// to reach the snapshotted decision point, then clear lazy fields.
    /// No-op if already live. Idempotent.
    void ensureLive() const;

    const RiftboundGame& rb_game_;

    // Per-game engine resources. `event_bus_` must outlive `engine_`
    // (engine holds a reference to it). Both are heap-allocated via
    // unique_ptr so a lazy clone can leave them null until the first
    // mutating call — `ensureLive` populates them at that point.
    // `mutable` because lazy initialization happens via `ensureLive()`
    // which is logically a const operation from OpenSpiel's perspective
    // (LegalActions/CurrentPlayer are const but may need to materialize
    // the engine for paths beyond plain snapshot read).
    mutable std::unique_ptr<::riftbound::EventBus> event_bus_;
    mutable std::unique_ptr<::riftbound::GameEngine> engine_;

    // Lazy-clone snapshot fields. When `is_lazy_` is true, the engine
    // is null and read-only methods serve these. After `ensureLive()`
    // they are cleared and lose their authoritative role.
    mutable bool is_lazy_ = false;
    mutable ::riftbound::GameState lazy_state_;
    mutable ::riftbound::StepResult lazy_step_;
    mutable ::riftbound::GameResult lazy_result_;

    /// Concrete seed used by the engine. Resolved once in the constructor —
    /// if the Game's seed param is 0 ("nondeterministic"), we still pick
    /// a deterministic random seed for THIS state so Clone() can replay it
    /// faithfully. Without this, Clone()'s fresh engine would seed itself
    /// with `std::random_device{}()` and play a different game.
    uint64_t engine_seed_;
};

} // namespace riftbound::openspiel
