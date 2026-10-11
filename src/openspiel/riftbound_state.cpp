#include "riftbound_state.h"
#include "riftbound_game.h"

#include "action_vocab.h"

#include "ml/cfr_util.h"
#include "ml/feature_extractor.h"

static_assert(::riftbound::openspiel::kNumCardDefIds == ::riftbound::ml::kCardVocabSize,
              "action vocab and state-feature card id spaces must match");

#include "open_spiel/spiel_utils.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>

namespace riftbound::openspiel {

namespace {

::open_spiel::Player playerIdToOpenSpiel(::riftbound::PlayerId pid) {
    if (pid == ::riftbound::PlayerId::Player1) return 0;
    if (pid == ::riftbound::PlayerId::Player2) return 1;
    return ::open_spiel::kTerminalPlayerId;
}

/// The engine's legal Intents as OpenSpiel action ids: vocab slots,
/// deduplicated (several Intents can share a slot) and in ascending order,
/// which is what OpenSpiel's LegalActions() contract asks for.
std::vector<::open_spiel::Action> encodeLegal(
    const std::vector<::riftbound::Intent>& legal,
    const ::riftbound::GameState& state) {
    std::vector<::open_spiel::Action> out;
    out.reserve(legal.size());
    for (const auto& intent : legal) {
        out.push_back(static_cast<::open_spiel::Action>(
            encodeAction(intent, state)));
    }
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

// While replaying, snapshot a resumable decision only when it is among the
// last few: the nearest one is all a clone needs.
constexpr size_t kBaseRefreshWindow = 8;

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
      event_bus_(std::make_unique<::riftbound::EventBus>()),
      engine_(std::make_unique<::riftbound::GameEngine>(
          rb_game_.cardDb(), *event_bus_, rb_game_.cardRegistry())),
      engine_seed_(engine_seed) {
    // Runs the engine (in its fiber) until the first decision or
    // termination; consult the result via engine_->currentStep().
    engine_->beginGame(rb_game_.deck1(), rb_game_.deck2(), engine_seed_);
}

RiftboundState::RiftboundState(std::shared_ptr<const ::open_spiel::Game> game,
                                uint64_t engine_seed,
                                std::shared_ptr<const Snapshot> snapshot,
                                std::shared_ptr<const Snapshot> base,
                                SnapshotTag)
    : ::open_spiel::State(game),
      rb_game_(*static_cast<const RiftboundGame*>(game.get())),
      lazy_(std::move(snapshot)),
      base_(std::move(base)),
      engine_seed_(engine_seed) {
    // No event bus, no engine. The snapshot holds the answers; ensureLive()
    // builds the engine on the first mutating call.
}

void RiftboundState::replayTokens(::riftbound::GameEngine& engine,
                                   const std::vector<int64_t>& tokens,
                                   size_t from, bool refresh_base) const {
    for (size_t i = from; i < tokens.size(); ++i) {
        if (engine.isStepDone()) {
            throw std::runtime_error(
                "RiftboundState: replay ended the game " +
                std::to_string(tokens.size() - i) + " decisions early.");
        }
        const auto& step = engine.currentStep();
        if (refresh_base && tokens.size() - i <= kBaseRefreshWindow &&
            ::riftbound::GameEngine::isResumableDecision(engine.state(),
                                                         step.legal)) {
            auto snap = std::make_shared<Snapshot>();
            snap->state     = engine.state();
            snap->step      = step;
            snap->rng       = engine.rngState();
            snap->resumable = true;
            base_ = std::move(snap);
        }
        const int legal_index =
            findHistoryToken(tokens[i], step.legal, engine.state());
        if (legal_index < 0) {
            throw std::runtime_error(
                "RiftboundState: replay diverged at decision " +
                std::to_string(i) + " (the recorded choice is not legal).");
        }
        engine.applyChoice(legal_index);
    }
}

void RiftboundState::ensureLive() const {
    if (!lazy_) return;
    const Snapshot& snap = *lazy_;

    // Build into locals and commit at the end, so a failed rebuild leaves
    // this state as the snapshot it was. Order matters: the bus must
    // outlive the engine.
    auto bus = std::make_unique<::riftbound::EventBus>();
    auto engine = std::make_unique<::riftbound::GameEngine>(
        rb_game_.cardDb(), *bus, rb_game_.cardRegistry());

    if (snap.resumable) {
        // Fast path: the snapshot is a top-level main-phase decision, and
        // everything the engine needs to continue from it is in GameState.
        engine->resumeFromSnapshot(::riftbound::GameState(snap.state),
                                   snap.rng, snap.step);
        base_ = lazy_;
    } else {
        // The snapshot was taken inside a call GameState does not capture
        // (mulligan, chain, showdown, combat, cost payment, a play-time
        // prompt) or after the game ended. Start from the nearest position
        // an engine CAN be started from and replay the exact choices made
        // since, then check we arrived where the snapshot is.
        const std::shared_ptr<const Snapshot> base = base_;
        size_t from = 0;
        if (base) {
            engine->resumeFromSnapshot(::riftbound::GameState(base->state),
                                       base->rng, base->step);
            from = base->state.action_history.size();
        } else {
            engine->beginGame(rb_game_.deck1(), rb_game_.deck2(), engine_seed_);
        }
        const auto& tokens = snap.state.action_history;
        if (from > tokens.size()) {
            throw std::runtime_error(
                "RiftboundState: snapshot history is shorter than its base.");
        }
        replayTokens(*engine, tokens, from, /*refresh_base=*/false);

        const char* mismatch = nullptr;
        if (engine->state().action_history != tokens) {
            mismatch = "the replayed history differs";
        } else if (snap.step.kind == ::riftbound::StepKind::Done) {
            if (!engine->isStepDone()) mismatch = "the replayed game is not over";
        } else if (engine->isStepDone()) {
            mismatch = "the replayed game is over";
        } else {
            const auto& step = engine->currentStep();
            if (step.perspective != snap.step.perspective) {
                mismatch = "a different player is deciding";
            } else if (step.legal != snap.step.legal) {
                mismatch = "the legal actions differ";
            }
        }
        if (mismatch) {
            throw std::runtime_error(
                std::string("RiftboundState: the replayed game did not arrive "
                            "at the snapshotted decision (") + mismatch +
                "; replayed " + std::to_string(tokens.size() - from) +
                " of " + std::to_string(tokens.size()) + " decisions from " +
                (base ? "a resumed snapshot" : "the start of the game") +
                "). Wrong seed, or a state that was not produced by its own "
                "action history.");
        }
    }

    event_bus_ = std::move(bus);
    engine_ = std::move(engine);
    lazy_.reset();
}

::open_spiel::Player RiftboundState::CurrentPlayer() const {
    if (IsTerminal()) return ::open_spiel::kTerminalPlayerId;
    if (lazy_) return playerIdToOpenSpiel(lazy_->step.perspective);
    return playerIdToOpenSpiel(engine_->currentStep().perspective);
}

std::vector<::open_spiel::Action> RiftboundState::LegalActions() const {
    if (IsTerminal()) return {};
    if (lazy_) return encodeLegal(lazy_->step.legal, lazy_->state);
    return encodeLegal(engine_->currentStep().legal, engine_->state());
}

std::string RiftboundState::ActionToString(::open_spiel::Player,
                                            ::open_spiel::Action a) const {
    if (lazy_) {
        if (lazy_->step.kind == ::riftbound::StepKind::Done) return "<terminal>";
        if (const auto* intent = decodeAction(static_cast<int>(a),
                                              lazy_->step.legal, lazy_->state)) {
            return intent->describe();
        }
        return "<unknown action_id>";
    }
    if (engine_->isStepDone()) return "<terminal>";
    const auto& step = engine_->currentStep();
    if (const auto* intent = decodeAction(static_cast<int>(a), step.legal,
                                          engine_->state())) {
        return intent->describe();
    }
    return "<unknown action_id>";
}

std::string RiftboundState::ToString() const {
    if (lazy_) {
        if (lazy_->step.kind == ::riftbound::StepKind::Done) {
            return std::string("<terminal: ")
                + lazy_->result.termination_reason + ">";
        }
        return std::string("Riftbound state @ turn ")
            + std::to_string(lazy_->state.turn.turn_number);
    }
    if (engine_->isStepDone()) {
        return std::string("<terminal: ")
            + engine_->stepResult().termination_reason + ">";
    }
    return std::string("Riftbound state @ turn ")
        + std::to_string(engine_->state().turn.turn_number);
}

bool RiftboundState::IsTerminal() const {
    // Hard cap on action count. MCTS's RandomRolloutEvaluator runs
    // rollouts to terminal with no internal length bound — if the
    // engine reaches a state that can't progress under random play
    // (mutual stalemate, forced multi-step sequences with k=1),
    // rollouts otherwise run unbounded inside MCTSBot::MCTSearch.
    // Returns() returns {0,0} (draw) when forcibly terminated — the
    // right MCTS signal for "unknown outcome / out of budget."
    // At the cap the state is terminal in every respect: CurrentPlayer()
    // is kTerminalPlayerId and LegalActions() is empty.
    if (MoveNumber() >= kMaxGameLength) return true;
    if (lazy_) return lazy_->step.kind == ::riftbound::StepKind::Done;
    // A game the engine failed in has no outcome. Never report it as a
    // finished game (it would score as a draw): rethrow the failure.
    engine_->rethrowIfStepFailed();
    return engine_->isStepDone();
}

std::vector<double> RiftboundState::Returns() const {
    const ::riftbound::GameResult* result = nullptr;
    if (lazy_) {
        if (lazy_->step.kind != ::riftbound::StepKind::Done) return {0.0, 0.0};
        result = &lazy_->result;
    } else {
        engine_->rethrowIfStepFailed();
        if (!engine_->isStepDone()) return {0.0, 0.0};
        result = &engine_->stepResult();
    }
    if (result->winner == ::riftbound::PlayerId::Player1) return {1.0, -1.0};
    if (result->winner == ::riftbound::PlayerId::Player2) return {-1.0, 1.0};
    return {0.0, 0.0};
}

uint64_t RiftboundState::InfoSetId(::open_spiel::Player player) const {
    // Thin wrapper: extract masked features + delegate hashing to the
    // pure ml::computeInfoSetId so the hash function itself is unit-
    // testable without a full game.
    ::riftbound::PlayerId rb_player = (player == 0)
        ? ::riftbound::PlayerId::Player1
        : ::riftbound::PlayerId::Player2;
    const ::riftbound::GameState& gs = engineState();
    auto feats = ::riftbound::ml::extractStateFeatures(
        gs, rb_player, rb_game_.cardDb());
    // The features describe the board, not the question being asked. Two
    // decisions in a row can leave the board untouched (a card's mode prompt
    // followed by its target prompt, two "you may" prompts) yet offer
    // different actions, and a search tree or regret table keyed on the
    // board alone would file both under one node. So the key also carries
    // who is deciding and, for the decider, the actions on offer — which
    // the decider sees, and which makes "same key => same action set" hold
    // by construction. Other players get only the public "who decides".
    const ::open_spiel::Player decider = CurrentPlayer();
    std::vector<int64_t> offered;
    if (player == decider) {
        for (auto a : LegalActions()) offered.push_back(static_cast<int64_t>(a));
    }
    return ::riftbound::ml::mixDecisionIntoInfoSetId(
        ::riftbound::ml::computeInfoSetId(feats, player),
        static_cast<int>(decider), offered);
}

std::string RiftboundState::ObservationString(::open_spiel::Player player) const {
    // Compact hex of the InfoSetId — ISMCTSBot keys node lookups on
    // this string. 16 hex chars = 64-bit hash, tight map keys.
    uint64_t h = InfoSetId(player);
    char buf[17];
    std::snprintf(buf, sizeof(buf), "%016llx", static_cast<unsigned long long>(h));
    return std::string(buf);
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
        engineState(), rb_player, rb_game_.cardDb());
    // The tensor shape the Game advertises is kStateFeatureDim; a mismatch
    // on either side is a layout bug, not something to pad over.
    SPIEL_CHECK_EQ(feats.size(),
                   static_cast<size_t>(::riftbound::ml::kStateFeatureDim));
    SPIEL_CHECK_EQ(values.size(), feats.size());
    std::copy(feats.begin(), feats.end(), values.begin());
}

std::vector<int64_t> RiftboundState::actionHistory() const {
    const auto& tokens = engineState().action_history;
    std::vector<int64_t> slots;
    slots.reserve(tokens.size());
    for (int64_t token : tokens) slots.push_back(historyTokenSlot(token));
    return slots;
}

std::shared_ptr<const RiftboundState::Snapshot>
RiftboundState::snapshotLive() const {
    if (live_snapshot_) return live_snapshot_;
    auto snap = std::make_shared<Snapshot>();
    snap->state  = engine_->state();
    snap->result = engine_->stepResult();
    snap->rng    = engine_->rngState();
    if (!engine_->isStepDone()) {
        snap->step = engine_->currentStep();
        snap->resumable = ::riftbound::GameEngine::isResumableDecision(
            snap->state, snap->step.legal);
    }
    live_snapshot_ = snap;
    // This position can now be restarted from directly; anything that
    // descends from it replays from here instead of from further back.
    if (snap->resumable) base_ = live_snapshot_;
    return live_snapshot_;
}

std::unique_ptr<::open_spiel::State> RiftboundState::Clone() const {
    // The clone shares an immutable snapshot of this position: no engine is
    // built until the clone is advanced. Clone-of-clone, and repeated clones
    // of an un-advanced state, share the same snapshot — a pointer copy.
    std::unique_ptr<RiftboundState> clone;
    if (lazy_) {
        clone.reset(new RiftboundState(game_, engine_seed_, lazy_, base_,
                                       SnapshotTag{}));
    } else {
        engine_->rethrowIfStepFailed();
        auto snap = snapshotLive();
        clone.reset(new RiftboundState(game_, engine_seed_, snap, base_,
                                       SnapshotTag{}));
    }
    // OpenSpiel's own bookkeeping is part of the state too: IsTerminal()'s
    // length cap reads MoveNumber(), and History() must match the original.
    clone->history_     = history_;
    clone->move_number_ = move_number_;
    return clone;
}

void RiftboundState::applyLegalIndex(int legal_index) {
    live_snapshot_.reset();
    engine_->applyChoice(legal_index);
}

void RiftboundState::DoApplyAction(::open_spiel::Action action) {
    ensureLive();
    engine_->rethrowIfStepFailed();
    if (engine_->isStepDone()) {
        throw std::logic_error(
            "RiftboundState: ApplyAction on a finished game.");
    }

    // Translate the vocab slot into the engine-side legal-list index: the
    // first legal Intent that encodes to it (slots can alias several
    // Intents; see action_vocab.h).
    const auto& step = engine_->currentStep();
    const auto& state = engine_->state();
    int legal_index = -1;
    for (size_t i = 0; i < step.legal.size(); ++i) {
        if (encodeAction(step.legal[i], state) == action) {
            legal_index = static_cast<int>(i);
            break;
        }
    }
    if (legal_index < 0) {
        // Not an id LegalActions() offered here. Applying some other action
        // in its place would silently play a different game.
        throw std::invalid_argument(
            "RiftboundState: action " + std::to_string(action) +
            " is not legal in this state.");
    }
    applyLegalIndex(legal_index);
}

bool RiftboundState::syncToHistory(const ::riftbound::GameState& live_state,
                                    const ::riftbound::StepResult& live_step) {
    ensureLive();
    const auto& tokens = live_state.action_history;
    const auto& mine = engine_->state().action_history;
    if (mine.size() > tokens.size() ||
        !std::equal(mine.begin(), mine.end(), tokens.begin())) {
        return false;
    }
    live_snapshot_.reset();
    try {
        replayTokens(*engine_, tokens, mine.size(), /*refresh_base=*/true);
    } catch (const std::runtime_error&) {
        // Divergence. (An engine failure is rethrown by the calls below.)
        engine_->rethrowIfStepFailed();
        return false;
    }
    engine_->rethrowIfStepFailed();
    if (engine_->isStepDone()) return false;

    // The history alone can match a different game (another seed deals other
    // cards but offers the same first choices). Compare what the choices
    // were made on: the pending decision and every hidden-order zone.
    const auto& step = engine_->currentStep();
    const auto& replayed = engine_->state();
    if ((live_step.perspective != ::riftbound::PlayerId::None &&
         step.perspective != live_step.perspective) ||
        step.legal != live_step.legal ||
        replayed.action_history != tokens ||
        replayed.turn.turn_number != live_state.turn.turn_number ||
        replayed.turn.phase != live_state.turn.phase) {
        return false;
    }
    for (int p = 0; p < 2; ++p) {
        const auto& a = replayed.players[p];
        const auto& b = live_state.players[p];
        if (a.score != b.score || a.hand != b.hand ||
            a.main_deck != b.main_deck || a.rune_deck != b.rune_deck ||
            a.trash != b.trash || a.banishment != b.banishment) {
            return false;
        }
    }
    return true;
}

std::unique_ptr<RiftboundState> RiftboundState::rebuildFromHistory(
    std::shared_ptr<const ::open_spiel::Game> game,
    uint64_t engine_seed,
    const ::riftbound::GameState& live_state,
    const ::riftbound::StepResult& live_step) {
    auto state = std::make_unique<RiftboundState>(std::move(game), engine_seed);
    if (!state->syncToHistory(live_state, live_step)) return nullptr;
    return state;
}

std::unique_ptr<RiftboundState> RiftboundState::makeFromSnapshot(
    std::shared_ptr<const ::open_spiel::Game> game,
    uint64_t engine_seed,
    const ::riftbound::GameState& snap_state,
    ::riftbound::StepResult        snap_step,
    ::riftbound::GameResult        snap_result,
    uint64_t                       rng_seed) {
    // The single deep copy lives here — snap_state crosses ownership
    // boundaries into the shared snapshot.
    auto snap = std::make_shared<Snapshot>();
    snap->state  = snap_state;
    snap->step   = std::move(snap_step);
    snap->result = std::move(snap_result);
    snap->rng.seed(rng_seed);
    snap->resumable =
        snap->step.kind == ::riftbound::StepKind::NeedDecision &&
        ::riftbound::GameEngine::isResumableDecision(snap->state,
                                                     snap->step.legal);
    std::shared_ptr<const Snapshot> base = snap->resumable ? snap : nullptr;
    return std::unique_ptr<RiftboundState>(new RiftboundState(
        std::move(game), engine_seed, std::move(snap), std::move(base),
        SnapshotTag{}));
}

} // namespace riftbound::openspiel
