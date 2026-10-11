#pragma once
/// @file riftbound_state.h
/// OpenSpiel State subclass wrapping a live Riftbound game.
///
/// All threading + halting machinery lives inside `GameEngine`
/// (`beginGame` / `applyChoice` / `currentStep` — see game_engine.h).
/// This wrapper holds the engine + its EventBus, plus the seed it was
/// constructed with so a clone can be rebuilt deterministically.
///
/// ── Clones ──
///
/// `Clone()` does not build an engine. The clone holds a shared, immutable
/// snapshot of the original (`GameState`, the pending decision, the result,
/// the engine's RNG stream) and serves the read-only methods from it. The
/// first `DoApplyAction` on the clone — or anything that hands out the
/// EventBus / sets a decision callback — calls `ensureLive()`, which builds
/// an engine that is in exactly the original's position:
///
///   * Snapshot taken at a top-level main-phase decision
///     (`GameEngine::isResumableDecision`): the engine is resumed straight
///     from the snapshot. This is the common case and costs one GameState
///     copy.
///   * Snapshot taken anywhere else (mulligan, chain priority, showdown,
///     combat, cost payment, a card's play-time prompt): the engine is in
///     the middle of a call that GameState does not capture, so it is
///     rebuilt from the nearest resumable ancestor snapshot (or from
///     `beginGame`) by replaying the exact choices made since —
///     `GameState::action_history` holds one history token per decision
///     (action_vocab.h), which identifies the chosen Intent, not just its
///     lossy vocab slot.
///
/// Either way the rebuilt engine is checked against the snapshot's pending
/// decision; a mismatch throws instead of silently playing a different game.
///
/// MCTS produces many clones that are never advanced (search-tree leaves,
/// rollout heads that terminate early). Those cost a pointer copy.
///
/// Action IDs are dense vocab slots in [0, kVocabSize) (see action_vocab.h)
/// keyed by CardDefId — stable across clones, sized for AlphaZero policy
/// heads.

#include "open_spiel/spiel.h"

#include "core/events.h"
#include "core/game_state.h"
#include "engine/game_engine.h"

#include <memory>
#include <random>

namespace riftbound::openspiel {

class RiftboundGame;

class RiftboundState : public ::open_spiel::State {
public:
    explicit RiftboundState(std::shared_ptr<const ::open_spiel::Game> game);
    /// Construct with an explicit engine seed (the one-argument constructor
    /// draws a random_device seed when the Game's seed param is 0).
    RiftboundState(std::shared_ptr<const ::open_spiel::Game> game,
                    uint64_t engine_seed);
    RiftboundState(const RiftboundState&) = delete;
    RiftboundState& operator=(const RiftboundState&) = delete;
    ~RiftboundState() override = default;

    /// Build a state that is in exactly the position of a game being run
    /// elsewhere (a `GameEngine::runGame` game an agent is deciding in), by
    /// replaying `live_state.action_history` from `beginGame` with
    /// `engine_seed`. Returns nullptr when the history does not reproduce
    /// `live_state` / `live_step` — a wrong seed, or a state edited behind
    /// the engine's back. Engine exceptions propagate.
    ///
    /// `live_step.perspective` may be PlayerId::None when the caller does
    /// not know which seat is being asked (an agent is not told); the
    /// rebuilt state's CurrentPlayer() then says who it is.
    ///
    /// The result is exact, RNG stream included, so clones of it play the
    /// live game's future. Its OpenSpiel move number starts at 0: the
    /// IsTerminal() length cap then bounds a search from this position
    /// rather than counting the live game's past.
    static std::unique_ptr<RiftboundState> rebuildFromHistory(
        std::shared_ptr<const ::open_spiel::Game> game,
        uint64_t engine_seed,
        const ::riftbound::GameState& live_state,
        const ::riftbound::StepResult& live_step);

    /// Bring a state made by `rebuildFromHistory` forward to the live
    /// game's current decision by replaying only the history tokens it has
    /// not applied yet. Returns false (and leaves this state unusable — make
    /// a new one) if this state's history is not a prefix of the live one or
    /// the replay does not arrive at `live_step`.
    bool syncToHistory(const ::riftbound::GameState& live_state,
                       const ::riftbound::StepResult& live_step);

    /// Build a state directly from a GameState snapshot, without a replay.
    /// For callers that cannot reproduce the game from its history
    /// (`rebuildFromHistory` returned nullptr).
    ///
    /// NOT exact: the engine's RNG stream is not part of GameState, so the
    /// clone shuffles with a stream seeded from `rng_seed`, not the live
    /// game's. And only a snapshot at a resumable decision
    /// (`GameEngine::isResumableDecision(snap_state, snap_step.legal)`) can
    /// be advanced this way; for any other, the first ApplyAction falls back
    /// to replaying the history from `engine_seed` and throws if that does
    /// not reproduce the snapshot.
    ///
    /// `snap_step.kind` should be NeedDecision (if the game isn't over),
    /// `snap_step.perspective` the deciding player, `snap_step.legal` the
    /// engine's full Intent list at that decision. `snap_result` is a
    /// default-constructed GameResult unless `snap_state.game_over`.
    static std::unique_ptr<RiftboundState> makeFromSnapshot(
        std::shared_ptr<const ::open_spiel::Game> game,
        uint64_t engine_seed,
        const ::riftbound::GameState& snap_state,
        ::riftbound::StepResult        snap_step,
        ::riftbound::GameResult        snap_result,
        uint64_t                       rng_seed);

