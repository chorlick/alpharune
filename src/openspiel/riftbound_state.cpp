#include "riftbound_state.h"
#include "riftbound_game.h"

#include "action_encoding.h"

#include "ml/feature_extractor.h"

#include "open_spiel/spiel_utils.h"

#include <algorithm>
#include <random>
#include <unordered_set>

namespace riftbound::openspiel {

namespace {

::open_spiel::Player playerIdToOpenSpiel(::riftbound::PlayerId pid) {
    if (pid == ::riftbound::PlayerId::Player1) return 0;
    if (pid == ::riftbound::PlayerId::Player2) return 1;
    return ::open_spiel::kTerminalPlayerId;
}

} // namespace

RiftboundState::RiftboundState(std::shared_ptr<const ::open_spiel::Game> game)
    : RiftboundState(game,
                      static_cast<const RiftboundGame*>(game.get())->seed() != 0
                        ? static_cast<const RiftboundGame*>(game.get())->seed()
                        : std::random_device{}()) {}

RiftboundState::RiftboundState(std::shared_ptr<const ::open_spiel::Game> game,
                                uint64_t engine_seed)
    : ::open_spiel::State(game),
      rb_game_(*static_cast<const RiftboundGame*>(game.get())),
      engine_(rb_game_.cardDb(), event_bus_, rb_game_.cardRegistry()),
      engine_seed_(engine_seed) {
    // Engine spawns its own worker thread internally and runs until the
    // first decision (or termination). Returns the first StepResult,
    // which we'll consult via engine_.currentStep().
    engine_.beginGame(rb_game_.deck1(), rb_game_.deck2(), engine_seed_);
}

::open_spiel::Player RiftboundState::CurrentPlayer() const {
    if (engine_.isStepDone()) return ::open_spiel::kTerminalPlayerId;
    return playerIdToOpenSpiel(engine_.currentStep().perspective);
}

std::vector<::open_spiel::Action> RiftboundState::LegalActions() const {
    if (engine_.isStepDone()) return {};
    const auto& legal = engine_.currentStep().legal;
    const auto& state = engine_.state();
    std::vector<::open_spiel::Action> out;
    out.reserve(legal.size());
    std::unordered_set<int64_t> seen;
    seen.reserve(legal.size());
    for (const auto& intent : legal) {
        int64_t id = encodeAction(intent, state);
        // Collapse encoding collisions — two legal Intents with the
        // same structural ID are game-tree-equivalent decisions and
        // OpenSpiel expects unique action IDs per legal slot.
        if (seen.insert(id).second) {
            out.push_back(static_cast<::open_spiel::Action>(id));
        }
    }
    return out;
}

std::string RiftboundState::ActionToString(::open_spiel::Player,
                                            ::open_spiel::Action a) const {
    if (engine_.isStepDone()) return "<terminal>";
    const auto& legal = engine_.currentStep().legal;
    if (const auto* intent = decodeAction(a, legal, engine_.state())) {
        return intent->describe();
    }
    return "<unknown action_id>";
}

std::string RiftboundState::ToString() const {
    if (engine_.isStepDone()) {
        return std::string("<terminal: ")
            + engine_.stepResult().termination_reason + ">";
    }
    return std::string("Riftbound state @ turn ")
        + std::to_string(engine_.state().turn.turn_number);
}

bool RiftboundState::IsTerminal() const { return engine_.isStepDone(); }

std::vector<double> RiftboundState::Returns() const {
    if (!engine_.isStepDone()) return {0.0, 0.0};
    const auto& result = engine_.stepResult();
    if (result.winner == ::riftbound::PlayerId::Player1) return {1.0, -1.0};
    if (result.winner == ::riftbound::PlayerId::Player2) return {-1.0, 1.0};
    return {0.0, 0.0};
}

void RiftboundState::ObservationTensor(::open_spiel::Player player,
                                        absl::Span<float> values) const {
    // extractStateFeatures already implements information-set masking:
    //   - opponent hand identities are NOT serialized (is_self gate)
    //   - opponent main_deck identities are NEVER emitted (only the SELF
    //     deck/trash/banishment + opponent's PUBLIC trash/banishment are
    //     written as multi-hot zone vectors)
    //   - facedown cards are featurized by count and age only — never
    //     their card_def_id contents
    // So perspective masking is purely a function of `perspective`.
    ::riftbound::PlayerId rb_player = (player == 0)
        ? ::riftbound::PlayerId::Player1
        : ::riftbound::PlayerId::Player2;
    auto feats = ::riftbound::ml::extractStateFeatures(
        engine_.state(), rb_player, rb_game_.cardDb());
    const size_t n = std::min(values.size(), feats.size());
    for (size_t i = 0; i < n; ++i) values[i] = feats[i];
    for (size_t i = n; i < values.size(); ++i) values[i] = 0.0f;
}

std::unique_ptr<::open_spiel::State> RiftboundState::Clone() const {
    // Replay-based Clone: spawn a fresh state on the same Game (same seed,
    // same decks, same registry) and re-apply our recorded action history.
    //
    // Slow — O(n) per clone where n = action count so far. Phase C-1 will
    // replace this with memcpy(GameState) once the native step machine
    // makes the engine state copyable.
    auto fresh = std::make_unique<RiftboundState>(game_, engine_seed_);
    const auto history = engine_.state().action_history;
    for (auto a : history) {
        if (fresh->IsTerminal()) break;
        fresh->ApplyAction(static_cast<::open_spiel::Action>(a));
    }
    return fresh;
}

void RiftboundState::DoApplyAction(::open_spiel::Action action) {
    // Defensive no-op on terminal: there's a benign race where the
    // OpenSpiel-side IsTerminal() check returns false but the engine
    // finishes by the time DoApplyAction runs (a previous waitForDecision
    // returned with at_decision_=true, but the engine can subsequently
    // process more selectAction calls internally — e.g. empty-legal-action
    // sites — and reach markDone before we get here).
    if (engine_.isStepDone()) {
        return;
    }

    // Translate bit-packed action_id into the engine-side legal-list
    // index. The engine's StepDriver returns paused_legal_[index] back
    // to the engine; we find the first index whose Intent encodes to
    // the same id.
    const auto& legal = engine_.currentStep().legal;
    const auto& state = engine_.state();
    int legal_index = -1;
    for (size_t i = 0; i < legal.size(); ++i) {
        if (encodeAction(legal[i], state) == action) {
            legal_index = static_cast<int>(i);
            break;
        }
    }
    if (legal_index < 0) {
        // Defensive: unknown action id. Fall back to first legal so the
        // engine doesn't deadlock. This branch shouldn't fire for action
        // ids produced by LegalActions() on this state.
        legal_index = 0;
    }

    engine_.mutableState().action_history.push_back(static_cast<int64_t>(action));
    engine_.applyChoice(legal_index);
}

} // namespace riftbound::openspiel
