#include "game_engine.h"
#include "cards/card.h"
#include "openspiel/action_vocab.h"

#include <algorithm>
#include <cassert>
#include <numeric>
#include <set>
#include <sstream>
#include <stdexcept>

namespace riftbound {

GameEngine::GameEngine(const CardDB& card_db, EventBus& event_bus,
                       const CardRegistry& card_registry)
    : card_db_(card_db), events_(event_bus), card_registry_(card_registry) {
    // Chain subsystems are initialized per-game in runGame

    // Observation tracking. Each CardRevealedEvent updates the
    // observer's PlayerState::observed_cards bank by card_def_id. This
    // lets cards / tests / future ML features verify which identities
    // a player has seen. See CardRevealedEvent in core/events.h for
    // the emit contract; the bump logic lives on GameState::recordReveal
    // so unit tests (which don't construct a GameEngine) can subscribe
    // their own bus to the same helper.
    card_revealed_conn_ = events_.on_card_revealed.connect(
        [this](const CardRevealedEvent& e) { state_.recordReveal(e); });
}

GameEngine::~GameEngine() {
    // Disconnect the on_card_revealed subscriber before this is freed.
    // Without this, a successor engine on the same EventBus would
    // emit reveals into a dangling `this`.
    if (card_revealed_conn_.connected()) card_revealed_conn_.disconnect();
    // Fiber refactor: StepDriver's destructor drains the fiber by
    // feeding choice=0 until done. No separate thread to join — the
    // fiber lives in the same call stack as this destructor.
    // step_driver_'s unique_ptr destruction handles it.
}

// ═══════════════════════════════════════════════════════════════════════════════
// Game lifecycle
// ═══════════════════════════════════════════════════════════════════════════════

GameResult GameEngine::runGame(
    const DeckSubmission& deck1,
    const DeckSubmission& deck2,
    AgentInterface& agent1,
    AgentInterface& agent2,
    uint64_t seed) {

    rng_.seed(seed == 0 ? std::random_device{}() : seed);
    agents_[0] = &agent1;
    agents_[1] = &agent2;

    state_ = GameState{};
    state_.mode = ModeOfPlay{};

    // Initialize Phase 2 subsystems
    chain_manager_ = std::make_unique<ChainManager>(state_, events_, card_db_);
    chain_manager_->setAffordCheck(
        [this](PlayerId p, GameObjectId card) { return canAfford(p, card); });
    chain_manager_->setPayCost(
        [this](PlayerId p, GameObjectId card) { return payCardCost(p, card); });
    effect_executor_ = std::make_unique<EffectExecutor>(state_, events_, card_db_, &card_registry_);
    effect_executor_->setRng(&rng_);
    effect_executor_->setAgentQuery(
        [this](PlayerId p, const std::vector<Intent>& actions) -> Intent {
            return queryAgentForChain(p, actions);
        });
    chain_manager_->setEffectExecutor(effect_executor_.get());
    trigger_manager_ = std::make_unique<TriggerManager>(
        state_, events_, card_db_, *chain_manager_, card_registry_);
    trigger_manager_->setEffectExecutor(effect_executor_.get());
    trigger_manager_->subscribe();

    setupGame(deck1, deck2);
    drawOpeningHands();
    runMulligans();

    events_.emit(PhaseChangedEvent{TurnPhase::Setup, TurnPhase::AwakenPhase,
                                    state_.turn.turn_player});

    runTurnLoop();

    GameResult result;
    result.winner = state_.winner;
    result.final_scores[0] = state_.players[0].score;
    result.final_scores[1] = state_.players[1].score;
    result.total_turns = state_.turn.turn_number;
    result.total_decisions = state_.decision_index;
    result.termination_reason = state_.game_over_reason;
    return result;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Step machine API (Phase 11 C-1)
//
// Implementation strategy: this version of the step machine still runs the
// engine on an internal worker thread via StepDriver (Phase C-1 commits
// 1–2 + this wrapper relocation). The threading is now an engine-internal
// implementation detail, not the caller's concern — which lets the
// OpenSpiel wrapper drop its own thread and halting agent.
//
// Subsequent C-1 commits (3–6) replace the threading with a native
// resumable step machine, at which point Clone() can collapse to
// memcpy(GameState). The beginGame/applyChoice/currentStep surface stays
// stable across that transition — callers don't change.
// ═══════════════════════════════════════════════════════════════════════════════

void GameEngine::refreshStepFromDriver() {
    if (!step_driver_) {
        current_step_.kind = StepKind::Done;
        current_step_.legal.clear();
        current_step_.perspective = PlayerId::None;
        return;
    }
    if (step_driver_->isDone()) {
        current_step_.kind = StepKind::Done;
        current_step_.legal.clear();
        current_step_.perspective = PlayerId::None;
        return;
    }
    current_step_.kind = StepKind::NeedDecision;
    current_step_.legal = step_driver_->legalActionsSnapshot();
    current_step_.perspective = step_driver_->currentPlayerSnapshot();
}

StepResult GameEngine::beginGame(
    const DeckSubmission& deck1,
    const DeckSubmission& deck2,
    uint64_t seed) {
    // Disallow restart on the same engine — too easy to get wrong with
    // a previous session's thread still alive. Make a fresh GameEngine
    // instead.
    if (step_driver_) {
        throw std::logic_error(
            "GameEngine::beginGame called on an engine that already has a "
            "step-machine session. Construct a fresh GameEngine.");
    }
    step_driver_ = std::make_unique<StepDriver>();
    step_result_ = GameResult{};

    // Snapshot decks so the fiber's lambda can capture them by value
    // safely (the fiber may outlive this stack frame's locals).
    auto d1 = deck1;
    auto d2 = deck2;
    StepDriver* driver = step_driver_.get();
    driver->runInFiber([this, d1, d2, seed, driver]() {
        step_result_ = runGame(d1, d2, *driver, *driver, seed);
        driver->markDone();
    });

    // No-op for fiber backend (selectAction's yield already returned
    // us here). Kept for API symmetry with the old thread backend.
    step_driver_->waitForDecision();
    refreshStepFromDriver();
    return current_step_;
}

StepResult GameEngine::resumeFromSnapshot(GameState snapshot_state,
                                            uint64_t engine_seed) {
    if (step_driver_) {
        throw std::logic_error(
            "GameEngine::resumeFromSnapshot called on an engine that already "
            "has a step-machine session. Construct a fresh GameEngine.");
    }

    // Substitute the snapshotted state.
    state_ = std::move(snapshot_state);
    rng_.seed(engine_seed == 0 ? std::random_device{}() : engine_seed);

    // Initialise subsystems (parallel to runGame's first block). These
    // hold references into `state_` and `events_`, which are now
    // populated from the snapshot.
    chain_manager_ = std::make_unique<ChainManager>(state_, events_, card_db_);
    chain_manager_->setAffordCheck(
        [this](PlayerId p, GameObjectId card) { return canAfford(p, card); });
    chain_manager_->setPayCost(
        [this](PlayerId p, GameObjectId card) { return payCardCost(p, card); });
    effect_executor_ = std::make_unique<EffectExecutor>(state_, events_, card_db_, &card_registry_);
    effect_executor_->setRng(&rng_);
    effect_executor_->setAgentQuery(
        [this](PlayerId p, const std::vector<Intent>& actions) -> Intent {
            return queryAgentForChain(p, actions);
        });
    chain_manager_->setEffectExecutor(effect_executor_.get());
    trigger_manager_ = std::make_unique<TriggerManager>(
        state_, events_, card_db_, *chain_manager_, card_registry_);
    trigger_manager_->setEffectExecutor(effect_executor_.get());
    trigger_manager_->subscribe();

    step_driver_ = std::make_unique<StepDriver>();
    step_result_ = GameResult{};
    StepDriver* driver = step_driver_.get();
    agents_[0] = driver;
    agents_[1] = driver;

    // Capture the resume point — the phase the snapshot was at. The
    // dispatcher below uses this to skip already-completed phases.
    TurnPhase resume_at = state_.turn.phase;
    PlayerId  turn_player = state_.turn.turn_player;

    driver->runInFiber([this, resume_at, turn_player, driver]() {
        // Finish the current turn from where it paused, then continue
        // the normal turn loop. Mirrors the body of runTurnLoop minus
        // the first-turn entry.
        try {
            if (!state_.game_over) {
                runTurnFromPhase(turn_player, resume_at);
            }
            if (!state_.game_over) {
                // Apply the same turn-transition logic runTurnLoop uses.
                auto& p1 = state_.players[0];
                auto& p2 = state_.players[1];
                if (!p1.additional_turns.empty()) {
                    auto next = p1.additional_turns.front();
                    p1.additional_turns.erase(p1.additional_turns.begin());
                    state_.turn.turn_player = next;
                    state_.turn.is_additional_turn = true;
                    events_.logTrace(std::string("ADDITIONAL_TURN: ") + toString(next));
                } else if (!p2.additional_turns.empty()) {
                    auto next = p2.additional_turns.front();
                    p2.additional_turns.erase(p2.additional_turns.begin());
                    state_.turn.turn_player = next;
                    state_.turn.is_additional_turn = true;
                    events_.logTrace(std::string("ADDITIONAL_TURN: ") + toString(next));
                } else {
                    state_.turn.turn_player = opponent(state_.turn.turn_player);
                    state_.turn.is_additional_turn = false;
                }
                state_.turn.turn_number++;
                runTurnLoop();
            }

            // Finalise (mirrors runGame's tail).
            step_result_.winner          = state_.winner;
            step_result_.final_scores[0] = state_.players[0].score;
            step_result_.final_scores[1] = state_.players[1].score;
            step_result_.total_turns     = state_.turn.turn_number;
            step_result_.total_decisions = state_.decision_index;
            step_result_.termination_reason = state_.game_over_reason;
        } catch (...) {
            // Defensive — must always reach markDone, otherwise the
            // caller hangs (fiber backend: it doesn't hang per se,
            // but isDone() never becomes true and the fiber leaks).
        }
        driver->markDone();
    });

    step_driver_->waitForDecision();
    refreshStepFromDriver();
    return current_step_;
}

StepResult GameEngine::currentStep() const {
    return current_step_;
}

StepResult GameEngine::applyChoice(int legal_index) {
    if (!step_driver_) {
        throw std::logic_error(
            "GameEngine::applyChoice called before beginGame.");
    }
    if (current_step_.kind == StepKind::Done) {
        return current_step_;
    }
    step_driver_->provideChoice(legal_index);
    step_driver_->waitForDecision();
    refreshStepFromDriver();
    return current_step_;
}

bool GameEngine::isStepDone() const {
    if (!step_driver_) return false;
    return step_driver_->isDone();
}

// ═══════════════════════════════════════════════════════════════════════════════
// Setup
// ═══════════════════════════════════════════════════════════════════════════════

void GameEngine::setupGame(const DeckSubmission& deck1,
                            const DeckSubmission& deck2) {
    state_.turn.phase = TurnPhase::Setup;

    state_.players[0].id = PlayerId::Player1;
    state_.players[1].id = PlayerId::Player2;

    setupPlayer(PlayerId::Player1, deck1);
    setupPlayer(PlayerId::Player2, deck2);

    determineTurnOrder();
    setupBattlefields(deck1, deck2);
}

void GameEngine::setupPlayer(PlayerId player, const DeckSubmission& deck) {
    auto& ps = state_.player(player);

    // Legend → Legend Zone
    auto legend_id = instantiateCard(deck.legend, player);
    auto& legend_obj = state_.getObject(legend_id);
    legend_obj.zone = ZoneType::LegendZone;
    ps.legend_zone = legend_id;

    // Chosen Champion → Champion Zone (starts here, can be played from here)
    auto champ_id = instantiateCard(deck.chosen_champion, player);
    auto& champ_obj = state_.getObject(champ_id);
    champ_obj.zone = ZoneType::ChampionZone;
    // Champion zone has no board location — it's a non-board zone
    ps.champion_zone = champ_id;

    // Main Deck → shuffled
    for (auto card_def_id : deck.main_deck) {
        auto obj_id = instantiateCard(card_def_id, player);
        state_.getObject(obj_id).zone = ZoneType::MainDeck;
        ps.main_deck.push_back(obj_id);
    }
    shuffleDeck(player);

    // Rune Deck → shuffled
    for (auto rune_def_id : deck.rune_deck) {
        auto obj_id = instantiateCard(rune_def_id, player);
        state_.getObject(obj_id).zone = ZoneType::RuneDeck;
        ps.rune_deck.push_back(obj_id);
    }
    shuffleRuneDeck(player);

    // Store battlefield card defs (selection happens in setupBattlefields)
    // For now, store all 3 as potential choices
}

void GameEngine::setupBattlefields(const DeckSubmission& deck1,
                                     const DeckSubmission& deck2) {
    // Each player picks one of their three battlefields (CR 480.5 says
    // random for single-game, CR 481.5 says agent choice for match
    // play). We unconditionally route through the agent — RandomAgent
    // picks uniformly which satisfies CR 480.5; HumanAgent / MCTS get
    // to actually choose, which CR 481.5 requires for match play.
    //
    // The Intent::chosen_battlefield field carries the INDEX into the
    // deck's battlefield list (0..N-1) here, NOT a runtime BattlefieldId
    // — no live BFs exist on the board yet at this point. Action-vocab
    // encoding via bfSlot handles 0..2 (well within its 0..7 range).
    auto pickFromDeck = [&](PlayerId player,
                             const std::vector<CardDefId>& bfs) -> CardDefId {
        if (bfs.empty()) return kInvalidId;
        // Publish the candidate pool to PlayerState so the legal-action
        // renderer can label each ChooseBattlefield by its printed name
        // ("Vilemaw's Lair", "Rockfall Path", …) instead of just "BF #N".
        state_.player(player).battlefield_pool = bfs;
        // Do NOT short-circuit on bfs.size() == 1. Per the project
        // contract, every engine-asked decision MUST be surfaced to
        // the agent — including forced single-option ones — so that
        // training data, action_history replay, and the policy head's
        // exposure to "constrained" positions stay consistent.
        // The decision still costs nothing for a 1-option pool
        // (HumanAgent / RandomAgent / MCTSAgent each immediately
        // return the only entry), but the position is recorded.

        std::vector<Intent> choices;
        choices.reserve(bfs.size());
        for (size_t i = 0; i < bfs.size(); ++i) {
            Intent c;
            c.type = IntentType::ChooseBattlefield;
            c.player = player;
            c.chosen_battlefield = static_cast<BattlefieldId>(i);  // deck index
            choices.push_back(c);
        }

        // Briefly mark this player as the decision-maker so HumanAgent's
        // UI snapshot (which reads state.turn.turn_player) shows the
        // correct seat. Restore turn_player after the query — the real
        // turn order isn't owned by this setup step.
        PlayerId saved_turn = state_.turn.turn_player;
        state_.turn.turn_player = player;
        state_.decision_index++;
        events_.logTrace("DECISION #" + std::to_string(state_.decision_index) +
                          " (" + toString(player) + " — battlefield setup): " +
                          std::to_string(choices.size()) + " options");
        auto chosen = getAgent(player).selectAction(state_, choices);
        recordAppliedIntent(chosen);
        state_.turn.turn_player = saved_turn;

        size_t idx = static_cast<size_t>(chosen.chosen_battlefield);
        if (idx >= bfs.size()) idx = 0;  // defensive — fall back to first
        events_.logTrace("  CHOSE BF #" + std::to_string(idx) + " (" +
                          card_db_.get(bfs[idx]).name + ")");
        return bfs[idx];
    };

    auto bf1_def = pickFromDeck(PlayerId::Player1, deck1.battlefields);
    auto bf2_def = pickFromDeck(PlayerId::Player2, deck2.battlefields);

    // Create battlefield game objects and state
    BattlefieldId next_bf_id = 0;

    auto addBF = [&](CardDefId def_id, PlayerId contributor) {
        auto obj_id = instantiateCard(def_id, contributor);
        auto& obj = state_.getObject(obj_id);
        obj.zone = ZoneType::BattlefieldZone;

        BattlefieldState bf;
        bf.id = next_bf_id++;
        bf.card_object_id = obj_id;
        bf.contributed_by = contributor;

        // Parse static battlefield restrictions from ability text. These
        // flags are consulted by the action generators (move + play)
        // without needing per-BF Card dispatch on every legal-action
        // check.
        if (def_id != kInvalidId) {
            const auto& def = card_db_.get(def_id);
            std::string t = def.ability_text;
            std::transform(t.begin(), t.end(), t.begin(), ::tolower);
            if (t.find("can't move from here to base") != std::string::npos)
                bf.blocks_move_to_base = true;
            if (t.find("can't be played here") != std::string::npos)
                bf.blocks_unit_play = true;
        }
        // Turn-gated scoring is a structured property the BF card decides
        // (Forgotten Monument). Engine asks; card answers.
        if (auto* bfc = dynamic_cast<const BattlefieldCard*>(
                card_registry_.get(def_id))) {
            bf.min_turn_to_score = bfc->minTurnToScore();
        }
        state_.battlefields.push_back(bf);
    };

    addBF(bf1_def, PlayerId::Player1);
    addBF(bf2_def, PlayerId::Player2);
}

void GameEngine::determineTurnOrder() {
    // Random coin flip for first player (CR 116)
    std::uniform_int_distribution<int> coin(0, 1);
    state_.turn.turn_player = coin(rng_) == 0 ? PlayerId::Player1 : PlayerId::Player2;
    state_.turn.starting_player = state_.turn.turn_player;
    state_.turn.turn_number = 0;

    // Mark both players as being on their first turn
    state_.players[0].is_first_turn = true;
    state_.players[1].is_first_turn = true;
}

void GameEngine::drawOpeningHands() {
    // Each player draws 4 (CR 117)
    events_.logTrace("── Opening Hands ──");
    events_.logTrace(std::string("DRAW_HAND: ") + toString(PlayerId::Player1) + " draws 4");
    drawCards(PlayerId::Player1, 4);
    events_.logTrace(std::string("DRAW_HAND: ") + toString(PlayerId::Player2) + " draws 4");
    drawCards(PlayerId::Player2, 4);
}

// C-1 commit 8 — mulligan loop split into advance/resolve halves.
GameEngine::MulliganAdvance GameEngine::advanceMulligan(PlayerId player) {
    MulliganAdvance adv;
    adv.legal = generateMulliganActions(player);
    if (adv.legal.empty()) {
        adv.kind = MulliganAdvance::Kind::Done;
        return adv;
    }
    adv.kind = MulliganAdvance::Kind::NeedDecision;
    adv.deciding = player;
    return adv;
}

void GameEngine::resolveMulliganDecision(const Intent& chosen) {
    // Log what was chosen. Including the player tag inline so each
    // CHOSE: line is self-contained — important for the web UI's log
    // panel, which prepends events (latest first), so a CHOSE line
    // can be visually separated from its DECISION # context.
    std::string who = std::string("[") + toString(chosen.player) + "] ";
    if (chosen.cards_to_mulligan.empty()) {
        events_.logTrace("CHOSE: " + who + "Keep hand");
    } else {
        std::string mull_str;
        for (auto cid : chosen.cards_to_mulligan) {
            if (!mull_str.empty()) mull_str += ", ";
            if (state_.objectExists(cid)) mull_str += state_.getObject(cid).name;
        }
        events_.logTrace("CHOSE: " + who + "Mulligan " +
                         std::to_string(chosen.cards_to_mulligan.size()) +
                         " [" + mull_str + "]");
    }
    if (chosen.type == IntentType::MulliganDecision) {
        executeMulligan(chosen);
    }
}

void GameEngine::runMulligans() {
    // In turn order, each player may mulligan up to 2 cards (CR 118).
    // Bridge: legacy path calls queryAgent between advance + resolve.
    // The step-machine path (C-1 commit 9) will surface advance's legal
    // options out to applyChoice() and feed the chosen Intent back into
    // resolve.
    events_.logTrace("── Mulligan Phase ──");
    state_.turn.phase = TurnPhase::Mulligan;

    for (auto player : {state_.turn.turn_player,
                        opponent(state_.turn.turn_player)}) {
        auto adv = advanceMulligan(player);
        if (adv.kind == MulliganAdvance::Kind::Done) continue;

        state_.decision_index++;
        if (player == PlayerId::Player1) state_.turn.turn_decisions_p1++;
        else state_.turn.turn_decisions_p2++;

        events_.logTrace("DECISION #" + std::to_string(state_.decision_index) +
                         " (" + toString(player) + "): Mulligan, " +
                         std::to_string(adv.legal.size()) + " options");

        auto chosen = getAgent(player).selectAction(state_, adv.legal);
        recordAppliedIntent(chosen);

        if (on_decision) {
            on_decision(state_, adv.legal, chosen);
        }

        resolveMulliganDecision(chosen);
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Turn loop
// ═══════════════════════════════════════════════════════════════════════════════

void GameEngine::runTurnLoop() {
    // Safety limit to prevent infinite games
    constexpr int kMaxTurns = 200;

    while (!state_.game_over && state_.turn.turn_number < kMaxTurns) {
        runTurn(state_.turn.turn_player);

        if (state_.game_over) break;

        // Check for additional turns (CR 734-738)
        // Check both players' queues
        auto& p1 = state_.players[0];
        auto& p2 = state_.players[1];
        if (!p1.additional_turns.empty()) {
            auto next = p1.additional_turns.front();
            p1.additional_turns.erase(p1.additional_turns.begin());
            state_.turn.turn_player = next;
            state_.turn.is_additional_turn = true;
            events_.logTrace(std::string("ADDITIONAL_TURN: ") + toString(next));
        } else if (!p2.additional_turns.empty()) {
            auto next = p2.additional_turns.front();
            p2.additional_turns.erase(p2.additional_turns.begin());
            state_.turn.turn_player = next;
            state_.turn.is_additional_turn = true;
            events_.logTrace(std::string("ADDITIONAL_TURN: ") + toString(next));
        } else {
            // Normal turn alternation
            state_.turn.turn_player = opponent(state_.turn.turn_player);
            state_.turn.is_additional_turn = false;
        }
        state_.turn.turn_number++;
    }

    if (!state_.game_over) {
        // Max turns reached — draw
        state_.game_over = true;
        state_.winner = PlayerId::None;
        state_.game_over_reason = "Max turns reached";
        events_.emit(GameOverEvent{PlayerId::None, "Max turns reached"});
    }
}

void GameEngine::runTurn(PlayerId player) {
    events_.logTrace("════ TURN " + std::to_string(state_.turn.turn_number) +
                     " (" + toString(player) + ") ════ Score: P1=" +
                     std::to_string(state_.players[0].score) + " P2=" +
                     std::to_string(state_.players[1].score));
    state_.player(player).resetTurnTracking();
    state_.turn.turn_decisions_p1 = 0;
    state_.turn.turn_decisions_p2 = 0;
    state_.turn.any_unit_died_this_turn = false;

    awakenPhase();
    if (state_.game_over) return;

    beginningStep();
    if (state_.game_over) return;

    scoringStep();
    if (state_.game_over) return;

    channelPhase();
    if (state_.game_over) return;

    drawPhase();
    if (state_.game_over) return;

    mainPhase();
    if (state_.game_over) return;

    endingStep();
    if (state_.game_over) return;

    expirationStep();

    // Mark first turn as done
    state_.player(player).is_first_turn = false;
}

// Resume a turn that's already in progress. Used by resumeFromSnapshot
// after the clone substitutes the snapshotted GameState into a fresh
// engine. Skips phases that have already completed (per resume_at) and
// runs the rest in order. Per-turn tracking is NOT reset here (already
// captured in the snapshot).
//
// Fallthrough switch — each phase falls through to the next so we
// finish the turn from wherever it was paused.
void GameEngine::runTurnFromPhase(PlayerId player, TurnPhase resume_at) {
    events_.logTrace("════ RESUME TURN " + std::to_string(state_.turn.turn_number) +
                     " (" + toString(player) + ") at phase " +
                     toString(resume_at) +
                     " ════ Score: P1=" +
                     std::to_string(state_.players[0].score) + " P2=" +
                     std::to_string(state_.players[1].score));

    switch (resume_at) {
        case TurnPhase::AwakenPhase:
            awakenPhase();
            if (state_.game_over) return;
            [[fallthrough]];
        case TurnPhase::BeginningStep:
            // Only run beginningStep if we're resuming AT or BEFORE it.
            // (When resume_at is later, we drop through this case.)
            if (resume_at == TurnPhase::BeginningStep ||
                resume_at == TurnPhase::AwakenPhase) {
                beginningStep();
                if (state_.game_over) return;
            }
            [[fallthrough]];
        case TurnPhase::ScoringStep:
            if (resume_at == TurnPhase::ScoringStep ||
                resume_at == TurnPhase::BeginningStep ||
                resume_at == TurnPhase::AwakenPhase) {
                scoringStep();
                if (state_.game_over) return;
            }
            [[fallthrough]];
        case TurnPhase::ChannelPhase:
            if (resume_at == TurnPhase::ChannelPhase ||
                resume_at == TurnPhase::ScoringStep ||
                resume_at == TurnPhase::BeginningStep ||
                resume_at == TurnPhase::AwakenPhase) {
                channelPhase();
                if (state_.game_over) return;
            }
            [[fallthrough]];
        case TurnPhase::DrawPhase:
            if (resume_at == TurnPhase::DrawPhase ||
                resume_at == TurnPhase::ChannelPhase ||
                resume_at == TurnPhase::ScoringStep ||
                resume_at == TurnPhase::BeginningStep ||
                resume_at == TurnPhase::AwakenPhase) {
                drawPhase();
                if (state_.game_over) return;
            }
            [[fallthrough]];
        case TurnPhase::MainPhase:
            // When resuming AT MainPhase, skip the entry side effects
            // (mainPhase() resets ns_state/oc_state/priority_holder
            // and emits PhaseChangedEvent — both clobber the snapshot).
            // Just re-enter the action loop. Once the loop drains, fall
            // through to endingStep as normal.
            mainPhaseLoop();
            if (state_.game_over) return;
            [[fallthrough]];
        case TurnPhase::EndingStep:
            endingStep();
            if (state_.game_over) return;
            [[fallthrough]];
        case TurnPhase::ExpirationStep:
            expirationStep();
            break;
        default:
            // Setup / Mulligan / GameOver — caller shouldn't call us here.
            throw std::logic_error(
                "GameEngine::runTurnFromPhase called with phase that has no "
                "turn-internal resume point: " + std::string(toString(resume_at)));
    }
    state_.player(player).is_first_turn = false;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Phases
// ═══════════════════════════════════════════════════════════════════════════════

void GameEngine::awakenPhase() {
    events_.logTrace("── Awaken Phase ──");
    auto old_phase = state_.turn.phase;
    state_.turn.phase = TurnPhase::AwakenPhase;
    events_.emit(PhaseChangedEvent{old_phase, TurnPhase::AwakenPhase,
                                    state_.turn.turn_player});

    // Count this player's own turns (drives turn-gated rules, e.g. Forgotten
    // Monument). 1 on their first turn.
    state_.player(state_.turn.turn_player).turns_taken++;

    // Ready all game objects the turn player controls (CR 315.1.b)
    auto player = state_.turn.turn_player;
    for (auto& [id, obj] : state_.objects) {
        if (obj.controller == player && obj.is_exhausted && obj.location.has_value()) {
            obj.is_exhausted = false;
            events_.emit(ObjectStateChangedEvent{id, "readied"});
        }
    }
    // Also ready the legend
    auto legend_id = state_.player(player).legend_zone;
    if (legend_id != kInvalidId && state_.objectExists(legend_id)) {
        auto& legend = state_.getObject(legend_id);
        if (legend.is_exhausted) {
            legend.is_exhausted = false;
            events_.emit(ObjectStateChangedEvent{legend_id, "readied"});
        }
    }
}

void GameEngine::beginningStep() {
    events_.logTrace("── Beginning Step ──");
    auto old_phase = state_.turn.phase;
    state_.turn.phase = TurnPhase::BeginningStep;

    // Kill Temporary permanents before scoring (CR: "Kill me at the start of
    // my controller's Beginning Phase, before scoring"). Applies to both units
    // AND gear — Fading Memories (180) can grant [Temporary] to either.
    std::vector<GameObjectId> units_to_kill;
    std::vector<GameObjectId> gear_to_kill;
    for (auto& [id, obj] : state_.objects) {
        if (!obj.keywords.has(Keyword::Temporary)) continue;
        if (obj.controller != state_.turn.turn_player) continue;
        if (!obj.location.has_value()) continue;
        if (obj.isUnit()) units_to_kill.push_back(id);
        else if (obj.isGear()) gear_to_kill.push_back(id);
    }
    for (auto id : units_to_kill) {
        killUnit(id);
    }
    for (auto id : gear_to_kill) {
        effect_executor_->killObject(id);  // gear-aware kill (Banishment/trash)
    }

    events_.emit(PhaseChangedEvent{old_phase, TurnPhase::BeginningStep,
                                    state_.turn.turn_player});

    // Process "At the start of your Beginning Phase" triggers
    if (chain_manager_->chainExists()) runChain();
}

void GameEngine::scoringStep() {
    events_.logTrace("── Scoring Step ──");
    auto old_phase = state_.turn.phase;
    state_.turn.phase = TurnPhase::ScoringStep;
    events_.emit(PhaseChangedEvent{old_phase, TurnPhase::ScoringStep,
                                    state_.turn.turn_player});

    // Turn player Holds all battlefields they control (CR 315.2.b)
    auto player = state_.turn.turn_player;
    for (auto& bf : state_.battlefields) {
        if (bf.controller.has_value() && *bf.controller == player) {
            scoreHold(player, bf.id);
            if (state_.game_over) return;
            // Process hold/score triggers
            if (chain_manager_->chainExists()) runChain();
            if (state_.game_over) return;
        }
    }

    cleanup();
}

void GameEngine::channelPhase() {
    events_.logTrace("── Channel Phase ──");
    auto old_phase = state_.turn.phase;
    state_.turn.phase = TurnPhase::ChannelPhase;
    events_.emit(PhaseChangedEvent{old_phase, TurnPhase::ChannelPhase,
                                    state_.turn.turn_player});

    auto player = state_.turn.turn_player;
    int count = 2;

    // Second player channels an extra rune on first turn (CR 480.7)
    if (state_.player(player).is_first_turn &&
        player != state_.turn.turn_player) {
        // This is the player going second — but wait, on their first turn
        // they ARE the turn player. The rule is about being the second player.
    }
    // Simplified: if this is the second player's first turn, channel 3
    auto first_player = state_.turn.turn_number == 0 ? state_.turn.turn_player
                        : opponent(state_.turn.turn_player);
    // The player who did NOT go first gets the extra channel on their first turn
    if (state_.player(player).is_first_turn && player != first_player) {
        count = 3;
    }

    channelRunes(player, count);
}

void GameEngine::drawPhase() {
    events_.logTrace("── Draw Phase ──");
    auto old_phase = state_.turn.phase;
    state_.turn.phase = TurnPhase::DrawPhase;
    events_.emit(PhaseChangedEvent{old_phase, TurnPhase::DrawPhase,
                                    state_.turn.turn_player});

    auto player = state_.turn.turn_player;

    // CR 482.7 / 483.7 / 484.7: the player going first does not draw a
    // card during their first Draw Phase of the game. (The player going
    // second receives a compensating extra channel in channelPhase().)
    if (state_.player(player).is_first_turn &&
        player == state_.turn.starting_player) {
        events_.logTrace("DRAW PHASE: " + std::string(player == PlayerId::Player1 ? "P1" : "P2") +
                          " skips draw (first turn, going first — CR 482.7)");
    } else {
        // Draw 1 card (CR 315.4.b)
        drawCards(player, 1);
    }

    // Empty rune pools (CR 315.4.d)
    emptyRunePools();
}

// C-1 commit 5 — refactor mainPhase into a step-machine-shaped split.
// Behavior is identical to the prior monolithic loop; the two halves
// (advance / resolve) are the surface that the future step machine
// (C-1 commit 9) will call directly without going through queryAgent.

namespace {
constexpr int kMainPhaseMaxActions = 500;  // safety cap
}  // namespace

GameEngine::MainPhaseAdvance GameEngine::advanceMainPhase(int& action_count) {
    MainPhaseAdvance adv;
    if (state_.game_over || action_count >= kMainPhaseMaxActions) {
        adv.kind = MainPhaseAdvance::Kind::Done;
        return adv;
    }

    // Check for staged showdowns/combats after cleanup
    processContestedBattlefields();
    if (state_.game_over) {
        adv.kind = MainPhaseAdvance::Kind::Done;
        return adv;
    }

    // If in showdown, run it. Sub-calls still use the recursive path
    // until C-1 commit 6 converts the chain / combat subsystems.
    for (auto& bf : state_.battlefields) {
        if (bf.showdown_staged && !bf.showdown_in_progress &&
            !bf.combat_staged) {
            runShowdown(bf.id);
            if (state_.game_over) {
                adv.kind = MainPhaseAdvance::Kind::Done;
                return adv;
            }
            bf.showdown_staged = false;
        }
        if (bf.combat_staged && !bf.combat_in_progress) {
            runCombat(bf.id);
            if (state_.game_over) {
                adv.kind = MainPhaseAdvance::Kind::Done;
                return adv;
            }
            bf.combat_staged = false;
        }
    }

    auto actions = generateLegalActions();
    if (actions.empty()) {
        adv.kind = MainPhaseAdvance::Kind::Done;
        return adv;
    }

    adv.kind = MainPhaseAdvance::Kind::NeedDecision;
    adv.legal = std::move(actions);
    return adv;
}

bool GameEngine::resolveMainPhaseDecision(const Intent& intent,
                                          int& action_count) {
    if (intent.type == IntentType::EndTurn) return false;

    executeIntent(intent);

    // Process any triggered abilities that were queued during execution
    // (play triggers, spell resolution triggers, etc.)
    if (chain_manager_->chainExists()) runChain();

    cleanup();

    // Process death triggers / cleanup triggers
    if (chain_manager_->chainExists()) runChain();

    action_count++;
    return !state_.game_over;
}

void GameEngine::mainPhase() {
    events_.logTrace("── Main Phase ──");
    auto old_phase = state_.turn.phase;
    state_.turn.phase = TurnPhase::MainPhase;
    state_.turn.ns_state = NeutralShowdownState::Neutral;
    state_.turn.oc_state = OpenClosedState::Open;
    state_.turn.priority_holder = state_.turn.turn_player;

    events_.emit(PhaseChangedEvent{old_phase, TurnPhase::MainPhase,
                                    state_.turn.turn_player});

    mainPhaseLoop();
}

// Loop body of mainPhase() — extracted so resumeFromSnapshot can re-enter
// the action loop without firing the entry side effects (which reset
// ns_state/oc_state/priority_holder and re-emit PhaseChangedEvent).
// Those resets would corrupt a snapshot taken mid-chain or mid-showdown.
void GameEngine::mainPhaseLoop() {
    int action_count = 0;
    while (true) {
        auto adv = advanceMainPhase(action_count);
        if (adv.kind == MainPhaseAdvance::Kind::Done) return;

        auto intent = queryAgent(state_.turn.turn_player);
        if (!resolveMainPhaseDecision(intent, action_count)) return;
    }
}

void GameEngine::endingStep() {
    events_.logTrace("── Ending Step ──");
    auto old_phase = state_.turn.phase;
    state_.turn.phase = TurnPhase::EndingStep;
    events_.emit(PhaseChangedEvent{old_phase, TurnPhase::EndingStep,
                                    state_.turn.turn_player});

    // CR 423.1.a.2 — Stunned units lose Stunned at the beginning of the
    // NEXT Ending Step (i.e., this one). Phase 6q+ correction: previously
    // cleared in expirationStep which is the wrong CR step.
    for (auto& [id, obj] : state_.objects) {
        if (obj.is_stunned) {
            obj.is_stunned = false;
        }
    }

    // Process "At the end of your turn" triggers (e.g., Dazzling Aurora)
    if (chain_manager_->chainExists()) runChain();

    // Cleanup AFTER the chain finishes — same pattern as the main-phase
    // action loop. The end-of-turn chain can damage units (Aurora plays
    // Elder Dragon which damages enemies, Aurora plays a unit whose
    // WhenYouPlayMe damages, etc.). Without this pass, damage sits on
    // damage_marked forever — processLethalDamage doesn't run, so
    // Elder Dragon's "any-damage-lethal" aura never kills the damaged
    // unit, and natural-lethal units (might-1 Mousers etc.) survive
    // into the next turn at 1/1 dmg.
    cleanup();
}

void GameEngine::expireTemporaryKeywords(GameObject& obj, const CardDB& db) {
    if (obj.temp_keywords.bits == 0) return;
    KeywordSet base_keywords;
    if (obj.card_def_id != kInvalidId) {
        base_keywords = db.get(obj.card_def_id).keywords;
    }
    for (int k = 0; k < static_cast<int>(Keyword::Count); ++k) {
        Keyword kw = static_cast<Keyword>(k);
        if (!obj.temp_keywords.has(kw)) continue;
        bool from_base = base_keywords.has(kw);
        bool from_aura = obj.aura_keywords.has(kw);
        if (!from_base && !from_aura) {
            obj.keywords.clear(kw);
        }
    }
    obj.temp_keywords.reset();
}

void GameEngine::expirationStep() {
    events_.logTrace("── Expiration Step ──");
    auto old_phase = state_.turn.phase;
    state_.turn.phase = TurnPhase::ExpirationStep;
    events_.emit(PhaseChangedEvent{old_phase, TurnPhase::ExpirationStep,
                                    state_.turn.turn_player});

    // CR 317.2.f.1 — "If any items underwent the FEPR process, return to
    // the start of the Expiration Step." Loop the body + chain drain
    // until no chain item resolves during a pass. Phase 6q+ fix —
    // pre-fix, expirationStep was a single pass and any triggers fired
    // by healAllUnits / clearStun / emptyRunePools would sit on the chain
    // unresolved or resolve without re-running expiration.
    constexpr int kMaxExpirationPasses = 8;
    for (int pass = 0; pass < kMaxExpirationPasses; ++pass) {
        doExpirationBody();
        if (!chain_manager_->chainExists()) break;
        runChain();
        // If running the chain produced new chain items (cascading
        // triggers from expiration effects), keep looping. Otherwise
        // we're stable.
        if (!chain_manager_->chainExists()) break;
    }
}

void GameEngine::doExpirationBody() {
    // Heal all units (CR 317.2.b)
    healAllUnits();

    // (Stun clearing moved to endingStep per CR 423.1.a.2)

    // Expire "this turn" effects (CR 317.2.c)
    for (auto& [id, obj] : state_.objects) {
        // Lotus Trap-style damage doubling expires here.
        obj.damage_doubled_this_turn = false;

        // Tactical Retreat's "next time it would die THIS TURN" replacement
        // expires unused at end of turn.
        obj.death_replacement_recall_pending = false;

        // Counter Strike's one-shot damage prevention is "this turn".
        obj.prevent_next_damage_this_turn = false;

        // Per-unit move counter is "this turn" (Kayn, Unleashed).
        obj.moves_this_turn = 0;

        // Temp might is a separate field from buff_count (permanent buffs);
        // just clear it. recomputeMight (below / in cleanup) drops the bonus.
        bool had_temp_might = (obj.temp_might_bonus != 0);
        obj.temp_might_bonus = 0;
        if (obj.temp_assault_value != 0) {
            obj.assault_value -= obj.temp_assault_value;
            obj.temp_assault_value = 0;
            if (obj.assault_value <= 0) {
                obj.assault_value = 0;
                // Only clear keyword if base card doesn't have it
                if (obj.card_def_id != kInvalidId) {
                    const auto& def = card_db_.get(obj.card_def_id);
                    if (!def.keywords.has(Keyword::Assault)) {
                        obj.keywords.clear(Keyword::Assault);
                    }
                } else {
                    obj.keywords.clear(Keyword::Assault);
                }
            }
        }
        if (obj.temp_shield_value != 0) {
            obj.shield_value -= obj.temp_shield_value;
            obj.temp_shield_value = 0;
            if (obj.shield_value <= 0) {
                obj.shield_value = 0;
                if (obj.card_def_id != kInvalidId) {
                    const auto& def = card_db_.get(obj.card_def_id);
                    if (!def.keywords.has(Keyword::Shield)) {
                        obj.keywords.clear(Keyword::Shield);
                    }
                } else {
                    obj.keywords.clear(Keyword::Shield);
                }
            }
        }
        (void)had_temp_might;  // temp fields cleared above; recompute regardless
        obj.recomputeMight();

        // Pure-flag keywords granted "this turn" (Bounty Hunter's
        // Ganking, etc.). Delegated to a static helper so tests can
        // exercise the revoke logic without setting up the engine's
        // full subsystem stack.
        expireTemporaryKeywords(obj, card_db_);
    }
    // Recompute might for all units after expiration
    for (auto& [id, obj] : state_.objects) {
        if (obj.isUnit() && obj.location.has_value()) {
            obj.recomputeMight();
        }
    }

    // Empty rune pools (CR 317.2.d)
    emptyRunePools();

    // CR 416 + "take control of … until end of turn" — revert any
    // EOT-scoped control changes. Phase 6q+ wiring of the
    // EffectExecutor::takeControl primitive (until_end_of_turn=true).
    for (auto& [id, obj] : state_.objects) {
        if (obj.control_reverts_eot) {
            auto restored = obj.original_controller_eot;
            if (restored != PlayerId::None && restored != obj.controller) {
                events_.logTrace(std::string("REVERT_CONTROL: ") + obj.name +
                                 " (id=" + std::to_string(id) + ") " +
                                 toString(obj.controller) + " -> " +
                                 toString(restored));
                obj.controller = restored;
                events_.emit(ObjectStateChangedEvent{id, "controller_reverted"});
            }
            obj.control_reverts_eot = false;
            obj.original_controller_eot = PlayerId::None;
        }
    }

    // Expire delayed abilities that expire at end of turn (CR 389)
    state_.delayed_abilities.erase(
        std::remove_if(state_.delayed_abilities.begin(), state_.delayed_abilities.end(),
            [&](const DelayedAbility& da) {
                return da.expires_on_turn == state_.turn.turn_number;
            }),
        state_.delayed_abilities.end());
}

// ═══════════════════════════════════════════════════════════════════════════════
// Action execution
// ═══════════════════════════════════════════════════════════════════════════════

void GameEngine::executeIntent(const Intent& intent) {
    events_.logTrace(std::string("INTENT: ") + toString(intent.type) + " by " + toString(intent.player));

    switch (intent.type) {
        case IntentType::PlayCard:
        case IntentType::PlayActionCard: {
            auto& card = state_.getObject(intent.card);
            if (card.isSpell()) {
                executePlaySpell(intent);
            } else {
                executePlayCard(intent);
            }
            break;
        }
        case IntentType::ActivateAbility:
        case IntentType::ActivateActionAbility: {
            auto& source = state_.getObject(intent.ability_source);
            // Skip card-def lookup for tokens (no CardDef). The rest of this
            // handler reads from `source` directly; the previous `def` binding
            // was dead and threw std::out_of_range when source was a token.
            if (source.card_def_id == kInvalidId) break;

            // Equip: card handles its own cost payment and attachment.
            // Phase 6r — gears with needsEquipTimeTarget=true get an
            // empty-targets intent here; onEquip uses pickTarget to
            // resolve. We pass kInvalidId as the unit id; the gear's
            // onEquip override is responsible for the pick.
            if (source.isGear() && !source.attached_to.has_value()) {
                Card* gear_card = card_registry_.get(source.card_def_id);
                if (gear_card && gear_card->hasEquipAbility()) {
                    bool has_target = !intent.targets.empty();
                    if (has_target || gear_card->needsEquipTimeTarget()) {
                        CardContext equip_ctx{state_, events_, *effect_executor_,
                                             intent.player, intent.ability_source};
                        GameObjectId target = has_target ? intent.targets[0]
                                                          : kInvalidId;
                        if (gear_card->onEquip(equip_ctx, target)) {
                            cleanup();
                        }
                        break;
                    }
                }
            }

            // Activated ability: exhaust source, pay cost, add to chain

            // Pay activation cost via Card object. Phase 6r — read the
            // per-ability cost from activatedAbilities()[intent.ability_index]
            // so multi-ability cards pay the right cost for the right
            // ability variant. Single-ability cards have ability_index=0
            // (default) and the wrapping default impl returns their legacy
            // getActivationCost() as the only element.
            Card* ability_card = card_registry_.get(source.card_def_id);
            ActivationCost act_cost;
            if (ability_card) {
                auto abilities = ability_card->activatedAbilities();
                int idx = intent.ability_index;
                if (idx < 0 || idx >= static_cast<int>(abilities.size())) idx = 0;
                if (!abilities.empty()) act_cost = abilities[idx].cost;
                // State-aware energy reduction (Bashful Bloom), clamped ≥ 0.
                int red = ability_card->activationCostReduction(state_, intent.player, idx);
                if (red > 0) act_cost.energy = std::max(0, act_cost.energy - red);
            }
            if (act_cost.exhaust) {
                source.is_exhausted = true;
                events_.emit(ObjectStateChangedEvent{intent.ability_source, "exhausted"});
            }
            // Pay energy cost
            if (act_cost.energy > 0) {
                int needed = act_cost.energy;
                auto base_loc = BaseLocation{intent.player};
                for (auto& [id, obj] : state_.objects) {
                    if (needed <= 0) break;
                    if (!obj.isRune() || obj.controller != intent.player || obj.is_exhausted) continue;
                    if (!obj.location.has_value() || *obj.location != LocationId{base_loc}) continue;
                    obj.is_exhausted = true;
                    needed--;
                    events_.emit(ObjectStateChangedEvent{id, "exhausted"});
                }
            }
            // Pay recycle-self cost [C]
            if (act_cost.recycle_self) {
                events_.logTrace("ACTIVATE_COST: recycle self " + source.name);
                source.zone = ZoneType::MainDeck;
                source.location = std::nullopt;
                state_.player(intent.player).main_deck.insert(
                    state_.player(intent.player).main_deck.begin(), intent.ability_source);
            }
            // Pay discard cost
            if (act_cost.discard && act_cost.discard_count > 0) {
                events_.logTrace("ACTIVATE_COST: discard " + std::to_string(act_cost.discard_count));
                effect_executor_->discardCards(intent.player, act_cost.discard_count);
            }
            // Pay XP cost
            if (act_cost.xp_cost > 0) {
                state_.player(intent.player).xp -= act_cost.xp_cost;
                events_.logTrace("ACTIVATE_COST: spend " +
                                  std::to_string(act_cost.xp_cost) + " XP");
            }

            // Add ability to chain and resolve. is_activated=true so
            // stepResolve dispatches through Card::onActivate (not
            // onTrigger). Triggered abilities flow through TriggerManager
            // which calls addAbility with the default is_activated=false.
            chain_manager_->addAbility(intent.ability_source, intent.player,
                                        source.card_def_id, intent.targets,
                                        /*is_activated=*/true,
                                        intent.ability_index);
            runChain();
            break;
        }
        case IntentType::StandardMove:
            executeStandardMove(intent);
            break;
        case IntentType::EndTurn:
            executeEndTurn(intent);
            break;
        case IntentType::MulliganDecision:
            executeMulligan(intent);
            break;
        case IntentType::AssignCombatDamage:
            executeAssignCombatDamage(intent);
            break;
        case IntentType::PassFocus:
            executePassFocus(intent);
            break;
        case IntentType::HideCard:
            executeHideCard(intent);
            break;
        case IntentType::Concede:
            state_.game_over = true;
            state_.winner = opponent(intent.player);
            state_.game_over_reason = toString(intent.player) +
                                       std::string(" conceded");
            events_.emit(GameOverEvent{state_.winner, state_.game_over_reason});
            break;
        default:
            break;
    }
}

void GameEngine::executePlayCard(const Intent& intent) {
    auto& ps = state_.player(intent.player);
    auto& card = state_.getObject(intent.card);
    int trace_cost = (card.card_def_id != kInvalidId)
        ? card_db_.get(card.card_def_id).energy_cost : 0;
    events_.logTrace("PLAY: " + card.name + " (id=" + std::to_string(intent.card) +
                     ", " + toString(card.card_type) + ", cost=" +
                     std::to_string(trace_cost) + "E)");

    // Remove from current zone (hand or champion zone) (CR 354: step 1)
    if (card.zone == ZoneType::Hand) {
        auto it = std::find(ps.hand.begin(), ps.hand.end(), intent.card);
        if (it != ps.hand.end()) ps.hand.erase(it);
    } else if (card.zone == ZoneType::ChampionZone) {
        ps.champion_zone = kInvalidId;
    }

    // Pay the card's cost (CR 357). Surface the play_source via a
    // transient field on PlayerState so the cost path can consult it
    // for Rek'Sai's auto-Accelerate (and any future from-non-hand
    // discounts). Always clear after the call.
    ps.current_play_source = intent.play_source;
    payCardCost(intent.player, intent.card);
    ps.current_play_source = Intent::PlaySource::Hand;

    // "You may pay X as an additional cost to play me" (Akshan, Nami). Paid
    // here — after the base cost, before CardPlayedEvent fires WhenYouPlayMe —
    // so the card's play trigger can read card_counters[paid_flag].
    maybePayOptionalAdditionalCost(intent.player, intent.card);

    // Track play count
    ps.cards_played_this_turn++;
    // Per-turn gear/equipment counters (Emperor of the Sands gate; Ornn's
    // Forge "first gear each turn"). NOTE: this runs AFTER payCardCost, so the
    // first gear's cost reduction still sees gears_played_this_turn == 0.
    if (card.isGear()) {
        ps.gears_played_this_turn++;
        for (const auto& tag : card.tags) {
            if (tag == "Equipment") { ps.equipment_played_this_turn++; break; }
        }
    }
    int energy_spent = 0;
    if (card.card_def_id != kInvalidId) {
        energy_spent = card_db_.get(card.card_def_id).energy_cost;
    }
    events_.emit(CardPlayedEvent{intent.card, intent.player,
        card.card_type, ps.cards_played_this_turn, energy_spent});

    // Store the play location on the game object so resolvePermanent can use it.
    // Permanents choose location during finalization (CR 355.2.a).
    card.location = intent.play_location.value_or(BaseLocation{intent.player});

    // CR 135.2.b.3 + CR 355.1 — "As you play me" / "As I am played"
    // instructions execute during the play action itself, not as a
    // triggered ability. No chain item is created; no priority window
    // opens between this hook and the permanent landing on the board.
    // The card can mutate state (spawn tokens, change its own location)
    // before resolvePermanent emits EnteredBoardEvent. Used by Baron
    // Nashor to spawn the Baron Pit BF token and redirect itself there.
    if (card.card_def_id != kInvalidId) {
        if (Card* card_obj = card_registry_.get(card.card_def_id)) {
            CardContext on_play_ctx{state_, events_, *effect_executor_,
                                     intent.player, intent.card};
            card_obj->onPlay(on_play_ctx);
        }
    }

    // Route through chain — permanent resolves immediately at Finalize (CR 337.1.c)
    chain_manager_->addPermanent(intent.card, intent.player);
    runChain();
}

void GameEngine::executePlaySpell(const Intent& intent) {
    auto& ps = state_.player(intent.player);
    auto& card = state_.getObject(intent.card);
    {
        std::string tgt_str;
        for (auto t : intent.targets) {
            if (!tgt_str.empty()) tgt_str += ",";
            tgt_str += state_.objectExists(t) ? state_.getObject(t).name : "?";
            tgt_str += "(id=" + std::to_string(t) + ")";
        }
        events_.logTrace("SPELL: " + card.name + " (id=" + std::to_string(intent.card) +
                         ") targets=[" + tgt_str + "]");
    }

    // Remove from the source zone.
    if (card.zone == ZoneType::Hand) {
        auto it = std::find(ps.hand.begin(), ps.hand.end(), intent.card);
        if (it != ps.hand.end()) ps.hand.erase(it);
    } else if (intent.play_source == Intent::PlaySource::Trash &&
               card.zone == ZoneType::Trash) {
        // Trash-replay (Death from Below): play the spell straight out of trash.
        auto it = std::find(ps.trash.begin(), ps.trash.end(), intent.card);
        if (it != ps.trash.end()) ps.trash.erase(it);
    }

    // Pay cost. A trash-replay grant overrides the printed cost (and its own
    // additional costs); otherwise the normal play_source-aware path runs.
    bool paid_via_grant = false;
    if (intent.play_source == Intent::PlaySource::Trash) {
        paid_via_grant = payTrashReplayGrant(intent.player, intent.card);
    }
    if (!paid_via_grant) {
        ps.current_play_source = intent.play_source;
        payCardCost(intent.player, intent.card);
        ps.current_play_source = Intent::PlaySource::Hand;

        // "You may pay X as an additional cost to play me" (spell variant).
        maybePayOptionalAdditionalCost(intent.player, intent.card);
    }

    // Repeat (CR 820) — agent-driven, modeled on Accelerate. After
    // paying the base cost we poll the agent yes/no for each
    // additional tranche they can still afford. Each "yes" pays an
    // extra [N][D] and bumps repeats_paid. Stops when the agent
    // declines or affordability runs out. Replaces the prior model
    // where the action generator pre-encoded one Play intent per
    // repeat count — that produced visually-identical legal-action
    // entries and forced the agent to commit to N before seeing how
    // the spell would resolve.
    int repeats = 0;
    RepeatCost repeat_cost{};
    if (card.card_def_id != kInvalidId) {
        repeat_cost = parseRepeatCost(card_db_.get(card.card_def_id).ability_text);
    }
    // GRANTED [Repeat] (when the spell has none of its own printed):
    //   The Academy (772): "next spell gets [Repeat] = its base cost" (one-shot).
    //   Syndra, Transcendent (708): "your spells have [Repeat] [2][P]" (continuous
    //   while she's in a showdown). Academy's one-shot grant takes precedence.
    if (card.isSpell() && !repeat_cost.valid) {
        if (ps.grant_repeat_base_to_next_spell && card.card_def_id != kInvalidId) {
            repeat_cost.energy = card_db_.get(card.card_def_id).energy_cost;
            repeat_cost.power = 0;
            repeat_cost.power_domain = Domain::Count;  // universal [A]
            repeat_cost.valid = repeat_cost.energy > 0;
            ps.grant_repeat_base_to_next_spell = false;  // consume
        } else if (ps.spells_have_repeat_energy > 0 || ps.spells_have_repeat_power > 0) {
            repeat_cost.energy = ps.spells_have_repeat_energy;
            repeat_cost.power = ps.spells_have_repeat_power;
            repeat_cost.power_domain = ps.spells_have_repeat_domain;
            repeat_cost.valid = true;
        }
    }
    // Marai Spire (525): "friendly [Repeat] costs cost [1] less."
    if (repeat_cost.valid && ps.repeat_cost_reduction > 0) {
        repeat_cost.energy = std::max(0, repeat_cost.energy - ps.repeat_cost_reduction);
    }
    if (repeat_cost.valid) {
        constexpr int kRepeatSafetyCap = 32;
        while (repeats < kRepeatSafetyCap &&
               canPayAdditionalCost(intent.player,
                                     repeat_cost.energy,
                                     repeat_cost.power,
                                     repeat_cost.power_domain)) {
            Intent decline;
            decline.type = IntentType::MakeChoice;
            decline.player = intent.player;
            decline.chosen_value = 0;
            Intent accept;
            accept.type = IntentType::MakeChoice;
            accept.player = intent.player;
            accept.chosen_value = 1;
            std::vector<Intent> opts = {decline, accept};

            state_.decision_index++;
            events_.logTrace("DECISION #" + std::to_string(state_.decision_index) +
                             " (" + toString(intent.player) + "): "
                             "pay Repeat tranche " + std::to_string(repeats + 1) +
                             " for " + card.name + "? [decline|pay] [2 options]");
            auto chosen = getAgent(intent.player).selectAction(state_, opts);
            recordAppliedIntent(chosen);
            if (on_decision) on_decision(state_, opts, chosen);
            bool wants_pay = chosen.chosen_value.value_or(0) == 1;
            if (!wants_pay) break;
            if (!payRepeatCost(intent.player, repeat_cost)) break;
            ++repeats;
        }
        if (repeats > 0) {
            events_.logTrace("REPEAT: paid " + std::to_string(repeats) +
                              " extra tranche(s) for " + card.name);
        }
    }

    // Track play count
    ps.cards_played_this_turn++;
    int energy_spent = 0;
    if (card.card_def_id != kInvalidId) {
        energy_spent = card_db_.get(card.card_def_id).energy_cost;
    }
    int total_energy_spent = energy_spent + repeats * repeat_cost.energy;
    // Snapshot on PlayerState so legend triggers can read it. Forgotten
    // Library / Virtuoso gate on the TOTAL energy spent (base + repeats).
    ps.last_spell_energy_spent = total_energy_spent;
    // Track the most-expensive single spell paid for this turn (Jhin,
    // Meticulous Killer: "if you've spent [4]+ to play a spell this turn").
    if (card.isSpell()) {
        ps.max_spell_spent_this_turn =
            std::max(ps.max_spell_spent_this_turn, total_energy_spent);
        // Bind a pending "next spell deals N Bonus Damage" rider (Ravenborn Tome)
        // onto this spell object so it applies to ALL instances it deals.
        if (ps.next_spell_bonus_damage > 0) {
            card.spell_bonus_damage = ps.next_spell_bonus_damage;
            ps.next_spell_bonus_damage = 0;
        }
    }
    events_.emit(CardPlayedEvent{intent.card, intent.player,
        card.card_type, ps.cards_played_this_turn, total_energy_spent});

    // Add spell to chain with targets. Carry energy_spent and repeats_paid
    // onto the chain item — ChainManager re-resolves the spell `repeats_paid`
    // extra times in its post-resume loop.
    auto chain_id = chain_manager_->addSpell(intent.card, intent.player, intent.targets);
    (void)chain_id;
    if (!state_.chain.items.empty()) {
        state_.chain.items.back().total_energy_spent = total_energy_spent;
        state_.chain.items.back().repeats_paid = repeats;
    }

    // Run the FEPR loop
    runChain();
}

void GameEngine::runChain() {
    chain_manager_->processFEPR(
        // Agent query callback
        [this](PlayerId player, const std::vector<Intent>& actions) -> Intent {
            return queryAgentForChain(player, actions);
        },
        // Permanent resolution callback (CR 337.1.c)
        [this](const ChainItem& item) {
            resolvePermanent(item);
        },
        // Spell resolution callback
        [this](const ChainItem& item) {
            resolveSpell(item);
        },
        // Closed State action generator (handles targeting + affordability)
        [this](PlayerId player) -> std::vector<Intent> {
            return generateClosedStateActions(player);
        }
    );
}

void GameEngine::resolveSpell(const ChainItem& item) {
    {
        std::string name = state_.objectExists(item.source) ? state_.getObject(item.source).name : "?";
        events_.logTrace("RESOLVE: " + name + " (chain_item=" + std::to_string(item.id) +
                         ", ability=" + std::to_string(item.is_ability) +
                         ", spell=" + std::to_string(item.is_spell) + ")");
    }

    // Per-target legality re-validation at resolution time.
    //
    // CR 359.3.e.5: "If any of the spell's targets are no longer legal
    // when the spell resolves, those targets are unaffected by the
    // spell as it resolves" (per-target, not all-or-nothing).
    //
    // Phase 6q+ engine-audit CRITICAL #6 fix: pre-2026-05-19 the
    // engine fizzled the ENTIRE spell if any single target was
    // illegal at resolve. Challenge / Star-Crossed / Deathgrip
    // silently no-op'd whenever one of their two targets died,
    // bounced, or otherwise left the legal-target set between play
    // time and resolve time. Per CR, the legal target(s) should
    // still receive their effect; only the illegal one(s) are
    // skipped. We implement this by FILTERING `item.targets` rather
    // than aborting — cards then read targets[i] and naturally skip
    // any that have already been removed from the legal set.
    //
    // We pass the filtered list into onResolve/onActivate/onTrigger
    // via a local copy. The chain item itself is left untouched
    // (avoids interfering with chain resume / multi-resolve paths).
    //
    // Cards that defer target choice to resolve time via
    // pickTarget/pickTargetPair (Phase 6q opt-in) are unaffected —
    // they re-enumerate legal targets at resolve time themselves.
    Card* card = card_registry_.get(item.card_def_id);
    std::vector<GameObjectId> effective_targets = item.targets;
    if (card && !item.targets.empty()) {
        auto reqs = card->getTargetRequirements();
        if (reqs.count > 0) {
            auto legal = card->enumerateLegalTargets(state_, item.controller);
            std::vector<GameObjectId> filtered;
            filtered.reserve(item.targets.size());
            int dropped = 0;
            for (auto t : item.targets) {
                if (std::find(legal.begin(), legal.end(), t) != legal.end()) {
                    filtered.push_back(t);
                } else {
                    ++dropped;
                }
            }
            if (dropped > 0) {
                std::string name = state_.objectExists(item.source)
                    ? state_.getObject(item.source).name : "?";
                events_.logTrace("RESOLVE: " + name + " — " +
                                  std::to_string(dropped) +
                                  " target(s) no longer legal "
                                  "(per-CR 359.3.e.5: unaffected, "
                                  "rest of spell still resolves)");
            }
            effective_targets = std::move(filtered);
        }
    }

    // Dispatch through Card object
    CardContext ctx{state_, events_, *effect_executor_,
                    item.controller, item.source};
    ctx.firing_trigger = item.fired_trigger;

    if (card) {
        if (item.is_ability) {
            // Activated abilities ([E]:, [N]:, etc. — player chose to
            // pay cost + activate) dispatch through onActivate; triggered
            // abilities (event-driven, e.g. WhenIPlay, AtEndOfTurn)
            // dispatch through onTrigger. The two have different
            // semantics so cards override the matching method only —
            // the other defaults to a no-op. The is_activated_ability
            // flag is set on the chain item by ChainManager::addAbility.
            if (item.is_activated_ability) {
                // Phase 6r — dispatch via the multi-ability overload.
                // Default Card impl falls through to onActivate(ctx, targets)
                // so single-ability cards keep working unchanged.
                card->onActivate(ctx, item.ability_index, effective_targets);
                // Prize of Progress: a gear's activated ability was used.
                if (state_.objectExists(item.source) &&
                    state_.getObject(item.source).isGear()) {
                    events_.emit(ObjectStateChangedEvent{item.source,
                                                         "gear_ability_used"});
                }
            } else {
                card->onTrigger(ctx, effective_targets);
            }
        } else {
            // Spell resolution
            card->onResolve(ctx, effective_targets);
        }
    }
}

void GameEngine::resolvePermanent(const ChainItem& item) {
    // CR 337.1.c / CR 359.2: Permanent leaves chain, becomes game object on board.
    // Location was pre-selected during executePlayCard and stored on the object.
    auto& card = state_.getObject(item.source);
    auto loc = card.location.value_or(BaseLocation{item.controller});

    card.zone = ZoneType::Base;
    card.location = loc;

    // Units enter exhausted (CR 143.4) unless Accelerate or Ambush-to-BF
    if (card.isUnit()) {
        bool enters_ready = false;
        // Accelerate (CR 805.1.a): "You may pay [1] and 1 Power as an
        // additional cost. If you do, I enter ready."
        //   • CR 805.1.a.1 — single-domain unit: Power must match the
        //     unit's domain.
        //   • CR 805.1.a.2 — multi-domain or no-domain unit: Power may be
        //     any domain (i.e., [A]).
        // The "you may" makes this OPTIONAL — surfaced as a yes/no
        // decision to the agent. (Previously auto-applied whenever
        // affordable, which silently spent the player's energy + rune
        // every time the Accelerate cost happened to be payable.)
        if (card.keywords.has(Keyword::Accelerate)) {
            bool single_domain = (card.domains.size() == 1);
            std::optional<Domain> domain_to_pay;
            if (single_domain) {
                if (canPayAdditionalCost(item.controller, 1, 1, card.domains[0])) {
                    domain_to_pay = card.domains[0];
                }
            } else {
                for (int di = 0; di < static_cast<int>(Domain::Count); ++di) {
                    Domain d = static_cast<Domain>(di);
                    if (canPayAdditionalCost(item.controller, 1, 1, d)) {
                        domain_to_pay = d;
                        break;
                    }
                }
            }
            if (domain_to_pay.has_value()) {
                Intent decline;
                decline.type = IntentType::MakeChoice;
                decline.player = item.controller;
                decline.chosen_value = 0;  // 0 = decline
                Intent accept;
                accept.type = IntentType::MakeChoice;
                accept.player = item.controller;
                accept.chosen_value = 1;   // 1 = pay [1][D] for Accelerate
                std::vector<Intent> opts = {decline, accept};

                state_.decision_index++;
                events_.logTrace("DECISION #" + std::to_string(state_.decision_index) +
                                 " (" + toString(item.controller) + "): "
                                 "pay [1][" + toString(*domain_to_pay) +
                                 "] for Accelerate on " + card.name +
                                 "? [decline|pay] [2 options]");

                auto chosen = getAgent(item.controller).selectAction(state_, opts);
                recordAppliedIntent(chosen);
                if (on_decision) on_decision(state_, opts, chosen);

                bool wants_pay = chosen.chosen_value.value_or(0) == 1;
                if (wants_pay) {
                    events_.logTrace("ACCELERATE: " + card.name + " pays [1][" +
                                     toString(*domain_to_pay) + "] to enter ready");
                    payAdditionalCost(item.controller, 1, 1, *domain_to_pay);
                    enters_ready = true;
                } else {
                    events_.logTrace("ACCELERATE: " + card.name + " declined");
                }
            }
        }
        // Ambush units entering a battlefield enter ready
        if (card.hasKeyword(Keyword::Ambush) && card.isAtBattlefield()) {
            enters_ready = true;
        }
        // Cards that intrinsically enter ready (e.g. Daisy! "I enter ready").
        if (card.card_def_id != kInvalidId) {
            if (const Card* cdef = card_registry_.get(card.card_def_id)) {
                // Prefer the state-aware overload so cards with
                // conditional ready (Monch) can consult state. Default
                // impl falls through to the no-arg version.
                if (cdef->entersReadyOnPlay(state_, card.controller)) enters_ready = true;
            }
        }
        card.is_exhausted = !enters_ready;
    }
    // Gear enters ready (CR 149.1)
    if (card.isGear()) {
        card.is_exhausted = false;
    }

    card.recomputeMight();

    events_.emit(EnteredBoardEvent{item.source, item.controller,
        card.card_type, loc, true});

    // Quick-Draw: auto-attach to target unit if specified
    if (card.isGear() && card.hasKeyword(Keyword::QuickDraw) && !item.targets.empty()) {
        auto target_unit = item.targets[0];
        if (state_.objectExists(target_unit)) {
            events_.logTrace("QUICK_DRAW: auto-attaching " + card.name + " to " +
                             state_.getObject(target_unit).name);
            attachGearToUnit(item.source, target_unit);
        }
    }

    // Check if playing to a battlefield contests it
    if (card.isUnit() && std::holds_alternative<BattlefieldLocation>(loc)) {
        auto bf_id = std::get<BattlefieldLocation>(loc).id;
        auto& bf = getBattlefield(bf_id);

        if (!bf.controller.has_value() || *bf.controller != item.controller) {
            if (!bf.is_contested) {
                bf.is_contested = true;
                bf.contested_by = item.controller;
                events_.emit(ContestedEvent{bf_id, item.controller});
            }
            // Re-evaluate staging on every arrival at a contested BF.
            // See executeStandardMove for the rationale (showdowns that
            // end with 0 units leave the BF stuck without this).
            if (!bf.showdown_staged && !bf.combat_staged &&
                !bf.showdown_in_progress && !bf.combat_in_progress) {
                bool has_opponent = bf.hasUnitsFrom(
                    opponent(item.controller), state_.objects);
                if (has_opponent) {
                    bf.combat_staged = true;
                } else {
                    bf.showdown_staged = true;
                }
            }
        }
    }
}

Intent GameEngine::queryAgentForChain(PlayerId player,
                                       const std::vector<Intent>& actions) {
    state_.decision_index++;
    auto chosen = getAgent(player).selectAction(state_, actions);
    recordAppliedIntent(chosen);
    // Surface the chain-priority decision in the trace so V&V can
    // confirm priority is being passed correctly between players in
    // Closed State (CR 337). Without this, trivial-pass decisions are
    // invisible — the renderer suppresses the panel for single-option
    // decisions, leaving no marker at all that the engine offered
    // priority to player N. Format mirrors the main DECISION trace lines.
    events_.logTrace("DECISION #" + std::to_string(state_.decision_index) +
                     " (" + std::string(toString(player)) + " — chain priority): " +
                     std::to_string(actions.size()) + " legal actions");
    events_.logTrace("CHOSE: [" + std::string(toString(player)) + "] " +
                     std::string(toString(chosen.type)));
    if (on_decision) {
        on_decision(state_, actions, chosen);
    }
    return chosen;
}

void GameEngine::executeStandardMove(const Intent& intent) {
    if (!intent.move_destination.has_value()) return;
    auto dest = *intent.move_destination;

    for (auto unit_id : intent.units_to_move) {
        auto& unit_obj = state_.getObject(unit_id);
        std::string dest_str = std::holds_alternative<BattlefieldLocation>(dest)
            ? "BF#" + std::to_string(std::get<BattlefieldLocation>(dest).id)
            : "Base(" + std::string(toString(std::get<BaseLocation>(dest).player)) + ")";
        events_.logTrace("MOVE: " + unit_obj.name + " (id=" + std::to_string(unit_id) +
                         ") -> " + dest_str);
        auto& unit = state_.getObject(unit_id);

        // Pay cost: exhaust (CR 144.2)
        unit.is_exhausted = true;
        events_.emit(ObjectStateChangedEvent{unit_id, "exhausted"});

        auto old_location = unit.location;
        moveUnit(unit_id, dest);

        events_.emit(UnitMovedEvent{
            unit_id, intent.player,
            old_location.value_or(BaseLocation{intent.player}),
            dest, true
        });
    }

    // Check if this contests a battlefield
    if (std::holds_alternative<BattlefieldLocation>(dest)) {
        auto bf_id = std::get<BattlefieldLocation>(dest).id;
        auto& bf = getBattlefield(bf_id);

        // CR 459.2.b.3.a / .4.a — units arriving during ongoing combat
        // inherit the appropriate combat designation. Phase 6q+ fix:
        // previously the unit sat with combat_designation = None and
        // didn't contribute to damage assignment / step resolution.
        if (bf.combat_in_progress) {
            for (auto unit_id : intent.units_to_move) {
                auto& unit_obj = state_.getObject(unit_id);
                if (bf.attacker.has_value() && unit_obj.controller == *bf.attacker) {
                    unit_obj.combat_designation = CombatDesignation::Attacker;
                } else if (bf.defender.has_value() && unit_obj.controller == *bf.defender) {
                    unit_obj.combat_designation = CombatDesignation::Defender;
                }
                unit_obj.recomputeMight();
            }
        }

        // Contested if moving to a BF we don't control (CR 187.3.a)
        if (!bf.controller.has_value() || *bf.controller != intent.player) {
            if (!bf.is_contested) {
                bf.is_contested = true;
                bf.contested_by = intent.player;
                events_.emit(ContestedEvent{bf_id, intent.player});
            }
            // Always re-evaluate staging when a unit arrives at a
            // contested BF. Previously this block only fired on the
            // FIRST contest, which left BFs permanently stuck when a
            // showdown ended with 0 units on both sides (sole_player =
            // None → is_contested stays true → subsequent moves saw
            // `is_contested == true` and skipped staging → combat
            // never fired → no control ever established → 0-0 draws.
            if (!bf.showdown_staged && !bf.combat_staged &&
                !bf.showdown_in_progress && !bf.combat_in_progress) {
                bool has_opponent_units = bf.hasUnitsFrom(
                    opponent(intent.player), state_.objects);
                if (has_opponent_units) {
                    bf.combat_staged = true;
                } else {
                    bf.showdown_staged = true;
                }
            }
        }
    }
}

void GameEngine::executeEndTurn(const Intent&) {
    // Nothing to do — main phase loop handles this
}

void GameEngine::executeMulligan(const Intent& intent) {
    auto& ps = state_.player(intent.player);

    if (intent.cards_to_mulligan.empty()) return;

    // Set aside chosen cards (CR 118.1)
    std::vector<GameObjectId> set_aside;
    for (auto card_id : intent.cards_to_mulligan) {
        auto it = std::find(ps.hand.begin(), ps.hand.end(), card_id);
        if (it != ps.hand.end()) {
            set_aside.push_back(*it);
            ps.hand.erase(it);
            events_.logTrace("  MULLIGAN: " + std::string(toString(intent.player)) +
                             " set aside " + state_.getObject(card_id).name +
                             " (id=" + std::to_string(card_id) + ")");
        }
    }

    // Draw replacements (CR 118.2). Snapshot hand size so we can emit
    // explicit "MULLIGAN: drew X" lines for the replacements (the
    // executor's generic DREW: lines also fire, but the spell-prefixed
    // form makes the connection scan-able in the trace).
    size_t before_draw = ps.hand.size();
    drawCards(intent.player, static_cast<int>(set_aside.size()));
    for (size_t i = before_draw; i < ps.hand.size(); ++i) {
        auto did = ps.hand[i];
        events_.logTrace("  MULLIGAN: " + std::string(toString(intent.player)) +
                         " drew replacement " + state_.getObject(did).name +
                         " (id=" + std::to_string(did) +
                         ") — PRIVATE to " + toString(intent.player));
    }

    // Recycle set-aside cards to bottom of deck in random order (CR 118.3)
    std::shuffle(set_aside.begin(), set_aside.end(), rng_);
    for (auto card_id : set_aside) {
        state_.getObject(card_id).zone = ZoneType::MainDeck;
        ps.main_deck.insert(ps.main_deck.begin(), card_id); // bottom = front
    }
}

void GameEngine::executeAssignCombatDamage(const Intent& intent) {
    // Apply damage assignments
    for (auto& assignment : intent.damage_assignments) {
        auto& target = state_.getObject(assignment.target_unit);
        target.damage_marked += assignment.damage;
        events_.emit(DamageDealtEvent{
            assignment.target_unit, assignment.damage,
            kInvalidId, true
        });
    }
}

void GameEngine::executePassFocus(const Intent& intent) {
    state_.turn.players_passed_focus.insert(intent.player);
}

void GameEngine::executeHideCard(const Intent& intent) {
    auto& ps = state_.player(intent.player);
    auto& card = state_.getObject(intent.card);

    events_.logTrace("HIDE: " + card.name + " at BF#" +
                     std::to_string(intent.chosen_battlefield) +
                     " by " + toString(intent.player));

    // Remove from hand
    auto it = std::find(ps.hand.begin(), ps.hand.end(), intent.card);
    if (it != ps.hand.end()) ps.hand.erase(it);

    // Pay [A] cost — recycle a ready rune (universal power cost)
    auto base_loc = BaseLocation{intent.player};
    for (auto& [id, obj] : state_.objects) {
        if (!obj.isRune() || obj.controller != intent.player || obj.is_exhausted) continue;
        if (!obj.location.has_value() || *obj.location != LocationId{base_loc}) continue;
        // Recycle the rune for [A]
        events_.logTrace("HIDE_COST: recycled " + obj.name + " (id=" +
                         std::to_string(id) + ") to pay [A]");
        obj.location = std::nullopt;
        obj.zone = ZoneType::RuneDeck;
        ps.rune_deck.insert(ps.rune_deck.begin(), id);
        events_.emit(LeftBoardEvent{id, intent.player, CardType::Rune,
            base_loc, ZoneType::RuneDeck, false});
        break;
    }

    // Place facedown at the chosen battlefield
    auto bf_id = intent.chosen_battlefield;
    auto& bf = getBattlefield(bf_id);
    bf.facedown.push_back(intent.card);

    card.zone = ZoneType::FacedownZone;
    card.location = std::nullopt; // facedown zones are not locations per rules
    card.is_hidden = true;
    card.hidden_at = bf_id;
    card.hidden_on_turn = state_.turn.turn_number;

    events_.logDebug(std::string("HIDDEN: ") + card.name + " hidden at BF#" +
                     std::to_string(bf_id) + " by " + toString(intent.player));

    // "When you hide a card" (Katarina, Reckless).
    events_.emit(CardHiddenEvent{intent.card, intent.player});
}

void GameEngine::executePlayFromHidden(const Intent& intent) {
    auto& card = state_.getObject(intent.card);
    auto bf_id = card.hidden_at;
    auto& bf = getBattlefield(bf_id);

    // Remove from facedown zone
    auto it = std::find(bf.facedown.begin(), bf.facedown.end(), intent.card);
    if (it != bf.facedown.end()) bf.facedown.erase(it);

    card.is_hidden = false;
    card.hidden_at = kInvalidId;

    // "When you play a card from face down" (Katarina, Reckless).
    events_.emit(PlayedFromFacedownEvent{intent.card, intent.player});

    // Play ignoring base cost — permanents go to the BF they were hidden at
    if (card.isPermanent()) {
        card.location = BattlefieldLocation{bf_id};
        // Route through chain like normal plays
        auto& ps = state_.player(intent.player);
        ps.cards_played_this_turn++;
        // Hidden play is "ignoring its base cost" (CR 811) — energy_spent = 0.
        events_.emit(CardPlayedEvent{intent.card, intent.player,
            card.card_type, ps.cards_played_this_turn, /*energy_spent=*/0});
        chain_manager_->addPermanent(intent.card, intent.player);
        runChain();
    } else if (card.isSpell()) {
        auto& ps = state_.player(intent.player);
        ps.cards_played_this_turn++;
        events_.emit(CardPlayedEvent{intent.card, intent.player,
            card.card_type, ps.cards_played_this_turn, /*energy_spent=*/0});
        chain_manager_->addSpell(intent.card, intent.player, intent.targets);
        runChain();
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Legal action generation
// ═══════════════════════════════════════════════════════════════════════════════

std::vector<Intent> GameEngine::generateLegalActions() const {
    auto player = state_.turn.turn_player;

    // In Closed State, the priority holder acts (not necessarily turn player)
    if (state_.turn.isClosedState() && state_.turn.priority_holder.has_value()) {
        return generateClosedStateActions(*state_.turn.priority_holder);
    }

    switch (state_.turn.phase) {
        case TurnPhase::Mulligan:
            return generateMulliganActions(player);
        case TurnPhase::MainPhase:
            if (state_.turn.isNeutralOpen()) {
                return generateMainPhaseActions(player);
            }
            if (state_.turn.isShowdownOpen()) {
                // Focus holder acts in showdowns
                if (state_.turn.focus_holder.has_value()) {
                    return generateShowdownActions(*state_.turn.focus_holder);
                }
                return generateShowdownActions(player);
            }
            break;
        default:
            break;
    }

    return {};
}

std::vector<Intent> GameEngine::generateMainPhaseActions(PlayerId player) const {
    std::vector<Intent> actions;

    // Always can end turn
    actions.push_back(Intent::endTurn(player));

    // Standard Move: exhaust ready units to move them
    auto all_units = state_.allUnitsControlledBy(player);
    for (auto unit_id : all_units) {
        auto& unit = state_.getObject(unit_id);
        if (unit.is_exhausted) continue; // must be ready

        if (unit.isAtBase()) {
            // Can move from base to any battlefield (CR 144.4.a).
            // Rockfall Path's "can't be PLAYED here" does NOT restrict
            // movement — only the play-from-hand action generators
            // consult blocks_unit_play.
            for (auto& bf : state_.battlefields) {
                // Can't move to BF with units from 2 other players (CR 144.4.a.1)
                // In 1v1, this is never an issue
                LocationId dest = BattlefieldLocation{bf.id};
                actions.push_back(Intent::standardMove(
                    player, {unit_id}, dest));
            }
        } else if (unit.isAtBattlefield()) {
            // Can normally move back to base (CR 144.4.b). Vilemaw's Lair
            // and similar locking BFs set `blocks_move_to_base` to deny
            // this leg.
            auto unit_bf = unit.battlefieldId();
            bool allow_to_base = true;
            if (unit_bf) {
                const auto& src_bf = getBattlefield(*unit_bf);
                if (src_bf.blocks_move_to_base) allow_to_base = false;
            }
            if (unit.cant_move_to_base) allow_to_base = false;  // Determined Sentry
            if (allow_to_base) {
                actions.push_back(Intent::standardMove(
                    player, {unit_id}, BaseLocation{player}));
            }

            // BF→BF moves allowed when EITHER:
            //   - unit has Ganking (CR 144.4.c), OR
            //   - destination BF has `accepts_any_inbound` (Baron Pit's
            //     "Units can move here from anywhere" rule).
            // Either route emits one move per legal destination.
            // Rockfall Path's "can't be played here" does NOT restrict
            // BF→BF moves; only play-from-hand checks blocks_unit_play.
            const bool has_ganking = unit.hasKeyword(Keyword::Ganking);
            for (auto& bf : state_.battlefields) {
                if (!unit_bf || *unit_bf == bf.id) continue;
                const bool can_move = has_ganking || bf.accepts_any_inbound;
                if (!can_move) continue;
                actions.push_back(Intent::standardMove(
                    player, {unit_id}, BattlefieldLocation{bf.id}));
            }
        }
    }

    // Play units from hand — must be able to afford the cost.
    auto& ps = state_.player(player);
    const bool locked_out = ps.cant_play_cards_this_turn;  // Brynhir Thundersong
    for (auto card_id : ps.hand) {
        if (locked_out) break;  // skip all play-from-hand actions
        auto& card = state_.getObject(card_id);
        if (!card.isUnit()) continue;
        if (card.card_def_id == kInvalidId) continue;  // tokens have no CardDef
        if (!canAfford(player, card_id)) continue;

        // Check play-to-location abilities from ability_text
        const auto& def = card_db_.get(card.card_def_id);
        auto& text = def.ability_text;
        bool can_play_open_bf = (text.find("play me to an open battlefield") != std::string::npos);
        bool can_play_enemy_bf = (text.find("play me to an occupied enemy battlefield") != std::string::npos);
        bool can_play_any_bf = (text.find("play me to a battlefield") != std::string::npos ||
                                text.find("play me at a battlefield") != std::string::npos);
        // Board-wide grant: a friendly unit (e.g. Miss Fortune, Buccaneer) lets
        // ALL of this player's units be played to open battlefields.
        if (ps.grant_friendly_units_open_bf) can_play_open_bf = true;

        Intent play_intent;
        play_intent.type = IntentType::PlayCard;
        play_intent.player = player;
        play_intent.card = card_id;

        // NARROWING hook (Perched Grimwyrm): the card dictates its own legal
        // play locations and suppresses the default base + BF plays entirely.
        const Card* card_obj = card_registry_.get(card.card_def_id);
        if (card_obj && card_obj->restrictsPlayLocations()) {
            for (const auto& loc : card_obj->getPlayLocations(state_, player)) {
                Intent restricted = play_intent;
                restricted.play_location = loc;
                actions.push_back(restricted);
            }
            continue;  // no default base/BF plays for this card
        }

        // Can always play to base (CR 355.2.a)
        play_intent.play_location = BaseLocation{player};
        actions.push_back(play_intent);

        // Mageseeker Warden: while it restricts this player, units may be
        // played ONLY to base — skip all battlefield plays.
        if (ps.units_play_base_only) continue;

        for (auto& bf : state_.battlefields) {
            if (bf.blocks_unit_play) continue;  // Rockfall Path
            bool controlled = bf.controller.has_value() && *bf.controller == player;
            bool open = !bf.controller.has_value();
            bool enemy_occupied = bf.controller.has_value() && *bf.controller != player &&
                                  bf.hasUnitsFrom(opponent(player), state_.objects);

            bool can_play_here = controlled; // default: controlled BFs
            if (can_play_open_bf && open) can_play_here = true;
            if (can_play_enemy_bf && enemy_occupied) can_play_here = true;
            if (can_play_any_bf) can_play_here = true;

            if (can_play_here) {
                Intent bf_play = play_intent;
                bf_play.play_location = BattlefieldLocation{bf.id};
                actions.push_back(bf_play);
            }
        }
    }

    // Play champion from champion zone (also must afford)
    if (ps.champion_zone != kInvalidId) {
        auto& champ = state_.getObject(ps.champion_zone);
        if (champ.zone == ZoneType::ChampionZone &&
            canAfford(player, ps.champion_zone)) {
            Intent play_intent;
            play_intent.type = IntentType::PlayCard;
            play_intent.player = player;
            play_intent.card = ps.champion_zone;
            play_intent.play_location = BaseLocation{player};
            actions.push_back(play_intent);
        }
    }

    // Play gear from hand (to base only, CR 149.2)
    for (auto card_id : ps.hand) {
        if (locked_out) break;
        auto& card = state_.getObject(card_id);
        if (!card.isGear()) continue;
        if (!canAfford(player, card_id)) continue;

        Intent play_intent;
        play_intent.type = IntentType::PlayCard;
        play_intent.player = player;
        play_intent.card = card_id;
        play_intent.play_location = BaseLocation{player};
        actions.push_back(play_intent);
    }

    // Play spells from hand — Neutral Open allows any spell (Action or not).
    // Brynhir lockout suppresses card plays but not activated abilities.
    if (!locked_out) {
        generateSpellActions(player, /*action_ok=*/true, /*reaction_ok=*/true,
                              actions);
        generateTrashReplayActions(player, /*action_ok=*/true, /*reaction_ok=*/true,
                                    actions);
    }

    // Activate abilities on gear/legends/units ([E]: abilities)
    generateActivateAbilityActions(player, actions);

    // Equip gear to units (CR 818). Phase 6r — if a gear opts into
    // needsEquipTimeTarget, emit ONE intent (no target) instead of one
    // per (gear, friendly_unit) pair. The engine's equip-execution
    // path calls onEquip(ctx, kInvalidId) and the gear's Card uses
    // pickTarget inside onEquip to resolve the target. Fixes the same
    // action-vocab collision class as needsPlayTimeTarget /
    // needs_activation_time_target for gear equip intents.
    for (auto& [id, obj] : state_.objects) {
        if (!obj.isGear() || obj.controller != player) continue;
        if (!obj.location.has_value()) continue;
        if (obj.attached_to.has_value()) continue; // already attached

        Card* gear_card = card_registry_.get(obj.card_def_id);
        if (!gear_card || !gear_card->hasEquipAbility()) continue;

        if (gear_card->needsEquipTimeTarget()) {
            // One intent per gear; target chosen at resolve via pickTarget.
            Intent equip;
            equip.type = IntentType::ActivateAbility;
            equip.player = player;
            equip.ability_source = id;
            actions.push_back(equip);
            continue;
        }

        // Legacy path: enumerate friendly units as equip targets (all
        // unmigrated gear). Per-target action_vocab slot collision lives
        // here until the gear opts into needsEquipTimeTarget.
        for (auto& [uid, unit] : state_.objects) {
            if (!unit.isUnit() || unit.controller != player) continue;
            if (!unit.location.has_value()) continue;

            Intent equip;
            equip.type = IntentType::ActivateAbility;
            equip.player = player;
            equip.ability_source = id;
            equip.targets = {uid};
            actions.push_back(equip);
        }
    }

    // Hide cards with [Hidden] keyword facedown at controlled battlefields (CR 811)
    for (auto card_id : ps.hand) {
        auto& card = state_.getObject(card_id);
        if (!card.keywords.has(Keyword::Hidden)) continue;

        // Need at least one rune to pay [A] cost
        bool has_rune = false;
        auto base_loc = BaseLocation{player};
        for (auto& [id, obj] : state_.objects) {
            if (obj.isRune() && obj.controller == player && !obj.is_exhausted &&
                obj.location.has_value() && *obj.location == LocationId{base_loc}) {
                has_rune = true;
                break;
            }
        }
        if (!has_rune) continue;

        // Can hide at any controlled BF with available facedown slot
        for (auto& bf : state_.battlefields) {
            if (!bf.controller.has_value() || *bf.controller != player) continue;
            if (static_cast<int>(bf.facedown.size()) >= bf.facedown_max_occupancy) continue;

            Intent hide;
            hide.type = IntentType::HideCard;
            hide.player = player;
            hide.card = card_id;
            hide.chosen_battlefield = bf.id;
            actions.push_back(hide);
        }
    }

    return actions;
}

std::vector<Intent> GameEngine::generateMulliganActions(PlayerId player) const {
    std::vector<Intent> actions;
    auto& ps = state_.player(player);

    // Can mulligan 0 cards (keep hand)
    actions.push_back(Intent::mulligan(player, {}));

    // Can mulligan 1 card
    for (auto card_id : ps.hand) {
        actions.push_back(Intent::mulligan(player, {card_id}));
    }

    // Can mulligan 2 cards
    for (size_t i = 0; i < ps.hand.size(); ++i) {
        for (size_t j = i + 1; j < ps.hand.size(); ++j) {
            actions.push_back(Intent::mulligan(
                player, {ps.hand[i], ps.hand[j]}));
        }
    }

    return actions;
}

std::vector<Intent> GameEngine::generateShowdownActions(PlayerId player) const {
    std::vector<Intent> actions;

    // Can always pass focus
    actions.push_back(Intent::passFocus(player));

    const bool locked_out = state_.player(player).cant_play_cards_this_turn;

    // Can play Action or Reaction spells during showdowns (CR 806, 813).
    // Lockout (Brynhir Thundersong) suppresses card plays but not abilities.
    if (!locked_out) {
        generateSpellActions(player, /*action_ok=*/true, /*reaction_ok=*/true,
                              actions);
        generateTrashReplayActions(player, /*action_ok=*/true, /*reaction_ok=*/true,
                                    actions);
    }

    // Ambush: play units with [Ambush] during showdowns
    for (auto card_id : state_.player(player).hand) {
        if (locked_out) break;
        auto& card = state_.getObject(card_id);
        if (!card.isUnit() || !card.hasKeyword(Keyword::Ambush)) continue;
        if (!canAfford(player, card_id)) continue;

        const Card* amb = card.card_def_id != kInvalidId
            ? card_registry_.get(card.card_def_id) : nullptr;
        const bool enemy_bf_ok = amb && amb->ambushToEnemyBattlefields();
        for (auto& bf : state_.battlefields) {
            if (bf.blocks_unit_play) continue;  // Rockfall Path
            bool here = bf.hasUnitsFrom(player, state_.objects);
            // Rengar, Trophy Hunter: may Ambush to a BF with enemy units even
            // without units of your own there.
            if (!here && enemy_bf_ok)
                here = bf.hasUnitsFrom(opponent(player), state_.objects);
            if (!here) continue;
            Intent ambush;
            ambush.type = IntentType::PlayActionCard;
            ambush.player = player;
            ambush.card = card_id;
            ambush.play_location = BattlefieldLocation{bf.id};
            actions.push_back(ambush);
        }
    }

    // Reaction-to-attack plays (e.g., Rengar, Pouncing — play to a BF where
    // you are currently attacking).
    for (auto card_id : state_.player(player).hand) {
        if (locked_out) break;
        auto& card = state_.getObject(card_id);
        if (!card.isUnit()) continue;
        if (card.card_def_id == kInvalidId) continue;
        Card* card_obj = card_registry_.get(card.card_def_id);
        if (!card_obj || !card_obj->playableAsReactionToAttack()) continue;
        if (!canAfford(player, card_id)) continue;

        for (auto& bf : state_.battlefields) {
            if (bf.blocks_unit_play) continue;  // Rockfall Path
            if (!bf.combat_in_progress) continue;
            if (!bf.attacker.has_value() || *bf.attacker != player) continue;
            Intent pounce;
            pounce.type = IntentType::PlayReaction;
            pounce.player = player;
            pounce.card = card_id;
            pounce.play_location = BattlefieldLocation{bf.id};
            actions.push_back(pounce);
        }
    }

    // Can activate [E]: abilities with [Action] timing during showdowns (CR 806)
    for (auto& [id, obj] : state_.objects) {
        if (obj.controller != player) continue;
        if (!obj.location.has_value() && obj.zone != ZoneType::LegendZone) continue;

        Card* card = card_registry_.get(obj.card_def_id);
        if (!card) continue;
        auto abilities = card->activatedAbilities();
        for (size_t ai = 0; ai < abilities.size(); ++ai) {
            const auto& ab = abilities[ai];
            if (!ab.is_action) continue;
            if (ab.cost.exhaust && obj.is_exhausted) continue;

            auto legal_targets = card->enumerateLegalTargets(
                state_, player, static_cast<int>(ai));
            if (ab.targets.count > 0 && legal_targets.empty() &&
                !ab.targets.optional) continue;

            if (ab.needs_activation_time_target) {
                Intent activate;
                activate.type = IntentType::ActivateActionAbility;
                activate.player = player;
                activate.ability_source = id;
                activate.ability_index = static_cast<int>(ai);
                actions.push_back(activate);
                continue;
            }

            if (ab.targets.count == 0) {
                Intent activate;
                activate.type = IntentType::ActivateActionAbility;
                activate.player = player;
                activate.ability_source = id;
                activate.ability_index = static_cast<int>(ai);
                actions.push_back(activate);
            } else {
                for (auto target : legal_targets) {
                    Intent activate;
                    activate.type = IntentType::ActivateActionAbility;
                    activate.player = player;
                    activate.ability_source = id;
                    activate.ability_index = static_cast<int>(ai);
                    activate.targets = {target};
                    actions.push_back(activate);
                }
            }
        }
    }

    return actions;
}

std::vector<Intent> GameEngine::generateClosedStateActions(
    PlayerId player) const {
    std::vector<Intent> actions;

    // Can always pass priority
    actions.push_back(Intent::passPriority(player));

    const bool locked_out = state_.player(player).cant_play_cards_this_turn;

    // Can only play Reaction spells in Closed State (CR 309.1.a).
    // Brynhir lockout suppresses these.
    if (!locked_out) {
        generateSpellActions(player, /*action_ok=*/false, /*reaction_ok=*/true,
                              actions);
        generateTrashReplayActions(player, /*action_ok=*/false, /*reaction_ok=*/true,
                                    actions);
    }

    // Quick-Draw: play gear with [Quick-Draw] as Reactions targeting a friendly unit
    for (auto card_id : state_.player(player).hand) {
        if (locked_out) break;
        auto& card = state_.getObject(card_id);
        if (!card.isGear() || !card.hasKeyword(Keyword::QuickDraw)) continue;
        if (!canAfford(player, card_id)) continue;

        for (auto& [uid, unit] : state_.objects) {
            if (!unit.isUnit() || unit.controller != player) continue;
            if (!unit.location.has_value()) continue;
            Intent qd;
            qd.type = IntentType::PlayReaction;
            qd.player = player;
            qd.card = card_id;
            qd.targets = {uid}; // target unit to auto-attach
            actions.push_back(qd);
        }
    }

    // Ambush: play units with [Ambush] as Reactions to BFs where you have units
    for (auto card_id : state_.player(player).hand) {
        if (locked_out) break;
        auto& card = state_.getObject(card_id);
        if (!card.isUnit() || !card.hasKeyword(Keyword::Ambush)) continue;
        if (!canAfford(player, card_id)) continue;

        const Card* amb = card.card_def_id != kInvalidId
            ? card_registry_.get(card.card_def_id) : nullptr;
        const bool enemy_bf_ok = amb && amb->ambushToEnemyBattlefields();
        for (auto& bf : state_.battlefields) {
            if (bf.blocks_unit_play) continue;  // Rockfall Path
            bool here = bf.hasUnitsFrom(player, state_.objects);
            if (!here && enemy_bf_ok)
                here = bf.hasUnitsFrom(opponent(player), state_.objects);
            if (!here) continue;
            Intent ambush;
            ambush.type = IntentType::PlayReaction;
            ambush.player = player;
            ambush.card = card_id;
            ambush.play_location = BattlefieldLocation{bf.id};
            actions.push_back(ambush);
        }
    }

    // Reaction-to-attack plays during Closed State (Rengar, Pouncing).
    for (auto card_id : state_.player(player).hand) {
        if (locked_out) break;
        auto& card = state_.getObject(card_id);
        if (!card.isUnit()) continue;
        if (card.card_def_id == kInvalidId) continue;
        Card* card_obj = card_registry_.get(card.card_def_id);
        if (!card_obj || !card_obj->playableAsReactionToAttack()) continue;
        if (!canAfford(player, card_id)) continue;

        for (auto& bf : state_.battlefields) {
            if (bf.blocks_unit_play) continue;  // Rockfall Path
            if (!bf.combat_in_progress) continue;
            if (!bf.attacker.has_value() || *bf.attacker != player) continue;
            Intent pounce;
            pounce.type = IntentType::PlayReaction;
            pounce.player = player;
            pounce.card = card_id;
            pounce.play_location = BattlefieldLocation{bf.id};
            actions.push_back(pounce);
        }
    }

    // Can play hidden cards (they gain Reaction on next turn after hiding)
    // CR 811.1.d.2 — hidden spell targets must come from the SAME BF where
    // the card was hidden. Phase 6q+ fix: enumerate targets per-BF and emit
    // one intent per (hidden card × valid target).
    for (auto& bf : state_.battlefields) {
        for (auto card_id : bf.facedown) {
            if (!state_.objectExists(card_id)) continue;
            auto& card = state_.getObject(card_id);
            if (card.controller != player) continue;
            // Must be hidden on a previous turn to gain Reaction
            if (card.hidden_on_turn >= state_.turn.turn_number) continue;

            // For spells with target requirements: filter legal targets
            // to objects at the hidden-at BF only (CR 811.1.d.2).
            if (card.card_def_id != kInvalidId && card.isSpell()) {
                Card* card_obj = card_registry_.get(card.card_def_id);
                if (card_obj) {
                    auto req = card_obj->getTargetRequirements();
                    if (req.count > 0) {
                        auto all_targets = card_obj->enumerateLegalTargets(
                            state_, player);
                        std::vector<GameObjectId> bf_targets;
                        for (auto t : all_targets) {
                            if (!state_.objectExists(t)) continue;
                            const auto& tgt = state_.getObject(t);
                            auto tgt_bf = tgt.battlefieldId();
                            if (tgt_bf && *tgt_bf == bf.id) {
                                bf_targets.push_back(t);
                            }
                        }
                        // If no BF-restricted targets exist, CR 811.1.d:
                        // "A card cannot be played from Hidden if it is a
                        // spell with no valid targets under these
                        // restrictions." Skip this hidden play entirely.
                        if (bf_targets.empty()) continue;
                        for (auto t : bf_targets) {
                            Intent play;
                            play.type = IntentType::PlayReaction;
                            play.player = player;
                            play.card = card_id;
                            play.targets = {t};
                            actions.push_back(play);
                        }
                        continue;
                    }
                }
            }

            // No-target hidden play (or permanent — its location is set
            // to the hidden-at BF by executePlayFromHidden).
            Intent play;
            play.type = IntentType::PlayReaction;
            play.player = player;
            play.card = card_id;
            actions.push_back(play);
        }
    }

    // CR 376 / 398-406 — activated abilities with [Reaction] timing
    // (e.g. Shadow [752]: "[Action] [1][A], [E]: stun attacker") can be
    // activated during Closed State. Phase 6q+ fix: pre-fix this re-tagged
    // ALL activated abilities as Reaction, leaking non-Reaction abilities
    // (Bounty Hunter [E]) into the closed-state action set. Now filtered
    // via Card::isReactionAbility() — opt-in per card. Tag the emitted
    // intents as ActivateReactionAbility so the dispatcher handles
    // priority correctly.
    if (!locked_out) {
        for (auto& [id, obj] : state_.objects) {
            if (obj.controller != player) continue;
            if (!obj.location.has_value() && obj.zone != ZoneType::LegendZone) continue;
            if (obj.card_def_id == kInvalidId) continue;
            Card* card_obj = card_registry_.get(obj.card_def_id);
            if (!card_obj) continue;
            auto abilities = card_obj->activatedAbilities();
            for (size_t ai = 0; ai < abilities.size(); ++ai) {
                const auto& ab = abilities[ai];
                if (!ab.is_reaction) continue;
                if (ab.cost.exhaust && obj.is_exhausted) continue;
                int net_energy = std::max(0, ab.cost.energy -
                    card_obj->activationCostReduction(state_, player, (int)ai));
                if (net_energy > 0 && availableEnergy(player) < net_energy) continue;
                if (ab.cost.discard && ab.cost.discard_count > 0 &&
                    static_cast<int>(state_.player(player).hand.size()) <
                        ab.cost.discard_count) continue;
                if (ab.cost.xp_cost > 0 &&
                    state_.player(player).xp < ab.cost.xp_cost) continue;

                auto legal_targets = card_obj->enumerateLegalTargets(
                    state_, player, static_cast<int>(ai));
                if (ab.targets.count > 0 && legal_targets.empty() &&
                    !ab.targets.optional) continue;

                if (ab.needs_activation_time_target) {
                    Intent a;
                    a.type = IntentType::ActivateReactionAbility;
                    a.player = player;
                    a.ability_source = id;
                    a.ability_index = static_cast<int>(ai);
                    actions.push_back(a);
                    continue;
                }

                if (ab.targets.count == 0) {
                    Intent a;
                    a.type = IntentType::ActivateReactionAbility;
                    a.player = player;
                    a.ability_source = id;
                    a.ability_index = static_cast<int>(ai);
                    actions.push_back(a);
                } else {
                    for (auto t : legal_targets) {
                        Intent a;
                        a.type = IntentType::ActivateReactionAbility;
                        a.player = player;
                        a.ability_source = id;
                        a.ability_index = static_cast<int>(ai);
                        a.targets = {t};
                        actions.push_back(a);
                    }
                }
            }
        }
    }

    return actions;
}

void GameEngine::generateSpellActions(PlayerId player, bool action_ok,
                                       bool reaction_ok,
                                       std::vector<Intent>& actions) const {
    auto& ps = state_.player(player);
    // Lilting Lullaby / cards-lockout: zero spell-play actions while the
    // controller is under either lockout flag this turn.
    if (ps.cant_play_cards_this_turn || ps.cant_play_spells_this_turn) return;
    for (auto card_id : ps.hand) {
        auto& card = state_.getObject(card_id);
        if (!card.isSpell()) continue;
        if (!canAfford(player, card_id)) continue;

        // Check timing keywords
        bool has_action = card.keywords.has(Keyword::Action);
        bool has_reaction = card.keywords.has(Keyword::Reaction);

        // Reaction grants all Action permissions too (CR 813.1.b)
        if (has_reaction) has_action = true;

        // In Neutral Open, any spell can be played (no timing restriction)
        // In Showdown, need Action or Reaction
        // In Closed State, need Reaction only
        bool allowed = false;
        if (state_.turn.isNeutralOpen()) {
            allowed = true; // any spell
        } else if (state_.turn.isShowdownOpen()) {
            allowed = has_action || has_reaction;
        } else if (state_.turn.isClosedState()) {
            allowed = has_reaction;
            if (!reaction_ok) allowed = false;
        } else {
            if (action_ok && has_action) allowed = true;
            if (reaction_ok && has_reaction) allowed = true;
        }

        if (!allowed) continue;

        // Get legal targets for this spell via Card object
        Card* spell_card = card_registry_.get(card.card_def_id);

        // Universal legality gate (CR 700.x). Default impl preserves the
        // historical "needs target but none exist" behavior; counter spells
        // with no chain target override this so they're not offered as legal
        // moves when they'd silently no-op.
        if (spell_card && !spell_card->hasLegalTargets(state_, player)) {
            continue;
        }

        auto legal_targets = spell_card
            ? spell_card->enumerateLegalTargets(state_, player)
            : std::vector<GameObjectId>{};
        auto req = spell_card
            ? spell_card->getTargetRequirements()
            : TargetRequirements{};

        // Determine intent type based on context
        IntentType intent_type = IntentType::PlayCard;
        if (state_.turn.isShowdownOpen()) {
            intent_type = IntentType::PlayActionCard;
        } else if (state_.turn.isClosedState()) {
            intent_type = IntentType::PlayReaction;
        }

        // Repeat (CR 820) is now an in-play decision (see
        // executePlaySpell). The action generator emits one Play intent
        // per spell; after base cost is paid the engine polls the agent
        // yes/no for each affordable tranche. Pre-fix this loop emitted
        // one Play per repeat-count up to 6 — which produced 4+
        // visually-identical "Play Hard Bargain" entries in the legal
        // list and forced the agent to commit to N before knowing what
        // the counterspell would resolve against. CR 820 is the same
        // shape as Accelerate's "you may pay additional" — a yes/no at
        // cost time, not a pre-encoded vocab slot.
        auto emit = [&](Intent play, int /*R*/) {
            actions.push_back(play);
        };
        constexpr int kSingleEmit = 0;  // sentinel arg for emit() readability

        // Phase 6q — target decoupling. When the card opts in via
        // needsPlayTimeTarget() or needsPlayTimeTargetPair(), emit
        // ONE Play intent per card with NO targets. The card's
        // onResolve then uses Card::pickTarget / pickTargetPair to
        // publish follow-up MakeChoice intents keyed by target
        // card_def_id (Phase 5g int-coded MakeChoice slots). This
        // gives the AlphaZero policy head distinct vocab slots per
        // target choice instead of collapsing all (card, target)
        // variants (or the (friendly × enemy) Cartesian product for
        // dual-target spells like Challenge) into one Play slot.
        // hasLegalTargets() above already guarantees ≥1 legal target
        // exists at play time; pickTarget re-enumerates at resolve
        // time for CR-correctness against chain-driven state shifts.
        if (spell_card && req.count > 0 &&
            (spell_card->needsPlayTimeTarget() ||
             spell_card->needsPlayTimeTargetPair())) {
            Intent play;
            play.type = intent_type;
            play.player = player;
            play.card = card_id;
            // targets intentionally empty — agent picks via
            // pickTarget / pickTargetPair at resolve time.
            emit(play, kSingleEmit);
            continue;
        }

        if (req.count == 0) {
            // No targeting needed
            Intent play;
            play.type = intent_type;
            play.player = player;
            play.card = card_id;
            emit(play, kSingleEmit);
        } else if (req.optional && legal_targets.empty()) {
            // Optional targeting with no targets — play without targets
            Intent play;
            play.type = intent_type;
            play.player = player;
            play.card = card_id;
            emit(play, kSingleEmit);
        } else if (req.count == 2) {
            // Dual targeting (e.g., Challenge: friendly + enemy)
            // Separate targets into friendly and enemy
            std::vector<GameObjectId> friendly_targets, enemy_targets;
            for (auto tid : legal_targets) {
                auto& t = state_.getObject(tid);
                if (t.controller == player) friendly_targets.push_back(tid);
                else enemy_targets.push_back(tid);
            }
            // Generate all valid pairs
            for (auto ft : friendly_targets) {
                for (auto et : enemy_targets) {
                    Intent play;
                    play.type = intent_type;
                    play.player = player;
                    play.card = card_id;
                    play.targets = {ft, et};
                    emit(play, kSingleEmit);
                }
            }
        } else {
            // Generate one intent per legal target
            for (auto target : legal_targets) {
                Intent play;
                play.type = intent_type;
                play.player = player;
                play.card = card_id;
                play.targets = {target};
                emit(play, kSingleEmit);
            }
        }
    }
}

void GameEngine::generateTrashReplayActions(PlayerId player, bool action_ok,
                                             bool reaction_ok,
                                             std::vector<Intent>& actions) const {
    auto& ps = state_.player(player);
    if (ps.trash_replay_grants.empty()) return;
    if (ps.cant_play_cards_this_turn || ps.cant_play_spells_this_turn) return;

    for (const auto& grant : ps.trash_replay_grants) {
        if (grant.card == kInvalidId) continue;
        if (!state_.objectExists(grant.card)) continue;
        auto& card = state_.getObject(grant.card);
        if (card.zone != ZoneType::Trash) continue;   // grant valid only in trash
        if (!card.isSpell()) continue;

        // Timing gate — identical to generateSpellActions (CR 309/806/813).
        bool has_action = card.keywords.has(Keyword::Action);
        bool has_reaction = card.keywords.has(Keyword::Reaction);
        if (has_reaction) has_action = true;
        bool allowed = false;
        if (state_.turn.isNeutralOpen()) {
            allowed = true;
        } else if (state_.turn.isShowdownOpen()) {
            allowed = has_action || has_reaction;
        } else if (state_.turn.isClosedState()) {
            allowed = has_reaction && reaction_ok;
        } else {
            if (action_ok && has_action) allowed = true;
            if (reaction_ok && has_reaction) allowed = true;
        }
        if (!allowed) continue;

        // Affordability against the OVERRIDE cost, not the printed cost. For
        // an [A] grant, any single domain that's payable suffices.
        bool affordable = false;
        if (grant.any_domain) {
            for (int di = 0; di < static_cast<int>(Domain::Count); ++di) {
                if (canPayAdditionalCost(player, grant.energy, grant.power,
                                          static_cast<Domain>(di))) {
                    affordable = true;
                    break;
                }
            }
        } else {
            affordable = canPayAdditionalCost(player, grant.energy, grant.power,
                                               grant.power_domain);
        }
        if (!affordable) continue;

        Card* spell_card = card_registry_.get(card.card_def_id);
        if (spell_card && !spell_card->hasLegalTargets(state_, player)) continue;
        auto legal_targets = spell_card
            ? spell_card->enumerateLegalTargets(state_, player)
            : std::vector<GameObjectId>{};
        auto req = spell_card ? spell_card->getTargetRequirements()
                              : TargetRequirements{};

        IntentType intent_type = IntentType::PlayCard;
        if (state_.turn.isShowdownOpen()) intent_type = IntentType::PlayActionCard;
        else if (state_.turn.isClosedState()) intent_type = IntentType::PlayReaction;

        auto make = [&](std::vector<GameObjectId> tgts) {
            Intent play;
            play.type = intent_type;
            play.player = player;
            play.card = grant.card;
            play.play_source = Intent::PlaySource::Trash;
            play.targets = std::move(tgts);
            actions.push_back(play);
        };

        if (spell_card && req.count > 0 &&
            (spell_card->needsPlayTimeTarget() ||
             spell_card->needsPlayTimeTargetPair())) {
            make({});                                  // agent picks at resolve
        } else if (req.count == 0 || (req.optional && legal_targets.empty())) {
            make({});
        } else if (req.count == 2) {
            std::vector<GameObjectId> friendly, enemy;
            for (auto tid : legal_targets) {
                if (state_.getObject(tid).controller == player) friendly.push_back(tid);
                else enemy.push_back(tid);
            }
            for (auto ft : friendly)
                for (auto et : enemy) make({ft, et});
        } else {
            for (auto target : legal_targets) make({target});
        }
    }
}

void GameEngine::generateActivateAbilityActions(PlayerId player,
                                                  std::vector<Intent>& actions) const {
    // Phase 6r — multi-ability per Card. Loop over each ability descriptor
    // on the card; each gets its own affordability check and target loop.
    // Single-ability cards use the default Card::activatedAbilities() impl
    // which wraps the legacy single-ability virtuals into a one-element
    // vector — semantics unchanged for them.
    for (auto& [id, obj] : state_.objects) {
        if (obj.controller != player) continue;
        if (!obj.location.has_value() && obj.zone != ZoneType::LegendZone) continue;

        Card* card = card_registry_.get(obj.card_def_id);
        if (!card) continue;
        // Per-card "Use only if …" gate (Emperor of the Sands etc.).
        if (!card->canActivateAbility(state_, player)) continue;
        auto abilities = card->activatedAbilities();
        if (abilities.empty()) continue;

        for (size_t ai = 0; ai < abilities.size(); ++ai) {
            const auto& ab = abilities[ai];
            const auto& act_cost = ab.cost;

            // Check activation cost: must be ready if exhaust required
            if (act_cost.exhaust && obj.is_exhausted) continue;
            int net_energy = std::max(0, act_cost.energy -
                card->activationCostReduction(state_, player, (int)ai));
            if (net_energy > 0 && availableEnergy(player) < net_energy) continue;
            if (act_cost.discard && act_cost.discard_count > 0 &&
                static_cast<int>(state_.player(player).hand.size())
                    < act_cost.discard_count) continue;
            if (act_cost.xp_cost > 0 &&
                state_.player(player).xp < act_cost.xp_cost) continue;

            // Universal legality gate (CR 700.x). For multi-ability cards
            // the per-ability target requirements live on the descriptor;
            // ALWAYS consult ab.targets first. Only fall back to the
            // legacy Card::hasLegalTargets path for single-ability cards
            // that don't declare per-ability targets — e.g. counter
            // spells that gate on chain-item presence via their own
            // hasLegalTargets override. This fixes a bug where single-
            // ability cards with .targets.count>0 were being offered
            // even when no legal board targets existed (Bounty Hunter
            // at Turn 0 with no units on the board).
            if (ab.targets.count > 0 && !ab.targets.optional) {
                auto legal = card->enumerateLegalTargets(state_, player,
                                                          static_cast<int>(ai));
                if (legal.empty()) continue;
            } else if (abilities.size() == 1 &&
                        !card->hasLegalTargets(state_, player)) {
                continue;
            }

            // Phase 6q+ — if this ability defers target selection to
            // resolve-time via pickTarget (action-vocab collision fix),
            // emit ONE intent with empty targets regardless of count.
            if (ab.needs_activation_time_target) {
                Intent activate;
                activate.type = IntentType::ActivateAbility;
                activate.player = player;
                activate.ability_source = id;
                activate.ability_index = static_cast<int>(ai);
                actions.push_back(activate);
                continue;
            }

            if (ab.targets.count == 0) {
                Intent activate;
                activate.type = IntentType::ActivateAbility;
                activate.player = player;
                activate.ability_source = id;
                activate.ability_index = static_cast<int>(ai);
                actions.push_back(activate);
            } else {
                auto legal_targets = card->enumerateLegalTargets(
                    state_, player, static_cast<int>(ai));
                for (auto target : legal_targets) {
                    Intent activate;
                    activate.type = IntentType::ActivateAbility;
                    activate.player = player;
                    activate.ability_source = id;
                    activate.ability_index = static_cast<int>(ai);
                    activate.targets = {target};
                    actions.push_back(activate);
                }
            }
        }
    }
}

std::vector<Intent> GameEngine::generateCombatDamageActions(
    PlayerId player) const {
    std::vector<Intent> actions;
    // Phase 1: simplified — assign all damage to one unit at a time,
    // obeying lethal-before-next rule (CR 460.2.c.3)
    // For now, generate a single "spread evenly" assignment
    // TODO: Full damage assignment enumeration

    // Find the battlefield with active combat
    for (auto& bf : state_.battlefields) {
        if (!bf.combat_in_progress) continue;

        PlayerId enemy = opponent(player);
        auto enemy_units = state_.unitsAt(
            BattlefieldLocation{bf.id}, enemy);
        auto my_units = state_.unitsAt(
            BattlefieldLocation{bf.id}, player);

        if (enemy_units.empty() || my_units.empty()) continue;

        // Sum my might
        int total_might = 0;
        for (auto uid : my_units) {
            total_might += state_.getObject(uid).current_might;
        }

        // Generate one assignment: kill units in order until damage runs out
        std::vector<DamageAssignment> assignments;
        int remaining = total_might;
        for (auto target_id : enemy_units) {
            if (remaining <= 0) break;
            auto& target = state_.getObject(target_id);
            int lethal = target.current_might - target.damage_marked;
            if (lethal <= 0) continue;
            int assigned = std::min(remaining, lethal);
            assignments.push_back({target_id, assigned});
            remaining -= assigned;
        }
        // Dump remaining damage on last unit
        if (remaining > 0 && !assignments.empty()) {
            assignments.back().damage += remaining;
        }

        actions.push_back(Intent::assignCombatDamage(player, assignments));
        break;
    }

    return actions;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Combat and Showdown
// ═══════════════════════════════════════════════════════════════════════════════

void GameEngine::runShowdown(BattlefieldId bf_id) {
    auto& bf = getBattlefield(bf_id);
    events_.logTrace("SHOWDOWN: BF#" + std::to_string(bf_id) +
                     (bf.combat_staged ? " (combat)" : " (non-combat)"));

    bf.showdown_in_progress = true;
    state_.turn.ns_state = NeutralShowdownState::Showdown;
    state_.turn.oc_state = OpenClosedState::Open;

    events_.emit(ShowdownStartedEvent{bf_id, bf.combat_staged});

    // Showdown loop: players alternate focus, playing Action/Reaction spells.
    // Showdown closes when all players pass focus consecutively (CR 348.1).
    runShowdownLoop(bf_id);

    bf.showdown_in_progress = false;
    state_.turn.ns_state = NeutralShowdownState::Neutral;
    state_.turn.oc_state = OpenClosedState::Open;
    events_.emit(ShowdownEndedEvent{bf_id});

    // Non-combat showdown resolution: if only one player has units,
    // they establish control (CR 348.2.a)
    auto p1_units = state_.unitsAt(BattlefieldLocation{bf_id}, PlayerId::Player1);
    auto p2_units = state_.unitsAt(BattlefieldLocation{bf_id}, PlayerId::Player2);

    PlayerId sole_player = PlayerId::None;
    if (!p1_units.empty() && p2_units.empty()) sole_player = PlayerId::Player1;
    if (p1_units.empty() && !p2_units.empty()) sole_player = PlayerId::Player2;

    if (sole_player != PlayerId::None) {
        auto old_controller = bf.controller;
        bf.controller = sole_player;
        bf.is_contested = false;

        events_.emit(ControlChangedEvent{bf_id, old_controller, sole_player});

        // Conquer if not already scored this turn (CR 464.1)
        scoreConquer(sole_player, bf_id);
    }
}

// C-1 commit 7 — showdown loop split into advance/resolve halves.
// Behavior is identical to the prior monolithic body; the split lets
// the future step machine (commit 9) call the halves directly without
// going through queryAgent.

namespace {
constexpr int kMaxShowdownActions = 100; // safety cap
}  // namespace

GameEngine::ShowdownAdvance GameEngine::advanceShowdown(BattlefieldId bf_id,
                                                          PlayerId focus_holder,
                                                          int& action_count) {
    ShowdownAdvance adv;
    if (action_count >= kMaxShowdownActions || state_.game_over) {
        adv.kind = ShowdownAdvance::Kind::Done;
        return adv;
    }
    state_.turn.focus_holder = focus_holder;
    state_.turn.priority_holder = focus_holder;
    state_.turn.oc_state = OpenClosedState::Open;

    events_.emit(PriorityGrantedEvent{focus_holder, true});

    adv.kind = ShowdownAdvance::Kind::NeedDecision;
    adv.focus_holder = focus_holder;
    adv.legal = generateShowdownActions(focus_holder);
    (void)bf_id;  // bf only matters for the caller's later steps
    return adv;
}

std::optional<PlayerId> GameEngine::resolveShowdownDecision(
    BattlefieldId /*bf_id*/, const Intent& chosen, PlayerId current_focus,
    int& action_count) {
    action_count++;

    if (chosen.type == IntentType::PassFocus) {
        state_.turn.players_passed_focus.insert(current_focus);

        // Check if all players passed focus
        bool all_passed = true;
        for (auto pid : {PlayerId::Player1, PlayerId::Player2}) {
            if (state_.turn.players_passed_focus.find(pid) ==
                state_.turn.players_passed_focus.end()) {
                all_passed = false;
                break;
            }
        }

        if (all_passed) return std::nullopt;  // showdown closes
        return opponent(current_focus);
    }
    // CR 806 + 813 + 819 + 822 — every action available during a
    // showdown should be routed through executeIntent. Pre-2026-05-19
    // engine-audit CRITICAL #3 fix: the switch only handled
    // PlayActionCard. Ambush units (PlayActionCard with play_location)
    // would route here correctly because they use PlayActionCard, BUT
    // Pouncing units use PlayReaction, Quick-Draw gear uses
    // PlayReaction, and activated abilities with [Action] timing use
    // ActivateActionAbility. All three silently no-op'd until the
    // 100-action safety cap fired.
    //
    // The same "play a thing, reset passes, focus to opponent" shape
    // applies to all of these. We dispatch via executeIntent (which
    // already routes each type to the correct executor) and reset
    // focus passes.
    switch (chosen.type) {
        case IntentType::PlayActionCard:     // Action spells + Ambush units
        case IntentType::PlayReaction:       // Pouncing units, Quick-Draw gear, Reaction spells in showdown
        case IntentType::PlayCard:           // generic play (safety)
        case IntentType::ActivateActionAbility: // [Action] [E]: abilities
        case IntentType::ActivateReactionAbility: // [Reaction] [E]: abilities
        case IntentType::ActivateAbility: {  // generic activate (safety)
            state_.turn.players_passed_focus.clear();  // reset passes
            executeIntent(chosen);
            cleanup();
            // After chain resolves, focus passes to opponent
            return opponent(current_focus);
        }
        default:
            // Any other intent type — fall through with focus unchanged
            // so the outer loop can try again. Mirrors the legacy body's
            // permissive handling of unexpected intents.
            return current_focus;
    }
}

void GameEngine::runShowdownLoop(BattlefieldId bf_id) {
    // Focus passing loop (CR 313, CR 348.1).
    // Turn player gets focus first. Players alternate.
    // When both pass focus consecutively without adding chain items, showdown closes.
    //
    // Bridge: legacy path calls queryAgent between advance + resolve.
    // The step-machine path (C-1 commit 9) will surface advance's legal
    // actions out to applyChoice() and feed the chosen intent back into
    // resolve — no thread, no condvar.
    state_.turn.players_passed_focus.clear();
    PlayerId current_focus = state_.turn.turn_player;

    int action_count = 0;
    while (action_count < kMaxShowdownActions && !state_.game_over) {
        auto adv = advanceShowdown(bf_id, current_focus, action_count);
        if (adv.kind == ShowdownAdvance::Kind::Done) return;

        state_.decision_index++;
        auto chosen = getAgent(current_focus).selectAction(state_, adv.legal);
        recordAppliedIntent(chosen);
        if (on_decision) {
            on_decision(state_, adv.legal, chosen);
        }

        auto next = resolveShowdownDecision(bf_id, chosen, current_focus, action_count);
        if (!next.has_value()) return;
        current_focus = *next;
    }
}

void GameEngine::runCombat(BattlefieldId bf_id) {
    auto& bf = getBattlefield(bf_id);
    bf.combat_in_progress = true;

    // Determine attacker/defender (CR 459.2.b)
    bf.attacker = bf.contested_by;
    bf.defender = opponent(bf.contested_by);
    events_.logTrace("COMBAT: BF#" + std::to_string(bf_id) + " attacker=" +
                     toString(*bf.attacker) + " defender=" + toString(*bf.defender));

    // Assign combat designations to units
    for (auto& [id, obj] : state_.objects) {
        if (!obj.isUnit() || !obj.isAtBattlefield()) continue;
        auto unit_bf = obj.battlefieldId();
        if (!unit_bf || *unit_bf != bf_id) continue;

        if (obj.controller == *bf.attacker) {
            obj.combat_designation = CombatDesignation::Attacker;
        } else if (obj.controller == *bf.defender) {
            obj.combat_designation = CombatDesignation::Defender;
        }
    }

    // Recompute might (assault/shield apply based on designation)
    for (auto& [id, obj] : state_.objects) {
        if (obj.isUnit() && obj.combat_designation != CombatDesignation::None) {
            obj.recomputeMight();
        }
    }

    events_.emit(CombatStartedEvent{bf_id, *bf.attacker, *bf.defender});

    // Drain any WhenIAttack / WhenIDefend / WhenIAttackOrDefend triggers
    // that the CombatStartedEvent dispatched to the chain (CR 459.2.d).
    // Pre-2026-05-19 engine-audit CRITICAL #2: addAbility put these on
    // the chain but nothing called runChain() — they sat until a
    // showdown spell happened to be played, OR until the post-combat
    // mainPhase resumed. Effects intended to fire BEFORE combat damage
    // (kill an attacker, bounce a defender) were silently delayed.
    //
    // Process FEPR now so the triggers (and any Reactions players have
    // for them) resolve fully before we enter the showdown decision
    // loop. Players still get full priority windows because
    // processFEPR handles Closed State priority passing internally.
    if (!state_.chain.items.empty()) {
        runChain();
    }

    // Re-check combat viability AFTER triggers resolve — kills or
    // bounces in trigger resolution may have emptied one side.
    {
        auto att_after_trig = state_.unitsAt(BattlefieldLocation{bf_id}, *bf.attacker);
        auto def_after_trig = state_.unitsAt(BattlefieldLocation{bf_id}, *bf.defender);
        if (att_after_trig.empty() || def_after_trig.empty()) {
            bf.combat_phase = CombatPhase::ResolutionStep;
            combatResolutionStep(bf_id);
            return;
        }
    }

    // Step 1: Combat Showdown — players can play Action/Reaction spells
    bf.combat_phase = CombatPhase::ShowdownStep;
    runShowdownLoop(bf_id);

    // After showdown: check if combat can continue (CR 460.1)
    // Damage step only occurs if both sides still have units
    auto att_check = state_.unitsAt(BattlefieldLocation{bf_id}, *bf.attacker);
    auto def_check = state_.unitsAt(BattlefieldLocation{bf_id}, *bf.defender);

    if (att_check.empty() || def_check.empty()) {
        // One side was eliminated during showdown — skip damage, go to resolution
        bf.combat_phase = CombatPhase::ResolutionStep;
        combatResolutionStep(bf_id);
        return;
    }

    // Step 2: Combat Damage
    bf.combat_phase = CombatPhase::DamageStep;
    combatDamageStep(bf_id);

    // Step 3: Resolution
    bf.combat_phase = CombatPhase::ResolutionStep;
    combatResolutionStep(bf_id);
}

void GameEngine::combatDamageStep(BattlefieldId bf_id) {
    auto& bf = getBattlefield(bf_id);

    auto att_units = state_.unitsAt(BattlefieldLocation{bf_id}, *bf.attacker);
    auto def_units = state_.unitsAt(BattlefieldLocation{bf_id}, *bf.defender);

    if (att_units.empty() || def_units.empty()) return;

    {
        std::string att_str, def_str;
        for (auto uid : att_units) {
            auto& u = state_.getObject(uid);
            if (!att_str.empty()) att_str += ", ";
            att_str += u.name + "(" + std::to_string(u.current_might) + "M" +
                       (u.is_stunned ? ",STUNNED" : "") + ")";
        }
        for (auto uid : def_units) {
            auto& u = state_.getObject(uid);
            if (!def_str.empty()) def_str += ", ";
            def_str += u.name + "(" + std::to_string(u.current_might) + "M" +
                       (u.is_stunned ? ",STUNNED" : "") + ")";
        }
        events_.logTrace("DAMAGE_STEP: BF#" + std::to_string(bf_id) +
                         " ATK=[" + att_str + "] DEF=[" + def_str + "]");
    }

    // Reorder targets: Tank first, Backline last (CR 460.2.c)
    auto sortByTankBackline = [&](std::vector<GameObjectId>& units) {
        std::stable_sort(units.begin(), units.end(),
            [&](GameObjectId a, GameObjectId b) {
                auto& oa = state_.getObject(a);
                auto& ob = state_.getObject(b);
                bool a_tank = oa.hasKeyword(Keyword::Tank);
                bool b_tank = ob.hasKeyword(Keyword::Tank);
                bool a_back = oa.hasKeyword(Keyword::Backline);
                bool b_back = ob.hasKeyword(Keyword::Backline);
                // Tank before non-Tank, non-Backline before Backline
                if (a_tank != b_tank) return a_tank > b_tank;
                if (a_back != b_back) return a_back < b_back;
                return false;
            });
    };
    sortByTankBackline(att_units);
    sortByTankBackline(def_units);

    // Sum attacker might (CR 460.2.a) — stunned units contribute 0 (CR 423)
    int att_might = 0;
    for (auto uid : att_units) {
        auto& u = state_.getObject(uid);
        if (!u.dealsNoCombatDamage()) att_might += u.current_might;
    }

    // Sum defender might (CR 460.2.b)
    int def_might = 0;
    for (auto uid : def_units) {
        auto& u = state_.getObject(uid);
        if (!u.dealsNoCombatDamage()) def_might += u.current_might;
    }

    // Both players assign damage to opponent's units (CR 460.2.c).
    // Tank must be assigned lethal before non-Tank. Backline assigned last.
    // Agent chooses distribution within those constraints.
    //
    // Bridge: legacy path calls queryAgent between advance + resolve.
    // The step-machine path (C-1 commit 9) will surface advance's legal
    // options out to applyChoice() and feed the chosen Intent back into
    // resolve.
    auto runOneSide = [&](PlayerId assigner, int total_damage,
                          const std::vector<GameObjectId>& targets) {
        auto adv = advanceCombatDamage(assigner, total_damage, targets);
        if (adv.kind == CombatDamageAdvance::Kind::Skip) return;
        state_.decision_index++;
        auto chosen = getAgent(assigner).selectAction(state_, adv.legal);
        recordAppliedIntent(chosen);
        if (on_decision) {
            on_decision(state_, adv.legal, chosen);
        }
        resolveCombatDamageDecision(chosen);
    };

    // Attacker assigns damage to defender's units, defender assigns to attacker's
    runOneSide(*bf.attacker, att_might, def_units);
    runOneSide(*bf.defender, def_might, att_units);
}

GameEngine::CombatDamageAdvance GameEngine::testHook_advanceCombatDamage(
    PlayerId assigner, int total_damage,
    const std::vector<GameObjectId>& targets) {
    return advanceCombatDamage(assigner, total_damage, targets);
}

GameEngine::CombatDamageAdvance GameEngine::advanceCombatDamage(
    PlayerId assigner, int total_damage,
    const std::vector<GameObjectId>& targets) {

    CombatDamageAdvance adv;
    adv.assigner = assigner;
    if (total_damage <= 0 || targets.empty()) {
        adv.kind = CombatDamageAdvance::Kind::Skip;
        return adv;
    }

    // Build default assignment (greedy lethal, respecting caller's
    // Tank/Backline ordering on `targets`).
    auto buildDefault = [&]() -> std::vector<DamageAssignment> {
        std::vector<DamageAssignment> da;
        int remaining = total_damage;
        for (auto tid : targets) {
            if (remaining <= 0) break;
            auto& t = state_.getObject(tid);
            int lethal = std::max(1, t.current_might - t.damage_marked);
            int assigned = std::min(remaining, lethal);
            da.push_back({tid, assigned});
            remaining -= assigned;
        }
        if (remaining > 0 && !da.empty()) {
            da.back().damage += remaining;
        }
        return da;
    };

    // CR 460.2.c.3 (strict lethality) + 460.2.c.4 (no over-assign) + CR 815
    // (Tank first) + CR 826 (Backline last): the only CR-legal damage
    // allocations are greedy-lethal cascades, varying only by the WITHIN-
    // bucket order of targets (CR 460.2.c.6 — any order within tied
    // priority). We partition into Tank / non-Tank-non-Backline / Backline
    // buckets, enumerate permutations within each, concatenate, and run
    // greedy-lethal on every ordering. Identical resulting allocations
    // are deduped.
    std::vector<GameObjectId> tanks, mids, backs;
    for (auto tid : targets) {
        auto& obj = state_.getObject(tid);
        if (obj.hasKeyword(Keyword::Tank))         tanks.push_back(tid);
        else if (obj.hasKeyword(Keyword::Backline)) backs.push_back(tid);
        else                                        mids.push_back(tid);
    }

    auto greedyCascade = [&](const std::vector<GameObjectId>& order)
        -> std::vector<DamageAssignment> {
        std::vector<DamageAssignment> da;
        int remaining = total_damage;
        for (auto tid : order) {
            if (remaining <= 0) break;
            auto& t = state_.getObject(tid);
            int lethal = std::max(1, t.current_might - t.damage_marked);
            int assigned = std::min(remaining, lethal);
            da.push_back({tid, assigned});
            remaining -= assigned;
        }
        if (remaining > 0 && !da.empty()) {
            da.back().damage += remaining;
        }
        return da;
    };

    auto canonicalKey = [](const std::vector<DamageAssignment>& da) {
        // Order-independent key: sort by (target, damage) so two cascades
        // that produce the same FINAL marked-damage pattern dedupe to one.
        std::vector<std::pair<GameObjectId,int>> kv;
        for (auto& a : da) if (a.damage > 0) kv.emplace_back(a.target_unit, a.damage);
        std::sort(kv.begin(), kv.end());
        std::string s;
        for (auto& [t, d] : kv) {
            s += std::to_string(t); s += ':'; s += std::to_string(d); s += ',';
        }
        return s;
    };

    std::vector<Intent>& options = adv.legal;
    std::set<std::string> seen;

    auto emitIfNew = [&](std::vector<DamageAssignment>&& da) {
        auto key = canonicalKey(da);
        if (seen.insert(key).second) {
            options.push_back(Intent::assignCombatDamage(assigner, std::move(da)));
        }
    };

    // Enumerate permutations within each bucket, then concatenate.
    // std::next_permutation requires sorted input to traverse all
    // permutations. For small bucket sizes (typical combat: 0–4 units
    // per bucket) the permutation counts stay tiny (n! with n≤4 → ≤24).
    auto sortedTanks = tanks, sortedMids = mids, sortedBacks = backs;
    std::sort(sortedTanks.begin(), sortedTanks.end());
    std::sort(sortedMids.begin(), sortedMids.end());
    std::sort(sortedBacks.begin(), sortedBacks.end());

    auto tanks_perm = sortedTanks;
    do {
        auto mids_perm = sortedMids;
        do {
            auto backs_perm = sortedBacks;
            do {
                std::vector<GameObjectId> ordered;
                ordered.insert(ordered.end(), tanks_perm.begin(), tanks_perm.end());
                ordered.insert(ordered.end(), mids_perm.begin(), mids_perm.end());
                ordered.insert(ordered.end(), backs_perm.begin(), backs_perm.end());
                emitIfNew(greedyCascade(ordered));
            } while (std::next_permutation(backs_perm.begin(), backs_perm.end()));
        } while (std::next_permutation(mids_perm.begin(), mids_perm.end()));
    } while (std::next_permutation(tanks_perm.begin(), tanks_perm.end()));

    adv.kind = CombatDamageAdvance::Kind::NeedDecision;
    return adv;
}

void GameEngine::resolveCombatDamageDecision(const Intent& chosen) {
    int total_damage = 0;
    for (auto& da : chosen.damage_assignments) total_damage += da.damage;

    events_.logTrace(std::string("ASSIGN_DAMAGE: ") + toString(chosen.player) +
                     " assigns " + std::to_string(total_damage) + " damage:");
    for (auto& da : chosen.damage_assignments) {
        if (da.damage > 0) {
            auto& t = state_.getObject(da.target_unit);
            events_.logTrace("  -> " + t.name + " takes " +
                             std::to_string(da.damage) + " (was " +
                             std::to_string(t.damage_marked) + "/" +
                             std::to_string(t.current_might) + "M)");
            t.damage_marked += da.damage;
            events_.emit(DamageDealtEvent{da.target_unit, da.damage, kInvalidId, true});
        }
    }
}

void GameEngine::combatResolutionStep(BattlefieldId bf_id) {
    auto& bf = getBattlefield(bf_id);

    // Combat Cleanup: heal all units, kill lethally damaged (CR 461.1.a)
    processLethalDamage();

    // Heal surviving units (CR 461.1.a.1)
    for (auto& [id, obj] : state_.objects) {
        if (obj.isUnit() && obj.isAtBattlefield()) {
            auto unit_bf = obj.battlefieldId();
            if (unit_bf && *unit_bf == bf_id) {
                obj.damage_marked = 0;
            }
        }
    }

    // Recall attackers if defenders still present (CR 461.1.a.2)
    auto att_remaining = state_.unitsAt(BattlefieldLocation{bf_id}, *bf.attacker);
    auto def_remaining = state_.unitsAt(BattlefieldLocation{bf_id}, *bf.defender);

    if (!att_remaining.empty() && !def_remaining.empty()) {
        // Both still have units = a tie. Normally only attackers recall
        // (CR 461.1.a.2). Symbol of the Solari (227): if the attacker controls
        // it, recall ALL units (attackers AND defenders) instead.
        const bool recall_all = state_.player(*bf.attacker).recall_all_on_attacker_tie;
        for (auto uid : att_remaining) {
            moveUnit(uid, BaseLocation{*bf.attacker});
            events_.emit(UnitMovedEvent{uid, *bf.attacker,
                BattlefieldLocation{bf_id}, BaseLocation{*bf.attacker}, false});
        }
        if (recall_all) {
            for (auto uid : def_remaining) {
                moveUnit(uid, BaseLocation{*bf.defender});
                events_.emit(UnitMovedEvent{uid, *bf.defender,
                    BattlefieldLocation{bf_id}, BaseLocation{*bf.defender}, false});
            }
            events_.logTrace("SYMBOL OF THE SOLARI: attacker tie -> recall ALL units");
        }
    }

    // Determine combat result (CR 461.3)
    att_remaining = state_.unitsAt(BattlefieldLocation{bf_id}, *bf.attacker);
    def_remaining = state_.unitsAt(BattlefieldLocation{bf_id}, *bf.defender);

    PlayerId combat_winner = PlayerId::None;
    if (!att_remaining.empty() && def_remaining.empty()) {
        combat_winner = *bf.attacker;
    } else if (att_remaining.empty() && !def_remaining.empty()) {
        combat_winner = *bf.defender;
    }

    // Clear combat designations
    for (auto& [id, obj] : state_.objects) {
        if (obj.combat_designation != CombatDesignation::None) {
            obj.combat_designation = CombatDesignation::None;
            obj.recomputeMight();
        }
    }

    // End combat
    bf.combat_in_progress = false;
    bf.combat_phase = CombatPhase::None;
    bf.is_contested = false;
    bf.attacker = std::nullopt;
    bf.defender = std::nullopt;

    events_.emit(CombatEndedEvent{bf_id, combat_winner});

    // Establish control (CR 461.5)
    if (combat_winner != PlayerId::None) {
        auto old_controller = bf.controller;
        bf.controller = combat_winner;
        events_.emit(ControlChangedEvent{bf_id, old_controller, combat_winner});
        scoreConquer(combat_winner, bf_id);
    } else if (att_remaining.empty() && def_remaining.empty()) {
        bf.controller = std::nullopt;
    }

    state_.turn.ns_state = NeutralShowdownState::Neutral;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Scoring
// ═══════════════════════════════════════════════════════════════════════════════

bool GameEngine::isScoreGatedByTurn(PlayerId player, BattlefieldId bf) const {
    const auto& b = getBattlefield(bf);
    if (b.min_turn_to_score <= 0) return false;
    return state_.player(player).turns_taken < b.min_turn_to_score;
}

void GameEngine::scoreConquer(PlayerId player, BattlefieldId bf) {
    auto& ps = state_.player(player);

    // Stamp the conquer for "battlefield you conquered this turn" consumers
    // (Perched Grimwyrm). Reaching here means `player` conquered `bf`, even if
    // the point-gain below is blocked/gated.
    {
        auto& bfs = getBattlefield(bf);
        bfs.conquered_on_turn = state_.turn.turn_number;
        bfs.conquered_by_player = player;
    }

    // Continuous "can't gain points" lock (Tianna Crownguard, set via aura).
    if (ps.cannot_gain_points) {
        events_.logTrace(std::string("SCORE_BLOCKED: ") + toString(player) +
                         " can't gain points (Tianna)");
        return;
    }

    // Turn-gated scoring (Forgotten Monument): can't score here until the
    // player has taken `min_turn_to_score` of their own turns.
    if (isScoreGatedByTurn(player, bf)) {
        events_.logTrace(std::string("SCORE_GATED: ") + toString(player) +
                         " can't score BF#" + std::to_string(bf) + " until turn " +
                         std::to_string(getBattlefield(bf).min_turn_to_score) +
                         " (turns_taken=" + std::to_string(ps.turns_taken) + ")");
        return;
    }

    // Can only score once per BF per turn (CR 465)
    if (ps.battlefields_scored_this_turn.count(bf)) return;

    ps.battlefields_scored_this_turn.insert(bf);
    events_.logTrace(std::string("CONQUER: ") + toString(player) + " conquers BF#" +
                     std::to_string(bf) + " (score was " + std::to_string(ps.score) + ")");

    if (isWinningPointAttempt(player)) {
        if (canGainWinningPointViaConquer(player)) {
            ps.score++;
            events_.logTrace(std::string("SCORE: ") + toString(player) + " +1 via Conquer -> " +
                             std::to_string(ps.score));
            events_.emit(ScoreEvent{player, bf, ScoreMethod::Conquer, ps.score});
        } else {
            events_.logTrace(std::string("SCORE_BLOCKED: ") + toString(player) +
                             " winning point blocked (not all BFs scored), draw 1 instead");
            // Draw a card instead (CR 466.1.b.2)
            drawCards(player, 1);
        }
    } else {
        ps.score++;
        events_.logTrace(std::string("SCORE: ") + toString(player) + " +1 via Conquer -> " +
                         std::to_string(ps.score));
        events_.emit(ScoreEvent{player, bf, ScoreMethod::Conquer, ps.score});
    }

    checkWinCondition();
}

void GameEngine::scoreHold(PlayerId player, BattlefieldId bf) {
    auto& ps = state_.player(player);

    // Continuous "can't gain points" lock (Tianna Crownguard, set via aura).
    if (ps.cannot_gain_points) {
        events_.logTrace(std::string("SCORE_BLOCKED: ") + toString(player) +
                         " can't gain points (Tianna)");
        return;
    }

    if (isScoreGatedByTurn(player, bf)) {
        events_.logTrace(std::string("SCORE_GATED: ") + toString(player) +
                         " can't score BF#" + std::to_string(bf) + " until turn " +
                         std::to_string(getBattlefield(bf).min_turn_to_score) +
                         " (turns_taken=" + std::to_string(ps.turns_taken) + ")");
        return;
    }

    if (ps.battlefields_scored_this_turn.count(bf)) return;
    ps.battlefields_scored_this_turn.insert(bf);

    // Hold always grants Winning Point (CR 466.1.b.1)
    ps.score++;
    ps.hold_points_this_turn++;
    events_.logTrace(std::string("SCORE: ") + toString(player) + " +1 via Hold BF#" +
                     std::to_string(bf) + " -> " + std::to_string(ps.score));
    events_.emit(ScoreEvent{player, bf, ScoreMethod::Hold, ps.score});

    checkWinCondition();
}

bool GameEngine::isWinningPointAttempt(PlayerId player) const {
    return state_.player(player).score >= state_.mode.victory_score - 1;
}

bool GameEngine::canGainWinningPointViaConquer(PlayerId player) const {
    // Must have scored every battlefield this turn (CR 466.1.b.2)
    return state_.player(player).battlefields_scored_this_turn.size() >=
           state_.totalBattlefields();
}

// ═══════════════════════════════════════════════════════════════════════════════
// Aura recalculation (Phase 5b — tagged effects)
// ═══════════════════════════════════════════════════════════════════════════════

void GameEngine::recalculateAuras() {
    // Step 1: Clear all aura effects from all objects
    for (auto& [id, obj] : state_.objects) {
        obj.aura_effects.clear();
        obj.aura_might_bonus = 0;
        obj.aura_keywords.reset();
        obj.aura_bonus_damage_taken = 0;
        obj.immune_to_damage = false;  // re-asserted below by applyPassiveAura
        obj.spells_targeting_me_cost_reduction = 0;  // Irelia, Graceful
        obj.granted_abilities.clear();               // Forge / Gardens / Heimerdinger
    }

    // Step 1a: Reset per-player passive counters derived from on-board
    // presence. Bumped below by the corresponding aura scan.
    for (auto pid : {PlayerId::Player1, PlayerId::Player2}) {
        auto& ps = state_.player(pid);
        ps.deathknell_double_count = 0;
        ps.bonus_damage_dealt = 0;   // Annie, Fiery
        ps.cannot_gain_points = false; // Tianna Crownguard
        ps.grant_friendly_units_open_bf = false; // Miss Fortune, Buccaneer
        ps.units_play_base_only = false;         // Mageseeker Warden
        ps.tokens_enter_ready = false;           // Renata Glasc, Industrialist
        ps.recall_all_on_attacker_tie = false;   // Symbol of the Solari
        ps.effects_cant_ready_my_units = false;  // Mageseeker Warden cl2
        ps.spells_have_repeat_energy = 0;        // Syndra, Transcendent
        ps.spells_have_repeat_power = 0;
        ps.repeat_cost_reduction = 0;            // Marai Spire
        ps.has_reveal_peek = false;              // Void Hatchling
    }
    // Per-BF aura-derived flags (Mageseeker Investigator / Noxus Saboteur /
    // Altar of Blood). Reset here; re-asserted by unit/BF applyPassiveAura.
    for (auto& bf : state_.battlefields) {
        bf.surcharge_enemy_multi_move = false;
        bf.opp_hidden_unrevealable = false;
        bf.death_recall_for_pay = false;
    }

    // Step 1b: Refresh per-object targeting-protection flags from each
    // card's canBeChosenByEnemy() override. Cards on board need their
    // flag set; off-board cards stay default-false. Cheap O(n) pass over
    // GameState::objects; collocated with aura recalc so all "static
    // attributes of on-board cards" get refreshed in one place.
    for (auto& [id, obj] : state_.objects) {
        obj.untargetable_by_enemy = false;
        // On-board cards AND legends (LegendZone, no location) broadcast their
        // passive auras. Without including legends, legend-sourced passives
        // (Purifier's [Assault], Wuju Master's +1 [M], etc.) never fired.
        bool on_board = obj.location.has_value();
        bool legend_zone = obj.zone == ZoneType::LegendZone;
        if (!on_board && !legend_zone) continue;
        if (obj.card_def_id == kInvalidId) continue;
        const Card* card = card_registry_.get(obj.card_def_id);
        if (on_board && card && !card->canBeChosenByEnemy()) {
            obj.untargetable_by_enemy = true;
        }

        // Card-defined passive auras — each on-board card (or legend) gets a
        // chance to broadcast its passive effect (Karthus bumps the
        // controller's deathknell_double_count, future cards add their own).
        if (card) {
            card->applyPassiveAura(state_, obj.controller);
        }
    }

    // Step 1c: Battlefield-card passive auras. BF card objects have no
    // `location` (so the loop above skips them) but can carry static auras
    // (Forbidding Waste, Black Flame Altar, Ornn's Forge). Apply each,
    // attributed to the BF's controller (None if uncontrolled — the card's
    // own logic scopes by control where it matters).
    for (const auto& bf : state_.battlefields) {
        if (!state_.objectExists(bf.card_object_id)) continue;
        const auto& bobj = state_.getObject(bf.card_object_id);
        if (bobj.card_def_id == kInvalidId) continue;
        const Card* card = card_registry_.get(bobj.card_def_id);
        if (card) {
            card->applyPassiveAura(state_, bf.controller.value_or(PlayerId::None));
        }
    }

    // Step 2: Scan all objects on board for aura abilities
    for (auto& [src_id, src] : state_.objects) {
        if (!src.location.has_value() && src.zone != ZoneType::LegendZone) continue;
        if (src.card_def_id == kInvalidId) continue; // skip tokens without CardDef

        const auto& def = card_db_.get(src.card_def_id);
        auto& text = def.ability_text;
        if (text.empty()) continue;

        // Parse aura from ability_text (lowercase matching)
        std::string lower;
        lower.reserve(text.size());
        for (char c : text) lower += std::tolower(c);

        // Strip parenthetical for matching
        std::string clean;
        int depth = 0;
        for (char c : lower) {
            if (c == '(') depth++;
            else if (c == ')') { if (depth > 0) depth--; }
            else if (depth == 0) clean += c;
        }

        // Determine aura scope and effect
        struct AuraDef {
            bool is_keyword = false;
            Keyword keyword = Keyword::Count;
            int keyword_value = 0;
            int might_mod = 0;
            int might_min = 0;
            bool friendly = false;
            bool enemy = false;
            bool here_only = false;    // "here" = same battlefield as source
            bool other_only = false;   // "other" = excludes self
            bool all_friendly = false; // all friendly units anywhere
            bool tag_filter = false;
            std::string required_tag;
            bool stunned_only = false;
        };

        std::vector<AuraDef> auras;

        // "Other friendly units here have [Assault]"
        auto findAura = [&](const std::string& pattern, AuraDef base) -> bool {
            auto pos = clean.find(pattern);
            if (pos == std::string::npos) return false;
            auto clause = clean.substr(pos, 80);

            // Determine keyword or might
            if (clause.find("[assault]") != std::string::npos) {
                base.is_keyword = true; base.keyword = Keyword::Assault; base.keyword_value = 1;
            } else if (clause.find("[shield]") != std::string::npos) {
                base.is_keyword = true; base.keyword = Keyword::Shield; base.keyword_value = 1;
            } else if (clause.find("[ganking]") != std::string::npos) {
                base.is_keyword = true; base.keyword = Keyword::Ganking;
            } else if (clause.find("[vision]") != std::string::npos) {
                base.is_keyword = true; base.keyword = Keyword::Vision;
            } else if (clause.find("[deflect]") != std::string::npos) {
                base.is_keyword = true; base.keyword = Keyword::Deflect; base.keyword_value = 1;
            } else if (clause.find("+") != std::string::npos && clause.find("[m]") != std::string::npos) {
                // Extract +N
                auto plus = clause.find('+');
                int val = 0;
                for (size_t i = plus + 1; i < clause.size() && std::isdigit(clause[i]); ++i)
                    val = val * 10 + (clause[i] - '0');
                if (val == 0) val = 1;
                base.might_mod = val;
            } else if (clause.find("-") != std::string::npos && clause.find("[m]") != std::string::npos) {
                auto minus = clause.find('-');
                int val = 0;
                for (size_t i = minus + 1; i < clause.size() && std::isdigit(clause[i]); ++i)
                    val = val * 10 + (clause[i] - '0');
                if (val == 0) val = 1;
                base.might_mod = -val;
                // Check for minimum
                auto min_pos = clause.find("minimum of");
                if (min_pos != std::string::npos) {
                    int m = 0;
                    for (size_t i = min_pos + 10; i < clause.size() && std::isdigit(clause[i]); ++i)
                        m = m * 10 + (clause[i] - '0');
                    if (m == 0) m = 1;
                    base.might_min = m;
                }
            } else {
                return false; // unrecognized aura effect
            }

            auras.push_back(base);
            return true;
        };

        // Match patterns (most specific first)
        AuraDef d;

        // "Other friendly units here have ..."
        d = {}; d.friendly = true; d.here_only = true; d.other_only = true;
        findAura("other friendly units here have", d);

        // "Other friendly units have ..."
        d = {}; d.friendly = true; d.all_friendly = true; d.other_only = true;
        findAura("other friendly units have", d);

        // "Your units have ..."
        d = {}; d.friendly = true; d.all_friendly = true;
        findAura("your units have", d);

        // "Units here have ..."
        d = {}; d.here_only = true;
        findAura("units here have", d);

        // "Stunned enemy units here have ..."
        d = {}; d.enemy = true; d.here_only = true; d.stunned_only = true;
        if (clean.find("stunned enemy units here have") != std::string::npos) {
            findAura("stunned enemy units here have", d);
        }

        // "Your Mechs each have [Assault]"
        if (clean.find("your mechs each have") != std::string::npos) {
            d = {}; d.friendly = true; d.all_friendly = true; d.tag_filter = true;
            d.required_tag = "Mech";
            findAura("your mechs each have", d);
        }

        // Step 3: Apply each aura to matching targets
        for (auto& aura : auras) {
            for (auto& [tgt_id, tgt] : state_.objects) {
                if (!tgt.isUnit()) continue;
                if (!tgt.location.has_value()) continue;

                // Self-exclusion
                if (aura.other_only && tgt_id == src_id) continue;

                // Ownership filter
                if (aura.friendly && tgt.controller != src.controller) continue;
                if (aura.enemy && tgt.controller == src.controller) continue;

                // Location filter
                if (aura.here_only) {
                    auto src_bf = src.battlefieldId();
                    auto tgt_bf = tgt.battlefieldId();
                    // Source must be at a battlefield, and target at the same one
                    if (!src_bf || !tgt_bf || *src_bf != *tgt_bf) {
                        // Also check if source IS the battlefield card
                        bool src_is_bf = src.isBattlefield();
                        if (src_is_bf) {
                            // Find which BF this card belongs to
                            bool match = false;
                            for (auto& bf : state_.battlefields) {
                                if (bf.card_object_id == src_id) {
                                    auto tbf = tgt.battlefieldId();
                                    if (tbf && *tbf == bf.id) match = true;
                                    break;
                                }
                            }
                            if (!match) continue;
                        } else {
                            continue;
                        }
                    }
                }

                // Tag filter
                if (aura.tag_filter) {
                    bool has_tag = false;
                    for (auto& tag : tgt.tags) {
                        if (tag == aura.required_tag) { has_tag = true; break; }
                    }
                    if (!has_tag) continue;
                }

                // Stunned filter
                if (aura.stunned_only && !tgt.is_stunned) continue;

                // Apply the aura effect
                GameObject::AuraEffect ae;
                ae.source = src_id;
                ae.might_bonus = aura.might_mod;
                ae.might_minimum = aura.might_min;
                if (aura.is_keyword) {
                    ae.keyword = aura.keyword;
                    ae.keyword_value = aura.keyword_value;
                }
                tgt.aura_effects.push_back(ae);
            }
        }
    }

    // Step 3b: Conditional self-effects ("If X, I have [Keyword]")
    for (auto& [id, obj] : state_.objects) {
        if (!obj.isUnit() || !obj.location.has_value()) continue;
        if (obj.card_def_id == kInvalidId) continue; // skip tokens
        const auto& def = card_db_.get(obj.card_def_id);
        auto& text = def.ability_text;
        if (text.empty()) continue;

        std::string clean;
        { int d = 0;
          for (char c : text) {
              if (c == '(') d++;
              else if (c == ')') { if (d > 0) d--; }
              else if (d == 0) clean += std::tolower(c);
          }
        }

        // "If you've discarded a card this turn, I have [Assault] and [Ganking]"
        if (clean.find("if you've discarded") != std::string::npos ||
            clean.find("if you\xe2\x80\x99ve discarded") != std::string::npos) {
            if (state_.player(obj.controller).has_discarded_this_turn) {
                if (clean.find("[assault]") != std::string::npos) {
                    GameObject::AuraEffect ae; ae.source = id;
                    ae.keyword = Keyword::Assault; ae.keyword_value = 1;
                    obj.aura_effects.push_back(ae);
                }
                if (clean.find("[ganking]") != std::string::npos) {
                    GameObject::AuraEffect ae; ae.source = id;
                    ae.keyword = Keyword::Ganking;
                    obj.aura_effects.push_back(ae);
                }
            }
        }

        // "While I'm buffed, I have [Ganking]"
        if (clean.find("while i'm buffed") != std::string::npos ||
            clean.find("while i\xe2\x80\x99m buffed") != std::string::npos) {
            if (obj.buff_count > 0) {
                if (clean.find("[ganking]") != std::string::npos) {
                    GameObject::AuraEffect ae; ae.source = id;
                    ae.keyword = Keyword::Ganking;
                    obj.aura_effects.push_back(ae);
                }
                // "While I'm buffed, I have an additional +1 [M]"
                if (clean.find("additional +1 [m]") != std::string::npos) {
                    GameObject::AuraEffect ae; ae.source = id; ae.might_bonus = 1;
                    obj.aura_effects.push_back(ae);
                }
            }
        }

        // Brush battlefield aura (Green Father's replacement token).
        // "Bird, Cat, Dog, Poro, and Ivern units have +1 [M] in Brush."
        // Check if `obj` is a unit currently at a BF named "Brush" with
        // any of the named tags, and apply +1M.
        if (obj.isUnit() && obj.location.has_value() &&
            std::holds_alternative<BattlefieldLocation>(*obj.location)) {
            auto bf_id = std::get<BattlefieldLocation>(*obj.location).id;
            const auto* bf = (bf_id < (BattlefieldId)state_.battlefields.size())
                ? &state_.battlefields[bf_id] : nullptr;
            if (bf && state_.objectExists(bf->card_object_id) &&
                state_.getObject(bf->card_object_id).name == "Brush") {
                bool tag_match = false;
                for (const auto& tag : obj.tags) {
                    if (tag == "Bird" || tag == "Cat" || tag == "Dog" ||
                        tag == "Poro" || tag == "Ivern") { tag_match = true; break; }
                }
                if (tag_match) {
                    GameObject::AuraEffect ae;
                    ae.source = bf->card_object_id;
                    ae.might_bonus = 1;
                    obj.aura_effects.push_back(ae);
                }
            }
        }

        // "While you have another unit here, I have +1 [M]" (Trusty Ramhound).
        // Only fires when this unit is at a location AND there's another
        // friendly unit at the same location (units at base count separately
        // from units at battlefields, per the card's "here" wording).
        if (clean.find("while you have another unit here") != std::string::npos &&
            clean.find("+1 [m]") != std::string::npos &&
            obj.location.has_value()) {
            for (const auto& [other_id, other] : state_.objects) {
                if (other_id == id) continue;
                if (!other.isUnit()) continue;
                if (other.controller != obj.controller) continue;
                if (!other.location.has_value()) continue;
                if (*other.location != *obj.location) continue;
                GameObject::AuraEffect ae;
                ae.source = id;
                ae.might_bonus = 1;
                obj.aura_effects.push_back(ae);
                break;
            }
        }

        // "My Might is increased by your points" (Draven, Showboat)
        if (clean.find("my might is increased by your points") != std::string::npos) {
            int pts = state_.player(obj.controller).score;
            if (pts > 0) {
                GameObject::AuraEffect ae; ae.source = id; ae.might_bonus = pts;
                obj.aura_effects.push_back(ae);
            }
        }

        // "While I'm [Mighty], I have [Deflect], [Ganking], and [Shield]"
        if (clean.find("while i'm [mighty]") != std::string::npos ||
            clean.find("while i\xe2\x80\x99m [mighty]") != std::string::npos) {
            // Mighty = 5+ might (check base + buffs + temp, before this aura)
            int pre_aura_might = obj.base_might + obj.buff_count +
                                 obj.temp_might_bonus + obj.attachment_might_bonus;
            if (pre_aura_might >= 5) {
                if (clean.find("[deflect]") != std::string::npos) {
                    GameObject::AuraEffect ae; ae.source = id;
                    ae.keyword = Keyword::Deflect; ae.keyword_value = 1;
                    obj.aura_effects.push_back(ae);
                }
                if (clean.find("[ganking]") != std::string::npos) {
                    GameObject::AuraEffect ae; ae.source = id;
                    ae.keyword = Keyword::Ganking;
                    obj.aura_effects.push_back(ae);
                }
                if (clean.find("[shield]") != std::string::npos) {
                    GameObject::AuraEffect ae; ae.source = id;
                    ae.keyword = Keyword::Shield; ae.keyword_value = 1;
                    obj.aura_effects.push_back(ae);
                }
            }
        }
    }

    // Step 3c: Equipment-granted keywords and stats
    for (auto& [id, obj] : state_.objects) {
        if (!obj.isUnit() || !obj.location.has_value()) continue;
        for (auto gear_id : obj.attachments) {
            if (!state_.objectExists(gear_id)) continue;
            Card* gear_card = card_registry_.get(state_.getObject(gear_id).card_def_id);
            if (!gear_card) continue;

            // Keywords from effect_text
            auto ekw = gear_card->equippedKeywords();
            if (ekw.bits != 0) {
                for (int k = 0; k < static_cast<int>(Keyword::Count); ++k) {
                    if (ekw.has(static_cast<Keyword>(k))) {
                        GameObject::AuraEffect ae;
                        ae.source = gear_id;
                        ae.keyword = static_cast<Keyword>(k);
                        obj.aura_effects.push_back(ae);
                    }
                }
            }
            // Assault/Shield/Deflect from effect_text
            int ea = gear_card->equippedAssault();
            if (ea > 0) {
                GameObject::AuraEffect ae;
                ae.source = gear_id;
                ae.keyword = Keyword::Assault;
                ae.keyword_value = ea;
                obj.aura_effects.push_back(ae);
            }
            int es = gear_card->equippedShield();
            if (es > 0) {
                GameObject::AuraEffect ae;
                ae.source = gear_id;
                ae.keyword = Keyword::Shield;
                ae.keyword_value = es;
                obj.aura_effects.push_back(ae);
            }
        }
    }

    // Step 4: Rebuild cached aura values on all objects
    for (auto& [id, obj] : state_.objects) {
        obj.aura_might_bonus = 0;
        obj.aura_keywords.reset();
        obj.aura_no_combat_damage = false;
        obj.aura_bonus_damage_taken = 0;
        for (auto& ae : obj.aura_effects) {
            obj.aura_might_bonus += ae.might_bonus;
            if (ae.keyword != Keyword::Count) {
                obj.aura_keywords.set(ae.keyword);
            }
            if (ae.suppress_combat_damage) obj.aura_no_combat_damage = true;
            obj.aura_bonus_damage_taken += ae.bonus_damage_taken;
        }
        // Recompute might with new aura values
        if (obj.isUnit() && obj.location.has_value()) {
            obj.recomputeMight();
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Cleanup
// ═══════════════════════════════════════════════════════════════════════════════

void GameEngine::revertLapsedControl() {
    // "You control it until I leave the board" (Akshan, Mischievous). Once the
    // controlling source object is no longer in play (killed / bounced /
    // banished / otherwise off-board), control returns to the recorded owner.
    for (auto& [id, obj] : state_.objects) {
        if (!obj.control_reverts_on_source_leave) continue;
        GameObjectId src = obj.control_source_obj;
        bool source_in_play = state_.objectExists(src) &&
            (state_.getObject(src).location.has_value() ||
             state_.getObject(src).zone == ZoneType::LegendZone);
        if (source_in_play) continue;
        if (obj.controller != obj.control_revert_to) {
            events_.logTrace("CONTROL REVERTS: " + obj.name + " (id=" +
                             std::to_string(id) + ") returns to " +
                             toString(obj.control_revert_to) +
                             " (source left the board)");
            obj.controller = obj.control_revert_to;
            events_.emit(ObjectStateChangedEvent{id, "controller_changed"});
        }
        obj.control_reverts_on_source_leave = false;
        obj.control_source_obj = kInvalidId;
        obj.control_revert_to = PlayerId::None;
    }
}

void GameEngine::cleanup() {
    // CR 322 — "Cleanup is performed repeatedly until a Cleanup occurs
    // with no new change to the game state." Phase 6q+ engine-audit
    // CRITICAL #1 fix: pre-2026-05-19 the loop set `bool changed=false`
    // and never wrote `true`, so it always exited after one pass.
    // Multi-step death cascades (lethal → control change → aura
    // recompute → another death) silently truncated.
    //
    // Change-detection: snapshot a checksum of the cleanup-mutable
    // state before each pass; if the checksum is identical after, we
    // know nothing material changed and can stop. Cheaper than a deep
    // GameState compare; covers all paths the cleanup sub-routines
    // mutate (object count + damage + zone + aura might/keywords +
    // BF control + score).
    auto checksum = [&]() -> uint64_t {
        uint64_t h = 0xCBF29CE484222325ULL; // FNV-1a seed
        auto mix = [&](uint64_t x) {
            h ^= x;
            h *= 0x100000001B3ULL;
        };
        mix(state_.objects.size());
        for (auto& [id, obj] : state_.objects) {
            mix(id);
            mix(static_cast<uint64_t>(obj.zone));
            mix(static_cast<uint64_t>(obj.damage_marked));
            mix(static_cast<uint64_t>(obj.current_might));
            mix(static_cast<uint64_t>(obj.aura_might_bonus));
            mix(obj.untargetable_by_enemy ? 0xAAA : 0xBBB);
            mix(obj.is_exhausted ? 0xCCC : 0xDDD);
            mix(static_cast<uint64_t>(obj.controller));  // control reversions
        }
        for (auto& bf : state_.battlefields) {
            mix(static_cast<uint64_t>(bf.id));
            // bf.controller is std::optional<PlayerId> — encode the
            // has_value bit + the value (if any) into one mix.
            mix(bf.controller.has_value()
                ? (0x100ULL | static_cast<uint64_t>(*bf.controller))
                : 0ULL);
            mix(bf.is_contested ? 0xEEE : 0xFFF);
        }
        for (auto pid : {PlayerId::Player1, PlayerId::Player2}) {
            mix(static_cast<uint64_t>(state_.player(pid).score));
        }
        return h;
    };

    constexpr int kMaxCleanupPasses = 20;
    for (int pass = 0; pass < kMaxCleanupPasses; ++pass) {
        if (checkWinCondition()) return;
        uint64_t before = checksum();
        processLethalDamage();
        revertLapsedControl();           // "until I leave the board" (Akshan)
        updateBattlefieldControl();
        processContestedBattlefields();  // CR 323.8/9
        recalculateAuras();
        uint64_t after = checksum();
        if (before == after) break;
    }

    // ── "Becomes Mighty" edge detection (CR: a unit is Mighty at 5+ [M]) ──
    // Run once after the cleanup loop settles so the Might layers are final.
    // We remember per-unit Mighty state in card_counters["__was_mighty"] and
    // emit a generic ObjectStateChangedEvent only on a <5 -> >=5 transition;
    // TriggerManager::onObjectStateChanged dispatches WhenAUnitBecomesMighty.
    // Falling below 5 re-arms the edge so it can fire again later this/next
    // turn. (Fiora, Worthy 500 / Grand Duelist 519.) No card-specific logic
    // here — the engine only detects and announces the state change.
    for (auto& [id, obj] : state_.objects) {
        if (!obj.isUnit() || !obj.location.has_value()) continue;
        bool mighty = obj.current_might >= 5;
        bool was = obj.card_counters["__was_mighty"] != 0;
        if (mighty && !was) {
            obj.card_counters["__was_mighty"] = 1;
            events_.emit(ObjectStateChangedEvent{id, "became_mighty"});
        } else if (!mighty && was) {
            obj.card_counters["__was_mighty"] = 0;
        }
    }
}

// C-1 commit 8 — cleanup is naturally yield-free (no agent queries
// inside its body — triggers fire as events that the trigger manager
// queues onto the chain for runChain to process LATER, not during
// cleanup). The split is just a documented entry point for the
// step-machine dispatch (commit 9) so cleanup looks symmetric with
// the other phase subroutines.
void GameEngine::advanceCleanup() {
    cleanup();
}

bool GameEngine::checkWinCondition() {
    // CR 467 + Aspirant's Climb (id 776): "Increase the points needed to
    // win the game by 1." Phase 6q+ — scan in-play battlefield cards for
    // the modifier text and bump the effective victory threshold per BF
    // currently on the board.
    int effective_victory = state_.mode.victory_score;
    for (const auto& bf : state_.battlefields) {
        if (!state_.objectExists(bf.card_object_id)) continue;
        const auto& bf_obj = state_.getObject(bf.card_object_id);
        if (bf_obj.card_def_id == kInvalidId) continue;
        const auto& def = card_db_.get(bf_obj.card_def_id);
        if (def.ability_text.find("Increase the points needed to win") !=
            std::string::npos) {
            effective_victory += 1;
        }
    }

    // A player wins if they have >= victory score AND more than opponent (CR 467)
    for (auto pid : {PlayerId::Player1, PlayerId::Player2}) {
        auto& ps = state_.player(pid);
        auto& opp = state_.player(opponent(pid));
        if (ps.score >= effective_victory && ps.score > opp.score) {
            state_.game_over = true;
            state_.winner = pid;
            state_.game_over_reason = toString(pid) + std::string(" reached ") +
                                       std::to_string(ps.score) + " points";
            events_.emit(GameOverEvent{pid, state_.game_over_reason});
            return true;
        }
    }
    return false;
}

void GameEngine::processLethalDamage() {
    // Check if either player has Elder Dragon's "any damage = lethal" effect
    bool elder_p1 = false, elder_p2 = false;
    for (auto& [id, obj] : state_.objects) {
        if (!obj.isUnit() || !obj.location.has_value()) continue;
        if (obj.card_def_id == kInvalidId) continue;
        const auto& def = card_db_.get(obj.card_def_id);
        if (def.ability_text.find("Any amount of your damage is enough to kill") != std::string::npos ||
            def.ability_text.find("any amount of your damage is enough to kill") != std::string::npos) {
            if (obj.controller == PlayerId::Player1) elder_p1 = true;
            if (obj.controller == PlayerId::Player2) elder_p2 = true;
        }
    }

    std::vector<GameObjectId> to_kill;
    for (auto& [id, obj] : state_.objects) {
        if (!obj.isUnit() || !obj.location.has_value()) continue;
        if (obj.damage_marked <= 0) continue;

        bool lethal = obj.hasLethalDamage();

        // Elder Dragon: any damage kills enemy units
        if (!lethal) {
            PlayerId damager = opponent(obj.controller);
            if ((damager == PlayerId::Player1 && elder_p1) ||
                (damager == PlayerId::Player2 && elder_p2)) {
                lethal = true;
                events_.logTrace("ELDER_DRAGON: " + obj.name + " killed by any-damage rule");
            }
        }

        if (lethal) to_kill.push_back(id);
    }
    for (auto id : to_kill) {
        killUnit(id);
    }
}

void GameEngine::updateBattlefieldControl() {
    for (auto& bf : state_.battlefields) {
        if (bf.combat_in_progress || bf.showdown_in_progress) continue;

        if (bf.controller.has_value()) {
            bool has_units = bf.hasUnitsFrom(*bf.controller, state_.objects);
            if (!has_units && state_.turn.isNeutralOpen()) {
                auto old = bf.controller;
                bf.controller = std::nullopt;
                events_.emit(ControlChangedEvent{bf.id, old, std::nullopt});
            }
        }

        // Remove facedown cards that belong to a player who doesn't control this BF (CR 323.7.5)
        std::vector<GameObjectId> to_remove;
        for (auto card_id : bf.facedown) {
            if (!state_.objectExists(card_id)) continue;
            auto& card = state_.getObject(card_id);
            if (!bf.controller.has_value() || *bf.controller != card.controller) {
                to_remove.push_back(card_id);
            }
        }
        for (auto card_id : to_remove) {
            auto it = std::find(bf.facedown.begin(), bf.facedown.end(), card_id);
            if (it != bf.facedown.end()) bf.facedown.erase(it);

            auto& card = state_.getObject(card_id);
            card.is_hidden = false;
            card.hidden_at = kInvalidId;
            card.zone = ZoneType::Trash;
            card.location = std::nullopt;
            state_.player(card.owner).trash.push_back(card_id);
            events_.logDebug(std::string("HIDDEN: ") + card.name +
                             " removed from facedown (control lost)");
        }
    }
}

void GameEngine::processContestedBattlefields() {
    // CR 459.2.b.3.a / 459.2.b.4.a — units that become present at a
    // BF where combat is already in progress get the appropriate
    // combat designation during the cleanup following the action that
    // brought them there. Phase 6q+ fix: pre-fix, late-arriving units
    // (Pouncing, mid-combat plays) didn't get designated and were
    // invisible to combat damage assignment.
    for (auto& bf : state_.battlefields) {
        if (!bf.combat_in_progress) continue;
        if (!bf.attacker.has_value() || !bf.defender.has_value()) continue;
        for (auto& [id, obj] : state_.objects) {
            if (!obj.isUnit() || !obj.isAtBattlefield()) continue;
            auto u_bf = obj.battlefieldId();
            if (!u_bf || *u_bf != bf.id) continue;
            if (obj.combat_designation != CombatDesignation::None) continue;
            if (obj.controller == *bf.attacker) {
                obj.combat_designation = CombatDesignation::Attacker;
                obj.recomputeMight();
            } else if (obj.controller == *bf.defender) {
                obj.combat_designation = CombatDesignation::Defender;
                obj.recomputeMight();
            }
        }
    }

    // CR 323.8/9 — cleanup re-evaluates staging on each contested BF.
    // Phase 6q+: pre-fix this was a no-op and we relied on move/play
    // sites to stage. That left some edge cases unstaged (death-cleanup
    // changing presence, replacement effects relocating units, etc.).
    // This pass:
    //  • 323.8: Mark a Showdown as Staged on any BF Contested has been
    //    applied to that has units only from the contesting player.
    //  • 323.9: Mark a Combat as Staged on any contested BF with units
    //    from BOTH players.
    //  • 323.10: If a BF has Combat staged but no longer has units from
    //    opposing players, the Combat ceases to be Staged.
    for (auto& bf : state_.battlefields) {
        if (!bf.is_contested) continue;
        if (bf.showdown_in_progress || bf.combat_in_progress) continue;

        bool p1_units = bf.hasUnitsFrom(PlayerId::Player1, state_.objects);
        bool p2_units = bf.hasUnitsFrom(PlayerId::Player2, state_.objects);

        if (p1_units && p2_units) {
            // Combat staging — both sides present.
            if (!bf.combat_staged) {
                bf.combat_staged = true;
                bf.showdown_staged = false;  // promote showdown → combat
            }
        } else if (p1_units || p2_units) {
            // One side present — showdown staging.
            if (!bf.showdown_staged && !bf.combat_staged) {
                bf.showdown_staged = true;
            }
            // CR 323.10: if combat WAS staged but only one side is now
            // present, the combat ceases to be staged (becomes showdown).
            if (bf.combat_staged) {
                bf.combat_staged = false;
                bf.showdown_staged = true;
            }
        } else {
            // No units from either player — contested status is moot.
            // The BF stays contested (uncontrolled) but neither stage is
            // active. Don't clear is_contested here — a subsequent move
            // will re-stage.
            bf.showdown_staged = false;
            bf.combat_staged = false;
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Board operations
// ═══════════════════════════════════════════════════════════════════════════════

GameObjectId GameEngine::instantiateCard(CardDefId def_id, PlayerId owner) {
    auto obj_id = state_.createObject();
    auto& obj = state_.getObject(obj_id);
    const auto& def = card_db_.get(def_id);

    obj.card_def_id = def_id;
    obj.owner = owner;
    obj.controller = owner;
    obj.name = def.name;
    obj.card_type = def.card_type;
    obj.super_type = def.super_type;
    obj.tags = def.tags;
    obj.domains = def.domains;
    obj.base_might = def.might;
    obj.current_might = def.might;
    obj.keywords = def.keywords;
    obj.assault_value = def.assault_value;
    obj.shield_value = def.shield_value;
    obj.deflect_value = def.deflect_value;
    obj.might_bonus = def.might_bonus;

    return obj_id;
}

void GameEngine::drawCards(PlayerId player, int count) {
    auto& ps = state_.player(player);
    int drawn = 0;
    for (int i = 0; i < count; ++i) {
        if (ps.main_deck.empty()) {
            // Burn Out (CR 431.2): recycle trash into deck, then the
            // burning-out player chooses an opponent to gain 1 point
            // (CR 431.2.c). 1v1 = exactly one opponent. CR 431.3: if
            // opponent reaches victory score they win immediately.
            if (ps.trash.empty()) {
                events_.logTrace(std::string("BURN_OUT: ") + toString(player) +
                                 " deck AND trash empty, cannot draw");
                break; // truly empty — nothing to do
            }
            ps.burned_out = true;
            for (auto card_id : ps.trash) {
                state_.getObject(card_id).zone = ZoneType::MainDeck;
                ps.main_deck.push_back(card_id);
            }
            ps.trash.clear();
            shuffleDeck(player);
            PlayerId opp_id = opponent(player);
            auto& opp_ps = state_.player(opp_id);
            opp_ps.score++;
            events_.logTrace(std::string("BURN_OUT: ") + toString(player) +
                             " deck empty, shuffled trash; " +
                             toString(opp_id) + " gains 1 point (CR 431.2.c) -> " +
                             std::to_string(opp_ps.score));
            if (opp_ps.score >= state_.mode.victory_score &&
                opp_ps.score > ps.score) {
                state_.game_over = true;
                state_.winner = opp_id;
                state_.game_over_reason = std::string(toString(opp_id)) +
                                           " wins via burn-out point (CR 431.3)";
                events_.emit(GameOverEvent{opp_id, state_.game_over_reason});
                return;
            }
            // If deck still empty after shuffle (shouldn't happen), stop
            if (ps.main_deck.empty()) break;
        }
        auto card_id = ps.main_deck.back();
        ps.main_deck.pop_back();
        ps.hand.push_back(card_id);
        state_.getObject(card_id).zone = ZoneType::Hand;
        events_.logTrace("  DREW: " + state_.getObject(card_id).name +
                         " (id=" + std::to_string(card_id) + ")");
        drawn++;
    }
    if (drawn > 0) {
        state_.player(player).draws_this_turn += drawn;
        events_.emit(CardsDrawnEvent{player, drawn});
    }
}

void GameEngine::channelRunes(PlayerId player, int count) {
    auto& ps = state_.player(player);
    for (int i = 0; i < count; ++i) {
        if (ps.rune_deck.empty()) break;
        auto rune_id = ps.rune_deck.back();
        ps.rune_deck.pop_back();

        auto& rune = state_.getObject(rune_id);
        rune.zone = ZoneType::Base;
        rune.location = BaseLocation{player};

        events_.logTrace("  CHANNELED: " + rune.name + " (id=" +
                         std::to_string(rune_id) + ", ready)");
        events_.emit(RuneChanneledEvent{rune_id, player});
        events_.emit(EnteredBoardEvent{
            rune_id, player, CardType::Rune, BaseLocation{player}, false});
    }
}

void GameEngine::shuffleDeck(PlayerId player) {
    auto& deck = state_.player(player).main_deck;
    std::shuffle(deck.begin(), deck.end(), rng_);
}

void GameEngine::shuffleRuneDeck(PlayerId player) {
    auto& deck = state_.player(player).rune_deck;
    std::shuffle(deck.begin(), deck.end(), rng_);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Cost payment
// ═══════════════════════════════════════════════════════════════════════════════

int GameEngine::availableEnergy(PlayerId player) const {
    // Energy comes from exhausting ready runes (CR 163.2.a: [E]: Add [1])
    // Each ready rune in base can be exhausted for 1 Energy
    int count = 0;
    auto base_loc = BaseLocation{player};
    for (auto& [id, obj] : state_.objects) {
        if (obj.isRune() && obj.controller == player && !obj.is_exhausted &&
            obj.location.has_value() && *obj.location == LocationId{base_loc}) {
            count++;
        }
    }
    // Also count Energy already in the pool
    count += state_.player(player).rune_pool.energy;
    return count;
}

int GameEngine::availablePower(PlayerId player, Domain domain) const {
    // Power comes from recycling a rune of matching domain (CR 163.2.b)
    // Each rune in base (exhausted or ready) of matching domain can be recycled for 1 Power
    int count = 0;
    auto base_loc = BaseLocation{player};
    for (auto& [id, obj] : state_.objects) {
        if (obj.isRune() && obj.controller == player &&
            obj.location.has_value() && *obj.location == LocationId{base_loc}) {
            for (auto d : obj.domains) {
                if (d == domain) { count++; break; }
            }
        }
    }
    // Also count matching Power already in pool
    count += state_.player(player).rune_pool.power[static_cast<int>(domain)];
    count += state_.player(player).rune_pool.universal_power;
    return count;
}

int GameEngine::availableAnyPower(PlayerId player) const {
    // Any rune in base can be recycled for 1 Power of its domain
    int count = 0;
    auto base_loc = BaseLocation{player};
    for (auto& [id, obj] : state_.objects) {
        if (obj.isRune() && obj.controller == player &&
            obj.location.has_value() && *obj.location == LocationId{base_loc}) {
            count++;
        }
    }
    return count;
}

// ─── Repeat (CR 820) ────────────────────────────────────────────────────────
//
// Parses the FIRST `[Repeat] [...]` token in ability_text. See header doc.
//
// Recognized forms:
//   [Repeat] [N]       → N energy
//   [Repeat] [N][D]    → N energy + 1 domain power (D ∈ A/B/C/F/O/M/P/R/U/V)
//   [Repeat] [D]       → 1 domain power
//   [Repeat] [A]       → 1 universal power
//
// Cards with multi-mode Repeat costs (Curtain Call: "[Repeat] [1]/[A]/[1][A]")
// are handled by the card's own onResolve modal logic, not by this parse.
GameEngine::RepeatCost GameEngine::parseRepeatCost(const std::string& text) {
    RepeatCost rc;
    auto pos = text.find("[Repeat]");
    if (pos == std::string::npos) return rc;
    pos += std::string("[Repeat]").size();
    // Skip whitespace
    while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos]))) ++pos;

    // First token: either [N] (digits) or [D] (single letter)
    if (pos >= text.size() || text[pos] != '[') return rc;
    auto end = text.find(']', pos);
    if (end == std::string::npos) return rc;
    std::string tok = text.substr(pos + 1, end - pos - 1);

    // CR 134.2 canonical domain-letter mapping (also in types.h:62-67):
    //   R = Fury (red), G = Calm (green), B = Mind (blue),
    //   O = Body (orange), P = Chaos (purple), Y = Order (yellow).
    //
    // The pre-2026-05-19 mapping had 4 wrong entries (B→Body, O→Order,
    // P→Mind, R→Calm) plus speculative entries for C/F/M/V/U that do
    // not appear in any printed Riftbound card text. Cross-checked
    // against cards/card_index.json — every [Repeat] [N][D] cost token
    // in the registry uses one of {R, G, B, O, P, Y} (or [A] for
    // universal). Symptoms of the bug: Piercing Light (Fury) `[R]`
    // tranches were paid from the Calm rune pool; Rocket Barrage /
    // Bellows Breath (Mind) `[B]` from Body; Called Shot / Syndra
    // (Chaos) `[P]` from Mind. Affordability + payment both wrong.
    // Phase 6q+ engine-audit CRITICAL #4.
    auto domainFromLetter = [](char c) -> Domain {
        switch (c) {
            case 'R': return Domain::Fury;
            case 'G': return Domain::Calm;
            case 'B': return Domain::Mind;
            case 'O': return Domain::Body;
            case 'P': return Domain::Chaos;
            case 'Y': return Domain::Order;
            default:  return Domain::Count;
        }
    };

    auto allDigits = [](const std::string& s) {
        if (s.empty()) return false;
        for (char c : s) if (!std::isdigit(static_cast<unsigned char>(c))) return false;
        return true;
    };

    if (allDigits(tok)) {
        rc.energy = std::stoi(tok);
        rc.valid = true;
        // Look for an optional trailing [D] domain power token.
        pos = end + 1;
        if (pos < text.size() && text[pos] == '[') {
            auto end2 = text.find(']', pos);
            if (end2 != std::string::npos) {
                std::string tok2 = text.substr(pos + 1, end2 - pos - 1);
                if (tok2 == "A") {
                    rc.power = 1;
                    rc.power_domain = Domain::Count;  // universal
                } else if (tok2.size() == 1) {
                    Domain d = domainFromLetter(tok2[0]);
                    if (d != Domain::Count) {
                        rc.power = 1;
                        rc.power_domain = d;
                    }
                }
            }
        }
    } else if (tok == "A") {
        rc.power = 1;
        rc.power_domain = Domain::Count;
        rc.valid = true;
    } else if (tok.size() == 1) {
        Domain d = domainFromLetter(tok[0]);
        if (d != Domain::Count) {
            rc.power = 1;
            rc.power_domain = d;
            rc.valid = true;
        }
    }
    return rc;
}

// Pay one additional Repeat tranche (R is the per-tranche cost). Iterates
// the same primitives canAfford / payCardCost use, but for ONE extra
// (energy, domain_power) charge — called by executePlaySpell after the
// base cost has already been paid.
bool GameEngine::payRepeatCost(PlayerId player, const RepeatCost& cost) {
    if (!cost.valid) return true;
    auto base_loc = BaseLocation{player};

    // Energy: exhaust ready runes
    int need_energy = cost.energy;
    if (need_energy > 0) {
        for (auto& [id, obj] : state_.objects) {
            if (need_energy == 0) break;
            if (!obj.isRune() || obj.controller != player || obj.is_exhausted) continue;
            if (!obj.location.has_value() || *obj.location != LocationId{base_loc}) continue;
            obj.is_exhausted = true;
            --need_energy;
        }
        if (need_energy > 0) return false;
    }

    // Power: recycle exhausted matching-domain rune to deck
    int need_power = cost.power;
    if (need_power > 0) {
        auto& ps = state_.player(player);
        bool universal = (cost.power_domain == Domain::Count);
        for (auto& [id, obj] : state_.objects) {
            if (need_power == 0) break;
            if (!obj.isRune() || obj.controller != player) continue;
            if (!obj.location.has_value() || *obj.location != LocationId{base_loc}) continue;
            if (!obj.is_exhausted) continue;
            bool match = universal;
            if (!match) {
                for (auto d : obj.domains) if (d == cost.power_domain) { match = true; break; }
            }
            if (!match) continue;
            obj.zone = ZoneType::MainDeck;
            obj.location = std::nullopt;
            ps.main_deck.insert(ps.main_deck.begin(), id);
            --need_power;
        }
        if (need_power > 0) return false;
    }
    return true;
}

void GameEngine::maybePayOptionalAdditionalCost(PlayerId player, GameObjectId card_id) {
    if (!state_.objectExists(card_id)) return;
    auto& card = state_.getObject(card_id);
    if (card.card_def_id == kInvalidId) return;
    const Card* cdef = card_registry_.get(card.card_def_id);
    if (!cdef) return;
    auto cost = cdef->optionalAdditionalCost();
    if (!cost.valid) return;

    // Pick a payable domain for the power portion (specific, any, or none).
    std::optional<Domain> dom;
    if (cost.power == 0) {
        if (canPayAdditionalCost(player, cost.energy, 0, Domain::Fury)) dom = Domain::Fury;
    } else if (cost.any_domain) {
        for (int di = 0; di < static_cast<int>(Domain::Count); ++di) {
            if (canPayAdditionalCost(player, cost.energy, cost.power, static_cast<Domain>(di))) {
                dom = static_cast<Domain>(di);
                break;
            }
        }
    } else if (canPayAdditionalCost(player, cost.energy, cost.power, cost.power_domain)) {
        dom = cost.power_domain;
    }
    if (!dom.has_value()) return;  // can't afford -> not offered (CR: choice only when payable)

    // Optional ("you may") -> yes/no agent decision. Mirrors the Accelerate flow.
    Intent decline; decline.type = IntentType::MakeChoice; decline.player = player; decline.chosen_value = 0;
    Intent accept;  accept.type  = IntentType::MakeChoice; accept.player  = player; accept.chosen_value  = 1;
    std::vector<Intent> opts = {decline, accept};
    state_.decision_index++;
    events_.logTrace("DECISION #" + std::to_string(state_.decision_index) + " (" +
                     toString(player) + "): pay additional cost for " + card.name +
                     "? [decline|pay] [2 options]");
    auto chosen = getAgent(player).selectAction(state_, opts);
    recordAppliedIntent(chosen);
    if (on_decision) on_decision(state_, opts, chosen);

    if (chosen.chosen_value.value_or(0) == 1) {
        payAdditionalCost(player, cost.energy, cost.power, *dom);
        if (cost.paid_flag && cost.paid_flag[0] != '\0') {
            card.card_counters[cost.paid_flag] = 1;
        }
        events_.logTrace("ADDITIONAL_COST: " + card.name + " paid optional additional cost");
    } else {
        events_.logTrace("ADDITIONAL_COST: " + card.name + " declined additional cost");
    }
}

bool GameEngine::canAfford(PlayerId player, GameObjectId card_obj) const {
    auto& card = state_.getObject(card_obj);
    if (card.card_def_id == kInvalidId) return false;  // tokens have no cost
    const auto& def = card_db_.get(card.card_def_id);

    int energy_needed = def.energy_cost;
    int power_needed = def.power_cost;

    // Apply cost reductions
    auto& ps_const = state_.player(player);
    int min_cost = 0;
    // Combat-active scope: a CostModifier with combat_active_only=true is
    // only consulted while combat is in progress on any battlefield.
    // Used by Vex Cheerless ("While I'm in combat, friendly spells cost
    // [1] less, enemy spells cost [1] more").
    bool combat_active = false;
    for (const auto& bf : state_.battlefields) {
        if (bf.combat_in_progress) { combat_active = true; break; }
    }
    // Same controller of card_obj as the cost-payer in normal play
    // (the player paying for their own play). Cross-player consult
    // (enemy cost increase) requires the modifier's source player to
    // be tracked separately — see opponent modifier sweep below.
    for (auto& mod : ps_const.cost_modifiers) {
        if (mod.next_spell_only && !card.isSpell()) continue;
        if (mod.next_unit_only && !card.isUnit()) continue;
        if (mod.gear_only && !card.isGear()) continue;
        if (mod.first_gear_per_turn && ps_const.gears_played_this_turn > 0) continue;
        if (mod.affects_non_token_only && card.super_type == SuperType::Token) continue;
        if (mod.combat_active_only && !combat_active) continue;
        if (mod.affects_enemy_only) continue;  // friendly's own list, skip enemy-only
        energy_needed -= mod.energy_reduction;
        energy_needed += mod.energy_increase;
        if (mod.min_cost > min_cost) min_cost = mod.min_cost;
    }
    // Cross-player modifiers: the OPPONENT's cost_modifiers list may
    // contain affects_enemy_only entries that target THIS player. Vex
    // Cheerless (controlled by P2) wants to make P1's spells cost more.
    for (auto& mod : state_.player(opponent(player)).cost_modifiers) {
        if (!mod.affects_enemy_only) continue;
        if (mod.next_spell_only && !card.isSpell()) continue;
        if (mod.next_unit_only && !card.isUnit()) continue;
        if (mod.affects_non_token_only && card.super_type == SuperType::Token) continue;
        if (mod.combat_active_only && !combat_active) continue;
        energy_needed -= mod.energy_reduction;
        energy_needed += mod.energy_increase;
        if (mod.min_cost > min_cost) min_cost = mod.min_cost;
    }
    // Self-cost reduction hook (e.g., Noxus Hopeful "[Legion] I cost [2] less").
    if (auto* self_card = card_registry_.get(card.card_def_id)) {
        energy_needed -= self_card->selfCostReduction(state_, player);
    }
    energy_needed = std::max(min_cost, energy_needed);
    energy_needed = std::max(0, energy_needed);

    // Count available runes in base, partitioned by ready/exhausted
    // and matching/non-matching-domain. The CR cost-payment ordering
    // (exhaust ready runes for energy, THEN recycle exhausted runes
    // for power) lets a single ready rune provide BOTH 1 energy and
    // 1 power: exhaust it in step 1, recycle it in step 2.
    auto base_loc = BaseLocation{player};
    int ready_total = 0;
    int matching_ready = 0;
    int matching_exhausted = 0;
    for (auto& [id, obj] : state_.objects) {
        if (!obj.isRune() || obj.controller != player) continue;
        if (!obj.location.has_value() || *obj.location != LocationId{base_loc}) continue;
        bool matches_domain = false;
        if (power_needed > 0) {
            for (auto d : def.domains) {
                for (auto rd : obj.domains) {
                    if (d == rd) { matches_domain = true; break; }
                }
                if (matches_domain) break;
            }
        }
        if (!obj.is_exhausted) {
            ++ready_total;
            if (matches_domain) ++matching_ready;
        } else if (matches_domain) {
            ++matching_exhausted;
        }
    }

    const auto& pool = state_.player(player).rune_pool;

    // Energy can come from the pool's existing energy AND from
    // exhausting ready runes (each gives 1 energy).
    int energy_available = ready_total + pool.energy;
    if (energy_available < energy_needed) return false;

    // Power can come from:
    //   • Pool's already-spent power for any of the card's domains
    //     (per-domain bucket).
    //   • Pool's universal power.
    //   • Newly-recycled runes whose domains match the card. Recycle
    //     candidates = exhausted matching runes + matching runes we
    //     just exhausted in the energy step (the energy step exhausts
    //     `energy_needed` ready runes, and we're free to pick the
    //     matching-domain ones first when doing so).
    int power_from_pool = pool.universal_power;
    for (auto d : def.domains) {
        int di = static_cast<int>(d);
        if (di >= 0 && di < static_cast<int>(Domain::Count)) {
            power_from_pool += pool.power[di];
        }
    }
    int power_remaining = std::max(0, power_needed - power_from_pool);
    int matching_exhausted_after_step1 =
        matching_exhausted + std::min(matching_ready, energy_needed);
    return matching_exhausted_after_step1 >= power_remaining;
}

// C-1 commit 8 — payCardCost split into a cursor-based state machine.
// The legacy payCardCost() body is now the BRIDGE: it runs the cursor
// in a queryAgent loop. The step-machine dispatch (commit 9) will
// drive the cursor directly without queryAgent.

namespace {
// Helper: enumerate ready runes in `player`'s base.
std::vector<GameObjectId> readyRunesInBase(const GameState& state, PlayerId player) {
    std::vector<GameObjectId> out;
    auto base_loc = BaseLocation{player};
    for (auto& [id, obj] : state.objects) {
        if (!obj.isRune() || obj.controller != player || obj.is_exhausted) continue;
        if (!obj.location.has_value() || *obj.location != LocationId{base_loc}) continue;
        out.push_back(id);
    }
    return out;
}
// Helper: enumerate runes (any state) in `player`'s base whose domains
// intersect `wanted`. Used for power recycling.
std::vector<GameObjectId> matchingRunesInBase(const GameState& state, PlayerId player,
                                                const std::vector<Domain>& wanted) {
    std::vector<GameObjectId> out;
    auto base_loc = BaseLocation{player};
    for (auto& [id, obj] : state.objects) {
        if (!obj.isRune() || obj.controller != player) continue;
        if (!obj.location.has_value() || *obj.location != LocationId{base_loc}) continue;
        bool match = false;
        for (auto d : wanted) {
            for (auto rd : obj.domains) {
                if (d == rd) { match = true; break; }
            }
            if (match) break;
        }
        if (match) out.push_back(id);
    }
    return out;
}
}  // namespace

GameEngine::CostPaymentAdvance GameEngine::beginCostPayment(
    PlayerId player, GameObjectId card_obj) {

    CostPaymentAdvance done;
    done.kind = CostPaymentAdvance::Kind::Done;
    done.deciding = player;

    auto& card = state_.getObject(card_obj);
    if (card.card_def_id == kInvalidId) {
        state_.cost_cursor.reset();
        return done;  // tokens cost nothing
    }
    const auto& def = card_db_.get(card.card_def_id);

    int energy_needed = def.energy_cost;
    int power_needed = def.power_cost;

    // Apply cost reductions
    auto& ps = state_.player(player);
    int min_cost = 0;
    bool combat_active = false;
    for (const auto& bf : state_.battlefields) {
        if (bf.combat_in_progress) { combat_active = true; break; }
    }
    for (auto& mod : ps.cost_modifiers) {
        if (mod.next_spell_only && !card.isSpell()) continue;
        if (mod.next_unit_only && !card.isUnit()) continue;
        if (mod.gear_only && !card.isGear()) continue;
        if (mod.first_gear_per_turn && ps.gears_played_this_turn > 0) continue;
        if (mod.affects_non_token_only && card.super_type == SuperType::Token) continue;
        if (mod.combat_active_only && !combat_active) continue;
        if (mod.affects_enemy_only) continue;
        energy_needed -= mod.energy_reduction;
        energy_needed += mod.energy_increase;
        if (mod.min_cost > min_cost) min_cost = mod.min_cost;
    }
    // Cross-player modifiers (Vex Cheerless makes opponent's spells cost more).
    for (auto& mod : state_.player(opponent(player)).cost_modifiers) {
        if (!mod.affects_enemy_only) continue;
        if (mod.next_spell_only && !card.isSpell()) continue;
        if (mod.next_unit_only && !card.isUnit()) continue;
        if (mod.affects_non_token_only && card.super_type == SuperType::Token) continue;
        if (mod.combat_active_only && !combat_active) continue;
        energy_needed -= mod.energy_reduction;
        energy_needed += mod.energy_increase;
        if (mod.min_cost > min_cost) min_cost = mod.min_cost;
    }
    // Self-cost reduction hook (e.g., Noxus Hopeful Legion discount).
    if (auto* self_card = card_registry_.get(card.card_def_id)) {
        energy_needed -= self_card->selfCostReduction(state_, player);
    }
    // Rek'Sai, Breacher: "Friendly units played from anywhere other than
    // a player's hand have Accelerate." Implemented as a self-discount
    // when (a) play_source != Hand and (b) a friendly Rek'Sai is on board.
    // The current cost-payment path doesn't have direct access to the
    // Intent (and thus play_source); we surface it via a transient field
    // on PlayerState before calling beginCostPayment. See executePlayCard
    // for the set/clear pattern.
    if (card.isUnit() && ps.current_play_source != Intent::PlaySource::Hand) {
        for (auto& [oid, obj] : state_.objects) {
            if (obj.controller != player) continue;
            if (obj.card_def_id != 352) continue;  // Rek'Sai, Breacher
            if (!obj.location.has_value()) continue;
            // Auto-Accelerate: pay 1 extra energy at play, enter ready.
            // We mark via card_counters so resolvePermanent reads it.
            card.card_counters["__rek_sai_accel"] = 1;
            energy_needed += 1;  // Accelerate's mandatory +[1]
            break;
        }
    }
    energy_needed = std::max(min_cost, energy_needed);
    energy_needed = std::max(0, energy_needed);

    // Consume one-shot modifiers that applied
    ps.cost_modifiers.erase(
        std::remove_if(ps.cost_modifiers.begin(), ps.cost_modifiers.end(),
            [&](const PlayerState::CostModifier& m) {
                if (m.next_spell_only && card.isSpell()) return true;
                if (m.next_unit_only && card.isUnit()) return true;
                return false;
            }),
        ps.cost_modifiers.end());

    events_.logTrace("PAY_COST: " + card.name + " E=" + std::to_string(energy_needed) +
                     " P=" + std::to_string(power_needed));

    // Spend pool energy first (no agent decision needed)
    if (energy_needed > 0) {
        int from_pool = std::min(ps.rune_pool.energy, energy_needed);
        ps.rune_pool.energy -= from_pool;
        energy_needed -= from_pool;
    }

    // Initialize cursor for the rune-pick phase
    GameState::CostPaymentCursor cur;
    cur.card = card_obj;
    cur.player = player;
    cur.energy_remaining = energy_needed;
    cur.power_remaining = power_needed;
    cur.power_domains = def.domains;
    cur.phase = (energy_needed > 0)
        ? GameState::CostPaymentCursor::Phase::PayingEnergy
        : ((power_needed > 0)
            ? GameState::CostPaymentCursor::Phase::PayingPower
            : GameState::CostPaymentCursor::Phase::Done);
    state_.cost_cursor = cur;

    return advanceCostPayment();
}

GameEngine::CostPaymentAdvance GameEngine::advanceCostPayment() {
    CostPaymentAdvance adv;
    if (!state_.cost_cursor.has_value()) {
        adv.kind = CostPaymentAdvance::Kind::Done;
        return adv;
    }
    auto& cur = *state_.cost_cursor;
    adv.deciding = cur.player;

    using Phase = GameState::CostPaymentCursor::Phase;

    // Skip-ahead: if energy phase is exhausted, move to power.
    if (cur.phase == Phase::PayingEnergy && cur.energy_remaining <= 0) {
        cur.phase = (cur.power_remaining > 0) ? Phase::PayingPower : Phase::Done;
    }
    // Same for power phase.
    if (cur.phase == Phase::PayingPower && cur.power_remaining <= 0) {
        cur.phase = Phase::Done;
    }

    if (cur.phase == Phase::Done) {
        state_.cost_cursor.reset();
        adv.kind = CostPaymentAdvance::Kind::Done;
        return adv;
    }

    // Build the candidate list for the current phase.
    std::vector<GameObjectId> candidates;
    if (cur.phase == Phase::PayingEnergy) {
        candidates = readyRunesInBase(state_, cur.player);
    } else {
        candidates = matchingRunesInBase(state_, cur.player, cur.power_domains);
    }

    if (candidates.empty()) {
        // Can't continue — but the contract is canAfford() was checked
        // upstream, so we should never hit this. Defensively close out.
        state_.cost_cursor.reset();
        adv.kind = CostPaymentAdvance::Kind::Done;
        return adv;
    }

    adv.kind = CostPaymentAdvance::Kind::NeedDecision;
    adv.legal.reserve(candidates.size());
    for (auto rune_id : candidates) {
        Intent choice;
        choice.type = IntentType::MakeChoice;
        choice.player = cur.player;
        choice.chosen_objects = {rune_id};
        adv.legal.push_back(std::move(choice));
    }
    return adv;
}

GameEngine::CostPaymentAdvance GameEngine::resolveCostPaymentDecision(
    const Intent& chosen) {
    assert(state_.cost_cursor.has_value() &&
           "resolveCostPaymentDecision called without an active cursor");
    auto& cur = *state_.cost_cursor;

    GameObjectId rune_id = chosen.chosen_objects.empty()
        ? kInvalidId : chosen.chosen_objects[0];
    if (rune_id == kInvalidId || !state_.objectExists(rune_id)) {
        // Defensive — invalid pick, nothing to apply
        return advanceCostPayment();
    }

    using Phase = GameState::CostPaymentCursor::Phase;
    auto& rune = state_.getObject(rune_id);

    if (cur.phase == Phase::PayingEnergy) {
        events_.logTrace("  EXHAUST_RUNE: " + rune.name + " (id=" +
                         std::to_string(rune_id) +
                         ") for energy [agent choice]");
        rune.is_exhausted = true;
        events_.emit(ObjectStateChangedEvent{rune_id, "exhausted"});
        cur.energy_remaining--;
    } else if (cur.phase == Phase::PayingPower) {
        events_.logTrace("  RECYCLE_RUNE: " + rune.name + " (id=" +
                         std::to_string(rune_id) + ", " +
                         (rune.is_exhausted ? "exhausted" : "ready") +
                         ") for power [agent choice]");
        auto base_loc = BaseLocation{cur.player};
        rune.location = std::nullopt;
        rune.zone = ZoneType::RuneDeck;
        rune.is_exhausted = false;
        state_.player(cur.player).rune_deck.insert(
            state_.player(cur.player).rune_deck.begin(), rune_id);
        events_.emit(LeftBoardEvent{rune_id, cur.player, CardType::Rune,
            base_loc, ZoneType::RuneDeck, false});
        cur.power_remaining--;
        state_.player(cur.player).power_spent_this_turn++;  // Sivir, Mercenary
    }

    return advanceCostPayment();
}

bool GameEngine::canPayAdditionalCost(PlayerId player, int energy, int power,
                                       Domain domain) const {
    // CR 805 Accelerate is the canonical additional-cost shape (1 energy,
    // 1 domain-power). Floating pool may cover some of it; remainder needs
    // ready / domain-matching runes.
    auto& ps = state_.player(player);
    int e_need = std::max(0, energy);
    int p_need = std::max(0, power);

    // Pool first (no decision needed).
    int e_from_pool = std::min(ps.rune_pool.energy, e_need);
    e_need -= e_from_pool;
    int domain_idx = static_cast<int>(domain);
    int p_from_pool = std::min(ps.rune_pool.power[domain_idx], p_need);
    p_need -= p_from_pool;
    int p_from_uni = std::min(ps.rune_pool.universal_power, p_need);
    p_need -= p_from_uni;

    if (e_need == 0 && p_need == 0) return true;

    // Otherwise rune accounting (mirrors canAfford's worst-case logic).
    auto ready = readyRunesInBase(state_, player);
    std::vector<Domain> wanted = {domain};
    auto matching = matchingRunesInBase(state_, player, wanted);
    if (static_cast<int>(matching.size()) < p_need) return false;

    int matching_ready = 0;
    for (auto mr : matching) {
        if (!state_.getObject(mr).is_exhausted) matching_ready++;
    }
    int exhausted_matching = static_cast<int>(matching.size()) - matching_ready;
    int recycle_from_ready = std::max(0, p_need - exhausted_matching);
    int energy_available = static_cast<int>(ready.size()) - recycle_from_ready;
    return energy_available >= e_need;
}

bool GameEngine::payAdditionalCost(PlayerId player, int energy, int power,
                                    Domain domain) {
    auto& ps = state_.player(player);
    int e_need = std::max(0, energy);
    int p_need = std::max(0, power);

    // Spend pool first.
    int e_from_pool = std::min(ps.rune_pool.energy, e_need);
    ps.rune_pool.energy -= e_from_pool;
    e_need -= e_from_pool;
    int domain_idx = static_cast<int>(domain);
    int p_from_pool = std::min(ps.rune_pool.power[domain_idx], p_need);
    ps.rune_pool.power[domain_idx] -= p_from_pool;
    p_need -= p_from_pool;
    int p_from_uni = std::min(ps.rune_pool.universal_power, p_need);
    ps.rune_pool.universal_power -= p_from_uni;
    p_need -= p_from_uni;

    if (e_need == 0 && p_need == 0) {
        events_.logTrace("PAY_ADD: " + std::string(toString(player)) +
                         " [" + std::to_string(energy) + "][" +
                         toString(domain) + "] from pool");
        return true;
    }

    // Recycle matching runes for power (prefer exhausted to preserve ready).
    std::vector<Domain> wanted = {domain};
    auto matching = matchingRunesInBase(state_, player, wanted);
    // Sort: exhausted first (use them for power), then ready last.
    std::sort(matching.begin(), matching.end(),
              [&](GameObjectId a, GameObjectId b) {
                  return state_.getObject(a).is_exhausted >
                         state_.getObject(b).is_exhausted;
              });
    std::vector<GameObjectId> to_recycle;
    for (auto rid : matching) {
        if (p_need <= 0) break;
        to_recycle.push_back(rid);
        --p_need;
    }
    if (p_need > 0) return false;  // insufficient — canPay should have caught
    // Recycle inline (no EffectExecutor dependency).
    auto base_loc = BaseLocation{player};
    for (auto rid : to_recycle) {
        auto& rune = state_.getObject(rid);
        rune.location = std::nullopt;
        rune.zone = ZoneType::RuneDeck;
        rune.is_exhausted = false;
        ps.rune_deck.insert(ps.rune_deck.begin(), rid);
        events_.emit(LeftBoardEvent{rid, player, CardType::Rune,
            base_loc, ZoneType::RuneDeck, false});
    }
    if (!to_recycle.empty()) {
        events_.logTrace("PAY_ADD: " + std::string(toString(player)) +
                         " recycled " + std::to_string(to_recycle.size()) +
                         " " + toString(domain) + " rune(s) for power");
    }

    // Exhaust ready runes for the remaining energy. Skip any rune we
    // already recycled.
    std::set<GameObjectId> recycled(to_recycle.begin(), to_recycle.end());
    auto ready = readyRunesInBase(state_, player);
    int paid_e = 0;
    for (auto rid : ready) {
        if (e_need <= 0) break;
        if (recycled.count(rid)) continue;
        auto& rune = state_.getObject(rid);
        rune.is_exhausted = true;
        events_.emit(ObjectStateChangedEvent{rid, "exhausted"});
        --e_need;
        ++paid_e;
    }
    if (e_need > 0) return false;  // insufficient
    if (paid_e > 0) {
        events_.logTrace("PAY_ADD: " + std::string(toString(player)) +
                         " exhausted " + std::to_string(paid_e) +
                         " rune(s) for energy");
    }
    return true;
}

bool GameEngine::payTrashReplayGrant(PlayerId player, GameObjectId card_id) {
    auto& ps = state_.player(player);
    auto it = std::find_if(ps.trash_replay_grants.begin(),
                           ps.trash_replay_grants.end(),
                           [&](const PlayerState::TrashReplayGrant& g) {
                               return g.card == card_id;
                           });
    if (it == ps.trash_replay_grants.end()) return false;
    const auto grant = *it;   // copy before erase

    // Pick a payable domain for an [A] grant; otherwise use the fixed domain.
    Domain domain = grant.power_domain;
    if (grant.any_domain) {
        bool found = false;
        for (int di = 0; di < static_cast<int>(Domain::Count); ++di) {
            if (canPayAdditionalCost(player, grant.energy, grant.power,
                                      static_cast<Domain>(di))) {
                domain = static_cast<Domain>(di);
                found = true;
                break;
            }
        }
        if (!found) return false;  // action gen should have filtered this out
    }
    bool ok = payAdditionalCost(player, grant.energy, grant.power, domain);

    // Consume the grant whether or not payment fully succeeded — re-find since
    // payAdditionalCost may have mutated the vector indirectly (it does not,
    // but stay defensive against iterator invalidation).
    ps.trash_replay_grants.erase(
        std::remove_if(ps.trash_replay_grants.begin(),
                       ps.trash_replay_grants.end(),
                       [&](const PlayerState::TrashReplayGrant& g) {
                           return g.card == card_id;
                       }),
        ps.trash_replay_grants.end());

    if (ok) {
        events_.logTrace("TRASH_REPLAY: " + std::string(toString(player)) +
                         " replayed card id=" + std::to_string(card_id) +
                         " for [" + std::to_string(grant.energy) + "]+power");
    }
    return ok;
}

bool GameEngine::payCardCost(PlayerId player, GameObjectId card_obj) {
    // Bridge: legacy threaded path. Drives the cursor + queryAgent in
    // a loop. Same shape as the other commit-7 bridges
    // (runShowdownLoop, combatDamageStep). Step-machine dispatch
    // (commit 9) will use begin/advance/resolve directly.
    auto adv = beginCostPayment(player, card_obj);
    while (adv.kind == CostPaymentAdvance::Kind::NeedDecision) {
        // Always emit the decision snapshot (forced-choice visibility
        // — see prior commit). Trace label depends on phase.
        const char* label = state_.cost_cursor->phase ==
            GameState::CostPaymentCursor::Phase::PayingEnergy
            ? "pick rune to exhaust for energy"
            : "pick rune to recycle for power";
        state_.decision_index++;
        events_.logTrace("DECISION #" +
                          std::to_string(state_.decision_index) +
                          " (" + toString(player) + "): " + label +
                          " [" + std::to_string(adv.legal.size()) +
                          " options]");

        // Always query the agent — even when only one legal option
        // exists. Previously this short-circuited and auto-picked
        // adv.legal[0], which caused the agent's policy/training
        // signal to MISS the "forced last-rune" cost-payment step
        // (the last rune in a 3E/3-runes payment). The agent still
        // returns the only option for trivial cases; HumanAgent
        // auto-passes without surfacing UI, so the player UX is
        // unchanged. The win is training fidelity.
        auto chosen = getAgent(player).selectAction(state_, adv.legal);
        recordAppliedIntent(chosen);
        if (on_decision) on_decision(state_, adv.legal, chosen);
        adv = resolveCostPaymentDecision(chosen);
    }
    return true;
}

void GameEngine::moveUnit(GameObjectId unit_id, LocationId destination) {
    auto& unit = state_.getObject(unit_id);
    // Count this move for "moved N times this turn" effects (Kayn, Unleashed).
    // moveUnit is the single chokepoint for all unit movement (BF<->base and
    // BF<->BF, whether agent- or effect-driven), so counting here is complete.
    unit.moves_this_turn++;
    unit.location = destination;
    // Update zone type based on location
    if (std::holds_alternative<BaseLocation>(destination)) {
        unit.zone = ZoneType::Base;
    } else {
        unit.zone = ZoneType::BattlefieldZone;
    }

    // CR 719.3.a — when the top-most card moves, all attached cards move
    // with it. Phase 6q+ fix: previously attached gear stayed at the
    // original location, so a unit dragging Equipment from base to a BF
    // would leave its gear behind (or vice versa). Move every attached
    // object to the same destination and update its zone in lock-step.
    for (auto attached_id : unit.attachments) {
        if (!state_.objectExists(attached_id)) continue;
        auto& a = state_.getObject(attached_id);
        a.location = destination;
        if (std::holds_alternative<BaseLocation>(destination)) {
            a.zone = ZoneType::Base;
        } else {
            a.zone = ZoneType::BattlefieldZone;
        }
    }
}

void GameEngine::killUnit(GameObjectId unit_id) {
    auto& unit = state_.getObject(unit_id);
    auto controller = unit.controller;
    events_.logTrace("KILL: " + unit.name + " (id=" + std::to_string(unit_id) +
                     ", " + std::to_string(unit.current_might) + "M, dmg=" +
                     std::to_string(unit.damage_marked) + ")");

    // Deferred one-shot death replacement on the dying unit itself
    // (Tactical Retreat 737: "the next time it would die this turn, heal it,
    // exhaust it, and recall it instead"). Consulted before any source-based
    // replacement; consumes the flag and aborts the death.
    if (unit.death_replacement_recall_pending) {
        unit.death_replacement_recall_pending = false;
        events_.logDebug("REPLACEMENT (self): " + unit.name +
                         " heals/exhausts/recalls instead of dying");
        detachAllGear(unit_id);                // CR 719.5 — recall sheds gear
        auto old_loc = unit.location;
        unit.damage_marked = 0;
        unit.is_exhausted = true;
        unit.combat_designation = CombatDesignation::None;
        unit.location = BaseLocation{controller};
        unit.zone = ZoneType::Base;
        events_.emit(ObjectStateChangedEvent{unit_id, "healed"});
        events_.emit(UnitMovedEvent{unit_id, controller,
            old_loc.value_or(BaseLocation{controller}),
            BaseLocation{controller}, false});
        return;  // replacement consumed — unit does NOT die
    }

    // Structured replacement effects (Card::hasReplacementEffect /
    // applyReplacement). Preferred over the legacy ability_text scan below:
    // a card decides whether it replaces THIS unit's death (e.g. Guardian
    // Angel only protects the unit it's attached to). If applyReplacement
    // returns true the unit does not die.
    for (auto& [id, obj] : state_.objects) {
        if (obj.controller != controller) continue;
        if (id == unit_id) continue;
        if (!obj.location.has_value() && obj.zone != ZoneType::LegendZone) continue;
        if (obj.card_def_id == kInvalidId) continue;
        Card* card = card_registry_.get(obj.card_def_id);
        if (!card || !card->hasReplacementEffect()) continue;
        CardContext ctx{state_, events_, *effect_executor_, controller, id};
        if (card->applyReplacement(ctx, unit_id)) {
            events_.logDebug(std::string("REPLACEMENT: ") + obj.name +
                             " prevents " + unit.name + " from dying");
            return; // replacement consumed — unit does NOT die
        }
    }

    // Legacy replacement effects: "would die → instead heal/exhaust/recall"
    // Scan all objects controlled by the same player for replacement abilities
    for (auto& [id, obj] : state_.objects) {
        if (obj.controller != controller) continue;
        if (id == unit_id) continue;
        if (!obj.location.has_value() && obj.zone != ZoneType::LegendZone) continue;
        if (obj.card_def_id == kInvalidId) continue;

        const auto& def = card_db_.get(obj.card_def_id);
        auto& text = def.ability_text;

        // Check for "would die" replacement pattern
        if (text.find("would die") != std::string::npos &&
            text.find("instead") != std::string::npos) {

            // Replacement applies — heal, exhaust, recall instead of dying
            events_.logDebug(std::string("REPLACEMENT: ") + obj.name +
                             " prevents " + unit.name + " from dying");

            unit.damage_marked = 0;
            unit.is_exhausted = true;
            // Recall to base
            auto old_loc = unit.location;
            unit.location = BaseLocation{controller};
            unit.zone = ZoneType::Base;
            events_.emit(ObjectStateChangedEvent{unit_id, "healed"});
            events_.emit(UnitMovedEvent{unit_id, controller,
                old_loc.value_or(BaseLocation{controller}),
                BaseLocation{controller}, false});

            // If the replacement source is a gear/spell that says "kill this instead",
            // destroy the replacement source
            if (text.find("kill this instead") != std::string::npos ||
                text.find("kill me instead") != std::string::npos) {
                auto rep_was_at = obj.location;
                obj.zone = ZoneType::Trash;
                obj.location = std::nullopt;
                state_.player(obj.owner).trash.push_back(id);
                events_.emit(LeftBoardEvent{id, obj.controller, obj.card_type,
                    rep_was_at.value_or(BaseLocation{obj.controller}),
                    ZoneType::Trash, true});
            }

            return; // replacement consumed — unit does NOT die
        }
    }

    // No replacement — unit dies normally
    // Detach all gear first (CR 719.5)
    detachAllGear(unit_id);

    auto was_at = unit.location;
    int might = unit.current_might;

    // CR 183.1: tokens cease to exist when leaving the board. Route
    // them to Banishment (no trash entry) instead of trash. Matches
    // EffectExecutor::killObject — same rule, different kill path
    // (this one is combat lethal damage; killObject is for spell-
    // /ability-driven kills like Disintegrate).
    const bool is_token = (unit.super_type == SuperType::Token);
    unit.zone = is_token ? ZoneType::Banishment : ZoneType::Trash;
    unit.last_location = unit.location;  // preserve for Deathknell triggers
    unit.location = std::nullopt;
    unit.damage_marked = 0;
    unit.combat_designation = CombatDesignation::None;
    if (!is_token) {
        state_.player(unit.owner).trash.push_back(unit_id);
    } else {
        events_.logTrace("  TOKEN CEASES TO EXIST (CR 183.1): " + unit.name +
                         " (id=" + std::to_string(unit_id) + ")");
    }

    state_.turn.any_unit_died_this_turn = true;
    // Shadow Watcher: a friendly unit dying during its controller's Beginning
    // Phase (Awaken/Beginning/Scoring steps of that player's own turn).
    {
        TurnPhase ph = state_.turn.phase;
        bool in_beginning = (ph == TurnPhase::AwakenPhase ||
                             ph == TurnPhase::BeginningStep ||
                             ph == TurnPhase::ScoringStep);
        if (in_beginning && state_.turn.turn_player == controller)
            state_.player(controller).unit_died_in_beginning_this_turn = true;
    }
    events_.emit(UnitDiedEvent{unit_id, controller,
        was_at.value_or(BaseLocation{controller}), might});
    events_.emit(LeftBoardEvent{unit_id, controller, CardType::Unit,
        was_at.value_or(BaseLocation{controller}),
        is_token ? ZoneType::Banishment : ZoneType::Trash, true});
}

void GameEngine::healAllUnits() {
    for (auto& [id, obj] : state_.objects) {
        if (obj.isUnit() && obj.damage_marked > 0 && obj.location.has_value()) {
            obj.damage_marked = 0;
        }
    }
}

void GameEngine::emptyRunePools() {
    state_.players[0].rune_pool.clear();
    state_.players[1].rune_pool.clear();
}

void GameEngine::recordAppliedIntent(const Intent& chosen) {
    int slot = ::riftbound::openspiel::encodeAction(chosen, state_);
    if (slot >= 0) {
        state_.action_history.push_back(static_cast<int64_t>(slot));
    }
    // Negative slot means the Intent couldn't be encoded — should be rare
    // (action_vocab covers the full live legal set). Leaving the history
    // entry off is the safest fallback; the OpenSpiel state replay would
    // diverge anyway if there were a real encoding hole.
}

Intent GameEngine::queryAgent(PlayerId player) {
    auto actions = generateLegalActions();
    state_.decision_index++;
    if (player == PlayerId::Player1) state_.turn.turn_decisions_p1++;
    else state_.turn.turn_decisions_p2++;

    events_.logTrace("DECISION #" + std::to_string(state_.decision_index) +
                     " (" + toString(player) + "): " +
                     std::to_string(actions.size()) + " legal actions");

    auto chosen = getAgent(player).selectAction(state_, actions);
    recordAppliedIntent(chosen);

    {
        std::string choice_str = toString(chosen.type);
        if (chosen.card != kInvalidId && state_.objectExists(chosen.card)) {
            choice_str += " " + state_.getObject(chosen.card).name;
        }
        if (chosen.ability_source != kInvalidId && state_.objectExists(chosen.ability_source)) {
            choice_str += " (src=" + state_.getObject(chosen.ability_source).name + ")";
        }
        if (!chosen.targets.empty()) {
            choice_str += " tgt=[";
            for (size_t i = 0; i < chosen.targets.size(); ++i) {
                if (i > 0) choice_str += ",";
                if (state_.objectExists(chosen.targets[i]))
                    choice_str += state_.getObject(chosen.targets[i]).name;
                else
                    choice_str += "?" + std::to_string(chosen.targets[i]);
            }
            choice_str += "]";
        }
        if (!chosen.units_to_move.empty()) {
            choice_str += " units=[";
            for (size_t i = 0; i < chosen.units_to_move.size(); ++i) {
                if (i > 0) choice_str += ",";
                if (state_.objectExists(chosen.units_to_move[i]))
                    choice_str += state_.getObject(chosen.units_to_move[i]).name;
            }
            choice_str += "]";
        }
        // MakeChoice carries its "answer" in chosen_objects (discard pick,
        // predict recycle, rune exhaust target, etc.). Name them in the
        // trace so a reader can correlate the MakeChoice line with what
        // actually moved zones in the surrounding trace.
        if (!chosen.chosen_objects.empty()) {
            choice_str += " pick=[";
            for (size_t i = 0; i < chosen.chosen_objects.size(); ++i) {
                if (i > 0) choice_str += ",";
                auto oid = chosen.chosen_objects[i];
                if (state_.objectExists(oid))
                    choice_str += state_.getObject(oid).name +
                                  "(id=" + std::to_string(oid) + ")";
                else
                    choice_str += "?" + std::to_string(oid);
            }
            choice_str += "]";
        }
        if (chosen.chosen_battlefield != kInvalidId) {
            choice_str += " bf=" + std::to_string(chosen.chosen_battlefield);
        }
        // Player tag at the front so the line scans independently of
        // the surrounding DECISION # context.
        events_.logTrace("CHOSE: [" + std::string(toString(player)) +
                         "] " + choice_str);
    }

    if (on_decision) {
        on_decision(state_, actions, chosen);
    }
    return chosen;
}

AgentInterface& GameEngine::getAgent(PlayerId player) {
    return *agents_[playerIndex(player)];
}

PlayerId GameEngine::activePlayer() const {
    if (state_.turn.priority_holder.has_value())
        return *state_.turn.priority_holder;
    return state_.turn.turn_player;
}

BattlefieldState& GameEngine::getBattlefield(BattlefieldId id) {
    for (auto& bf : state_.battlefields) {
        if (bf.id == id) return bf;
    }
    throw std::runtime_error("Battlefield not found: " + std::to_string(id));
}

const BattlefieldState& GameEngine::getBattlefield(BattlefieldId id) const {
    for (auto& bf : state_.battlefields) {
        if (bf.id == id) return bf;
    }
    throw std::runtime_error("Battlefield not found: " + std::to_string(id));
}

// ═══════════════════════════════════════════════════════════════════════════════
// Battlefield Replace/Swap-back (CR 438)
// ═══════════════════════════════════════════════════════════════════════════════

void GameEngine::replaceBattlefield(BattlefieldId original_bf,
                                     GameObjectId token_bf_card,
                                     PlayerId controller) {
    auto& bf = getBattlefield(original_bf);

    events_.logTrace("BF_REPLACE: BF#" + std::to_string(original_bf) +
                     " replaced by token (id=" + std::to_string(token_bf_card) + ")");

    // Original BF card goes to Banishment
    if (state_.objectExists(bf.card_object_id)) {
        auto& orig = state_.getObject(bf.card_object_id);
        auto owner = orig.owner;
        orig.zone = ZoneType::Banishment;
        orig.location = std::nullopt;
        state_.player(owner).banishment.push_back(bf.card_object_id);
    }

    // Token takes the slot
    bf.replaced_card = bf.card_object_id;
    bf.was_replaced = true;
    bf.is_token = true;
    bf.card_object_id = token_bf_card;
    bf.contributed_by = controller;

    // Set token card location
    auto& token_obj = state_.getObject(token_bf_card);
    token_obj.zone = ZoneType::BattlefieldZone;
    token_obj.location = BattlefieldLocation{original_bf};
}

void GameEngine::swapBackBattlefield(BattlefieldId token_bf) {
    auto& bf = getBattlefield(token_bf);
    if (!bf.was_replaced || !bf.replaced_card.has_value()) return;

    auto original_card = *bf.replaced_card;

    events_.logTrace("BF_SWAPBACK: BF#" + std::to_string(token_bf) +
                     " restoring original card (id=" + std::to_string(original_card) + ")");

    // Remove token card
    auto& token_obj = state_.getObject(bf.card_object_id);
    token_obj.zone = ZoneType::Trash;
    token_obj.location = std::nullopt;

    // Restore original from Banishment
    if (state_.objectExists(original_card)) {
        auto& orig = state_.getObject(original_card);
        auto owner = orig.owner;
        // Remove from banishment
        auto& ban = state_.player(owner).banishment;
        ban.erase(std::remove(ban.begin(), ban.end(), original_card), ban.end());

        orig.zone = ZoneType::BattlefieldZone;
        orig.location = BattlefieldLocation{token_bf};
    }

    bf.card_object_id = original_card;
    bf.replaced_card = std::nullopt;
    bf.was_replaced = false;
    bf.is_token = false;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Equip/Attach (CR 716-725)
// ═══════════════════════════════════════════════════════════════════════════════

bool GameEngine::attachGearToUnit(GameObjectId gear_id, GameObjectId unit_id) {
    if (!state_.objectExists(gear_id) || !state_.objectExists(unit_id)) return false;

    auto& gear = state_.getObject(gear_id);
    auto& unit = state_.getObject(unit_id);

    if (!gear.isGear() || !unit.isUnit()) return false;
    if (gear.attached_to.has_value()) return false; // already attached

    events_.logTrace("ATTACH: " + gear.name + " (id=" + std::to_string(gear_id) +
                     ") -> " + unit.name + " (id=" + std::to_string(unit_id) + ")");

    gear.attached_to = unit_id;
    gear.is_rules_text_inactive = true; // CR 718.2: rules text inactive while attached
    unit.attachments.push_back(gear_id);

    // Move gear to same location as unit
    gear.location = unit.location;
    gear.zone = unit.zone;

    // Apply might bonus (CR 472.3.c — Arithmetic layer)
    unit.attachment_might_bonus += gear.might_bonus;
    unit.recomputeMight();

    events_.emit(ObjectStateChangedEvent{gear_id, "attached"});
    events_.emit(ObjectStateChangedEvent{unit_id, "equipped"});

    return true;
}

void GameEngine::detachAllGear(GameObjectId unit_id) {
    if (!state_.objectExists(unit_id)) return;
    auto& unit = state_.getObject(unit_id);

    for (auto gear_id : unit.attachments) {
        if (!state_.objectExists(gear_id)) continue;
        auto& gear = state_.getObject(gear_id);

        events_.logTrace("DETACH: " + gear.name + " from " + unit.name);

        unit.attachment_might_bonus -= gear.might_bonus;
        gear.attached_to = std::nullopt;
        gear.is_rules_text_inactive = false;
        // Detached gear stays at its current location (CR 719.5)
        events_.emit(ObjectStateChangedEvent{gear_id, "detached"});
    }
    unit.attachments.clear();
    unit.recomputeMight();
}

} // namespace riftbound