    /// Inspect the underlying engine's GameResult after the game is terminal.
    /// Defined only when IsTerminal() — values are zero/None before then.
    const ::riftbound::GameResult& engineResult() const {
        return lazy_ ? lazy_->result : engine_->stepResult();
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

    /// Sequence of action ids (vocab slots) applied so far, in order, one
    /// per decision; -1 for a decision whose Intent had no slot. Derived
    /// from the engine's `GameState::action_history` tokens.
    std::vector<int64_t> actionHistory() const;

    /// Underlying engine GameState. Used by future Intent-aware agents
    /// (LibTorch when C++ AlphaZero training lands) to score the current
    /// decision point. Caller must NOT mutate.
    const ::riftbound::GameState& engineState() const {
        return lazy_ ? lazy_->state : engine_->state();
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
    /// In the engine's order, not LegalActions()' (which is deduplicated
    /// by slot and sorted) — so use this for scoring, then map the chosen
    /// Intent's encoded id back to LegalActions() to apply.
    ///
    /// A view, not a copy: valid until this state is next advanced (or
    /// destroyed). Copy it to keep it across an ApplyAction.
    const std::vector<::riftbound::Intent>& legalIntents() const {
        if (lazy_) return lazy_->step.legal;
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
    /// An immutable copy of a position: everything a clone needs to answer
    /// read-only queries, and to build an engine in that position.
    struct Snapshot {
        ::riftbound::GameState  state;
        ::riftbound::StepResult step;      // the pending decision (or Done)
        ::riftbound::GameResult result;
        std::mt19937_64         rng;       // engine RNG stream at this point
        /// GameEngine::isResumableDecision(state, step.legal): an engine can
        /// be resumed straight from this snapshot.
        bool resumable = false;
    };

    /// Private tag-dispatch constructor: a snapshot-only state with no
    /// engine. `base` is the snapshot's nearest resumable ancestor (itself
    /// if it is resumable; null = the start of the game).
    struct SnapshotTag {};
    RiftboundState(std::shared_ptr<const ::open_spiel::Game> game,
                    uint64_t engine_seed,
                    std::shared_ptr<const Snapshot> snapshot,
                    std::shared_ptr<const Snapshot> base,
                    SnapshotTag);

    /// Build the engine a snapshot-only state stands for (see the file
    /// comment). No-op if already live. Throws std::runtime_error if the
    /// engine cannot be brought to the snapshot's decision.
    void ensureLive() const;

    /// Copy the live engine's position (cached until the next action).
    std::shared_ptr<const Snapshot> snapshotLive() const;

    /// Apply the legal action at `legal_index` to the live engine.
    void applyLegalIndex(int legal_index);

    /// Replay `tokens[from..)` on `engine`. Throws std::runtime_error if a
    /// token matches no legal action. If `refresh_base`, keeps `base_` at
    /// the last resumable decision passed.
    void replayTokens(::riftbound::GameEngine& engine,
                      const std::vector<int64_t>& tokens, size_t from,
                      bool refresh_base) const;

    const RiftboundGame& rb_game_;

    // Per-game engine resources. `event_bus_` must outlive `engine_`
    // (engine holds a reference to it). Both are heap-allocated via
    // unique_ptr so a snapshot-only clone can leave them null until the
    // first mutating call — `ensureLive` populates them at that point.
    // `mutable` because that happens via `ensureLive()`, which is logically
    // a const operation from OpenSpiel's perspective.
    mutable std::unique_ptr<::riftbound::EventBus> event_bus_;
    mutable std::unique_ptr<::riftbound::GameEngine> engine_;

    // Non-null while this state is snapshot-only (no engine). Shared with
    // the state it was cloned from and with its own clones.
    mutable std::shared_ptr<const Snapshot> lazy_;

    // Nearest resumable ancestor of this state's position, or null for the
    // start of the game. A position that is not itself resumable is rebuilt
    // from here by replaying the history tokens recorded since.
    mutable std::shared_ptr<const Snapshot> base_;

    // Snapshot of the live engine's current position, made by the first
    // Clone() at it and reused by later ones. Reset by every action.
    mutable std::shared_ptr<const Snapshot> live_snapshot_;

    /// Concrete seed used by the engine. Resolved once in the constructor —
    /// if the Game's seed param is 0 ("nondeterministic"), we still pick
    /// a concrete seed for THIS state so a clone that has to be rebuilt
    /// from `beginGame` replays the same game.
    uint64_t engine_seed_;
};

} // namespace riftbound::openspiel
