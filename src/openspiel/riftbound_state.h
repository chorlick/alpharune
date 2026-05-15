#pragma once
/// @file riftbound_state.h
/// OpenSpiel State subclass wrapping a live Riftbound game.
///
/// All threading + halting machinery now lives inside `GameEngine`
/// (`beginGame` / `applyChoice` / `currentStep` — see game_engine.h).
/// This wrapper holds only the engine + its EventBus, plus the seed it
/// was constructed with so `Clone()` can replay deterministically.
///
/// Phase B: ActionIDs are bit-packed structural encodings of Intents
/// (see action_encoding.h) computed from CardDefIds — stable across clones.
/// Clone() still uses replay (spawns a fresh state with the same game
/// seed and re-applies the recorded action history). Phase C-1's native
/// step machine will collapse it to memcpy(GameState).

#include "open_spiel/spiel.h"

#include "core/events.h"
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
        return engine_.stepResult();
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

    /// Sequence of action_ids applied so far, in order. Lives on the
    /// engine's GameState — Clone() preserves it automatically once the
    /// native step machine replaces replay-based Clone.
    const std::vector<int64_t>& actionHistory() const {
        return engine_.state().action_history;
    }

protected:
    void DoApplyAction(::open_spiel::Action action) override;

private:
    const RiftboundGame& rb_game_;

    // Per-game engine resources. EventBus and GameEngine outlive any
    // step-machine session; the engine owns the worker thread internally
    // and joins it in its destructor.
    ::riftbound::EventBus event_bus_;
    ::riftbound::GameEngine engine_;

    /// Concrete seed used by the engine. Resolved once in the constructor —
    /// if the Game's seed param is 0 ("nondeterministic"), we still pick
    /// a deterministic random seed for THIS state so Clone() can replay it
    /// faithfully. Without this, Clone()'s fresh engine would seed itself
    /// with `std::random_device{}()` and play a different game.
    uint64_t engine_seed_;
};

} // namespace riftbound::openspiel
