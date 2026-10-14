#include <cstdio>
#include <cstdlib>
#include "engine/resource_payment.h"
#include "core/board_state.h"
#include "engine/deck_ops.h"
#include "engine/trash_replacement.h"
#include "game_engine.h"
#include "engine/keyword_abilities.h"
#include "cards/card.h"
#include "cards/card_helpers.h"
#include "openspiel/action_vocab.h"
#ifdef RIFTBOUND_CHECK_BOARD_INDEX
#include "ml/feature_extractor.h"   // cleanup(): the checked build compares what players observe
#endif

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

    // What players are shown. Every CardRevealedEvent reaches the knowledge
    // tracker here, for exactly the players the event names
    // (knowledge::recordReveal -> knowledge::shown), and bumps their
    // PlayerState::observed_cards count. Cards announce a reveal or a look
    // with the event; they do not tell the tracker themselves.
    card_revealed_conn_ = events_.on_card_revealed.connect(
        [this](const CardRevealedEvent& e) { knowledge::recordReveal(state_, e); });

    // RIFTBOUND_TRACE_STDERR=1 streams every engine log line to stderr as it
    // happens. Runners buffer the log and print it when a game finishes, so
    // this is how to see where a game that never finishes is stuck.
    if (std::getenv("RIFTBOUND_TRACE_STDERR")) {
        trace_stderr_conn_ = events_.on_log.connect([](const LogEvent& e) {
            std::fprintf(stderr, "%s\n", e.message.c_str());
        });
    }

    // Subsystems exist from construction so every engine operation is usable
    // on a hand-built state (tests drive phases and effects without running a
    // whole game). runGame / resumeFromSnapshot rebuild them for the new state.
    initSubsystems();
}

void GameEngine::initSubsystems() {
    // The trigger manager holds a reference to the chain manager and is
    // subscribed to the event bus: drop it first so nothing dangles.
    trigger_manager_.reset();
    chain_manager_ = std::make_unique<ChainManager>(state_, events_, card_db_);
    chain_manager_->setAffordCheck(
        [this](PlayerId p, GameObjectId card) { return canAfford(p, card); });
    // Every play pays the same way: the additional cost the card prints
    // (CR 356.2), then its cost with what that took off.
    chain_manager_->setPayCost([this](PlayerId p, GameObjectId card) {
        return payCardCost(p, card, payAdditionalPlayCosts(p, card, 0));
    });
    effect_executor_ = std::make_unique<EffectExecutor>(state_, events_, card_db_, &card_registry_);
    effect_executor_->setRng(&rng_);
    effect_executor_->setAgentQuery(
        [this](PlayerId p, const std::vector<Intent>& actions) -> Intent {
            return queryAgentForChain(p, actions);
        });
    effect_executor_->setAuraRecalc([this]() { recalculateAuras(); });
    effect_executor_->setQueueSpell([this](PlayerId p, GameObjectId spell) {
        state_.getObject(spell).zone = ZoneType::Chain;
        const ChainItemId id = chain_manager_->addSpell(spell, p, {});
        // An effect that plays a spell performs the whole play: its choices
        // are made now, as it is put on the chain — before the rest of the
        // effect that played it — not at the chain's next Finalize step.
        // That IS its finalization: the item is marked Finalized and announced
        // here, so the chain's Finalize step has nothing left to do for it.
        for (auto& ci : state_.chain.items)
            if (ci.id == id) {
                ci.status = ChainItemStatus::Finalized;
                chooseTargetsAtFinalize(ci);
                break;
            }
        for (const auto& ci : state_.chain.items)
            if (ci.id == id) {
                events_.emit(ChainItemFinalizedEvent{ci.id, ci.source, ci.controller});
                break;
            }
    });
    effect_executor_->setPlayCostFns(
        [this](PlayerId p, GameObjectId card) { return canAfford(p, card); },
        [this](PlayerId p, GameObjectId card) {
            // [Accelerate] is decided with the play's other choices, before the
            // cost, and paid with it (CR 355.1.a; testing phase D58).
            auto adj = payAdditionalPlayCosts(p, card, 0);
            adj = decideAccelerate(p, card, adj, 0);
            return payCardCost(p, card, adj);
        });
    effect_executor_->setOfferAccelerate([this](PlayerId p, GameObjectId card, bool from_hand) {
        return offerAccelerateOnItsOwn(p, card, from_hand);
    });
    chain_manager_->setEffectExecutor(effect_executor_.get());
    // CR 334: the chain's loop handles outstanding tasks — the Cleanup —
    // before it finalizes, after every finalization and after every
    // resolution (CR 319.3-319.5).
    chain_manager_->setHandleTasks([this]() { cleanup(); });
    chain_manager_->setOrderPending([this]() { orderPendingTriggers(); });
    chain_manager_->setActivity([this]() { return events_.lineStamp(); });
    // Tournament Rules 505.3-505.6: a loop that needs no player to maintain
    // it (505.3.a) and that no player chose to break — everyone passed
    // priority each time round — "ends in a draw". That is what a chain at
    // its bound is when no decision with more than one option was made in the
    // whole run. With such a decision in it, it is a loop a player keeps
    // going, which the engine ends before the bound (Card::confirmOptional,
    // ChainState::kMayLoopIterations); reaching the bound anyway is an error.
    chain_manager_->setUnbrokenLoop([this](const ChainItem& top) {
        if (decisions_with_a_choice_ != chain_run_choices_at_start_) return false;
        const std::string name = state_.objectExists(top.source)
            ? state_.getObject(top.source).name.get() : std::string("?");
        RIFT_WARN(events_, "LOOP: the chain did not empty and no player made a choice in it; "
                        "its newest item is " + name + " — the game is a draw "
                        "(Tournament Rules 505.6)");
        state_.game_over = true;
        state_.winner = PlayerId::None;
        state_.game_over_reason =
            "Draw: a loop of mandatory actions no player broke (Tournament Rules 505.6), at " + name;
        events_.emit(GameOverEvent{PlayerId::None, state_.game_over_reason});
        return true;
    });
    chain_manager_->setFinalizeChoices([this](ChainItem& item) { chooseTargetsAtFinalize(item); });
    chain_manager_->setActivateAbility(
        [this](const Intent& i) { return payAndQueueAbility(i); });
    chain_manager_->setPlayCard([this](const Intent& i) { executeIntent(i); });
    trigger_manager_ = std::make_unique<TriggerManager>(
        state_, events_, card_db_, *chain_manager_, card_registry_);
    trigger_manager_->setEffectExecutor(effect_executor_.get());
    // [Add] Reactions while a cost is being paid (CR 430.3, 445.2.c, 357.1.a;
    // testing phase D30).
    effect_executor_->setAddAbilities(
        [this](PlayerId p) { return addReactionsFor(p); },
        [this](const Intent& i) { (void)payAndQueueAbility(i); },
        [this](PlayerId p, const std::function<bool(const GameState&)>& check) {
            return probeAdds(p, check);
        });
    trigger_manager_->setResolveAddNow(
        [this](ChainItemId id, OpenClosedState before, bool was_empty) {
            resolveAddAbilityNow(id, before, was_empty);
        });
    effect_executor_->setRechoose([this](ChainItemId id) { makeNewChoicesFor(id); });
    effect_executor_->setQueueTrigger(
        [this](GameObjectId source, PlayerId controller, TriggerType t, GameObjectId subject) {
            trigger_manager_->queueTrigger(source, controller, t, subject);
        });
    effect_executor_->setQueueReflexive(
        [this](GameObjectId source, PlayerId controller, int index, std::vector<int32_t> data) {
            trigger_manager_->queueReflexive(source, controller, index, std::move(data));
        });
    trigger_manager_->subscribe();
}

GameEngine::~GameEngine() {
    // Disconnect the on_card_revealed subscriber before this is freed.
    // Without this, a successor engine on the same EventBus would
    // emit reveals into a dangling `this`.
    if (card_revealed_conn_.connected()) card_revealed_conn_.disconnect();
    // A step-machine game still in progress is abandoned: StepDriver's
    // destructor unwinds the engine fiber (it does not play the game out).
    // Do that first, while everything the parked frames refer to is intact.
    step_driver_.reset();
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
    forgetCleanupFacts();
    state_.mode = ModeOfPlay{};

    initSubsystems();

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
        if (step_driver_->failure()) {
            // The fiber ended by exception: there is no game result. Make
            // sure nothing reads the default "no winner" as a draw.
            step_result_ = GameResult{};
            step_result_.termination_reason = "engine_error";
        }
        return;
    }
    // The driver is parked at a decision: tidy what each player knows before
    // an agent looks at the state.
    knowledge::prune(state_);
    current_step_.kind = StepKind::NeedDecision;
    current_step_.legal = step_driver_->legalActionsSnapshot();
    current_step_.perspective = step_driver_->currentPlayerSnapshot();
}

const StepResult& GameEngine::beginGame(
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
        step_result_ = runGame(d1, d2, driver->seat(PlayerId::Player1),
                               driver->seat(PlayerId::Player2), seed);
        driver->markDone();
    });

    // No-op for fiber backend (selectAction's yield already returned
    // us here). Kept for API symmetry with the old thread backend.
    step_driver_->waitForDecision();
    refreshStepFromDriver();
    rethrowIfStepFailed();
    return current_step_;
}

void GameEngine::rethrowIfStepFailed() const {
    if (step_driver_ && step_driver_->failure()) {
        std::rethrow_exception(step_driver_->failure());
    }
}

bool GameEngine::isResumableDecision(const GameState& state,
                                     const std::vector<Intent>& legal) {
    // Only the top-level main-phase decision (mainPhaseLoop -> queryAgent) is
    // resumable: everything the engine was doing is in GameState and the
    // loop can simply be entered again. Every other decision is asked from
    // inside a call that is not captured by the state -- a chain being
    // resolved, a showdown, combat, a cost being paid, a card's play-time
    // prompt (the card has left its zone but is on no chain yet).
    //
    // generateMainPhaseActions is the only producer of EndTurn, and always
    // offers it, so an EndTurn option identifies that decision. The state
    // checks below are implied by it; they stay as a guard.
    bool offers_end_turn = false;
    for (const auto& intent : legal) {
        if (intent.type == IntentType::EndTurn) { offers_end_turn = true; break; }
    }
    if (!offers_end_turn) return false;
    if (state.game_over) return false;
    if (state.turn.phase != TurnPhase::MainPhase) return false;
    if (!state.turn.isNeutralOpen()) return false;
    if (state.chain.exists() || state.cost_cursor.has_value()) return false;
    for (const auto& bf : state.battlefields) {
        if (bf.combat_in_progress || bf.showdown_in_progress ||
            bf.combat_staged || bf.showdown_staged) {
            return false;
        }
    }
    return true;
}

const StepResult& GameEngine::resumeFromSnapshot(GameState snapshot_state,
                                            const std::mt19937_64& rng_state,
                                            const StepResult& snapshot_step) {
    if (step_driver_) {
        throw std::logic_error(
            "GameEngine::resumeFromSnapshot called on an engine that already "
            "has a step-machine session. Construct a fresh GameEngine.");
    }

    if (snapshot_step.kind != StepKind::NeedDecision ||
        !isResumableDecision(snapshot_state, snapshot_step.legal)) {
        throw std::invalid_argument(
            "GameEngine::resumeFromSnapshot: the snapshot is not at a "
            "top-level main-phase decision (see isResumableDecision).");
    }

    // Substitute the snapshotted state, and the RNG stream with it: the
    // engine's randomness (shuffles) is part of the position.
    state_ = std::move(snapshot_state);
    forgetCleanupFacts();
    rng_ = rng_state;

    // The snapshot was taken inside queryAgent, after it had counted this
    // decision. mainPhaseLoop asks the same question again; un-count it so
    // the resumed engine's counters match the engine it was copied from.
    state_.decision_index--;
    if (state_.turn.turn_player == PlayerId::Player1) state_.turn.turn_decisions_p1--;
    else state_.turn.turn_decisions_p2--;

    // Initialise subsystems (parallel to runGame's first block). These
    // hold references into `state_` and `events_`, which are now
    // populated from the snapshot.
    initSubsystems();

    step_driver_ = std::make_unique<StepDriver>();
    step_result_ = GameResult{};
    StepDriver* driver = step_driver_.get();
    agents_[0] = &driver->seat(PlayerId::Player1);
    agents_[1] = &driver->seat(PlayerId::Player2);

    // Capture the resume point — the phase the snapshot was at. The
    // dispatcher below uses this to skip already-completed phases.
    TurnPhase resume_at = state_.turn.phase;
    PlayerId  turn_player = state_.turn.turn_player;

    driver->runInFiber([this, resume_at, turn_player, driver]() {
        // Finish the current turn from where it paused, then continue
        // the normal turn loop. Mirrors the body of runTurnLoop minus
        // the first-turn entry. An exception leaves through the fiber
        // (StepDriver keeps it) and is rethrown to the step API's caller.
        {
            if (!state_.game_over) {
                runTurnFromPhase(turn_player, resume_at);
            }
            if (!state_.game_over) {
                // The same turn transition runTurnLoop makes.
                advanceTurnQueue();
                runTurnLoop();
            }

            // Finalise (mirrors runGame's tail).
            step_result_.winner          = state_.winner;
            step_result_.final_scores[0] = state_.players[0].score;
            step_result_.final_scores[1] = state_.players[1].score;
            step_result_.total_turns     = state_.turn.turn_number;
            step_result_.total_decisions = state_.decision_index;
            step_result_.termination_reason = state_.game_over_reason;
        }
        driver->markDone();
    });

    step_driver_->waitForDecision();
    refreshStepFromDriver();
    rethrowIfStepFailed();
    // The resumed engine must be asking the question the snapshot was taken
    // at. Anything else means the snapshot was not a faithful copy.
    if (current_step_.kind != StepKind::NeedDecision ||
        current_step_.perspective != snapshot_step.perspective ||
        current_step_.legal != snapshot_step.legal) {
        throw std::logic_error(
            "GameEngine::resumeFromSnapshot: the resumed engine did not "
            "reach the snapshotted decision.");
    }
    return current_step_;
}

const StepResult& GameEngine::currentStep() const {
    rethrowIfStepFailed();
    return current_step_;
}

const StepResult& GameEngine::applyChoice(int legal_index) {
    if (!step_driver_) {
        throw std::logic_error(
            "GameEngine::applyChoice called before beginGame.");
    }
    rethrowIfStepFailed();
    if (current_step_.kind == StepKind::Done) {
        return current_step_;
    }
    if (legal_index < 0 ||
        static_cast<size_t>(legal_index) >= current_step_.legal.size()) {
        throw std::out_of_range(
            "GameEngine::applyChoice: legal_index " +
            std::to_string(legal_index) + " is not one of the " +
            std::to_string(current_step_.legal.size()) + " legal actions.");
    }
    step_driver_->provideChoice(legal_index);
    step_driver_->waitForDecision();
    refreshStepFromDriver();
    rethrowIfStepFailed();
    return current_step_;
}

bool GameEngine::isStepDone() const {
    if (!step_driver_) return false;
    return step_driver_->isDone();
}

bool GameEngine::stepFailed() const {
    return step_driver_ && step_driver_->failure();
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
    ps.chosen_champion = champ_id;

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
    // Each player picks one of their three battlefields. CR 487.5 (1v1 Duel,
    // best of one) selects it at random; CR 488.5 (1v1 Match) lets the player
    // select it. An engine game asks the agent — the Match reading; a
    // RandomAgent's uniform pick is what a Duel would do. Which mode an engine
    // game is was left to the project (testing phase G4, pinned by
    // Batch4bTest.G4_Pin_EachPlayerChoosesTheirBattlefield).
    //
    // The choice is Intent::battlefield_pool_index: the position of the
    // card in the deck's battlefield list (0..N-1). It is not a
    // BattlefieldId — no battlefield exists on the board yet.
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
            choices.push_back(Intent::chooseBattlefield(
                player, BattlefieldPoolIndex::fromRaw(i)));
        }

        // Briefly mark this player as the decision-maker so HumanAgent's
        // UI snapshot (which reads state.turn.turn_player) shows the
        // correct seat. Restore turn_player after the query — the real
        // turn order isn't owned by this setup step.
        PlayerId saved_turn = state_.turn.turn_player;
        state_.turn.turn_player = player;
        state_.decision_index++;
        RIFT_TRACE_DECISION(events_, "DECISION #" + std::to_string(state_.decision_index) +
                          " (" + toString(player) + " — battlefield setup): " +
                          std::to_string(choices.size()) + " options");
        auto chosen = askAgent(player, choices);
        state_.turn.turn_player = saved_turn;

        // askAgent accepts only an offered option, so the index is in range
        // unless the player conceded (no index): the first card then.
        size_t idx = static_cast<size_t>(chosen.battlefield_pool_index.raw());
        if (idx >= bfs.size()) idx = 0;
        RIFT_TRACE(events_, "  CHOSE BF #" + std::to_string(idx) + " (" +
                          card_db_.get(bfs[idx]).name + ")");
        return bfs[idx];
    };

    auto bf1_def = pickFromDeck(PlayerId::Player1, deck1.battlefields);
    auto bf2_def = pickFromDeck(PlayerId::Player2, deck2.battlefields);

    // Create battlefield game objects and state
    uint32_t next_bf_slot = 0;  // a battlefield's id is its slot

    auto addBF = [&](CardDefId def_id, PlayerId contributor) {
        auto obj_id = instantiateCard(def_id, contributor);
        auto& obj = state_.getObject(obj_id);
        obj.zone = ZoneType::BattlefieldZone;

        BattlefieldState bf;
        bf.id = BattlefieldId::fromRaw(next_bf_slot++);
        bf.card_object_id = obj_id;
        bf.contributed_by = contributor;

        // Static restrictions and turn-gated scoring are structured
        // properties the battlefield card declares (Vilemaw's Lair, Rockfall
        // Path, Forgotten Monument). Engine asks; card answers. The flags are
        // consulted by the action generators (move + play).
        if (auto* bfc = dynamic_cast<const BattlefieldCard*>(
                card_registry_.get(def_id))) {
            bf.blocks_move_to_base = bfc->blocksMoveToBase();
            bf.blocks_unit_play = bfc->blocksUnitPlay();
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
    state_.turn.regular_turn_player = state_.turn.turn_player;
    state_.turn.turn_number = 0;

    // Mark both players as being on their first turn
    state_.players[0].is_first_turn = true;
    state_.players[1].is_first_turn = true;
}

void GameEngine::drawOpeningHands() {
    // Each player draws 4 (CR 117)
    RIFT_TRACE(events_, "── Opening Hands ──");
    RIFT_TRACE(events_, std::string("DRAW_HAND: ") + toString(PlayerId::Player1) + " draws 4");
    drawCards(PlayerId::Player1, 4);
    RIFT_TRACE(events_, std::string("DRAW_HAND: ") + toString(PlayerId::Player2) + " draws 4");
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
    if (!events_.logging()) {
        events_.noteUnheardLine();   // the CHOSE line below, unread
        if (!chosen.cards_to_mulligan.empty()) events_.noteUnheardLine();   // two copies
    } else {
        std::string who = std::string("[") + toString(chosen.player) + "] ";
        if (chosen.cards_to_mulligan.empty()) {
            RIFT_TRACE(events_, "CHOSE: " + who + "Keep hand");
        } else {
            std::string mull_str;
            for (auto cid : chosen.cards_to_mulligan) {
                if (!mull_str.empty()) mull_str += ", ";
                if (state_.objectExists(cid)) mull_str += state_.getObject(cid).name;
            }
            // Which cards are set aside is the player's own knowledge; the
            // other player learns how many (testing phase D11).
            const std::string count = std::to_string(chosen.cards_to_mulligan.size());
            RIFT_TRACE_FOR(events_, LogAudience::only(chosen.player),
                           "CHOSE: " + who + "Mulligan " + count + " [" + mull_str + "]");
            RIFT_TRACE_FOR(events_, LogAudience::only(opponent(chosen.player)),
                           "CHOSE: " + who + "Mulligan " + count);
        }
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
    RIFT_TRACE(events_, "── Mulligan Phase ──");
    state_.turn.phase = TurnPhase::Mulligan;

    for (auto player : {state_.turn.turn_player,
                        opponent(state_.turn.turn_player)}) {
        auto adv = advanceMulligan(player);
        if (adv.kind == MulliganAdvance::Kind::Done) continue;

        state_.decision_index++;
        if (player == PlayerId::Player1) state_.turn.turn_decisions_p1++;
        else state_.turn.turn_decisions_p2++;

        RIFT_TRACE_DECISION(events_, "DECISION #" + std::to_string(state_.decision_index) +
                         " (" + toString(player) + "): Mulligan, " +
                         std::to_string(adv.legal.size()) + " options");

        auto chosen = askAgent(player, adv.legal);

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
    playing_a_game_ = true;
    // Safety limit to prevent infinite games
    constexpr int kMaxTurns = 200;

    while (!state_.game_over && state_.turn.turn_number < kMaxTurns) {
        runTurn(state_.turn.turn_player);

        if (state_.game_over) break;

        advanceTurnQueue();
    }

    if (!state_.game_over) {
        // Max turns reached — draw
        state_.game_over = true;
        state_.winner = PlayerId::None;
        state_.game_over_reason = "Max turns reached";
        events_.emit(GameOverEvent{PlayerId::None, "Max turns reached"});
    }
}

void GameEngine::advanceTurnQueue() {
    // Turn order is a looping queue of the players' regular turns (CR 736).
    // An Additional Turn is inserted right after the current turn (CR 735,
    // 738: the newest first); it does not change the Turn Order, and after it
    // the queue goes on with its previously queued turns (CR 737).
    auto& t = state_.turn;
    if (!t.is_additional_turn) t.regular_turn_player = t.turn_player;
    if (!t.additional_turn_queue.empty()) {
        const PlayerId next = t.additional_turn_queue.front();
        t.additional_turn_queue.erase(t.additional_turn_queue.begin());
        t.turn_player = next;
        t.is_additional_turn = true;
        RIFT_TRACE(events_, std::string("ADDITIONAL_TURN: ") + toString(next));
    } else {
        t.turn_player = opponent(t.regular_turn_player);
        t.is_additional_turn = false;
    }
    t.turn_number++;
}

void GameEngine::runTurn(PlayerId player) {
    RIFT_TRACE(events_, "════ TURN " + std::to_string(state_.turn.turn_number) +
                     " (" + toString(player) + ") ════ Score: P1=" +
                     std::to_string(state_.players[0].score) + " P2=" +
                     std::to_string(state_.players[1].score));
    // "This turn" tracking is per turn, not per player: both players' counters
    // reset when any turn starts.
    state_.player(PlayerId::Player1).resetTurnTracking();
    state_.player(PlayerId::Player2).resetTurnTracking();
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
    RIFT_TRACE(events_, "════ RESUME TURN " + std::to_string(state_.turn.turn_number) +
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
            //
            // The loop's action counter (the per-turn safety cap) is the one
            // piece of main-phase progress that lives on the stack, not in
            // GameState. It equals the number of main-phase decisions the
            // turn player has completed this turn, which queryAgent counts
            // in turn_decisions_pN (resumeFromSnapshot has already un-counted
            // the decision being re-asked).
            mainPhaseLoop(resume_at == TurnPhase::MainPhase
                ? (player == PlayerId::Player1 ? state_.turn.turn_decisions_p1
                                               : state_.turn.turn_decisions_p2)
                : 0);
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
    RIFT_TRACE(events_, "── Awaken Phase ──");
    auto old_phase = state_.turn.phase;
    state_.turn.phase = TurnPhase::AwakenPhase;
    events_.emit(PhaseChangedEvent{old_phase, TurnPhase::AwakenPhase,
                                    state_.turn.turn_player});

    // Count this player's own turns (drives turn-gated rules, e.g. Forgotten
    // Monument). 1 on their first turn.
    state_.player(state_.turn.turn_player).turns_taken++;

    // Ready all game objects the turn player controls (CR 315.1.b)
    auto player = state_.turn.turn_player;
    for (auto& [id, obj] : state_.objects.onBoard()) {
        if (obj.controller == player && obj.is_exhausted && obj.location.has_value()) {
            const Card* card = obj.card_def_id != kInvalidId
                ? card_registry_.get(obj.card_def_id) : nullptr;
            if (card && !card->canBeReadied()) continue;   // "I can't be readied."
            obj.is_exhausted = false;
            events_.emit(ObjectStateChangedEvent{id, "readied"});
            events_.emit(ObjectStateChangedEvent{id, "became_ready"});
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
    // CR 335: a phase is left only with no outstanding task and nothing
    // pending — "When I become ready" abilities resolve in this phase.
    runChain();
}

void GameEngine::beginningStep() {
    RIFT_TRACE(events_, "── Beginning Step ──");
    auto old_phase = state_.turn.phase;
    state_.turn.phase = TurnPhase::BeginningStep;

    // [Temporary] (CR 816.1) is a triggered ability: "At the start of this
    // permanent's controller's Beginning Phase, before scoring, kill this."
    // TriggerManager queues one for each such permanent with the other
    // "start of your Beginning Phase" abilities as the phase is announced;
    // their controller orders them (CR 383.3.d), players get priority, and
    // all of it is resolved by the runChain below — before the Scoring Step.
    events_.emit(PhaseChangedEvent{old_phase, TurnPhase::BeginningStep,
                                    state_.turn.turn_player});

    // "At the start of your Beginning Phase" abilities resolve, each followed
    // by a Cleanup (CR 319), before the Scoring Step: a battlefield whose last
    // unit just died ([Temporary], a start-of-turn kill) is no longer
    // controlled and scores no Hold.
    runChain();
}

void GameEngine::scoringStep() {
    RIFT_TRACE(events_, "── Scoring Step ──");
    auto old_phase = state_.turn.phase;
    state_.turn.phase = TurnPhase::ScoringStep;
    events_.emit(PhaseChangedEvent{old_phase, TurnPhase::ScoringStep,
                                    state_.turn.turn_player});

    // CR 315.2.b.2: "The Turn Player Holds all Battlefields they Control" —
    // one action: every hold is scored, and only then is anything finalized.
    // The hold abilities are one batch, which their controllers order (CR
    // 383.3.d); nothing an ability or a reaction does can change whether the
    // next battlefield is held (finding D72: each hold's abilities used to
    // resolve before the next hold). A hold that wins the game ends it there
    // (scoreHold checks the win as the point is scored).
    auto player = state_.turn.turn_player;
    {
        const SimultaneousTriggers together(state_.chain);
        for (size_t bi = 0; bi < state_.battlefields.size(); ++bi) {
            const BattlefieldId held_id = state_.battlefields[bi].id;
            const auto held_by = state_.battlefields[bi].controller;
            if (held_by.has_value() && *held_by == player) {
                scoreHold(player, held_id);
                if (state_.game_over) return;
            }
        }
    }

    runChain();
}

void GameEngine::channelPhase() {
    RIFT_TRACE(events_, "── Channel Phase ──");
    auto old_phase = state_.turn.phase;
    state_.turn.phase = TurnPhase::ChannelPhase;
    events_.emit(PhaseChangedEvent{old_phase, TurnPhase::ChannelPhase,
                                    state_.turn.turn_player});

    auto player = state_.turn.turn_player;
    int count = 2;

    // The second player channels an extra rune on their first turn (CR 487.7 /
    // 488.7, the First Turn Process of a 1v1 Duel / Match).
    // Who went first is what the coin toss recorded (CR 116), not whose turn
    // number 0 happens to be: an extra turn or a changed turn order does not
    // make anyone else "the second player".
    const PlayerId first_player = state_.turn.starting_player;
    // The player who did NOT go first gets the extra channel on their first turn
    if (state_.player(player).is_first_turn && player != first_player) {
        count = 3;
    }
    // Sandstone Chimera: "players only channel 1 rune at the start of their
    // Channel Phase."
    if (state_.turn.channel_phase_cap > 0)
        count = std::min(count, state_.turn.channel_phase_cap);

    channelRunes(player, count);
    runChain();   // CR 335: nothing is pending when the phase ends
}

void GameEngine::drawPhase() {
    RIFT_TRACE(events_, "── Draw Phase ──");
    auto old_phase = state_.turn.phase;
    state_.turn.phase = TurnPhase::DrawPhase;
    events_.emit(PhaseChangedEvent{old_phase, TurnPhase::DrawPhase,
                                    state_.turn.turn_player});

    auto player = state_.turn.turn_player;

    // Skip (CR 444): "Skip your Draw Phase." replaces the phase with nothing —
    // no draw, and nothing that would happen as a result of it.
    if (state_.player(player).skip_draw_phase) {
        RIFT_TRACE(events_, "DRAW PHASE: skipped (CR 444)");
        emptyRunePools();
        runChain();
        return;
    }

    // Draw 1 card (CR 315.4.b). In 1v1 the player going first draws too: the
    // First Turn Process of a Duel and a Match (CR 487.7 / 488.7) is only the
    // second player's extra rune (channelPhase). "The player going first does
    // not draw a card during their first Draw Phase" is the text of the
    // multiplayer modes (CR 489.7 / 490.7 / 491.7), which this engine does
    // not play.
    drawCards(player, 1);

    // Empty rune pools (CR 315.4.d)
    emptyRunePools();
    // CR 334.2 / 335: what the draw triggered was pending while the phase's
    // tasks were done; it resolves before the Main Phase begins.
    runChain();
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
    if (state_.game_over) {
        adv.kind = MainPhaseAdvance::Kind::Done;
        return adv;
    }
    // Engine bound: every main-phase action costs something (a card, a rune,
    // an exhaustion), so no turn comes near this. Reaching it means an action
    // that can be repeated for free — an engine or card defect, reported as
    // one. (It used to end the Main Phase quietly.)
    if (action_count >= kMainPhaseMaxActions)
        throw std::logic_error("GameEngine::advanceMainPhase: " +
                               std::to_string(kMainPhaseMaxActions) +
                               " actions in one Main Phase (turn " +
                               std::to_string(state_.turn.turn_number) + ")");

    // Everything that is pending is settled before the turn player is asked:
    // staged showdowns and combats run, and whatever they (or the action
    // before) left on the chain resolves — a "When I conquer" from a showdown,
    // a death trigger from a combat. The main-phase decision is never reached
    // with an item on the chain or a Closed State. Running a combat can stage
    // another (CR 468.3.d.1), so this repeats until nothing is left.
    constexpr int kMaxSettlePasses = 64;
    for (int pass = 0; ; ++pass) {
        if (pass >= kMaxSettlePasses)
            throw std::logic_error("GameEngine::advanceMainPhase: staged showdowns, "
                                   "combats and the chain did not settle");
        bool progressed = false;

        // Check for staged showdowns/combats after cleanup
        processContestedBattlefields();
        if (state_.game_over) {
            adv.kind = MainPhaseAdvance::Kind::Done;
            return adv;
        }

        // CR 323.11 / 323.12 (Cleanup steps 9 and 10): in a Neutral Open
        // State, one staged showdown begins — at a battlefield with no combat
        // staged — or, with none, one staged combat; with several, the turn
        // player chooses where (finding I1). One per pass: what it leaves
        // (a conquer, a death, another staged combat, CR 468.3.d.1) is
        // settled by the next pass's Cleanup before the next one begins.
        bool is_combat = false;
        const BattlefieldId next = chooseNextStaged(is_combat);
        if (next != kInvalidId) {
            progressed = true;
            if (!is_combat) {
                runShowdown(next);
                if (state_.game_over) {
                    adv.kind = MainPhaseAdvance::Kind::Done;
                    return adv;
                }
                getBattlefield(next).showdown_staged = false;
                // What the showdown's end queued (a conquer) resolves before
                // anything else begins (CR 323.11 / 323.12, 344).
                runChain();
            } else {
                runCombat(next);
                if (state_.game_over) {
                    adv.kind = MainPhaseAdvance::Kind::Done;
                    return adv;
                }
                // Cleared unless the combat ended in "No Result" with both
                // players still here, which keeps the battlefield Contested
                // and stages the next combat (CR 468.3.d.1): a later pass of
                // this loop runs it.
                if (!getBattlefield(next).is_contested)
                    getBattlefield(next).combat_staged = false;
            }
            if (state_.game_over) {
                adv.kind = MainPhaseAdvance::Kind::Done;
                return adv;
            }
        }

        if (chain_manager_->chainExists()) progressed = true;
        if (!progressed) break;
        // The Cleanup, and whatever is on the chain with a Cleanup after
        // every step, until a Cleanup leaves the chain empty.
        runChain();
        if (state_.game_over) {
            adv.kind = MainPhaseAdvance::Kind::Done;
            return adv;
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

BattlefieldId GameEngine::chooseNextStaged(bool& is_combat) {
    // Showdowns first: step 9 of the Cleanup (CR 323.11) comes before step 10
    // (323.12), and once a showdown has begun the state is no longer a
    // Neutral Open State.
    std::vector<BattlefieldId> candidates;
    for (const auto& bf : state_.battlefields)
        if (bf.showdown_staged && !bf.showdown_in_progress && !bf.combat_staged)
            candidates.push_back(bf.id);
    is_combat = candidates.empty();
    if (is_combat)
        for (const auto& bf : state_.battlefields)
            if (bf.combat_staged && !bf.combat_in_progress) candidates.push_back(bf.id);
    if (candidates.empty()) return kInvalidId;
    if (candidates.size() == 1) return candidates.front();

    // "the Turn Player chooses one of those Battlefields": one option per
    // battlefield, its number as the value and its card as the object.
    const PlayerId chooser = state_.turn.turn_player;
    const std::string what = is_combat ? "Combat begins at" : "Showdown begins at";
    std::vector<Intent> options;
    for (auto id : candidates) {
        const auto& bf = getBattlefield(id);
        Intent o;
        o.type = IntentType::MakeChoice;
        o.player = chooser;
        o.chosen_value = static_cast<int>(id.raw());
        std::string name = "Battlefield #" + toString(id);
        if (bf.card_object_id != kInvalidId && state_.objectExists(bf.card_object_id)) {
            o.chosen_objects = {bf.card_object_id};
            name = state_.getObject(bf.card_object_id).name;
        }
        o.choice_label = what + " \xE2\x80\x94 " + name;
        options.push_back(std::move(o));
    }
    const Intent chosen = decideNow(chooser, options,
        std::string(is_combat ? "where the next combat begins (CR 323.12)"
                              : "where the next showdown begins (CR 323.11)"));
    for (auto id : candidates)
        if (chosen.chosen_value == static_cast<int>(id.raw())) return id;
    return candidates.front();
}

bool GameEngine::resolveMainPhaseDecision(const Intent& intent,
                                          int& action_count) {
    if (intent.type == IntentType::EndTurn) return false;

    executeIntent(intent);

    // HOT FEPR (CR 334): the Cleanup, then whatever is pending — play
    // triggers, death triggers, "becomes Mighty" — each step followed by a
    // Cleanup, until a Cleanup leaves the chain empty.
    runChain();

    action_count++;
    return !state_.game_over;
}

void GameEngine::mainPhase() {
    RIFT_TRACE(events_, "── Main Phase ──");
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
void GameEngine::mainPhaseLoop(int action_count) {
    while (true) {
        auto adv = advanceMainPhase(action_count);
        if (adv.kind == MainPhaseAdvance::Kind::Done) return;

        auto intent = queryAgent(state_.turn.turn_player);
        if (!resolveMainPhaseDecision(intent, action_count)) return;
    }
}

void GameEngine::endingStep() {
    RIFT_TRACE(events_, "── Ending Step ──");
    auto old_phase = state_.turn.phase;
    state_.turn.phase = TurnPhase::EndingStep;
    events_.emit(PhaseChangedEvent{old_phase, TurnPhase::EndingStep,
                                    state_.turn.turn_player});

    // CR 423.1.a.2 — Stunned units lose Stunned at the beginning of the
    // NEXT Ending Step (i.e., this one). Phase 6q+ correction: previously
    // cleared in expirationStep which is the wrong CR step.
    // (Every object, not the board lists: a unit that was stunned and has
    // since left the board — to a hand, a trash — must not come back stunned.
    // Once a turn; 0.4% of a random game.)
    for (auto& [id, obj] : state_.objects) {
        if (obj.is_stunned) {
            obj.is_stunned = false;
        }
    }

    // "At the end of your turn" abilities (e.g. Dazzling Aurora) resolve
    // here, a Cleanup after each, and so does everything they cause (a unit
    // Aurora plays that becomes Mighty, a death): nothing is pending when the
    // Expiration Step begins (CR 335).
    runChain();
}

void GameEngine::expireTemporaryKeywords(GameObject& obj, const CardDB& db) {
    obj.temp_deflect_value = 0;   // [Deflect N] given "this turn"
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
    RIFT_TRACE(events_, "── Expiration Step ──");
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
    // The body is the Ending Special Cleanup's insertion (CR 317.2.a-d); the
    // Cleanup itself follows it — a "this turn" Might penalty that expires
    // makes a unit Mighty again here, in the turn that is ending — and what
    // that puts on the chain goes through FEPR before the step is repeated.
    constexpr int kMaxExpirationPasses = 8;
    for (int pass = 0; ; ++pass) {
        if (pass >= kMaxExpirationPasses)
            throw std::logic_error("GameEngine::expirationStep: abilities kept going "
                                   "through FEPR for " + std::to_string(kMaxExpirationPasses) +
                                   " passes of the Expiration Step (CR 317.2.f)");
        const ChainItemId before = state_.chain.next_id;
        doExpirationBody();
        runChain();
        if (state_.game_over || state_.chain.next_id == before) break;
    }
}

void GameEngine::doExpirationBody() {
    // The body ends "this turn" effects by writing the objects directly,
    // without an announcement for each: the Cleanup after it is never the
    // skipped kind (cleanup(), (2)). Found by the checked build: a "this turn"
    // -5 [M] that expired made a unit Mighty again in the second pass of the
    // step, and the skipped Cleanup would not have noticed.
    quiet_cleanup_valid_ = false;
    // Heal all units (CR 317.2.b)
    healAllUnits();

    // (Stun clearing moved to endingStep per CR 423.1.a.2)

    // Expire "this turn" effects (CR 317.2.c)
    // (Every object, not the board lists: a "this turn" status or tally
    // stays on a unit that has left the board since it was given — the damage
    // it took this turn, a temporary Might or keyword, "can't be chosen this
    // turn" — and must be gone before the card is played again. Once a turn;
    // 1.0% of a random game.)
    for (auto& [id, obj] : state_.objects) {
        // Lotus Trap-style damage doubling expires here.
        obj.damage_doubled_this_turn = false;

        // Tactical Retreat's "next time it would die THIS TURN" replacement
        // expires unused at end of turn.
        obj.death_replacement_recall_pending = false;
        obj.banish_if_would_die_this_turn = false;   // Smite

        // Counter Strike's one-shot damage prevention is "this turn".
        obj.prevent_next_damage_this_turn = false;
        obj.prevent_damage_remaining = 0;   // Ki Barrier
        obj.damage_taken_this_turn = 0;
        obj.untargetable_by_enemy_this_turn = false;   // Twilight Shroud
        obj.cant_move_this_turn = false;               // Vex, Apathetic
        obj.temp_granted_abilities.clear();            // Dominus

        // ("Empower it. Disempower it at end of turn" and the reverse —
        // Sanction, Tornado Warrior — are delayed abilities of the Ending
        // Step, CR 317.1.a: DelayedAbility::subject.)

        // Per-unit move counter is "this turn" (Kayn, Unleashed).
        obj.moves_this_turn = 0;

        // Temp might is a separate field from buff_count (permanent buffs);
        // just clear it. recomputeMight (below / in cleanup) drops the bonus.
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
        obj.recomputeMight();   // the temp fields were cleared above

        // Pure-flag keywords granted "this turn" (Bounty Hunter's
        // Ganking, etc.). Delegated to a static helper so tests can
        // exercise the revoke logic without setting up the engine's
        // full subsystem stack.
        expireTemporaryKeywords(obj, card_db_);
    }
    // Recompute might for all units after expiration
    for (auto& [id, obj] : state_.objects.onBoard()) {
        if (obj.isUnit() && obj.location.has_value()) {
            obj.recomputeMight();
        }
    }

    // Empty rune pools (CR 317.2.d)
    emptyRunePools();

    // CR 416 + "take control of … until end of turn" — revert any
    // EOT-scoped control changes. Phase 6q+ wiring of the
    // EffectExecutor::takeControl primitive (until_end_of_turn=true).
    // (Every object, not the board lists: the card taken "until end of turn"
    // may have left the board since, and is still returned to its controller.
    // Once a turn; 0.4% of a random game.)
    for (auto& [id, obj] : state_.objects) {
        if (obj.control_reverts_eot) {
            auto restored = obj.original_controller_eot;
            if (restored != PlayerId::None && restored != obj.controller) {
                RIFT_TRACE(events_, std::string("REVERT_CONTROL: ") + obj.name +
                                 " (id=" + toString(id) + ") " +
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
                return da.expiresAtEndOf(state_.turn.turn_number);
            }),
        state_.delayed_abilities.end());
}

// ═══════════════════════════════════════════════════════════════════════════════
// Action execution
// ═══════════════════════════════════════════════════════════════════════════════

// Energy reduction for activating ability `index` of `source`: the card's own
// (and aura-computed) reduction, plus Piltovan Forge's "the first friendly gear
// activated ability played each turn costs [1] less".
int GameEngine::activationEnergyReduction(const Card& card, PlayerId player,
                                          GameObjectId source, int index) const {
    int r = card.activationCostReductionFor(state_, player, source, index);
    if (state_.objectExists(source) && state_.getObject(source).isGear())
        r += firstGearAbilityDiscount(player);
    return r;
}

// "The first friendly gear activated ability played each turn costs [1] less."
int GameEngine::firstGearAbilityDiscount(PlayerId player) const {
    const auto& ps = state_.player(player);
    return (ps.first_gear_ability_discount > 0 && ps.gear_abilities_activated_this_turn == 0)
        ? ps.first_gear_ability_discount : 0;
}

// The terms an Equip ACTIVATION is paid on (not Weaponmaster, which does not
// activate the ability — CR 821.1.c.6).
Card::EquipTerms GameEngine::equipTerms(PlayerId player) const {
    Card::EquipTerms terms;
    terms.energy_discount = firstGearAbilityDiscount(player);
    return terms;
}

// Whether `player` can activate `gear`'s Equip on `unit` right now: the
// predicate for the Equip generator, and what payAndQueueAbility pays by.
bool GameEngine::canPayEquip(PlayerId player, GameObjectId gear, GameObjectId unit) const {
    if (!state_.objectExists(gear)) return false;
    const auto& g = state_.getObject(gear);
    if (g.card_def_id == kInvalidId) return false;
    const Card* gc = card_registry_.get(g.card_def_id);
    return gc && gc->hasEquipAbility() &&
           gc->canPayEquip(state_, player, gear, unit, equipTerms(player));
}

// CR 355: the targets of a spell or ability are chosen as it is played — when
// it becomes a finalized chain item — not as it resolves. Cards that take one
// target through Card::pickTarget (needsPlayTimeTarget / an ability's
// needs_activation_time_target) get that choice here, so the target is visible
// on the chain to both players, Deflect (CR 809) is paid now, and abilities
// that trigger on being chosen go on the chain above the item. The card's
// pickTarget then returns this choice at resolution, or nothing if it is no
// longer legal (a mistarget: the instruction is ignored).
void GameEngine::chooseTargetsAtFinalize(ChainItem& item) {
    // A keyword's own ability has no card id: its Card is the keyword's
    // (engine/keyword_abilities.h), whose choices are made here too (D22).
    if (item.card_def_id == kInvalidId && !item.keyword_ability.has_value()) return;
    const ChainItemId this_id = item.id;
    chooseFirstExecution(item);
    // CR 820.2: the choices of each additional execution a Repeat paid for
    // are made now too, and need not be the same (820.2.a). Each is a fresh
    // set made the way "make new choices" makes one; the first execution's
    // are put back afterwards.
    if (choosing_repeat_) return;
    ChainItem* it = nullptr;
    for (auto& ci : state_.chain.items) if (ci.id == this_id) it = &ci;
    if (!it || !it->is_spell || it->repeats_paid <= 0 || !it->repeat_choices.empty()) return;
    const int repeats = it->repeats_paid;
    auto snapshot = [](const ChainItem& ci) {
        ChainItem::ExecutionChoices c;
        c.targets = ci.targets;
        c.chosen_objects = ci.chosen_objects;
        c.chosen_stamps = ci.chosen_stamps;
        c.chosen_item = ci.chosen_item;
        c.resume_point = ci.resume_point;
        c.resume_data = ci.resume_data;
        c.may_answer = ci.may_answer;
        c.choices_finalized = ci.choices_finalized;
        c.finalize_costs_paid = ci.finalize_costs_paid;
        return c;
    };
    auto load = [](ChainItem& ci, const ChainItem::ExecutionChoices& c) {
        ci.targets = c.targets;
        ci.chosen_objects = c.chosen_objects;
        ci.chosen_stamps = c.chosen_stamps;
        ci.chosen_item = c.chosen_item;
        ci.resume_point = c.resume_point;
        ci.resume_data = c.resume_data;
        ci.may_answer = c.may_answer;
        ci.choices_finalized = c.choices_finalized;
        ci.finalize_costs_paid = c.finalize_costs_paid;
    };
    const ChainItem::ExecutionChoices first = snapshot(*it);
    std::vector<ChainItem::ExecutionChoices> extra;
    choosing_repeat_ = true;
    try {
        for (int r = 0; r < repeats; ++r) {
            for (auto& ci : state_.chain.items)
                if (ci.id == this_id) ci.finalize_costs_paid = false;   // a cost per execution
            makeNewChoicesFor(this_id);
            for (auto& ci : state_.chain.items)
                if (ci.id == this_id) extra.push_back(snapshot(ci));
        }
    } catch (...) {
        choosing_repeat_ = false;
        throw;
    }
    choosing_repeat_ = false;
    for (auto& ci : state_.chain.items)
        if (ci.id == this_id) {
            load(ci, first);
            ci.finalize_checked = 0;
            ci.repeat_choices = std::move(extra);
        }
}

void GameEngine::adoptPrefinalizedChoices(ChainItem& item) {
    ChainItem done = std::move(*prefinalized_spell_);
    prefinalized_spell_.reset();
    const ChainItemId item_id = item.id;
    const PlayerId controller = item.controller;
    std::vector<GameObjectId> chosen = done.chosen_objects;
    for (auto& ci : state_.chain.items) {
        if (ci.id != item_id) continue;
        ci.resume_point = done.resume_point;
        ci.resume_data = std::move(done.resume_data);
        ci.chosen_objects = std::move(done.chosen_objects);
        ci.chosen_stamps = std::move(done.chosen_stamps);
        ci.chosen_item = done.chosen_item;
        ci.may_answer = done.may_answer;
        ci.finalize_costs_paid = done.finalize_costs_paid;
        ci.finalize_checked = 0;
        ci.choices_finalized = true;
        break;
    }
    for (auto t : chosen) {
        if (!state_.objectExists(t)) continue;
        const auto& tobj = state_.getObject(t);
        if (tobj.isUnit() && tobj.location.has_value() && tobj.controller == controller)
            events_.emit(ObjectStateChangedEvent{t, "chosen_by_controller"});
    }
}

void GameEngine::chooseFirstExecution(ChainItem& item) {
    if (item.is_ability && !item.is_activated_ability) {
        // A triggered ability: its "you may", costs and targets (CR 383.3)
        // are chosen by the card's own callback, through the choice helpers
        // (Card::confirmOptional / pickTarget / pickTargetPair), in the
        // finalize pass. The engine never fills `targets` for a trigger: the
        // event's subject travels in ChainItem::triggering_subject.
        // A reflexive trigger's choices are always made as it is finalized
        // (CR 388.4): Card::onReflexiveTrigger follows that contract.
        const Card* tcard = item.keyword_ability.has_value()
            ? keywordAbilityCard(*item.keyword_ability)
            : card_registry_.get(item.card_def_id);
        if (tcard && (item.reflexive_index >= 0 ||
                      (tcard->choicesAtFinalization() & Card::kFinalizeTrigger)))
            runFinalizePass(item);
        return;
    }
    const Card* card = (item.is_ability && item.granted_ability_def != kInvalidId)
        ? card_registry_.get(item.granted_ability_def)
        : card_registry_.get(item.card_def_id);
    if (!card) return;
    if (item.is_spell && prefinalized_spell_.has_value() &&
        prefinalized_spell_->source == item.source) {
        // Its choices were made before its cost was paid (executePlaySpell):
        // take them over, and announce them now that the spell is on the chain.
        adoptPrefinalizedChoices(item);
        return;
    }
    if (item.is_spell && item.targets.empty() && card->needsPlayTimeTarget() &&
        (card->choicesAtFinalization() & Card::kFinalizeResolve)) {
        // A spell whose target and other choices are all made in its finalize
        // pass, played by an effect or making new choices: the pass asks them.
        runFinalizePass(item);
        return;
    }
    if (!item.targets.empty()) {
        // Its targets were locked in as it was played. A spell may still have
        // other choices the rules place at finalization — the units a split is
        // divided among (CR 355.14.b), a "may": its callback makes them now.
        if (item.is_spell && (card->choicesAtFinalization() & Card::kFinalizeResolve))
            runFinalizePass(item);
        return;
    }

    std::vector<GameObjectId> legal;
    // A spell that takes ONE required target from its play intent
    // (getTargetRequirements) and has none: it is played by an effect, or is
    // making new choices (CR 753.3: under the same requirements). The engine
    // asks for that target as for a play-time-target spell, and the card's
    // finalize pass (if it has one) runs with it — a card that reads
    // targets[0] must never be finalized with nothing chosen (finding F2).
    const bool one_from_intent = [&] {
        if (!item.is_spell || card->needsPlayTimeTarget()) return false;
        const auto req = card->getTargetRequirements();
        return req.count == 1 && !req.optional;
    }();
    if (item.is_spell) {
        if (!card->needsPlayTimeTarget() && !one_from_intent) {
            if (!(card->choicesAtFinalization() & Card::kFinalizeResolve)) return;
            runFinalizePass(item);
            return;
        }
        legal = card->enumerateLegalTargets(state_, item.controller);
    } else {
        const auto& abs = card->abilities();
        if (item.ability_index < 0 || item.ability_index >= static_cast<int>(abs.size())) return;
        if (abs[item.ability_index].is_empower) return;
        if (!abs[item.ability_index].needs_activation_time_target) {
            if (card->choicesAtFinalization() & Card::kFinalizeActivate) runFinalizePass(item);
            return;
        }
        legal = card->enumerateLegalTargets(state_, item.controller, item.ability_index, item.source);
        // The ability's cost is owed after this choice (CR 403.2 -> 405.1): a
        // target whose Deflect can't be paid with it is not offered (G15/F5).
        if (const auto owed = effect_executor_->owedCost(item.controller)) {
            std::vector<GameObjectId> payable;
            for (auto t : legal) {
                const int deflect = card->ignoresDeflect() ? 0
                    : deflectCostFor(state_, item.controller, t);
                if (effect_executor_->canPayWithAdds(item.controller, owed->energy, owed->power,
                                                     owed->mask, owed->spend_for, deflect))
                    payable.push_back(t);
            }
            legal = std::move(payable);
        }
    }
    if (legal.empty()) {
        // Nothing to choose now: the card handles it as it resolves — and in
        // its finalize pass, where its helpers store that there is nothing.
        if (one_from_intent && (card->choicesAtFinalization() & Card::kFinalizeResolve))
            runFinalizePass(item);
        return;
    }

    std::vector<Intent> options;
    options.reserve(legal.size());
    for (auto t : legal) {
        Intent i;
        i.type = IntentType::MakeChoice;
        i.player = item.controller;
        i.chosen_objects = {t};
        options.push_back(std::move(i));
    }
    state_.decision_index++;
    RIFT_TRACE_DECISION(events_, "DECISION #" + std::to_string(state_.decision_index) + " (" +
                     toString(item.controller) + "): choose a target for " +
                     (state_.objectExists(item.source) ? state_.getObject(item.source).name
                                                       : std::string("?")) +
                     " [" + std::to_string(options.size()) + " options]");
    const PlayerId chooser = item.controller;
    const ChainItemId item_id = item.id;
    auto chosen = askAgent(chooser, options);
    if (on_decision) on_decision(state_, options, chosen);
    GameObjectId t = legal.front();
    if (!chosen.chosen_objects.empty() &&
        std::find(legal.begin(), legal.end(), chosen.chosen_objects.front()) != legal.end())
        t = chosen.chosen_objects.front();

    // The agent call may have reallocated the chain: find the item again.
    ChainItem* it = nullptr;
    for (auto& ci : state_.chain.items) if (ci.id == item_id) { it = &ci; break; }
    if (!it) return;
    it->targets = {t};
    it->chosen_objects.push_back(t);
    while (it->resume_data.size() < 6) it->resume_data.push_back(0);
    it->resume_data[5] = 1;   // "target was chosen at finalization" (read by Card::pickTarget)
    // A facedown card or one in a hand is named only for who may know it (D11).
    RIFT_TRACE_FOR(events_, audienceNaming(state_, chooser, {t}),
                   "TARGET (finalize): " +
                       (state_.objectExists(it->source) ? state_.getObject(it->source).name
                                                        : std::string("?")) +
                       " -> " + state_.getObject(t).name);

    commitEngineChosenTarget(*card, chooser, t, item_id);
    if (one_from_intent && (card->choicesAtFinalization() & Card::kFinalizeResolve))
        for (auto& ci : state_.chain.items)
            if (ci.id == item_id) { runFinalizePass(ci); break; }
}

// What choosing `t` as a target owes, for every target the ENGINE asks for
// (at finalization, and when an item makes new choices): Deflect (CR 809) is
// paid, a friendly unit hears it was chosen, and the choice is counted
// (GameState::noteChosen — "if you've chosen an enemy unit this turn").
void GameEngine::commitEngineChosenTarget(const Card& card, PlayerId chooser, GameObjectId t,
                                          ChainItemId item_id) {
    if (!state_.objectExists(t)) return;
    if (!card.ignoresDeflect()) {
        const int deflect = deflectCostFor(state_, chooser, t);
        if (deflect > 0) {
            bool by_spell = false;
            for (const auto& ci : state_.chain.items)
                if (ci.id == item_id) by_spell = ci.is_spell;
            if (state_.chain.resuming.has_value() && state_.chain.resuming->id == item_id)
                by_spell = state_.chain.resuming->is_spell;
            effect_executor_->payAnyPower(chooser, deflect,
                                          by_spell ? SpendFor::Spell : SpendFor::Other);
            RIFT_TRACE(events_, "DEFLECT: " + std::string(toString(chooser)) + " pays " +
                             std::to_string(deflect) + " [A] to choose " +
                             state_.getObject(t).name);
        }
    }
    const auto& tobj = state_.getObject(t);
    if (tobj.isUnit() && tobj.location.has_value() && tobj.controller == chooser)
        events_.emit(ObjectStateChangedEvent{t, "chosen_by_controller"});
    const ChainItem* by = nullptr;
    for (const auto& ci : state_.chain.items) if (ci.id == item_id) by = &ci;
    state_.noteChosen(chooser, t, by);
}

Intent GameEngine::decideNow(PlayerId player, const std::vector<Intent>& options,
                             const std::string& what) {
    state_.decision_index++;
    RIFT_TRACE_DECISION(events_, "DECISION #" + std::to_string(state_.decision_index) + " (" +
                     toString(player) + "): " + what + " [" +
                     std::to_string(options.size()) + " options]");
    auto chosen = askAgent(player, options);
    if (on_decision) on_decision(state_, options, chosen);
    return chosen;
}

// CR 750-755: the item's (new) controller re-chooses its targets. Nothing is
// re-chosen when there is no legal choice to make (753.2), and the choices
// obey the same requirements as the originals (753.3).
void GameEngine::makeNewChoicesFor(ChainItemId item_id) {
    ChainItem* item = nullptr;
    for (auto& ci : state_.chain.items) if (ci.id == item_id) { item = &ci; break; }
    if (!item || item->card_def_id == kInvalidId) return;
    const Card* card = card_registry_.get(item->card_def_id);
    if (!card) return;
    const PlayerId chooser = item->controller;
    const auto legal = card->enumerateLegalTargets(state_, chooser);

    const auto requirement = card->getTargetRequirements();
    // (A finalize-pass spell that takes its one target from the play intent:
    // the target is asked for again and the pass re-run, as for an effect
    // play. One with no finalize pass keeps the Deflect-aware loop below.)
    const bool one_from_intent = item->is_spell && !card->needsPlayTimeTarget() &&
                                 requirement.count == 1 && !requirement.optional &&
                                 (card->choicesAtFinalization() & Card::kFinalizeResolve) != 0;
    if (item->is_spell && (card->needsPlayTimeTarget() || one_from_intent)) {
        if (legal.empty()) return;
        item->targets.clear();
        item->chosen_objects.clear();
        item->resume_point = 0;
        item->resume_data.clear();
        chooseTargetsAtFinalize(*item);
        return;
    }
    if (card->needsPlayTimeTargetPair() || card->choicesAtFinalization() != 0) {
        // Chosen through the card's own helpers: forget the answers. A card
        // whose choices belong at finalization makes the new ones NOW (CR
        // 750-755: the new choices are made as this effect resolves), in a
        // finalize pass of its own; a cost it paid in its first pass is not
        // paid again. A card that only ever chooses as it resolves
        // (needsPlayTimeTargetPair with no finalize opt-in) is asked then.
        item->targets.clear();
        item->chosen_objects.clear();
        item->resume_point = 0;
        item->resume_data.clear();
        item->may_answer = -1;
        item->chosen_stamps.clear();
        item->chosen_item = kInvalidId;
        item->choices_finalized = false;
        item->finalize_checked = 0;
        const unsigned want = item->is_spell ? Card::kFinalizeResolve
            : (item->is_activated_ability ? Card::kFinalizeActivate : Card::kFinalizeTrigger);
        if (card->choicesAtFinalization() & want) runFinalizePass(*item);
        return;
    }
    // Targets locked in as the spell was played: choose that many again.
    const size_t needed = item->targets.size();
    if (needed == 0 || legal.size() < needed) return;
    // Each new target is chosen like any target: one whose Deflect cannot be
    // paid is not offered, and choosing it pays the Deflect and counts.
    const bool deflect_matters = !card->ignoresDeflect();
    std::vector<GameObjectId> remaining = legal, picked;
    while (picked.size() < needed) {
        std::vector<Intent> options;
        const int payable = effect_executor_->payableAnyPower(
            chooser, item->is_spell ? SpendFor::Spell : SpendFor::Other);
        std::vector<GameObjectId> offered;
        for (auto t : remaining)
            if (!deflect_matters || deflectCostFor(state_, chooser, t) <= payable)
                offered.push_back(t);
        if (offered.empty()) return;   // no legal choice left to make (CR 753.2)
        for (auto t : offered) {
            Intent i;
            i.type = IntentType::MakeChoice;
            i.player = chooser;
            i.chosen_objects = {t};
            options.push_back(std::move(i));
        }
        const Intent chosen = decideNow(chooser, options, "make a new choice");
        GameObjectId t = offered.front();
        if (!chosen.chosen_objects.empty() &&
            std::find(offered.begin(), offered.end(), chosen.chosen_objects.front()) !=
                offered.end())
            t = chosen.chosen_objects.front();
        picked.push_back(t);
        remaining.erase(std::remove(remaining.begin(), remaining.end(), t), remaining.end());
        commitEngineChosenTarget(*card, chooser, t, item_id);
    }
    // decideNow may have reallocated the chain.
    for (auto& ci : state_.chain.items)
        if (ci.id == item_id) { ci.targets = picked; ci.chosen_objects = picked; }
}

// CR 383.3.d: "If more than one Triggered Ability is Triggered simultaneously,
// then the player that controls the Abilities selects the order to place them
// on the Chain." 383.3.d.1: "starting with the Turn Player and proceeding in
// Turn Order, each player orders their Triggered Abilities on the Chain."
// What is simultaneous is ChainItem::trigger_batch. The first one placed is
// the lowest: it is finalized first and resolves last.
void GameEngine::orderPendingTriggers() {
    auto isPending = [](const ChainItem& ci) { return ci.status == ChainItemStatus::Pending; };
    const auto oldest = std::find_if(state_.chain.items.begin(), state_.chain.items.end(),
                                     isPending);
    if (oldest == state_.chain.items.end()) return;
    const uint32_t batch = oldest->trigger_batch;
    if (batch == 0 || oldest->batch_ordered) return;
    const PlayerId first_player = oldest->batch_first != PlayerId::None
        ? oldest->batch_first : state_.turn.turn_player;

    std::vector<ChainItemId> ids;      // the batch, in the order it was queued
    for (auto& ci : state_.chain.items) {
        if (!isPending(ci) || ci.trigger_batch != batch) continue;
        ci.batch_ordered = true;
        ids.push_back(ci.id);
    }
    if (ids.size() < 2) return;
    auto itemOf = [this](ChainItemId id) -> const ChainItem* {
        for (const auto& ci : state_.chain.items) if (ci.id == id) return &ci;
        return nullptr;
    };
    // The same ability twice (a doubled Deathknell, an Equipment's copy of
    // its bearer's text): their order cannot matter.
    auto same = [](const ChainItem& a, const ChainItem& b) {
        return a.source == b.source && a.controller == b.controller &&
               a.card_def_id == b.card_def_id && a.fired_trigger == b.fired_trigger &&
               a.triggering_subject == b.triggering_subject &&
               a.equipped_unit == b.equipped_unit && a.keyword_ability == b.keyword_ability;
    };
    auto label = [this](const ChainItem& ci) {
        std::string l = state_.objectExists(ci.source) ? state_.getObject(ci.source).name.get()
                                                       : std::string("?");
        if (ci.keyword_ability.has_value()) l += std::string(" [") + toString(*ci.keyword_ability) + "]";
        if (ci.equipped_unit != kInvalidId && state_.objectExists(ci.equipped_unit))
            l += " on " + state_.getObject(ci.equipped_unit).name;
        else if (ci.triggering_subject != kInvalidId && ci.triggering_subject != ci.source &&
                 state_.objectExists(ci.triggering_subject))
            l += " (" + state_.getObject(ci.triggering_subject).name + ")";
        return l;
    };

    std::vector<ChainItemId> ordered;
    for (int pass = 0; pass < 2; ++pass) {
        std::vector<ChainItemId> mine;
        PlayerId who = PlayerId::None;
        for (auto id : ids) {
            const ChainItem* ci = itemOf(id);
            if (!ci || (ci->controller == first_player) != (pass == 0)) continue;
            mine.push_back(id);
            who = ci->controller;
        }
        while (mine.size() > 1) {
            // One option per ability that differs from the others.
            std::vector<ChainItemId> distinct;
            for (auto id : mine) {
                const ChainItem* ci = itemOf(id);
                if (!ci) continue;
                bool seen = false;
                for (auto d : distinct)
                    if (const ChainItem* di = itemOf(d); di && same(*di, *ci)) seen = true;
                if (!seen) distinct.push_back(id);
            }
            if (distinct.size() < 2) break;   // all the same: nothing to decide
            std::vector<Intent> options;
            for (size_t k = 0; k < distinct.size(); ++k) {
                Intent o;
                o.type = IntentType::MakeChoice;
                o.player = who;
                o.chosen_value = static_cast<int>(k);
                o.choice_label = "Order — " + label(*itemOf(distinct[k]));
                options.push_back(std::move(o));
            }
            const Intent chosen = decideNow(who, options,
                "order simultaneous abilities: which goes on the chain next "
                "(it resolves after the ones chosen later)");
            size_t k = static_cast<size_t>(std::max(0, chosen.chosen_value.value_or(0)));
            if (k >= distinct.size()) k = 0;
            const ChainItemId next = distinct[k];
            if (const ChainItem* ni = itemOf(next))
                RIFT_TRACE(events_, "ORDER: " + std::string(toString(who)) + " puts " +
                                 label(*ni) + " on the chain next");
            ordered.push_back(next);
            mine.erase(std::remove(mine.begin(), mine.end(), next), mine.end());
        }
        for (auto id : mine) ordered.push_back(id);
    }
    // Rewrite the batch's own places on the chain in the chosen order.
    std::vector<ChainItem> in_order;
    for (auto id : ordered)
        if (const ChainItem* ci = itemOf(id)) in_order.push_back(*ci);
    size_t n = 0;
    for (auto& ci : state_.chain.items) {
        if (n >= in_order.size()) break;
        if (std::find(ids.begin(), ids.end(), ci.id) == ids.end()) continue;
        ci = in_order[n++];
    }
}

void GameEngine::placeBelowWhatItsPlayTriggered(ChainItemId item_id, ChainItemId first_new) {
    auto& items = state_.chain.items;
    const auto self = std::find_if(items.begin(), items.end(),
        [&](const ChainItem& ci) { return ci.id == item_id; });
    if (self == items.end()) return;
    const auto first = std::find_if(items.begin(), items.end(), [&](const ChainItem& ci) {
        return ci.id != item_id && ci.id >= first_new;
    });
    if (first == items.end() || first > self) return;
    std::rotate(first, self, self + 1);
}

namespace {
// `discount` units of "[1] or [A] less" taken off (energy, power), the Energy
// half first or the Power half first; what one half cannot use goes to the other.
std::pair<int, int> afterEitherDiscount(int energy, int power, int discount, bool power_first) {
    int& first = power_first ? power : energy;
    int& second = power_first ? energy : power;
    const int a = std::min(first, discount);
    first -= a;
    second -= std::min(second, discount - a);
    return {energy, power};
}
}  // namespace

bool GameEngine::accelerateAvailable(PlayerId player, GameObjectId card_id,
                                     bool from_hand) const {
    if (!state_.objectExists(card_id)) return false;
    const auto& card = state_.getObject(card_id);
    if (!card.isUnit() || card.card_def_id == kInvalidId) return false;
    if (card.keywords.has(Keyword::Accelerate)) return true;
    // A card on the player's board may give the unit being played
    // [Accelerate] for this play (Rek'Sai, Breacher: "played from anywhere
    // other than a player's hand"; Card::grantsAccelerateTo).
    for (const auto& [oid, obj] : state_.objects.onBoard()) {
        if (obj.controller != player || obj.card_def_id == kInvalidId) continue;
        const Card* granter = card_registry_.get(obj.card_def_id);
        if (granter && granter->grantsAccelerateTo(state_, oid, card, from_hand)) return true;
    }
    return false;
}

std::optional<std::pair<int, int>> GameEngine::chooseAccelerateHalf(
    PlayerId player, const std::function<bool(int energy, int power)>& payable) {
    // [Accelerate] is an Optional Additional Cost (CR 805.1): "optional
    // additional costs you pay cost [1] or [A] less" applies to it (CR
    // 356.4.c, 356.4.f; Card::optionalCostDiscount), the payer choosing the
    // half when both are payable and differ.
    const int disc = optionalCostDiscount(player);
    const auto e_first = afterEitherDiscount(1, 1, disc, false);
    const auto p_first = afterEitherDiscount(1, 1, disc, true);
    const bool e_ok = payable(e_first.first, e_first.second);
    const bool p_ok = payable(p_first.first, p_first.second);
    if (!e_ok && !p_ok) return std::nullopt;
    if (e_ok && p_ok && e_first != p_first) {
        Intent e; e.type = IntentType::MakeChoice; e.player = player;
        e.chosen_value = 0; e.choice_label = "cost [1] less";
        Intent pw = e; pw.chosen_value = 1; pw.choice_label = "cost [A] less";
        const bool pf = decideNow(player, {e, pw},
            "optional additional cost: [1] or [A] less").chosen_value.value_or(0) == 1;
        return pf ? p_first : e_first;
    }
    return e_ok ? e_first : p_first;
}

bool GameEngine::askAccelerate(PlayerId player, const GameObject& card) {
    Intent decline;
    decline.type = IntentType::MakeChoice;
    decline.player = player;
    decline.chosen_value = 0;  // 0 = decline
    Intent accept = decline;
    accept.chosen_value = 1;   // 1 = pay [1][C] for Accelerate
    decline.declines = true;   // the "no" of a yes/no (Intent::declines)
    std::vector<Intent> opts = {decline, accept};
    state_.decision_index++;
    RIFT_TRACE_DECISION(events_, "DECISION #" + std::to_string(state_.decision_index) +
                     " (" + toString(player) + "): pay [1][" +
                     (card.domains.empty() ? std::string("A")
                                           : std::string(toString(card.domains.front()))) +
                     "] for Accelerate on " + card.name + "? [decline|pay] [2 options]");
    auto chosen = askAgent(player, opts);
    if (on_decision) on_decision(state_, opts, chosen);
    return chosen.chosen_value.value_or(0) == 1;
}

Card::PlayCostAdjust GameEngine::decideAccelerate(PlayerId player, GameObjectId card_id,
                                                  const Card::PlayCostAdjust& prepay,
                                                  int extra_any_power) {
    auto& card = state_.getObject(card_id);
    card.card_counters.erase(Card::kAccelerateDecided);
    if (!card.isUnit()) return prepay;
    const bool from_hand =
        state_.player(player).current_play_source == Intent::PlaySource::Hand;
    card.card_counters[Card::kAccelerateFromHand] = from_hand ? 1 : 2;
    if (prepay.ignore_cost) return prepay;
    if (!accelerateAvailable(player, card_id, from_hand)) return prepay;
    // A unit with no Domain pays the Power with [A] (CR 805.1.b.2), which is
    // not the card's own Power: that case is asked as the unit enters
    // (resolvePermanent), from what is left after the cost.
    if (card.domains.empty()) return prepay;
    card.card_counters[Card::kAccelerateDecided] = 1;   // decided: not paid
    // [1] and one Power of the unit's own Domains (CR 805.1.b.1), less any
    // discount on optional additional costs, added to the card's cost: one
    // payment, judged as a whole.
    auto with_half = [&](int energy, int power) {
        Card::PlayCostAdjust with = prepay;
        with.energy_reduction -= energy;
        with.power_reduction -= power;
        return with;
    };
    auto payable = [&](int energy, int power) {
        return canAffordWith(player, card_id, extra_any_power, with_half(energy, power));
    };
    if (!chooseAccelerateHalfPayable(player, payable)) return prepay;
    if (!askAccelerate(player, state_.getObject(card_id))) {
        RIFT_TRACE(events_, "ACCELERATE: " + state_.getObject(card_id).name + " declined");
        return prepay;
    }
    const auto half = chooseAccelerateHalf(player, payable);
    auto& again = state_.getObject(card_id);
    if (!half.has_value()) return prepay;   // (judged payable just above)
    RIFT_TRACE(events_, "ACCELERATE: " + again.name + " pays [" + std::to_string(half->first) +
                     "] + " + std::to_string(half->second) + " power to enter ready");
    again.card_counters[Card::kAccelerateDecided] = 2;   // paid with the cost: enters ready
    return with_half(half->first, half->second);
}

bool GameEngine::chooseAccelerateHalfPayable(
    PlayerId player, const std::function<bool(int energy, int power)>& payable) const {
    const int disc = optionalCostDiscount(player);
    const auto e_first = afterEitherDiscount(1, 1, disc, false);
    const auto p_first = afterEitherDiscount(1, 1, disc, true);
    return payable(e_first.first, e_first.second) || payable(p_first.first, p_first.second);
}

bool GameEngine::offerAccelerateOnItsOwn(PlayerId player, GameObjectId card_id, bool from_hand) {
    // For a play that pays no cost of its own first — from facedown, "play it,
    // ignoring its cost" (CR 356.1.b.3: the Accelerate cost is still added),
    // a unit with no Domain: [1] and one Power of the unit's Domains ([A] for
    // none), on its own.
    if (!accelerateAvailable(player, card_id, from_hand)) return false;
    const auto& card = state_.getObject(card_id);
    const resource::DomainMask mask = resource::maskOf(card.domains.get());
    auto payable = [&](int energy, int power) {
        return resource::canPayIn(state_, player, energy, power, mask, SpendFor::Unit);
    };
    if (!chooseAccelerateHalfPayable(player, payable)) return false;
    if (!askAccelerate(player, card)) {
        RIFT_TRACE(events_, "ACCELERATE: " + card.name + " declined");
        return false;
    }
    const auto half = chooseAccelerateHalf(player, payable);
    if (!half.has_value()) return false;
    if (!effect_executor_->payResources(player, half->first, half->second, mask, SpendFor::Unit))
        return false;
    RIFT_TRACE(events_, "ACCELERATE: " + state_.getObject(card_id).name + " pays [" +
                     std::to_string(half->first) + "] + " + std::to_string(half->second) +
                     " power to enter ready");
    return true;
}

void GameEngine::announcePermanentPlayed(GameObjectId card_id, PlayerId player) {
    if (!state_.objectExists(card_id)) return;
    auto& card = state_.getObject(card_id);
    auto it = card.card_counters.find("__announce_play");
    if (it == card.card_counters.end()) return;   // announced by its own path
    const int ordinal = it->second;
    card.card_counters.erase(it);
    int energy = 0;
    bool not_from_hand = false;
    if (auto e = card.card_counters.find("__announce_energy"); e != card.card_counters.end()) {
        energy = e->second;
        card.card_counters.erase(e);
    }
    if (auto n = card.card_counters.find("__announce_not_hand"); n != card.card_counters.end()) {
        not_from_hand = n->second != 0;
        card.card_counters.erase(n);
    }
    events_.emit(CardPlayedEvent{card_id, player, state_.getObject(card_id).card_type,
                                 ordinal, energy});
    if (not_from_hand && state_.objectExists(card_id))
        events_.emit(ObjectStateChangedEvent{card_id, "played_not_from_hand"});
}

// Runs an opted-in card's callback once as its chain item is finalized, so the
// choice helpers (confirmOptional / pickMode / pickTarget / pickTargetPair) ask
// the agent now and costs are paid now; the card returns at its
// Card::finalizingChoices marker, before the effect. The answers are written
// back to the item, and the helpers return them when it resolves.
void GameEngine::runFinalizePass(ChainItem& item) {
    // An item may be mid-resolution — the one whose effect put this item on
    // the chain, or told it to make new choices. Its place in the resume slot
    // is kept aside for the length of this pass and put back afterwards (it
    // is in the middle of its callback, not waiting for an answer), so this
    // item's choices are made now, where the rules make them, not as it
    // resolves.
    const ChainItemId item_id = item.id;
    ChainItem done = collectFinalizeChoices(item);
    // A triggered ability (a reflexive one included, CR 388.4) that is not
    // performed leaves the chain instead of being finalized:
    //   CR 403.1.a  "If they choose not to perform the Triggered Ability now,
    //               it is removed from the chain."
    //   CR 405.2    "players may decline to pay for Triggered Abilities that
    //               have incurred a cost. If they do, the ability will cease
    //               being a Pending Item and be removed from the Chain."
    //               (383.3.b.1: the cost must be paid to finalize it — a "may"
    //               that is not offered because it cannot be paid is the same.)
    //   CR 403.4    "If there are not enough options to make legal choices for
    //               a Triggered Ability that has been put on the chain, remove
    //               it from the Chain now. It ceases to be a Pending Item but
    //               never becomes a Finalized Chain Item."
    // Not a spell (it resolves whatever its targets, CR 359.3.e.1), not an
    // activated ability (its cost is paid), and not an ability whose target
    // becomes illegal later (a mistarget: it was finalized).
    const bool not_performed = done.is_ability && !done.is_activated_ability &&
                               ((done.may_answer == 0 && !done.may_is_partial) ||
                                done.required_choice_missing);
    for (auto& ci : state_.chain.items) {
        if (ci.id != item_id) continue;
        ci.remove_at_finalize = not_performed && !keep_unperformed_triggers_;
        ci.resume_point = done.resume_point;
        ci.resume_data = std::move(done.resume_data);
        ci.chosen_objects = std::move(done.chosen_objects);
        ci.chosen_stamps = std::move(done.chosen_stamps);
        ci.chosen_item = done.chosen_item;
        ci.owed_energy = done.owed_energy;
        ci.owed_power = done.owed_power;
        ci.owed_power_mask = done.owed_power_mask;
        ci.may_answer = done.may_answer;
        ci.finalize_costs_paid = done.finalize_costs_paid;
        ci.finalize_checked = 0;
        ci.choices_finalized = true;
        break;
    }
}

// The pass itself. `item` need not be on the chain (a spell choosing before
// its cost is paid — executePlaySpell).
ChainItem GameEngine::collectFinalizeChoices(ChainItem item, const FinalizeAnswerer* answer,
                                             bool* completed) {
    if (completed) *completed = true;
    size_t asked = 0;
    std::optional<ChainItem> outer;
    if (state_.chain.resuming.has_value()) outer = std::move(*state_.chain.resuming);
    state_.chain.resuming = std::move(item);
    state_.chain.resuming->finalizing = true;

    // A card may ask as many questions as its text needs (one per unit on a
    // crowded board). The bound only stops a card that never stops asking,
    // and reaching it is an engine error, not a quiet end of the choices.
    constexpr int kMaxChoices = 1024;
    for (int iter = 0; ; ++iter) {
        state_.chain.resuming->finalize_checked &= static_cast<uint8_t>(~32);
        resolveSpell(*state_.chain.resuming);
        if (!effect_executor_->hasPendingChoice()) break;
        // The cost contract (card.h): a cost is paid immediately before the
        // end-of-choices marker. A card that paid — the state changed since
        // costDueNow() said "due" in this run — and then asked a question
        // would be re-entered with the cost due again and pay it twice. That
        // is a card bug, reported here instead of charged to the player.
        if ((state_.chain.resuming->finalize_checked & 32) &&
            Card::costStateFingerprint(state_) != state_.chain.resuming->cost_due_fingerprint) {
            const GameObjectId src = state_.chain.resuming->source;
            throw std::logic_error(
                "GameEngine::runFinalizePass: " +
                (state_.objectExists(src) ? state_.getObject(src).name : std::string("?")) +
                " paid a cost and then asked for a choice before its end-of-choices"
                " marker (costs go immediately before finalizingChoices)");
        }
        if (iter >= kMaxChoices) {
            const GameObjectId src = state_.chain.resuming->source;
            throw std::logic_error(
                "GameEngine::runFinalizePass: " +
                (state_.objectExists(src) ? state_.getObject(src).name : std::string("?")) +
                " asked more than " + std::to_string(kMaxChoices) +
                " choices at finalization");
        }
        auto pending = effect_executor_->consumePendingChoice();
        RIFT_TRACE(events_, std::string("CHOICE-REQUEST (finalize): ") +
                         (pending.label.empty() ? "MakeChoice" : pending.label) + " [" +
                         std::to_string(pending.legal.size()) + " options] (" +
                         toString(pending.player) + ")");
        // An answer the engine refuses (askAgent) ends the game with an
        // exception; the chain is still left as it was before this pass —
        // no item "resolving", no prompt pending — so the state a caller
        // inspects afterwards is a consistent one.
        if (answer) {
            auto given = (*answer)(asked++, pending.player, pending.legal);
            if (!given.has_value()) {
                // Stopped here: the item as far as it got.
                ChainItem partial = std::move(*state_.chain.resuming);
                state_.chain.resuming.reset();
                if (outer.has_value()) state_.chain.resuming = std::move(*outer);
                if (completed) *completed = false;
                return partial;
            }
            effect_executor_->recordChoice(std::move(*given));
            continue;
        }
        try {
            const EffectExecutor::Asking named(*effect_executor_, pending.label.empty()
                ? (state_.chain.resuming.has_value() &&
                   state_.objectExists(state_.chain.resuming->source)
                       ? state_.getObject(state_.chain.resuming->source).name + ": a choice"
                       : std::string("a choice"))
                : pending.label);
            Intent choice = queryAgentForChain(pending.player, pending.legal);
            effect_executor_->recordChoice(std::move(choice));
        } catch (...) {
            state_.chain.resuming.reset();
            throw;
        }
    }

    ChainItem done = std::move(*state_.chain.resuming);
    state_.chain.resuming.reset();
    if (outer.has_value()) state_.chain.resuming = std::move(*outer);
    // The pass of a card that opted its callback into choices at finalization
    // ends at the card's end-of-choices marker, or where a choice helper
    // stored "nothing to decide" (the rule at Card::finalizingChoices). One
    // that returned anywhere else stored nothing the resolution can rely on: the
    // card would be entered again as it resolves and its helpers would ask
    // then, after everyone had the chance to respond. That is a card bug,
    // reported here. (A reflexive trigger's pass is the engine's doing, not
    // an opt-in, and has no marker to reach.)
    if (!(done.finalize_checked & (64 | 128)) && done.reflexive_index < 0 &&
        (done.card_def_id != kInvalidId || done.keyword_ability.has_value())) {
        const Card* owner = done.keyword_ability.has_value()
            ? keywordAbilityCard(*done.keyword_ability)
            : (done.is_ability && done.granted_ability_def != kInvalidId)
                ? card_registry_.get(done.granted_ability_def)
                : card_registry_.get(done.card_def_id);
        const unsigned bit = done.is_spell ? Card::kFinalizeResolve
                           : (done.is_ability && !done.is_activated_ability)
                               ? Card::kFinalizeTrigger : Card::kFinalizeActivate;
        if (owner && (owner->choicesAtFinalization() & bit)) {
            const GameObjectId src = done.source;
            throw std::logic_error(
                "GameEngine::runFinalizePass: " +
                (state_.objectExists(src) ? state_.getObject(src).name : std::string("?")) +
                " returned from its finalize pass before its end-of-choices marker and not"
                " because a choice helper had nothing to decide (the rule is at"
                " Card::finalizingChoices)");
        }
    }
    return done;
}

// Activate an ability (CR 400-407, 377.3): its chain item is added (pending,
// CR 402.1), its choices are made (targets, modes, "may": CR 403.2 — the
// target a play intent names, the engine's target prompt, or the card's
// finalize pass), then its cost is paid (CR 404-405: a choice's Deflect and the
// cost are judged and paid together — EffectExecutor::OwedCost), and it is
// finalized (CR 407). What the choices or the payment triggered goes above it
// (CR 818.1.c.1). Returns true if a chain item was added — the caller runs or
// resumes the chain — and false if the activation was not possible (nothing is
// changed then). An ability that [Add]s resolves at once instead (CR 430.2:
// resolveAddAbilityNow) and returns false: it leaves no item and passes no
// priority or Focus.
bool GameEngine::payAndQueueAbility(const Intent& intent) {
    last_activation_added_ = false;
    if (!state_.objectExists(intent.ability_source)) return false;
    // Skip card-def lookup for tokens (no CardDef).
    if (state_.getObject(intent.ability_source).card_def_id == kInvalidId) return false;
    // Only an object in play has abilities to activate, and only its
    // controller activates them (the generators' first gate): an activation
    // of a card in a trash, a hand or a deck is refused, nothing is paid.
    {
        const auto& src = state_.getObject(intent.ability_source);
        if (src.controller != intent.player) return false;
        if (!src.location.has_value() && src.zone != ZoneType::LegendZone) return false;
    }
    const PlayerId player = intent.player;
    const GameObjectId source_id = intent.ability_source;

    // Equip (CR 818): "[Cost]: Attach this gear to a unit you control"
    // (818.1.c.2) — an activated ability on the chain (CR 377.3; testing
    // phase D15). The unit is its target (818.1.b.1), named by the intent; the
    // gear's declared cost (Card::equipCost, less what applies to a gear's
    // activated ability) is paid as it is activated (Card::payEquip); it
    // attaches as it resolves (Card::attachByEquip). Attached Equipment is not
    // handled here: its Equip is Inactive (CR 718.2, 721.2).
    {
        const auto& src = state_.getObject(source_id);
        Card* gear_card = src.isGear() && !src.attached_to.has_value()
            ? card_registry_.get(src.card_def_id) : nullptr;
        if (gear_card && gear_card->hasEquipAbility() && !intent.targets.empty()) {
            const GameObjectId unit = intent.targets[0];
            if (!canPayEquip(player, source_id, unit)) return false;
            const Card::EquipTerms terms = equipTerms(player);
            const auto cost = gear_card->netEquipCost(state_, player, source_id, unit, terms);
            effect_executor_->setOwedCost(EffectExecutor::OwedCost{
                player, cost.energy, cost.power,
                resource::maskOf(cost.power_any_domain ? Domain::Count : cost.power_domain),
                SpendFor::Gear});
            const ChainItemId item_id = chain_manager_->addAbility(
                source_id, player, src.card_def_id, {unit}, /*is_activated=*/true,
                intent.ability_index);
            effect_executor_->setOwedCost(std::nullopt);
            for (auto& ci : state_.chain.items)
                if (ci.id == item_id) { ci.is_equip = true; break; }
            // It counts as this turn's gear ability as it is activated.
            auto& aps = state_.player(player);
            aps.abilities_activated_this_turn++;
            aps.gear_abilities_activated_this_turn++;
            CardContext equip_ctx{state_, events_, *effect_executor_, player, source_id};
            equip_ctx.registry = &card_registry_;
            if (!gear_card->payEquip(equip_ctx, unit, terms))
                throw std::logic_error("GameEngine::payAndQueueAbility: the Equip of " +
                                       state_.getObject(source_id).name +
                                       " was offered and could not be paid");
            finalizeActivatedItem(item_id);
            return true;
        }
    }

    // The ability's cost, from the card the ability comes from (an
    // aura-granted ability's descriptor is the granting card's; the bearer
    // pays it).
    Card* ability_card = card_registry_.get(state_.getObject(source_id).card_def_id);
    Card* cost_card = (intent.granted_ability_def != kInvalidId)
        ? card_registry_.get(intent.granted_ability_def) : ability_card;
    ActivationCost act_cost;
    bool adds = false;
    if (cost_card) {
        const auto& abilities = cost_card->abilities();
        int idx = intent.ability_index;
        if (idx < 0 || idx >= static_cast<int>(abilities.size())) idx = 0;
        if (!abilities.empty()) {
            act_cost = abilities[idx].cost;
            adds = abilities[idx].adds_resources;
        }
        // State-aware reductions, Energy and Power (Bashful Bloom, Risen
        // Altar), clamped >= 0.
        act_cost = netActivationCost(*cost_card, player, source_id, idx, act_cost);
    }
    const SpendFor spend_for = spendForObject(state_.getObject(source_id));
    // Nothing is done unless everything can be paid: an activation whose cost
    // is out of reach is refused, never put on the chain for free. The
    // generators gate on the same conditions, so this only stops a stale or
    // hand-built intent.
    if (cost_card) {
        const auto& gate_abilities = cost_card->abilities();
        int gate_idx = intent.ability_index;
        if (gate_idx < 0 || gate_idx >= static_cast<int>(gate_abilities.size())) gate_idx = 0;
        if (!cost_card->canActivateAbility(state_, player)) return false;
        if (!gate_abilities.empty() &&
            !cost_card->canActivateAbilityAt(state_, player, source_id, gate_idx))
            return false;
    }
    if (act_cost.exhaust && state_.getObject(source_id).is_exhausted) return false;
    const Domain act_domain = act_cost.power_any_domain ? Domain::Count : act_cost.power_domain;
    if ((act_cost.power > 0 || act_cost.energy > 0) &&
        !effect_executor_->canPayWithAdds(player, act_cost.energy, act_cost.power,
                                          resource::maskOf(act_domain), spend_for))
        return false;
    // A typed discard ("Discard a gear") needs that many cards of the type.
    auto typedDiscards = [&]() {
        std::vector<GameObjectId> out;
        for (auto hid : state_.player(player).hand)
            if (static_cast<int>(state_.getObject(hid).card_type) + 1 ==
                    act_cost.discard_type_filter) out.push_back(hid);
        return out;
    };
    if (act_cost.discard && act_cost.discard_count > 0 && act_cost.discard_type_filter != 0 &&
        static_cast<int>(typedDiscards().size()) < act_cost.discard_count)
        return false;
    if (act_cost.xp_cost > 0 && state_.player(player).xp < act_cost.xp_cost) return false;
    if (act_cost.kill_self && !state_.getObject(source_id).location.has_value()) return false;

    // 1. The pending item (CR 402.1). The cost is owed while the choices are
    //    made: a choice's Deflect is judged with it and paid from what it
    //    cannot use (EffectExecutor::OwedCost).
    effect_executor_->setOwedCost(EffectExecutor::OwedCost{
        player, act_cost.energy, act_cost.power, resource::maskOf(act_domain), spend_for});
    struct ClearOwed {
        EffectExecutor& ex;
        ~ClearOwed() { ex.setOwedCost(std::nullopt); }
    } clear_owed{*effect_executor_};
    const OpenClosedState oc_before = state_.turn.oc_state;
    const bool chain_was_empty = state_.chain.items.empty();
    const ChainItemId item_id =
        chain_manager_->addAbility(source_id, player, state_.getObject(source_id).card_def_id,
                                    intent.targets, /*is_activated=*/true, intent.ability_index);
    // Carry the granted-ability source onto the chain item so resolution
    // dispatches to the granting card's onActivate (Forge/Gardens/Heimer).
    // Found by id: a "when you choose a friendly unit" trigger may already
    // sit above the ability on the chain.
    if (intent.granted_ability_def != kInvalidId)
        for (auto& ci : state_.chain.items)
            if (ci.id == item_id) {
                ci.granted_ability_def = intent.granted_ability_def;
                break;
            }

    // 2. Its choices (CR 403.2): the engine's target prompt, or the card's
    //    finalize pass (where costs it pays under costDueNow are paid too).
    for (auto& ci : state_.chain.items)
        if (ci.id == item_id) {
            ci.status = ChainItemStatus::Finalized;   // its choices are being made now
            chooseTargetsAtFinalize(ci);
            break;
        }
    for (const auto& ci : state_.chain.items)
        if (ci.id == item_id && ci.required_choice_missing)
            throw std::logic_error(
                "GameEngine::payAndQueueAbility: " + state_.getObject(source_id).name +
                " was activated with no legal choice for a required target (CR 403.3: it is"
                " not legal to activate it)");
    effect_executor_->setOwedCost(std::nullopt);

    // 3. Its cost (CR 405.1).
    auto& source = state_.getObject(source_id);
    if (act_cost.exhaust) {
        source.is_exhausted = true;
        events_.emit(ObjectStateChangedEvent{source_id, "exhausted"});
    }
    // "Disempower this," as a cost (Questionable Tome, Hextech Disc, ...).
    if (act_cost.disempower_self)
        effect_executor_->disempower(source_id);
    // "Kill a friendly unit" as a cost: the chosen unit rides in targets.
    if (act_cost.kill_friendly_unit && !intent.targets.empty() &&
        state_.objectExists(intent.targets[0]))
        killUnit(intent.targets[0]);
    // Energy (+ Power): the shared resource path (floating pool, runes, and
    // [Add] Reactions while paying, CR 430.3).
    if (act_cost.power > 0 || act_cost.energy > 0) {
        const bool paid = effect_executor_->payResources(player, act_cost.energy, act_cost.power,
                                                         resource::maskOf(act_domain), spend_for);
        if (!paid)
            throw std::logic_error("GameEngine::payAndQueueAbility: " +
                                   state_.getObject(source_id).name +
                                   " made choices its cost could not then be paid with");
    }
    // "Kill this" as a cost (Gold, Divining Shells, Forge of the Future): the
    // source is gone before anyone can respond (testing phase D38).
    if (act_cost.kill_self && state_.getObject(source_id).location.has_value()) {
        RIFT_TRACE(events_, "ACTIVATE_COST: kill self " + state_.getObject(source_id).name);
        effect_executor_->killObject(source_id);
    }
    // Pay recycle-self cost: the source leaves the board for its OWNER's Main
    // Deck. Gear comes off (CR 719.5), a token ceases to exist instead
    // (CR 183.1), and the departure is announced like any other.
    if (act_cost.recycle_self && state_.getObject(source_id).location.has_value()) {
        auto& src = state_.getObject(source_id);
        RIFT_TRACE(events_, "ACTIVATE_COST: recycle self " + src.name);
        const LocationId was_at = *src.location;
        effect_executor_->snapshotLastKnown(source_id, /*dying=*/false);
        if (src.attached_to.has_value())
            effect_executor_->unattachGear(source_id);
        detachAllGear(source_id);
        const bool is_token = src.isToken();
        if (is_token) {
            src.zone = ZoneType::Banishment;
            src.location = std::nullopt;
        } else {
            // From the board, so both players know which card went down.
            deck::putOnBottom(state_, src.owner, source_id, deck::Place::Public);
        }
        events_.emit(LeftBoardEvent{source_id, player, src.card_type,
            was_at, is_token ? ZoneType::Banishment : ZoneType::MainDeck, false});
        effect_executor_->announceRecycledToMainDeck(source_id);
        // Announced as it left: in the deck it is a new object (CR 124.1).
        effect_executor_->leftBoard(source_id);
    }
    // "Banish this" as a cost.
    if (act_cost.banish_self && state_.getObject(source_id).location.has_value()) {
        RIFT_TRACE(events_, "ACTIVATE_COST: banish self " + state_.getObject(source_id).name);
        effect_executor_->banishObject(source_id);
    }
    // Pay discard cost
    if (act_cost.discard && act_cost.discard_count > 0) {
        RIFT_TRACE(events_, "ACTIVATE_COST: discard " + std::to_string(act_cost.discard_count));
        if (act_cost.discard_type_filter == 0) {
            effect_executor_->discardCards(player, act_cost.discard_count);
        } else {
            // Typed discard ("Discard a spell" / "Discard a gear"): the
            // player chooses which matching card, one at a time (CR 422.1.a),
            // asked even when only one matches, as an untyped discard is.
            for (int n = 0; n < act_cost.discard_count; ++n) {
                const auto matching = typedDiscards();
                if (matching.empty()) break;
                std::vector<Intent> options;
                for (auto hid : matching) {
                    Intent o;
                    o.type = IntentType::MakeChoice;
                    o.player = player;
                    o.chosen_objects = {hid};
                    options.push_back(std::move(o));
                }
                const Intent chosen = decideNow(player, options,
                                                "choose a card to discard (cost)");
                GameObjectId pick = matching.front();
                if (!chosen.chosen_objects.empty() &&
                    std::find(matching.begin(), matching.end(),
                              chosen.chosen_objects.front()) != matching.end())
                    pick = chosen.chosen_objects.front();
                effect_executor_->applyDiscard(player, pick);
            }
        }
    }
    // Pay XP cost
    if (act_cost.xp_cost > 0) {
        state_.player(player).xp -= act_cost.xp_cost;
        RIFT_TRACE(events_, "ACTIVATE_COST: spend " + std::to_string(act_cost.xp_cost) + " XP");
    }
    {
        auto& aps = state_.player(player);
        aps.abilities_activated_this_turn++;
        if (state_.getObject(source_id).isGear()) aps.gear_abilities_activated_this_turn++;
    }
    if (state_.objectExists(source_id)) {
        auto& src = state_.getObject(source_id);
        // "Only once each turn" abilities count the activation, not the
        // resolution: a countered activation is still this turn's use.
        src.card_counters["__ability_used_turn_" + std::to_string(intent.ability_index)] =
            state_.turn.turn_number + 1;
        // "When you play ... an activated ability with Energy cost N" (the
        // printed cost, before reductions).
        int printed_energy = 0;
        if (cost_card) {
            const auto& abilities = cost_card->abilities();
            if (intent.ability_index >= 0 &&
                intent.ability_index < static_cast<int>(abilities.size()))
                printed_energy = abilities[intent.ability_index].cost.energy;
        }
        src.card_counters["__ability_energy_cost"] = printed_energy;
        events_.emit(ObjectStateChangedEvent{source_id, "ability_activated"});
    }

    // 4. Finalized (CR 407), or — an [Add] ability — resolved at once (CR
    //    430.2, 401.2).
    last_activation_added_ = adds;
    if (adds) {
        resolveAddAbilityNow(item_id, oc_before, chain_was_empty);
        return false;
    }
    finalizeActivatedItem(item_id);
    return true;
}

// CR 430.2: "Triggered and activated abilities that Add resources finalize
// immediately when they are played, even when handling tasks or during the
// resolution of spells and abilities, and resolve as soon as they are
// finalized." 430.2.a: "Priority and Focus will not pass from Add abilities
// ... Any ongoing Task or step in the FEPR process is paused." The item is
// taken off the chain and resolved here — whatever is resolving now is set
// aside meanwhile, as for a finalize pass — and the chain, the state and
// whoever holds priority or Focus are as they were (testing phase D2).
void GameEngine::resolveAddAbilityNow(ChainItemId item_id, OpenClosedState state_before,
                                      bool chain_was_empty) {
    auto item = ChainManager::removeItemFrom(state_, item_id);
    if (!item.has_value()) return;
    if (chain_was_empty && state_.chain.items.empty()) state_.turn.oc_state = state_before;
    item->status = ChainItemStatus::Finalized;
    // A prompt the interrupted effect has published and not yet had answered
    // is kept aside, and put back after.
    std::optional<EffectExecutor::PendingChoice> outer_prompt;
    if (effect_executor_->hasPendingChoice()) outer_prompt = effect_executor_->consumePendingChoice();
    std::optional<ChainItem> outer;
    if (state_.chain.resuming.has_value()) outer = std::move(*state_.chain.resuming);
    state_.chain.resuming = std::move(*item);
    RIFT_TRACE(events_, "ADD: " +
                     (state_.objectExists(state_.chain.resuming->source)
                          ? state_.getObject(state_.chain.resuming->source).name.get()
                          : std::string("?")) +
                     " resolves as it is played (CR 430.2)");
    events_.emit(ChainItemFinalizedEvent{state_.chain.resuming->id, state_.chain.resuming->source,
                                         state_.chain.resuming->controller});
    constexpr int kMaxChoices = 1024;
    try {
        for (int iter = 0; ; ++iter) {
            if (iter >= kMaxChoices)
                throw std::logic_error("GameEngine::resolveAddAbilityNow: an [Add] ability asked "
                                       "more than 1024 choices");
            resolveSpell(*state_.chain.resuming);
            if (!effect_executor_->hasPendingChoice()) break;
            auto pending = effect_executor_->consumePendingChoice();
            // A probe (nothing seen, EventBus::quiet) answers with the first
            // option; a game asks the player.
            Intent choice = events_.quiet() || pending.legal.size() == 1
                ? pending.legal.front()
                : queryAgentForChain(pending.player, pending.legal);
            effect_executor_->recordChoice(std::move(choice));
        }
    } catch (...) {
        state_.chain.resuming.reset();
        if (outer.has_value()) state_.chain.resuming = std::move(*outer);
        throw;
    }
    const ChainItem done = std::move(*state_.chain.resuming);
    state_.chain.resuming.reset();
    if (outer.has_value()) state_.chain.resuming = std::move(*outer);
    if (outer_prompt.has_value())
        effect_executor_->requestChoice(outer_prompt->player, std::move(outer_prompt->legal),
                                        std::move(outer_prompt->label));
    events_.emit(ChainItemResolvedEvent{done.id, done.source, done.controller});
}

// The [Reaction] abilities that [Add] which `player` could activate now (CR
// 430.3: "can be activated at any time that spells or abilities require
// resources be paid").
std::vector<Intent> GameEngine::addReactionsFor(PlayerId player) const {
    std::vector<Intent> all, out;
    generateActivateAbilityActions(player, all, AbilityTiming::Reaction,
                                   IntentType::ActivateReactionAbility);
    for (auto& i : all) {
        if (!state_.objectExists(i.ability_source)) continue;
        const Card* c = i.granted_ability_def != kInvalidId
            ? card_registry_.get(i.granted_ability_def)
            : card_registry_.get(state_.getObject(i.ability_source).card_def_id);
        if (!c) continue;
        const auto& abs = c->abilities();
        if (i.ability_index < 0 || i.ability_index >= static_cast<int>(abs.size())) continue;
        if (abs[i.ability_index].adds_resources) out.push_back(std::move(i));
    }
    return out;
}

// Whether `check` holds once `player` has activated every [Add] Reaction they
// can (each re-listed after the last) — tried on a copy of the game, quietly,
// and thrown away: the game is moved aside (its objects stay where references
// up the call stack point) and moved back after (as prePlayProbe does).
bool GameEngine::probeAdds(PlayerId player, const std::function<bool(const GameState&)>& check) {
    if (adds_probing_) return check(state_);
    struct Parked {
        GameState& live;
        GameState game;
        explicit Parked(GameState& s) : live(s), game(std::move(s)) {}
        ~Parked() { live = std::move(game); }
    };
    struct ForgetFacts {
        GameEngine& engine;
        ~ForgetFacts() { engine.forgetCleanupFacts(); }
    } forget_facts{*this};
    struct Flag {
        bool& f;
        explicit Flag(bool& b) : f(b) { f = true; }
        ~Flag() { f = false; }
    } flag(adds_probing_);
    std::optional<EffectExecutor::PendingChoice> prompt;
    if (effect_executor_->hasPendingChoice()) prompt = effect_executor_->consumePendingChoice();
    bool ok = false;
    {
        Parked parked(state_);
        state_ = parked.game;
        forgetCleanupFacts();
        struct QuietRun {
            EventBus& bus;
            EventBus::Quiet q;
            explicit QuietRun(EventBus& b) : bus(b), q(b.beginQuiet()) {}
            ~QuietRun() { bus.endQuiet(q); }
        } quiet(events_);
        try {
            for (int k = 0; k < 32 && !ok; ++k) {
                if (check(state_)) { ok = true; break; }
                const auto adds = addReactionsFor(player);
                if (adds.empty()) break;
                (void)payAndQueueAbility(adds.front());
            }
            if (!ok) ok = check(state_);
        } catch (...) {
            ok = false;
        }
        if (effect_executor_->hasPendingChoice()) effect_executor_->consumePendingChoice();
    }
    if (prompt.has_value())
        effect_executor_->requestChoice(prompt->player, std::move(prompt->legal),
                                        std::move(prompt->label));
    return ok;
}

// CR 407: the activated ability is finalized — announced; players get
// priority before it resolves.
void GameEngine::finalizeActivatedItem(ChainItemId item_id) {
    for (auto& ci : state_.chain.items) {
        if (ci.id != item_id) continue;
        ci.status = ChainItemStatus::Finalized;
        events_.emit(ChainItemFinalizedEvent{ci.id, ci.source, ci.controller});
        return;
    }
}

void GameEngine::executeIntent(const Intent& intent) {
    RIFT_TRACE(events_, std::string("INTENT: ") + toString(intent.type) + " by " + toString(intent.player));

    switch (intent.type) {
        case IntentType::PlayCard:
        case IntentType::PlayActionCard:
        case IntentType::PlayReaction: {   // a [Reaction] card played in an Open State
            auto& card = state_.getObject(intent.card);
            if (card.is_hidden) {
                executePlayFromHidden(intent);
            } else if (card.isSpell()) {
                executePlaySpell(intent);
            } else {
                executePlayCard(intent);
            }
            break;
        }
        case IntentType::ActivateAbility:
        case IntentType::ActivateActionAbility:
        case IntentType::ActivateReactionAbility: {
            // An Equip adds no chain item, but paying for it can trigger
            // abilities (a rune recycled for its Power): whatever is on the
            // chain afterwards is run, not left for a later action.
            const bool queued = payAndQueueAbility(intent);
            if (queued || chain_manager_->chainExists()) runChain();
            break;
        }
        case IntentType::StandardMove:
            executeStandardMove(intent);
            break;
        case IntentType::EndTurn:
            // Nothing to execute: resolveMainPhaseDecision ends the main
            // phase on this intent.
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
            if (state_.game_over) break;   // GameEngine::askAgent ended the game already
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

// CR 356.2: an additional cost the card being played prints is chosen and paid
// here, before its Energy / Power cost is determined, so "If you do, reduce my
// cost by ..." reaches the payment. The card makes the choices and pays
// (Card::payAdditionalPlayCost); the engine only says what the play could
// still afford.
void GameEngine::forgetPlayPayments(PlayerId player, GameObjectId card_id) {
    if (!state_.objectExists(card_id)) return;
    const auto& card = state_.getObject(card_id);
    Card::forgetPlayPayments(state_,
        card.card_def_id != kInvalidId ? card_registry_.get(card.card_def_id) : nullptr,
        player, card_id);
}

Card::PlayCostAdjust GameEngine::payAdditionalPlayCosts(PlayerId player, GameObjectId card_id,
                                                        int extra_any_power) {
    if (!state_.objectExists(card_id)) return {};
    const auto& card = state_.getObject(card_id);
    if (card.card_def_id == kInvalidId) return {};
    Card* cc = card_registry_.get(card.card_def_id);
    if (!cc) return {};
    // Also reached by plays that do not come through executePlayCard /
    // executePlaySpell (a reaction played off the chain, an effect that plays
    // a card and pays for it): the play starts here for them.
    Card::forgetPlayPayments(state_, cc, player, card_id);
    const auto addl = cc->additionalPlayCost(state_, card_db_, player, card_id);
    if (!addl.exists || !addl.payable) return {};
    CardContext ctx{state_, events_, *effect_executor_, player, card_id};
    ctx.registry = &card_registry_;
    const auto adj = cc->payAdditionalPlayCost(ctx, [&](const Card::PlayCostAdjust& a) {
        return canAffordWith(player, card_id, extra_any_power, a);
    });
    if (adj.ignore_cost || adj.energy_reduction > 0 || adj.power_reduction > 0)
        RIFT_TRACE(events_, "ADDITIONAL_COST: " + state_.getObject(card_id).name +
                         (adj.ignore_cost ? " ignores its cost"
                              : " costs [" + std::to_string(adj.energy_reduction) + "] and " +
                                std::to_string(adj.power_reduction) + " Power less"));
    return adj;
}

// What the targets a spell chose take off its cost: "your spells that choose
// me cost [1] or [A] less" (GameObject::spells_targeting_me_cost_reduction —
// the player picks which half when the spell has both), and a battlefield's
// "each spell that chooses a friendly unit here costs [A] less".
// `prefer_power`: take the "or [A]" half where there is a choice.
Card::PlayCostAdjust GameEngine::targetCostDiscount(PlayerId player,
                                                    const std::vector<GameObjectId>& targets,
                                                    bool prefer_power, bool* has_choice) const {
    Card::PlayCostAdjust out;
    int either = 0;
    for (auto t : targets) {
        if (!state_.objectExists(t)) continue;
        const auto& tobj = state_.getObject(t);
        // "YOUR spells that choose me": the discount is its controller's.
        if (tobj.controller == player)
            either = std::max(either, tobj.spells_targeting_me_cost_reduction);
        if (!tobj.isUnit() || tobj.controller != player) continue;
        if (auto tbf = tobj.battlefieldId())
            for (const auto& b : state_.battlefields)
                if (b.id == *tbf)
                    out.power_reduction =
                        std::max(out.power_reduction, b.friendly_spell_discount_power);
    }
    if (has_choice) *has_choice = either > 0;
    if (prefer_power) out.power_reduction += either;
    else out.energy_reduction += either;
    return out;
}

// The play gate for a spell with the targets it would choose (CR 355 → 356):
// its cost after what those targets take off, plus the Deflect (CR 809) each
// of them adds, must be payable together.
bool GameEngine::canAffordChoosing(PlayerId player, GameObjectId card_id,
                                   const std::vector<GameObjectId>& targets) const {
    return canAffordChoosing(player, card_id, targets, targets);
}

bool GameEngine::canAffordChoosing(PlayerId player, GameObjectId card_id,
                                   const std::vector<GameObjectId>& targets,
                                   const std::vector<GameObjectId>& unpaid_deflect) const {
    if (targets.empty()) return canAfford(player, card_id);
    if (!passesPlayGates(player, card_id)) return false;
    const auto& card = state_.getObject(card_id);
    const Card* cc = card.card_def_id != kInvalidId ? card_registry_.get(card.card_def_id)
                                                    : nullptr;
    int deflect = 0;
    if (!cc || !cc->ignoresDeflect())
        for (auto t : unpaid_deflect) deflect += deflectCostFor(state_, player, t);
    Card::AdditionalPlayCost addl;
    if (cc) addl = cc->additionalPlayCost(state_, card_db_, player, card_id);
    if (addl.exists && addl.mandatory && !addl.payable) return false;
    for (int prefer_power = 0; prefer_power < 2; ++prefer_power) {
        bool has_choice = false;
        const auto disc = targetCostDiscount(player, targets, prefer_power == 1, &has_choice);
        if (prefer_power == 1 && !has_choice) break;
        auto with = [&](Card::PlayCostAdjust a) {
            a.energy_reduction += disc.energy_reduction;
            a.power_reduction += disc.power_reduction;
            return canAffordWith(player, card_id, deflect, a);
        };
        if (addl.exists && addl.payable && with(addl.best)) return true;
        if (!(addl.exists && addl.mandatory) && with(Card::PlayCostAdjust{})) return true;
    }
    return false;
}

// ── A spell's choices before its cost: probing and the viable pass ──────────

GameEngine::ScriptedPass GameEngine::runScriptedPass(const GameState& from, const ChainItem& pre,
                                                     const std::vector<int>& script,
                                                     bool stop_after_script) {
    // The record of decisions made so far belongs to the game, not to the
    // state a run starts from.
    auto history = std::move(state_.action_history);
    const int decisions = state_.decision_index;
    state_ = from;
    forgetCleanupFacts();
    ++pre_play_state_copies_;
    state_.action_history = std::move(history);
    state_.decision_index = decisions;
    return runScriptedPassHere(pre, script, stop_after_script);
}

GameEngine::ScriptedPass GameEngine::runScriptedPassHere(const ChainItem& pre,
                                                         const std::vector<int>& script,
                                                         bool stop_after_script,
                                                         bool note_discounts) {
    ScriptedPass out;
    bool dead = false;
    FinalizeAnswerer answer = [&](size_t k, PlayerId p,
                                  const std::vector<Intent>& legal) -> std::optional<Intent> {
        out.option_counts.push_back(static_cast<int>(legal.size()));
        if (note_discounts) {
            std::vector<int> pays;
            for (size_t i = 0; i < legal.size(); ++i)
                for (auto t : legal[i].chosen_objects) {
                    if (!state_.objectExists(t) || !state_.getObject(t).location.has_value())
                        continue;
                    // The pre-play choice filter is the test: with what is
                    // chosen so far, choosing `t` leaves the spell payable.
                    if (effect_executor_->choiceAllowed(t)) {
                        pays.push_back(static_cast<int>(i));
                        break;
                    }
                }
            out.discount_options.push_back(std::move(pays));
        }
        if (k < script.size()) {
            if (script[k] < 0 || script[k] >= static_cast<int>(legal.size())) {
                dead = true;
                return std::nullopt;
            }
            return legal[static_cast<size_t>(script[k])];
        }
        if (stop_after_script) {
            out.next_options = legal;
            out.next_player = p;
            return std::nullopt;
        }
        if (legal.empty()) { dead = true; return std::nullopt; }
        return legal.front();
    };
    bool completed = false;
    try {
        out.done = collectFinalizeChoices(pre, &answer, &completed);
    } catch (...) {
        // Every throw of a finalize pass is a card breaking a contract (a
        // prompt after a paid cost, an early end, an endless pass). It is an
        // error here as it is in the real pass: counting the sequence as one
        // not to offer would withhold the play and hide the card's defect.
        state_.chain.resuming.reset();
        if (effect_executor_->hasPendingChoice()) effect_executor_->consumePendingChoice();
        throw;
    }
    out.completed = completed && !dead;
    if (dead) out.next_options.clear();
    return out;
}

bool GameEngine::prePlaySequenceExists(const ToPassStart& to_start, bool at_start,
                                       const ChainItem& pre, const std::vector<int>& script,
                                       const PrePlayPayable& payable) {
    // A run that throws must not leave the bus quiet.
    struct QuietSearch {
        EventBus& bus;
        EventBus::Quiet q;
        explicit QuietSearch(EventBus& b) : bus(b), q(b.beginQuiet()) {}
        ~QuietSearch() { bus.endQuiet(q); }
    };
    const QuietSearch quiet(events_);
    bool found = false;
    int runs = 0;
    std::vector<std::vector<int>> todo{script};
    while (!todo.empty() && runs < kPrePlayProbeRuns && !found) {
        const std::vector<int> prefix = std::move(todo.back());
        todo.pop_back();
        // Every run starts from the start of the pass. The first one of a
        // search that was handed the state there needs no reset.
        if (runs > 0 || !at_start) to_start();
        ++runs;
        ++pre_play_runs_total_;
        pre_play_runs_last_ = runs;
        pre_play_runs_max_ = std::max(pre_play_runs_max_, runs);
        const ScriptedPass run = runScriptedPassHere(pre, prefix, /*stop_after_script=*/false,
                                                     /*note_discounts=*/true);
        // (A sequence that leaves a required target unchosen is no way to
        // play it: CR 355.8.)
        if (run.completed && !run.done.required_choice_missing &&
            payable(run.done.chosen_objects)) { found = true; break; }
        // Every other answer to each prompt this run met past its prefix: all
        // of them are walked, so "none is payable" is an answer and not a
        // guess. The order is what keeps a search short when one is: an
        // answer naming an object with which the spell is payable there and
        // then is tried before the others (it goes on the stack last). A
        // spell that offers
        // every unit on the board at a prompt (Flash) is then decided in two
        // or three runs instead of one per unit (plans/09 item 9.II1).
        for (int pass = 0; pass < 2; ++pass)
            for (size_t k = run.option_counts.size(); k-- > prefix.size();) {
                const auto& pays = run.discount_options[k];
                for (int alt = run.option_counts[k] - 1; alt >= 1; --alt) {
                    const bool discount =
                        std::find(pays.begin(), pays.end(), alt) != pays.end();
                    if (discount != (pass == 1)) continue;
                    std::vector<int> next = prefix;
                    next.resize(k, 0);
                    next.push_back(alt);
                    todo.push_back(std::move(next));
                }
            }
    }
    // The bound was reached with sequences still unwalked and none found: the
    // answer is not known. Saying "none" would withhold a play that may be
    // legal, so it is an error naming the card, as the engine's other bounds
    // are; the remedy is a card whose pass asks less, or a larger bound.
    // (No registered spell comes near it: SweepCostsTest.
    // FinalizePassSpell_NoRegisteredSpellComesNearTheProbeBound.)
    if (!found && !todo.empty())
        throw std::logic_error("GameEngine::prePlaySequenceExists: " +
                               state_.getObject(pre.source).name + " has more than " +
                               std::to_string(kPrePlayProbeRuns) +
                               " sequences of choices before its cost and none walked so far "
                               "makes it payable: the engine cannot tell whether to offer it");
    return found;
}

// The contract this relies on: a finalize pass is REPEATABLE — run again from
// the same state with the same answers, it asks the same prompts with the same
// options in the same order. The pass is run many times here (to find the next
// prompt, to test each option, to ask, and once more for real), each from a
// copy of the starting state; a card whose prompts depend on anything outside
// the GameState and its chain item (a member of the card object, a static, a
// clock) breaks that, and one of the three throws below names it.
ChainItem GameEngine::choosePrePlayViable(const ChainItem& pre, const PrePlayPayable& payable) {
    const GameState pass_start = state_;
    ++pre_play_state_copies_;
    std::vector<int> script;
    // A run that throws must not leave the bus quiet.
    struct QuietRun {
        EventBus& bus;
        EventBus::Quiet q;
        explicit QuietRun(EventBus& b) : bus(b), q(b.beginQuiet()) {}
        ~QuietRun() { bus.endQuiet(q); }
    };
    // Back to the start of the pass, the record of decisions kept.
    const ToPassStart to_start = [&] {
        auto history = std::move(state_.action_history);
        const int decisions = state_.decision_index;
        state_ = pass_start;
        forgetCleanupFacts();
        ++pre_play_state_copies_;
        state_.action_history = std::move(history);
        state_.decision_index = decisions;
    };
    for (;;) {
        // Where the choices so far lead: the next prompt, or the end.
        ScriptedPass at;
        {
            const QuietRun quiet(events_);
            at = runScriptedPass(pass_start, pre, script, /*stop_after_script=*/true);
        }
        if (at.completed) break;
        if (at.next_options.empty()) {
            const std::string name = state_.getObject(pre.source).name;
            to_start();
            throw std::logic_error("GameEngine::choosePrePlayViable: " + name +
                                   " reached a prompt with no options (or its finalize pass "
                                   "is not repeatable)");
        }
        // Only the options a payable completion can still follow. (Each
        // search leaves the state where its last run ended: the next one, and
        // the run that asks, start from pass_start themselves.)
        std::vector<int> viable;
        const std::vector<Intent> options = at.next_options;
        const PlayerId asked_player = at.next_player;
        for (int i = 0; i < static_cast<int>(options.size()); ++i) {
            std::vector<int> with = script;
            with.push_back(i);
            bool exists = false;
            try {
                exists = prePlaySequenceExists(to_start, /*at_start=*/false, pre, with, payable);
            } catch (...) {
                to_start();
                throw;
            }
            if (exists) viable.push_back(i);
        }
        if (viable.empty()) {
            const std::string name = state_.getObject(pre.source).name;
            to_start();
            throw std::logic_error("GameEngine::choosePrePlayViable: " + name +
                                   " was offered with no payable sequence of choices (or "
                                   "its finalize pass is not repeatable: the same answers must "
                                   "meet the same prompts)");
        }
        // Ask with the state as the choices so far left it.
        {
            const QuietRun quiet(events_);
            (void)runScriptedPass(pass_start, pre, script, /*stop_after_script=*/true);
        }
        std::vector<Intent> offered;
        offered.reserve(viable.size());
        for (int i : viable) offered.push_back(options[static_cast<size_t>(i)]);
        RIFT_TRACE(events_, std::string("CHOICE-REQUEST (before payment): [") +
                         std::to_string(offered.size()) + " of " +
                         std::to_string(options.size()) + " options keep the spell payable] (" +
                         toString(asked_player) + ")");
        const EffectExecutor::Asking named(*effect_executor_,
            (state_.objectExists(pre.source) ? state_.getObject(pre.source).name : std::string("?")) +
            ": a choice made as it is played");
        const Intent chosen = queryAgentForChain(asked_player, offered);
        int picked = viable.front();
        for (size_t v = 0; v < offered.size(); ++v)
            if (offered[v] == chosen && offered[v].choice_label == chosen.choice_label) {
                picked = viable[v];
                break;
            }
        script.push_back(picked);
    }
    // The choices are made: run the pass for real, seen by everyone.
    ScriptedPass final_run = runScriptedPass(pass_start, pre, script, /*stop_after_script=*/false);
    if (!final_run.completed)
        throw std::logic_error("GameEngine::choosePrePlayViable: " +
                               state_.getObject(pre.source).name +
                               " did not complete the choices it had just made (its "
                               "finalize pass is not repeatable)");
    return final_run.done;
}

bool GameEngine::prePlayProbe(PlayerId player, GameObjectId card_id,
                              const PlayerState::TrashReplayGrant* grant) {
    if (state_.chain.resuming.has_value() || !state_.objectExists(card_id)) return false;
    // The game is set aside whole — moved, not copied: its objects stay where
    // every reference held up the call stack points — and the probe plays on
    // a copy, which is thrown away when the game is moved back. One copy of
    // the GameState per probe that finds its answer in one run (it was five:
    // a saved state, a pass-start state, one per run, and two restores), one
    // more for each further run (plans/09 item 9.HH4).
    struct Parked {
        GameState& live;
        GameState game;
        explicit Parked(GameState& s) : live(s), game(std::move(s)) {}
        ~Parked() { live = std::move(game); }
    };
    // (Declared first, so it runs after `parked` has put the game back.)
    struct ForgetFacts {
        GameEngine& engine;
        ~ForgetFacts() { engine.forgetCleanupFacts(); }
    } forget_facts{*this};
    Parked parked(state_);
    ++pre_play_probes_;
    // As the play would begin: the card leaves its zone.
    ChainItem pre;
    const ToPassStart to_start = [&] {
        state_ = parked.game;
        forgetCleanupFacts();
        ++pre_play_state_copies_;
        auto& ps = state_.player(player);
        ps.hand.erase(std::remove(ps.hand.begin(), ps.hand.end(), card_id), ps.hand.end());
        ps.trash.erase(std::remove(ps.trash.begin(), ps.trash.end(), card_id), ps.trash.end());
        forgetPlayPayments(player, card_id);
        // The same id every time: the counter is part of the copied state.
        pre.id = state_.chain.allocateId();
    };
    to_start();
    pre.status = ChainItemStatus::Pending;
    pre.source = card_id;
    pre.card_def_id = state_.getObject(card_id).card_def_id;
    pre.controller = player;
    pre.is_spell = true;
    pre.pre_play_choices = true;
    // The grant, if any, is looked up again in each run: the pointer handed in
    // points into the game that is set aside.
    const bool via_grant = grant != nullptr;
    struct FilterSet {
        EffectExecutor& ex;
        ~FilterSet() { ex.setChoiceFilter(nullptr); }
    };
    effect_executor_->setChoiceFilter([this, player, card_id, via_grant](GameObjectId t) {
        std::vector<GameObjectId> all;
        if (state_.chain.resuming.has_value()) all = state_.chain.resuming->chosen_objects;
        all.push_back(t);
        const auto* g = via_grant ? trashGrantFor(player, card_id) : nullptr;
        return g ? canPayTrashGrant(player, card_id, *g, all, {t})
                 : (!via_grant && canAffordChoosing(player, card_id, all, {t}));
    });
    const FilterSet filter_set{*effect_executor_};
    const PrePlayPayable payable = [this, player, card_id, via_grant](
                                       const std::vector<GameObjectId>& chosen) {
        const auto* g = via_grant ? trashGrantFor(player, card_id) : nullptr;
        if (via_grant) return g && canPayTrashGrant(player, card_id, *g, chosen, {});
        return chosen.empty() ? canAfford(player, card_id)
                              : canAffordChoosing(player, card_id, chosen, {});
    };
    // (A card-contract error thrown by the search leaves through the guards
    // above: the filter is cleared, the bus is as it was, the game is back.)
    return prePlaySequenceExists(to_start, /*at_start=*/true, pre, {}, payable);
}

void GameEngine::executePlayCard(const Intent& intent) {
    auto& ps = state_.player(intent.player);
    auto& card = state_.getObject(intent.card);
    forgetPlayPayments(intent.player, intent.card);   // a new play: nothing is paid yet
    // Where the card is played from. The Chosen Champion's play intent is the
    // hand play's (the generator lists it with the hand's units, CR 420.1.a),
    // but it is not from the hand: "from anywhere other than your hand"
    // (Heart of the Tempest, finding G11) and "from hand" cost modifiers
    // read this.
    const Intent::PlaySource play_source =
        (intent.play_source == Intent::PlaySource::Hand && card.zone == ZoneType::ChampionZone)
            ? Intent::PlaySource::ChampionZone : intent.play_source;
    // CR 354: the card goes on the chain as the play begins. Its item is made
    // below, once the cost is paid; whatever the payment triggers meanwhile
    // is newer than it (placeBelowWhatItsPlayTriggered).
    const ChainItemId first_new_item = state_.chain.next_id;
    int trace_cost = (card.card_def_id != kInvalidId)
        ? card_db_.get(card.card_def_id).energy_cost : 0;
    RIFT_TRACE(events_, "PLAY: " + card.name + " (id=" + toString(intent.card) +
                     ", " + toString(card.card_type) + ", cost=" +
                     std::to_string(trace_cost) + "E)");

    // Remove from current zone (hand or champion zone) (CR 354: step 1)
    if (card.zone == ZoneType::Hand) {
        auto it = std::find(ps.hand.begin(), ps.hand.end(), intent.card);
        if (it != ps.hand.end()) ps.hand.erase(it);
    } else if (card.zone == ZoneType::ChampionZone) {
        ps.champion_zone = kInvalidId;
    } else if (intent.play_source == Intent::PlaySource::Trash &&
               card.zone == ZoneType::Trash) {
        // Played from the trash (a play-from-trash permission).
        auto& owner_trash = state_.player(card.owner).trash;
        auto it = std::find(owner_trash.begin(), owner_trash.end(), intent.card);
        if (it != owner_trash.end()) owner_trash.erase(it);
    }

    // Pay the card's cost (CR 357). Surface the play_source via a
    // transient field on PlayerState so the cost path can consult it
    // for Rek'Sai's auto-Accelerate (and any future from-non-hand
    // discounts). Always clear after the call.
    // An additional cost the card prints (CR 356.2) is chosen and paid BEFORE
    // the cost itself, so its discount applies. Not offered on a play for an
    // alternative cost or for a play-from-trash grant's cost.
    int location_power = 0;   // a battlefield's pay-to-play-here cost (Dragon Roost)
    if (intent.pay_location_cost && intent.play_location.has_value() &&
        std::holds_alternative<BattlefieldLocation>(*intent.play_location)) {
        const auto bf_id = std::get<BattlefieldLocation>(*intent.play_location).id;
        for (const auto& bf : state_.battlefields)
            if (bf.id == bf_id) location_power = std::max(0, bf.paid_play_power);
    }
    // "As you play me" choices (CR 355.1), made as the play begins, before
    // anything is paid.
    effect_executor_->makeAsYouPlayChoices(intent.player, intent.card);
    bool paid_via_grant = false;
    if (intent.play_source == Intent::PlaySource::Trash)
        paid_via_grant = payTrashReplayGrant(intent.player, intent.card);
    Card::PlayCostAdjust prepay;
    if (!intent.use_alt_play_cost && !paid_via_grant)
        prepay = payAdditionalPlayCosts(intent.player, intent.card, location_power);

    ps.current_play_source = play_source;
    if (intent.use_alt_play_cost) {
        // Alternative play cost (Jhin, Meticulous Killer: "play me for [B]"):
        // pay the card's alternativePlayCost instead of the printed cost.
        const Card* c = (card.card_def_id != kInvalidId)
            ? card_registry_.get(card.card_def_id) : nullptr;
        Card::AltPlayCost alt = c ? c->alternativePlayCost(state_, intent.player)
                                  : Card::AltPlayCost{};
        // [A] is any Domain, and several [A] need not share one.
        const Domain d = alt.any_domain ? Domain::Count : alt.power_domain;
        payAdditionalCost(intent.player, alt.energy, alt.power, d, spendForObject(card));
        RIFT_TRACE(events_, "ALT_COST: " + card.name + " played for its alternative cost");
    } else if (!paid_via_grant) {
        // Choices before costs (CR 355.1.a): whether to pay [Accelerate] and
        // the card's own "you may pay ... as an additional cost" are decided
        // now, each only if everything stays payable; then one payment.
        prepay = decideAccelerate(intent.player, intent.card, prepay, location_power);
        maybePayOptionalAdditionalCost(intent.player, intent.card, location_power, prepay);
        payCardCost(intent.player, intent.card, prepay);
    }
    ps.current_play_source = Intent::PlaySource::Hand;

    // A battlefield's pay-to-play-here additional cost (Dragon Roost).
    if (intent.pay_location_cost && intent.play_location.has_value() &&
        std::holds_alternative<BattlefieldLocation>(*intent.play_location)) {
        const auto bf_id = std::get<BattlefieldLocation>(*intent.play_location).id;
        for (const auto& bf : state_.battlefields) {
            if (bf.id != bf_id || bf.paid_play_power <= 0) continue;
            effect_executor_->payAnyPower(intent.player, bf.paid_play_power);
            RIFT_TRACE(events_, "ADDITIONAL_COST: " + card.name + " pays " +
                             std::to_string(bf.paid_play_power) + " [A] to be played to BF#" +
                             toString(bf_id));
        }
    }

    // ("You may pay X as an additional cost to play me" — Akshan, Nami — was
    // decided and paid above, before the card's cost: its play trigger reads
    // card_counters[paid_flag].)
    if (intent.use_alt_play_cost || paid_via_grant)
        maybePayOptionalAdditionalCost(intent.player, intent.card, 0, Card::PlayCostAdjust{});

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
    // Store the play location on the game object so resolvePermanent can use it.
    // Permanents choose location during finalization (CR 355.2.a) — before
    // the play is announced, so "when you play a unit here" abilities can
    // tell where it is being played.
    card.location = intent.play_location.value_or(BaseLocation{intent.player});

    // CR 383.4.a.2: play effects "are put on the Chain as Pending Items after
    // the Permanent these effects correspond to is finalized and enters the
    // board." The play is announced by resolvePermanent, once the card is on
    // the board (announcePermanentPlayed); what it needs travels on the card.
    card.card_counters["__announce_play"] = ps.cards_played_this_turn;
    card.card_counters["__announce_energy"] = energy_spent;
    card.card_counters["__announce_not_hand"] =
        play_source != Intent::PlaySource::Hand ? 1 : 0;

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
    const ChainItemId permanent_item = chain_manager_->addPermanent(intent.card, intent.player);
    placeBelowWhatItsPlayTriggered(permanent_item, first_new_item);
    runChain();
}

void GameEngine::executePlaySpell(const Intent& intent) {
    auto& ps = state_.player(intent.player);
    auto& card = state_.getObject(intent.card);
    forgetPlayPayments(intent.player, intent.card);   // a new play: nothing is paid yet
    spell_play_first_new_item_ = state_.chain.next_id;   // CR 354: on the chain from here on
    if (!events_.logging()) {
        events_.noteUnheardLine();   // the SPELL line below, unread
    } else {
        std::string tgt_str;
        for (auto t : intent.targets) {
            if (!tgt_str.empty()) tgt_str += ",";
            tgt_str += state_.objectExists(t) ? state_.getObject(t).name.get() : "?";
            tgt_str += "(id=" + toString(t) + ")";
        }
        RIFT_TRACE(events_, "SPELL: " + card.name + " (id=" + toString(intent.card) +
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

    // CR 355: a spell's targets are chosen before its cost is determined and
    // paid, so effects that depend on what it chooses apply to the payment
    // (Sandswept Tomb, Irelia, Graceful). Spells that take one target through
    // Card::pickTarget choose it here; the rest keep the targets they were
    // played with.
    std::vector<GameObjectId> play_targets = intent.targets;
    bool target_chosen_at_play = false;
    if (play_targets.empty() && card.card_def_id != kInvalidId) {
        const Card* sc = card_registry_.get(card.card_def_id);
        // (A spell that also makes choices in a finalize pass asks for this
        // target in that pass, with the rest, below. Ignoring Deflect does not
        // move its target after the cost — testing phase D34, Decree of
        // Insight: canAffordChoosing counts no Deflect for it.)
        if (sc && sc->needsPlayTimeTarget() &&
            !(sc->choicesAtFinalization() & Card::kFinalizeResolve)) {
            // Only targets the spell can still be paid for with: its cost
            // and that target's Deflect are owed together (CR 356.2.a.2).
            std::vector<GameObjectId> legal;
            // A play through a trash-replay grant is judged on the grant's
            // cost — the gate the generator offered it by.
            const PlayerState::TrashReplayGrant* grant =
                intent.play_source == Intent::PlaySource::Trash
                    ? trashGrantFor(intent.player, intent.card) : nullptr;
            for (auto t : sc->enumerateLegalTargets(state_, intent.player))
                if (grant ? canPayTrashGrant(intent.player, intent.card, *grant, {t})
                          : canAffordChoosing(intent.player, intent.card, {t}))
                    legal.push_back(t);
            if (!legal.empty()) {
                std::vector<Intent> options;
                options.reserve(legal.size());
                for (auto t : legal) {
                    Intent i;
                    i.type = IntentType::MakeChoice;
                    i.player = intent.player;
                    i.chosen_objects = {t};
                    options.push_back(std::move(i));
                }
                const Intent chosen = decideNow(intent.player, options,
                                                "choose a target for " + card.name);
                GameObjectId t = legal.front();
                if (!chosen.chosen_objects.empty() &&
                    std::find(legal.begin(), legal.end(), chosen.chosen_objects.front()) !=
                        legal.end())
                    t = chosen.chosen_objects.front();
                play_targets = {t};
                target_chosen_at_play = true;
                RIFT_TRACE(events_, "TARGET (play): " + card.name + " -> " +
                                 state_.getObject(t).name);
            }
        }
    }

    // A spell that chooses in its finalize pass (kFinalizeResolve) makes those
    // choices here too, before its cost is determined (CR 355 -> 356), so a
    // discount from what it chooses counts. Only objects it can still be paid
    // for with are on offer; the choices are announced, and adopted by the
    // chain item, when the spell goes on the chain (chooseTargetsAtFinalize).
    // `cost_targets` is what the cost is judged on; Deflect for these choices
    // is paid by the choice helper as each is made.
    // Each choice's Deflect (CR 809) is paid as it is made, in this same step
    // of the play, and a unit is on offer only while the spell's cost and
    // that Deflect can be paid together.
    std::vector<GameObjectId> cost_targets = play_targets;
    prefinalized_spell_.reset();
    if (card.card_def_id != kInvalidId && !state_.chain.resuming.has_value()) {
        const Card* sc = card_registry_.get(card.card_def_id);
        if (sc && (sc->choicesAtFinalization() & Card::kFinalizeResolve)) {
            const PlayerState::TrashReplayGrant* grant =
                intent.play_source == Intent::PlaySource::Trash
                    ? trashGrantFor(intent.player, intent.card) : nullptr;
            const bool via_grant = grant != nullptr;
            ChainItem pre;
            pre.id = state_.chain.allocateId();
            pre.status = ChainItemStatus::Pending;
            pre.source = intent.card;
            pre.card_def_id = card.card_def_id;
            pre.controller = intent.player;
            pre.is_spell = true;
            pre.pre_play_choices = true;
            // Targets the play intent already names (one intent per target)
            // are the spell's own: its pass reads them, and the cost counts them.
            pre.targets = play_targets;
            const PlayerId who = intent.player;
            const GameObjectId what = intent.card;
            // (The grant is looked up afresh each time: the viable pass
            // replaces the state its pointer points into.)
            // The play intent's own targets (`pt`) count toward every check:
            // their discounts apply and their Deflect is still owed (it is
            // paid with the cost, finishPlaySpell).
            const std::vector<GameObjectId> pt = play_targets;
            auto withPlayTargets = [pt](std::vector<GameObjectId> chosen) {
                for (auto t : pt)
                    if (std::find(chosen.begin(), chosen.end(), t) == chosen.end())
                        chosen.push_back(t);
                return chosen;
            };
            effect_executor_->setChoiceFilter([this, who, what, via_grant, pt,
                                               withPlayTargets](GameObjectId t) {
                // Discounts from everything chosen so far and `t`; the
                // Deflect of the earlier choices is already paid. The same
                // for a play through a trash-replay grant.
                std::vector<GameObjectId> all;
                if (state_.chain.resuming.has_value()) all = state_.chain.resuming->chosen_objects;
                all.push_back(t);
                all = withPlayTargets(all);
                std::vector<GameObjectId> unpaid = pt;
                unpaid.push_back(t);
                const auto* g = via_grant ? trashGrantFor(who, what) : nullptr;
                return g ? canPayTrashGrant(who, what, *g, all, unpaid)
                         : (!via_grant && canAffordChoosing(who, what, all, unpaid));
            });
            // A Deflect paid ahead of the cost offers only the runes that
            // leave the spell payable with what is chosen so far (D9).
            effect_executor_->setAheadOfCostCheck([this, who, what, via_grant, pt,
                                                   withPlayTargets]() {
                std::vector<GameObjectId> chosen;
                if (state_.chain.resuming.has_value()) chosen = state_.chain.resuming->chosen_objects;
                chosen = withPlayTargets(chosen);
                const auto* g = via_grant ? trashGrantFor(who, what) : nullptr;
                if (via_grant) return g && canPayTrashGrant(who, what, *g, chosen, pt);
                return chosen.empty() ? canAfford(who, what)
                                      : canAffordChoosing(who, what, chosen, pt);
            });
            // (Deflect for these choices is paid as each is made: none is
            // owed when the spell's own cost is judged, but the intent's.)
            const PrePlayPayable payable = [this, who, what, via_grant, pt, withPlayTargets](
                                               const std::vector<GameObjectId>& chosen_in) {
                const std::vector<GameObjectId> chosen = withPlayTargets(chosen_in);
                const auto* g = via_grant ? trashGrantFor(who, what) : nullptr;
                if (via_grant) return g && canPayTrashGrant(who, what, *g, chosen, pt);
                return chosen.empty() ? canAfford(who, what)
                                      : canAffordChoosing(who, what, chosen, pt);
            };
            // Too dear on its own, the spell was offered because some
            // sequence of its choices makes it payable
            // (payableThroughAFinalizeChoice). Then only choices such a
            // sequence can still follow are put to the player, so the play
            // cannot end unpaid; otherwise the pass runs as it is.
            const bool too_dear = grant ? !canPayTrashGrant(who, what, *grant, pt, pt)
                                        : !(pt.empty() ? canAfford(who, what)
                                                       : canAffordChoosing(who, what, pt));
            ChainItem done;
            try {
                done = too_dear ? choosePrePlayViable(pre, payable)
                                : collectFinalizeChoices(std::move(pre));
            } catch (...) {
                effect_executor_->setChoiceFilter(nullptr);
                effect_executor_->setAheadOfCostCheck(nullptr);
                throw;
            }
            effect_executor_->setChoiceFilter(nullptr);
            effect_executor_->setAheadOfCostCheck(nullptr);
            done.pre_play_choices = false;
            cost_targets = play_targets;
            for (auto t : done.chosen_objects)
                if (std::find(cost_targets.begin(), cost_targets.end(), t) == cost_targets.end())
                    cost_targets.push_back(t);
            if (!payable(cost_targets))
                throw std::logic_error("GameEngine::executePlaySpell: " +
                                       state_.getObject(what).name +
                                       " made choices it cannot be paid for with");
            // CR 355.8: "In order to put a spell ... on the chain, valid choices
            // must be made for all targets." An offer whose pass meets a
            // required target with nothing to choose is a play that cannot be
            // made: the gate and the card's pick lists disagree (testing phase
            // D8). An engine error, as for any offered option that cannot be
            // executed.
            if (done.required_choice_missing)
                throw std::logic_error("GameEngine::executePlaySpell: " +
                                       state_.getObject(what).name +
                                       " was offered with no legal choice for a required target");
            prefinalized_spell_ = std::move(done);
        }
    }

    // The viable pass re-runs the choices from copies of the state, so the
    // references taken above may no longer point into it: the rest of the
    // play takes its own.
    finishPlaySpell(intent, std::move(play_targets), target_chosen_at_play,
                    std::move(cost_targets));
}

void GameEngine::finishPlaySpell(const Intent& intent, std::vector<GameObjectId> play_targets,
                                 bool target_chosen_at_play,
                                 std::vector<GameObjectId> cost_targets) {
    auto& ps = state_.player(intent.player);
    auto& card = state_.getObject(intent.card);
    const std::vector<GameObjectId> deflect_targets = play_targets;

    // Pay cost. A trash-replay grant overrides the printed cost (and its own
    // additional costs); otherwise the normal play_source-aware path runs.
    bool paid_via_grant = false;
    if (intent.play_source == Intent::PlaySource::Trash) {
        paid_via_grant = payTrashReplayGrant(intent.player, intent.card, cost_targets);
    }
    if (!paid_via_grant) {
        ps.current_play_source = intent.play_source;
        // What the chosen targets take off the cost (Irelia, Graceful: "your
        // spells that choose me cost [1] or [A] less"; Sandswept Tomb), and
        // the Deflect they add — owed together with the spell's own cost.
        int deflect = 0;
        {
            const Card* sc = card.card_def_id != kInvalidId
                ? card_registry_.get(card.card_def_id) : nullptr;
            if (!sc || !sc->ignoresDeflect())
                for (auto t : deflect_targets) deflect += deflectCostFor(state_, intent.player, t);
        }
        // An additional cost the spell prints (CR 356.2), chosen and paid
        // before the cost: "you may spend a buff ... If you do, ignore this
        // spell's cost." It may be declined only if the spell is affordable
        // without it — with the target discount at its better half.
        bool has_choice = false;
        const auto disc_energy = targetCostDiscount(intent.player, cost_targets, false, &has_choice);
        const auto disc_power = targetCostDiscount(intent.player, cost_targets, true, nullptr);
        auto plus = [](Card::PlayCostAdjust a, const Card::PlayCostAdjust& b) {
            a.energy_reduction += b.energy_reduction;
            a.power_reduction += b.power_reduction;
            a.ignore_cost = a.ignore_cost || b.ignore_cost;
            return a;
        };
        Card::PlayCostAdjust prepay;
        if (card.card_def_id != kInvalidId) {
            if (Card* sc = card_registry_.get(card.card_def_id)) {
                const auto addl = sc->additionalPlayCost(state_, card_db_, intent.player,
                                                         intent.card);
                if (addl.exists && addl.payable) {
                    CardContext pctx{state_, events_, *effect_executor_,
                                     intent.player, intent.card};
                    pctx.registry = &card_registry_;
                    prepay = sc->payAdditionalPlayCost(pctx,
                        [&](const Card::PlayCostAdjust& a) {
                            return canAffordWith(intent.player, intent.card, deflect,
                                                 plus(a, disc_energy)) ||
                                   (has_choice &&
                                    canAffordWith(intent.player, intent.card, deflect,
                                                  plus(a, disc_power)));
                        });
                }
            }
        }
        // "[1] or [A] less": the player picks the half when both would do
        // something and both leave the spell payable.
        Card::PlayCostAdjust total = plus(prepay, disc_energy);
        if (has_choice && !prepay.ignore_cost) {
            const auto before = netCardCost(intent.player, intent.card, prepay);
            const bool energy_ok = before.first > 0 &&
                canAffordWith(intent.player, intent.card, deflect, plus(prepay, disc_energy));
            const bool power_ok = before.second > 0 &&
                canAffordWith(intent.player, intent.card, deflect, plus(prepay, disc_power));
            bool take_power = power_ok && !energy_ok;
            if (energy_ok && power_ok) {
                Intent e; e.type = IntentType::MakeChoice; e.player = intent.player;
                e.chosen_value = 0; e.choice_label = "cost [1] less";
                Intent pw = e; pw.chosen_value = 1; pw.choice_label = "cost [A] less";
                take_power = decideNow(intent.player, {e, pw},
                    "discount for " + card.name + ": [1] or [A] less").chosen_value.value_or(0) == 1;
            }
            if (take_power) total = plus(prepay, disc_power);
        }
        // "You may pay X as an additional cost to play me" (spell variant):
        // chosen before anything is paid (CR 355.1.a), paid with the cost.
        maybePayOptionalAdditionalCost(intent.player, intent.card, deflect, total);
        payCardCost(intent.player, intent.card, total);
        ps.current_play_source = Intent::PlaySource::Hand;
    }

    // Repeat (CR 820) — an optional additional cost, asked yes/no per printed
    // Repeat cost once the base cost is paid. Each cost can be paid a single
    // time (CR 820.1.c.3) and each payment repeats the effect once more
    // (CR 820.3). The costs are the card's own declaration
    // (Card::repeatCosts); the engine reads no card text.
    struct RepeatTranche {
        int energy = 0;
        int power = 0;
        resource::DomainMask domains = resource::kAnyDomain;
        int discard_cards = 0;
    };
    std::vector<RepeatTranche> repeat_costs;
    if (card.card_def_id != kInvalidId) {
        if (const Card* sc = card_registry_.get(card.card_def_id))
            for (const auto& rc : sc->repeatCosts())
                repeat_costs.push_back({rc.energy, rc.power,
                                        resource::maskOf(rc.power_domain), rc.discard_cards});
    }
    // GRANTED [Repeat] (when the spell has none of its own printed):
    //   The Academy (772) / Temporal Portal (401): "next spell gets [Repeat]
    //   equal to its (base) cost" (one-shot).
    //   Syndra, Transcendent (708): "your spells have [Repeat] [2][P]" (continuous
    //   while she's in a showdown). The one-shot grant takes precedence.
    if (card.isSpell() && repeat_costs.empty()) {
        if (ps.grant_repeat_base_to_next_spell && card.card_def_id != kInvalidId) {
            // "[Repeat] equal to its base cost": Energy and Power both, the
            // Power payable in any of the spell's own Domains, as its base
            // cost is.
            const auto& base = card_db_.get(card.card_def_id);
            if (base.energy_cost > 0 || base.power_cost > 0)
                repeat_costs.push_back({base.energy_cost, base.power_cost,
                                        resource::maskOf(base.domains), 0});
        } else if (ps.spells_have_repeat_energy > 0 || ps.spells_have_repeat_power > 0) {
            repeat_costs.push_back({ps.spells_have_repeat_energy, ps.spells_have_repeat_power,
                                    resource::maskOf(ps.spells_have_repeat_domain), 0});
        }
    }
    // The one-shot grant is for "your next spell this turn": this spell uses
    // it up whether or not it applied (a spell with a printed [Repeat] keeps
    // its own).
    if (card.isSpell()) ps.grant_repeat_base_to_next_spell = false;
    int repeats = 0;
    int repeat_energy_spent = 0;
    for (auto& rc : repeat_costs) {
        // Marai Spire (525): "friendly [Repeat] costs cost [1] less."
        if (ps.repeat_cost_reduction > 0)
            rc.energy = std::max(0, rc.energy - ps.repeat_cost_reduction);
        if (static_cast<int>(ps.hand.size()) < rc.discard_cards) continue;
        if (!canPayOptionalCost(intent.player, rc.energy, rc.power, rc.domains,
                                SpendFor::Spell)) continue;
        Intent decline;
        decline.type = IntentType::MakeChoice;
        decline.player = intent.player;
        decline.chosen_value = 0;
        Intent accept = decline;
        accept.chosen_value = 1;
        decline.declines = true;   // the "no" of a yes/no (Intent::declines)
        std::vector<Intent> opts = {decline, accept};

        state_.decision_index++;
        RIFT_TRACE_DECISION(events_, "DECISION #" + std::to_string(state_.decision_index) +
                         " (" + toString(intent.player) + "): "
                         "pay Repeat tranche " + std::to_string(repeats + 1) +
                         " for " + card.name + "? [decline|pay] [2 options]");
        auto chosen = askAgent(intent.player, opts);
        if (on_decision) on_decision(state_, opts, chosen);
        if (chosen.chosen_value.value_or(0) != 1) continue;
        if (!payOptionalCost(intent.player, rc.energy, rc.power, rc.domains, SpendFor::Spell))
            continue;
        if (rc.discard_cards > 0)
            effect_executor_->discardCards(intent.player, rc.discard_cards);
        ++repeats;
        repeat_energy_spent += rc.energy;
    }
    if (repeats > 0) {
        RIFT_TRACE(events_, "REPEAT: paid " + std::to_string(repeats) +
                          " extra tranche(s) for " + card.name);
    }

    // Track play count
    ps.cards_played_this_turn++;
    int energy_spent = 0;
    if (card.card_def_id != kInvalidId) {
        energy_spent = card_db_.get(card.card_def_id).energy_cost;
    }
    int total_energy_spent = energy_spent + repeat_energy_spent;
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
    // "Played from anywhere other than your hand" is announced as the spell
    // resolves, with its other play triggers (CR 420.4.a; finding G12).
    card.card_counters[Card::kPlayedNotFromHand] =
        intent.play_source != Intent::PlaySource::Hand ? 1 : 0;
    events_.emit(CardPlayedEvent{intent.card, intent.player,
        card.card_type, ps.cards_played_this_turn, total_energy_spent});

    // Add spell to chain with targets. Carry energy_spent and repeats_paid
    // onto the chain item — ChainManager re-resolves the spell `repeats_paid`
    // extra times in its post-resume loop.
    // addSpell announces the spell's choices after pushing it, and an ability
    // that triggers on them ("when you choose a friendly unit") goes on the
    // chain above it — so the spell is found by its id, not as items.back().
    const auto chain_id = chain_manager_->addSpell(intent.card, intent.player, play_targets);
    // CR 354 -> 357: the spell went on the chain before its costs were paid,
    // so an ability a cost triggered (the Deathknell of a unit killed as an
    // additional cost) is the newer item and resolves first.
    if (spell_play_first_new_item_ != kInvalidId)
        placeBelowWhatItsPlayTriggered(chain_id, spell_play_first_new_item_);
    spell_play_first_new_item_ = kInvalidId;
    for (auto& added : state_.chain.items) {
        if (added.id != chain_id) continue;
        added.total_energy_spent = total_energy_spent;
        added.repeats_paid = repeats;
        if (target_chosen_at_play) {
            // Read by Card::pickTarget: the target is already chosen.
            while (added.resume_data.size() < 6) added.resume_data.push_back(0);
            added.resume_data[5] = 1;
        }
        break;
    }

    // Run the FEPR loop
    runChain();
}

void GameEngine::runChain() {
    if (!chain_manager_->running()) chain_run_choices_at_start_ = decisions_with_a_choice_;
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
        RIFT_TRACE(events_, std::string(item.finalizing ? "FINALIZE-CHOICES: " : "RESOLVE: ") +
                         (state_.objectExists(item.source) ? state_.getObject(item.source).name
                                                           : std::string("?")) +
                         " (chain_item=" + toString(item.id) +
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
    // (avoids interfering with chain resume / multi-resolve paths), and
    // that is the contract for target ROLES: the filtered list keeps the
    // chosen order but closes the gaps, so a card whose slots differ
    // ("kill this one, pump that one") reads each slot off the item
    // through Card::chosenTarget, which answers kInvalidId for a slot
    // whose target was dropped here.
    //
    // Cards that defer target choice to resolve time via
    // pickTarget/pickTargetPair (Phase 6q opt-in) are unaffected —
    // they re-enumerate legal targets at resolve time themselves.
    Card* card = item.keyword_ability ? keywordAbilityCard(*item.keyword_ability)
                                      : card_registry_.get(item.card_def_id);
    std::vector<GameObjectId> effective_targets = item.targets;
    if (card && !item.targets.empty()) {
        // An activated ability is re-validated against ITS OWN requirements
        // and legal set (multi-ability cards differ per ability).
        auto reqs = card->getTargetRequirements();
        auto legal = card->enumerateLegalTargets(state_, item.controller);
        if (item.is_ability && item.is_activated_ability) {
            const Card* acard = (item.granted_ability_def != kInvalidId)
                ? card_registry_.get(item.granted_ability_def) : card;
            if (acard) {
                const auto& abs = acard->abilities();
                if (item.ability_index >= 0 &&
                    item.ability_index < static_cast<int>(abs.size())) {
                    reqs = abs[item.ability_index].targets;
                    legal = acard->enumerateLegalTargets(state_, item.controller,
                                                         item.ability_index, item.source);
                }
            }
        }
        if (reqs.count > 0) {
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
                RIFT_TRACE(events_, "RESOLVE: " +
                                  (state_.objectExists(item.source)
                                       ? state_.getObject(item.source).name : std::string("?")) +
                                  " — " +
                                  std::to_string(dropped) +
                                  " target(s) no longer legal "
                                  "(per-CR 359.3.e.5: unaffected, "
                                  "rest of spell still resolves)");
            }
            effective_targets = std::move(filtered);
        }
    }

    // Dispatch through Card object
    static const std::vector<GameObjectId> kNoTriggerTargets;
    CardContext ctx{state_, events_, *effect_executor_,
                    item.controller, item.source};
    ctx.firing_trigger = item.fired_trigger;
    ctx.registry = &card_registry_;  // lets effects dispatch into other cards' logic

    if (card && item.is_equip) {
        // An Equip (CR 818.1.c.2): "attach this gear to a unit you control" —
        // the unit it chose, if it is still a legal target (testing phase D15).
        const GameObjectId unit = item.targets.empty() ? GameObjectId{kInvalidId}
                                                       : item.targets.front();
        if (unit != kInvalidId && card->attachByEquip(ctx, unit))
            RIFT_TRACE(events_, "EQUIP: " + state_.getObject(item.source).name + " -> " +
                             state_.getObject(unit).name);
        else
            RIFT_TRACE(events_, "EQUIP: nothing attached (the unit or the gear is gone)");
        return;
    }
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
                // Aura-granted ability (Forge/Gardens/Heimer): run the GRANTING
                // card's onActivate with the bearer (item.source) as ctx.source.
                Card* act_card = (item.granted_ability_def != kInvalidId)
                    ? card_registry_.get(item.granted_ability_def) : card;
                bool is_empower = false;
                if (act_card) {
                    const auto& abs = act_card->abilities();
                    is_empower = item.ability_index >= 0 &&
                        item.ability_index < static_cast<int>(abs.size()) &&
                        abs[item.ability_index].is_empower;
                }
                if (is_empower) {
                    // [Empower] (CR 827.2): the source becomes Empowered.
                    effect_executor_->empower(item.source);
                } else if (act_card) {
                    act_card->onActivate(ctx, item.ability_index, effective_targets);
                }
                // Prize of Progress: a gear's activated ability was used.
                if (!item.finalizing && state_.objectExists(item.source) &&
                    state_.getObject(item.source).isGear()) {
                    events_.emit(ObjectStateChangedEvent{item.source,
                                                         "gear_ability_used"});
                }
            } else if (item.reflexive_index >= 0) {
                // A reflexive trigger (CR 387-389), put on the chain by the
                // ability that contains it.
                card->onReflexiveTrigger(ctx, item.reflexive_index, item.reflexive_data);
            } else if (item.equipped_unit != kInvalidId) {
                // An Equipment's "When I ..." ability (TriggerManager::
                // queueEquippedTrigger): the gear's card, for the equipped unit.
                card->onEquippedTrigger(ctx, item.equipped_unit, kNoTriggerTargets);
            } else {
                // A triggered ability is never handed targets, whatever the
                // chain item holds (CR 383.3: it chooses them itself, through
                // pickTarget / pickTargetPair at finalization; the event's
                // subject is ChainItem::triggering_subject). Enforced here, so
                // a card that reads `targets` in onTrigger reads nothing.
                card->onTrigger(ctx, kNoTriggerTargets);
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

    card.zone = std::holds_alternative<BattlefieldLocation>(loc)
        ? ZoneType::BattlefieldZone : ZoneType::Base;
    card.location = loc;

    // Units enter exhausted (CR 143.4) unless Accelerate / an "enters ready" effect
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
        // Decided — and paid — with the card's cost (decideAccelerate; CR
        // 355.1.a, 805.2.b: "as long as the cost was paid, that unit will
        // still enter ready"). Asked here, on its own, for the plays that do
        // not pay a cost of their own first: from facedown, a unit with no
        // Domain (offerAccelerateOnItsOwn).
        int accelerate_decided = 0;
        if (auto ad = card.card_counters.find(Card::kAccelerateDecided); ad != card.card_counters.end()) {
            accelerate_decided = ad->second;
            card.card_counters.erase(ad);
        }
        if (accelerate_decided == 2) enters_ready = true;
        if (accelerate_decided == 0) {
            // Where the play came from: recorded with its choices (a play
            // from facedown is not from a hand).
            bool from_hand = !card.card_counters.count("__played_from_facedown") ||
                             card.card_counters["__played_from_facedown"] == 0;
            if (auto fh = card.card_counters.find(Card::kAccelerateFromHand);
                fh != card.card_counters.end())
                from_hand = fh->second == 1;
            if (offerAccelerateOnItsOwn(item.controller, item.source, from_hand))
                enters_ready = true;
        }
        state_.getObject(item.source).card_counters.erase(Card::kAccelerateFromHand);
        // [Ambush] (CR 822.1.b) grants only a play location and [Reaction]
        // timing ("I have [Reaction] as long as I'm being played to a
        // battlefield where you control Units"). It does NOT make the unit
        // enter ready — CR 143.4 still applies.
        // Cards that intrinsically enter ready (e.g. Daisy! "I enter ready").
        if (card.card_def_id != kInvalidId) {
            if (const Card* cdef = card_registry_.get(card.card_def_id)) {
                // Prefer the state-aware overload so cards with
                // conditional ready (Monch) can consult state. Default
                // impl falls through to the no-arg version.
                if (cdef->entersReadyOnPlay(state_, card.controller, card)) enters_ready = true;
            }
        }
        // "Units you play this turn enter ready." (a resolved spell's effect)
        if (state_.player(item.controller).units_enter_ready_this_turn) enters_ready = true;
        // A continuous effect of another unit on the board ("Other friendly
        // units enter ready." — its applyPassiveAura lists it as a source).
        if (state_.player(card.controller).otherUnitsEnterReady(card.id)) enters_ready = true;
        card.is_exhausted = !enters_ready;
    }
    // Gear enters ready (CR 149.1) unless it says "This enters exhausted."
    if (card.isGear()) {
        const Card* gear_def = card.card_def_id != kInvalidId
            ? card_registry_.get(card.card_def_id) : nullptr;
        card.is_exhausted = gear_def && gear_def->entersExhausted();
    }

    card.recomputeMight();

    events_.emit(EnteredBoardEvent{item.source, item.controller,
        card.card_type, loc, true});
    // The permanent is on the board: now the play is announced and its play
    // effects go on the chain (CR 383.4.a.2).
    announcePermanentPlayed(item.source, item.controller);

    // [Quick-Draw] (CR 819.1.d) is the keyword's triggered ability, queued as
    // the play is announced above (engine/keyword_abilities; testing phase
    // D14): its unit is chosen as it is finalized, not as the gear lands.

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
    // A priority decision (it offers PassPriority) can be rebuilt from the
    // state; a card's prompt asked through this path cannot.
    const bool is_priority = std::any_of(actions.begin(), actions.end(),
        [](const Intent& i) { return i.type == IntentType::PassPriority; });
    // Read before asking: the name lives only while the prompt does.
    const std::string asked = effect_executor_ ? effect_executor_->asking() : std::string();
    auto chosen = askAgent(player, actions,
        is_priority ? PendingQuery::ChainPriority : PendingQuery::None);
    // Surface the chain-priority decision in the trace so V&V can
    // confirm priority is being passed correctly between players in
    // Closed State (CR 337). Without this, trivial-pass decisions are
    // invisible — the renderer suppresses the panel for single-option
    // decisions, leaving no marker at all that the engine offered
    // priority to player N. Format mirrors the main DECISION trace lines.
    // The header says what was asked, not where in the turn the game is
    // (plans/09 9.PP1): "chain priority" only for a priority decision; a
    // prompt carries its own text (EffectExecutor::asking — a card's prompt
    // label, or the executor's name for one it asks itself).
    if (is_priority) {
        RIFT_TRACE_DECISION(events_, "DECISION #" + std::to_string(state_.decision_index) +
                         " (" + std::string(toString(player)) + " — chain priority): " +
                         std::to_string(actions.size()) + " legal actions");
    } else {
        RIFT_TRACE_DECISION(events_, "DECISION #" + std::to_string(state_.decision_index) +
                         " (" + std::string(toString(player)) + "): " +
                         (asked.empty() ? std::string("a choice") : asked) + " [" +
                         std::to_string(actions.size()) + " options]");
    }
    RIFT_TRACE(events_, "CHOSE: [" + std::string(toString(player)) + "] " +
                     std::string(toString(chosen.type)));
    if (on_decision) {
        on_decision(state_, actions, chosen);
    }
    return chosen;
}

void GameEngine::executeStandardMove(const Intent& intent) {
    if (!intent.move_destination.has_value()) return;
    auto dest = *intent.move_destination;

    // CR 144.3: "Players may perform multiple Units' standard move
    // simultaneously" — one action, one destination, any origins. The intent
    // names the first unit; the player then adds the others one at a time
    // (or stops), which keeps the action list linear in the number of units.
    // A battlefield may charge for each unit beyond the first
    // (BattlefieldState::multi_move_tax): that [A] is paid as the unit is
    // added, and a unit the player cannot pay for is not offered.
    std::vector<GameObjectId> moving = intent.units_to_move;
    if (moving.size() == 1) {
        int tax = 0;
        if (std::holds_alternative<BattlefieldLocation>(dest))
            tax = getBattlefield(std::get<BattlefieldLocation>(dest).id)
                      .multi_move_tax[playerIndex(intent.player)];
        while (true) {
            std::vector<GameObjectId> can_join;
            if (tax <= 0 || payableAnyPower(state_, intent.player) >= tax) {
                for (auto uid : state_.allUnitsControlledBy(intent.player)) {
                    if (std::find(moving.begin(), moving.end(), uid) != moving.end()) continue;
                    const auto dests = standardMoveDestinations(state_.getObject(uid),
                                                                intent.player);
                    if (std::find(dests.begin(), dests.end(), dest) != dests.end())
                        can_join.push_back(uid);
                }
            }
            if (can_join.empty()) break;
            std::vector<Intent> options;
            Intent stop;
            stop.type = IntentType::MakeChoice;
            stop.player = intent.player;
            stop.chosen_value = 0;
            stop.choice_label = "Move no more units";
            options.push_back(stop);
            for (auto uid : can_join) {
                Intent join;
                join.type = IntentType::MakeChoice;
                join.player = intent.player;
                join.chosen_objects = {uid};
                options.push_back(std::move(join));
            }
            const Intent chosen = decideNow(intent.player, options,
                tax > 0 ? "move another unit with this move (pay " + std::to_string(tax) + " [A])"
                        : std::string("move another unit with this move"));
            if (chosen.chosen_objects.empty() ||
                std::find(can_join.begin(), can_join.end(), chosen.chosen_objects.front()) ==
                    can_join.end()) break;
            if (tax > 0) {
                effect_executor_->payAnyPower(intent.player, tax);
                RIFT_TRACE(events_, "MOVE_TAX: " + std::string(toString(intent.player)) + " pays " +
                                 std::to_string(tax) + " [A] to move another unit there");
            }
            moving.push_back(chosen.chosen_objects.front());
        }
    }

    for (auto unit_id : moving) {
        auto& unit_obj = state_.getObject(unit_id);
        RIFT_TRACE(events_, "MOVE: " + unit_obj.name + " (id=" + toString(unit_id) +
                         ") -> " +
                         (std::holds_alternative<BattlefieldLocation>(dest)
                              ? "BF#" + toString(std::get<BattlefieldLocation>(dest).id)
                              : "Base(" + std::string(toString(std::get<BaseLocation>(dest).player)) + ")"));
    }
    // CR 144.3: the units move as ONE action — the exhaust costs are paid
    // simultaneously (144.3.c), then every unit is at the destination, and
    // only then is anything announced: an ability that triggers on the first
    // unit's move sees all of them arrived and all of them exhausted.
    for (auto unit_id : moving) state_.getObject(unit_id).is_exhausted = true;   // CR 144.2
    for (auto unit_id : moving) events_.emit(ObjectStateChangedEvent{unit_id, "exhausted"});
    std::vector<LocationId> origins;
    origins.reserve(moving.size());
    for (auto unit_id : moving) {
        origins.push_back(state_.getObject(unit_id).location.value_or(
            LocationId{BaseLocation{intent.player}}));
        moveUnit(unit_id, dest);
    }
    for (size_t i = 0; i < moving.size(); ++i)
        events_.emit(UnitMovedEvent{moving[i], intent.player, origins[i], dest, true});

    // Check if this contests a battlefield
    if (std::holds_alternative<BattlefieldLocation>(dest)) {
        auto bf_id = std::get<BattlefieldLocation>(dest).id;
        auto& bf = getBattlefield(bf_id);

        // CR 459.2.b.3.a / .4.a — units arriving during ongoing combat
        // inherit the appropriate combat designation. Phase 6q+ fix:
        // previously the unit sat with combat_designation = None and
        // didn't contribute to damage assignment / step resolution.
        if (bf.combat_in_progress) {
            for (auto unit_id : moving) {
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
            RIFT_TRACE_PRIVATE(events_, intent.player,
                             "  MULLIGAN: " + std::string(toString(intent.player)) +
                             " set aside " + state_.getObject(card_id).name +
                             " (id=" + toString(card_id) + ")");
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
        RIFT_TRACE_PRIVATE(events_, intent.player,
                         "  MULLIGAN: " + std::string(toString(intent.player)) +
                         " drew replacement " + state_.getObject(did).name +
                         " (id=" + toString(did) +
                         ") — PRIVATE to " + toString(intent.player));
    }

    // Recycle set-aside cards to bottom of deck in random order (CR 118.3)
    // The owner knows which cards went down, not their order; the opponent
    // knows only how many.
    std::shuffle(set_aside.begin(), set_aside.end(), rng_);
    std::vector<deck::Placed> placed;
    for (auto card_id : set_aside) {
        placed.push_back({card_id, deck::Place::Hand, knowledge::nobody()});
    }
    deck::putOnBottomTogether(state_, intent.player, placed);
    if (!set_aside.empty()) effect_executor_->announceRecycledToMainDeck(set_aside.front());
}

// ── Combat damage and replacement effects (CR 467.2.c.5) ─────────────────────
// "When assigning damage in this way, passive abilities and replacement effects
// that would apply to the resulting damage are considered to apply to the
// assignment instead." These helpers are the single place that says how an
// ASSIGNED amount becomes DEALT damage for a unit, so the assignment cascade
// and the application step always agree.
namespace {

/// Damage actually dealt when `assigned` combat damage is assigned to `t`.
int combatDamageDealt(const GameObject& t, int assigned) {
    if (assigned <= 0) return 0;
    if (t.immune_to_damage) return 0;                    // can't be dealt damage
    if (t.prevent_next_damage_this_turn) return 0;       // next instance prevented
    int dealt = std::max(0, assigned - t.prevent_damage_remaining);
    if (t.damage_doubled_this_turn) dealt *= 2;
    return dealt;
}

/// CR 467.2.c.10: "If a unit cannot be dealt damage, then no amount of damage
/// can be considered lethal. Such a unit is exempt from any considerations of
/// mandatory assignment."
bool canTakeCombatDamage(const GameObject& t) {
    return !t.immune_to_damage && !t.prevent_next_damage_this_turn;
}

/// Minimum ASSIGNED amount that deals lethal damage to `t` (CR 467.2.c.4.a).
/// `any_damage_lethal`: the assigning player's damage kills in any amount
/// (CR 142.4.c — Elder Dragon).
int combatDamageNeededForLethal(const GameObject& t, bool any_damage_lethal) {
    int lethal = any_damage_lethal ? 1 : std::max(1, t.current_might - t.damage_marked);
    if (t.damage_doubled_this_turn) lethal = (lethal + 1) / 2;
    return lethal + t.prevent_damage_remaining;
}

/// Greedy-lethal cascade over `order` (CR 467.2.c.3 / .4): each unit gets
/// exactly lethal before the next gets any; excess goes on the last unit that
/// can be damaged. Units that can't be dealt damage are skipped (467.2.c.10).
std::vector<DamageAssignment> lethalCascade(const GameState& state,
                                            const std::vector<GameObjectId>& order,
                                            int total_damage, bool any_damage_lethal) {
    std::vector<DamageAssignment> da;
    int remaining = total_damage;
    for (auto tid : order) {
        if (remaining <= 0) break;
        const auto& t = state.getObject(tid);
        if (!canTakeCombatDamage(t)) continue;
        int assigned = std::min(remaining, combatDamageNeededForLethal(t, any_damage_lethal));
        da.push_back({tid, assigned});
        remaining -= assigned;
    }
    if (remaining > 0) {
        // "unless no further units remain to have damage assigned to them"
        if (!da.empty()) da.back().damage += remaining;
        else if (!order.empty()) da.push_back({order.front(), remaining});  // nothing damageable
    }
    return da;
}

}  // namespace

bool GameEngine::anyDamageIsLethalFor(PlayerId damager) const {
    // "Any amount of your damage is enough to kill." (CR 142.4.c)
    return effect_executor_->anyDamageIsLethalFor(damager);
}

void GameEngine::applyCombatDamage(GameObjectId target_id, int assigned) {
    if (assigned <= 0 || !state_.objectExists(target_id)) return;
    auto& t = state_.getObject(target_id);
    // Damage assigned beyond what this unit needed for lethal is excess
    // ("if you assigned N or more excess damage").
    {
        const PlayerId assigner = opponent(t.controller);
        const int needed = canTakeCombatDamage(t)
            ? combatDamageNeededForLethal(t, anyDamageIsLethalFor(assigner)) : 0;
        if (assigned > needed) state_.player(assigner).excess_combat_damage += assigned - needed;
    }
    const int dealt = combatDamageDealt(t, assigned);
    // Consume the prevention that applied to this instance.
    if (t.prevent_next_damage_this_turn) {
        t.prevent_next_damage_this_turn = false;
    } else if (!t.immune_to_damage) {
        t.prevent_damage_remaining -= std::min(assigned, t.prevent_damage_remaining);
    }
    if (dealt != assigned)
        RIFT_TRACE(events_, "  COMBAT_DAMAGE: " + t.name + " assigned " + std::to_string(assigned) +
                         " -> dealt " + std::to_string(dealt));
    if (dealt <= 0) return;
    if (t.damage_marked == 0) EffectExecutor::forgetDamageSources(t);
    t.damage_marked += dealt;
    t.damage_taken_this_turn += dealt;
    t.damage_marked_by[playerIndex(opponent(t.controller))] += dealt;
    events_.emit(DamageDealtEvent{target_id, dealt, kInvalidId, true});
}

void GameEngine::executeAssignCombatDamage(const Intent& intent) {
    // Apply damage assignments
    for (auto& assignment : intent.damage_assignments) {
        applyCombatDamage(assignment.target_unit, assignment.damage);
    }
}

void GameEngine::executePassFocus(const Intent& intent) {
    state_.turn.players_passed_focus.insert(intent.player);
}

// What hiding a card costs `player` right now (CR 811.1.b: [A]), after every
// effect that changes or waives it: "You can hide cards ignoring costs this
// turn" and the cards the player has in play (Card::adjustHideCost — "You may
// pay [1] to hide a card instead of [A]").
Card::HideCost GameEngine::hideCostFor(PlayerId player) const {
    Card::HideCost cost;
    const auto& ps = state_.player(player);
    if (ps.hide_ignoring_costs_this_turn) cost.waived = true;
    auto ask = [&](GameObjectId id) {
        if (id == kInvalidId || !state_.objectExists(id)) return;
        const auto& obj = state_.getObject(id);
        if (obj.card_def_id == kInvalidId) return;
        if (const Card* c = card_registry_.get(obj.card_def_id))
            c->adjustHideCost(state_, player, cost);
    };
    ask(ps.legend_zone);
    std::vector<GameObjectId> in_play;
    for (const auto& [id, obj] : state_.objects.onBoard())
        if (obj.controller == player && obj.location.has_value() && !obj.isRune())
            in_play.push_back(id);
    std::sort(in_play.begin(), in_play.end());
    for (auto id : in_play) ask(id);
    return cost;
}

// The gate the Hide generator and executeHideCard share.
bool GameEngine::canPayHideCost(PlayerId player) const {
    const auto cost = hideCostFor(player);
    if (cost.waived) return true;
    if (canPayAdditionalCost(player, 0, cost.power, Domain::Count, SpendFor::Other)) return true;
    return cost.energy_instead &&
           canPayAdditionalCost(player, cost.energy, 0, Domain::Count, SpendFor::Other);
}

void GameEngine::executeHideCard(const Intent& intent) {
    auto& ps = state_.player(intent.player);
    auto& card = state_.getObject(intent.card);

    // Which card it is, is the hiding player's to know (LogAudience); that a
    // card was hidden, and where, both players see.
    RIFT_TRACE(events_, std::string("HIDE: ") + toString(intent.player) + " hides a card at BF#" +
                     toString(intent.chosen_battlefield));
    RIFT_TRACE_PRIVATE(events_, intent.player, "HIDE: " + card.name + " at BF#" +
                     toString(intent.chosen_battlefield) +
                     " by " + toString(intent.player));

    // Pay the Hide cost, [A] (CR 811.1.b) — one Power of any domain, through
    // the shared additional-cost path: floating Power first, else a recycled
    // rune (exhausted before ready). Unpaid, the card is not hidden. Effects
    // may waive it or let Energy be paid instead (hideCostFor); when both
    // ways can be paid the player picks.
    const auto hide_cost = hideCostFor(intent.player);
    if (!hide_cost.waived) {
        const bool power_ok = canPayAdditionalCost(intent.player, 0, hide_cost.power,
                                                   Domain::Count, SpendFor::Other);
        const bool energy_ok = hide_cost.energy_instead &&
            canPayAdditionalCost(intent.player, hide_cost.energy, 0, Domain::Count,
                                 SpendFor::Other);
        if (!power_ok && !energy_ok) {
            RIFT_TRACE_PRIVATE(events_, intent.player, "HIDE: " + card.name +
                                    " not hidden — the cost could not be paid");
            return;
        }
        bool pay_energy = energy_ok && !power_ok;
        if (energy_ok && power_ok) {
            Intent pw; pw.type = IntentType::MakeChoice; pw.player = intent.player;
            pw.chosen_value = 0; pw.choice_label = "pay [A]";
            Intent en = pw; en.chosen_value = 1;
            en.choice_label = "pay [" + std::to_string(hide_cost.energy) + "]";
            // (The prompt's header is a line both players read: it names no
            // card, the one being hidden is in the player's hand — D11.)
            pay_energy = decideNow(intent.player, {pw, en},
                "Hide cost").chosen_value.value_or(0) == 1;
        }
        if (pay_energy)
            payAdditionalCost(intent.player, hide_cost.energy, 0, Domain::Count, SpendFor::Other);
        else
            payAdditionalCost(intent.player, 0, hide_cost.power, Domain::Count, SpendFor::Other);
    }

    // Remove from hand — or from the Champion Zone (CR 811.1.b).
    auto it = std::find(ps.hand.begin(), ps.hand.end(), intent.card);
    if (it != ps.hand.end()) ps.hand.erase(it);
    else if (ps.champion_zone == intent.card) ps.champion_zone = kInvalidId;

    // Place facedown at the chosen battlefield
    auto bf_id = intent.chosen_battlefield;
    auto& bf = getBattlefield(bf_id);
    // (deck::putFacedown also tells the knowledge tracker: the other player
    // cannot tell which card left the hand; a champion hidden from its zone
    // they can.)
    deck::putFacedown(state_, intent.card, bf_id);
    (void)bf;
    card.is_hidden = true;
    card.hidden_at = bf_id;
    card.hidden_on_turn = state_.turn.turn_number;

    RIFT_LOG_FOR(events_, LogLevel::Debug, LogAudience::only(intent.player),
                   std::string("HIDDEN: ") + card.name + " hidden at BF#" +
                   toString(bf_id) + " by " + toString(intent.player));

    // "When you hide a card" (Katarina, Reckless).
    events_.emit(CardHiddenEvent{intent.card, intent.player});
}

void GameEngine::executePlayFromHidden(const Intent& intent) {
    auto& card = state_.getObject(intent.card);
    forgetPlayPayments(intent.player, intent.card);   // a new play: nothing is paid yet
    auto bf_id = card.hidden_at;
    auto& bf = getBattlefield(bf_id);

    // Remove from facedown zone
    auto it = std::find(bf.facedown.begin(), bf.facedown.end(), intent.card);
    if (it != bf.facedown.end()) bf.facedown.erase(it);

    card.is_hidden = false;
    card.hidden_at = kInvalidId;
    // Lets the card's own "When you play me from face down" tell this play
    // apart from a play from hand. Cleared when it next enters a hand/trash.
    card.card_counters["__played_from_facedown"] = 1;
    // Where and when, for a hidden SPELL's own text (CR 811.1.d: its choices
    // are tied to that battlefield; "if you played this from your hand").
    // Turn-stamped, so a stale value never reads as this play.
    card.card_counters["__facedown_play_bf"] = static_cast<int>(bf_id.raw()) + 1;
    card.card_counters["__facedown_play_turn"] = state_.turn.turn_number;

    // "When you play a card from face down" (Katarina, Reckless).
    events_.emit(PlayedFromFacedownEvent{intent.card, intent.player});

    // Play ignoring base cost — permanents go to the BF they were hidden at
    if (card.isPermanent()) {
        effect_executor_->makeAsYouPlayChoices(intent.player, intent.card);   // CR 355.1
        card.location = BattlefieldLocation{bf_id};
        // Route through chain like normal plays
        auto& ps = state_.player(intent.player);
        ps.cards_played_this_turn++;
        // Hidden play is "ignoring its base cost" (CR 811) — energy_spent = 0.
        // Announced once it has entered (CR 383.4.a.2; resolvePermanent).
        // Played from face down = from somewhere other than hand.
        card.card_counters["__announce_play"] = ps.cards_played_this_turn;
        card.card_counters["__announce_energy"] = 0;
        card.card_counters["__announce_not_hand"] = 1;
        chain_manager_->addPermanent(intent.card, intent.player);
        runChain();
    } else if (card.isSpell()) {
        auto& ps = state_.player(intent.player);
        ps.cards_played_this_turn++;
        // Played from face down = from somewhere other than hand; announced as
        // the spell resolves (CR 420.4.a; finding G12).
        card.card_counters[Card::kPlayedNotFromHand] = 1;
        events_.emit(CardPlayedEvent{intent.card, intent.player,
            card.card_type, ps.cards_played_this_turn, /*energy_spent=*/0});
        const auto chain_id =
            chain_manager_->addSpell(intent.card, intent.player, intent.targets);
        // A spell that takes its target through Card::pickTarget was played
        // with that target already chosen (the hidden-play intent names it):
        // tell pickTarget, as executePlaySpell does, or the card asks again
        // and the unit the Deflect was paid for is not the one affected.
        const Card* hc = card.card_def_id != kInvalidId
            ? card_registry_.get(card.card_def_id) : nullptr;
        if (hc && hc->needsPlayTimeTarget() && !intent.targets.empty())
            for (auto& added : state_.chain.items) {
                if (added.id != chain_id) continue;
                while (added.resume_data.size() < 6) added.resume_data.push_back(0);
                added.resume_data[5] = 1;
                break;
            }
        runChain();
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Legal action generation
// ═══════════════════════════════════════════════════════════════════════════════

std::optional<std::vector<Intent>> GameEngine::recomputePendingLegalActions() const {
    switch (pending_query_) {
        case PendingQuery::MainPhase:     return generateMainPhaseActions(pending_query_player_);
        case PendingQuery::Showdown:      return generateShowdownActions(pending_query_player_);
        case PendingQuery::ChainPriority: return generateClosedStateActions(pending_query_player_);
        case PendingQuery::None:          break;
    }
    return std::nullopt;
}

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

std::vector<LocationId> GameEngine::standardMoveDestinations(const GameObject& unit,
                                                             PlayerId player) const {
    std::vector<LocationId> dests;
    if (unit.is_exhausted) return dests;          // exhausting it is the cost (CR 144.2)
    if (unit.cant_move_this_turn) return dests;   // "They can't move it this turn."

    if (unit.isAtBase()) {
        // Can move from base to any battlefield (CR 144.4.a).
        // Rockfall Path's "can't be PLAYED here" does NOT restrict
        // movement — only the play-from-hand action generators
        // consult blocks_unit_play.
        for (auto& bf : state_.battlefields) {
            // Can't move to BF with units from 2 other players (CR 144.4.a.1)
            // In 1v1, this is never an issue
            dests.push_back(BattlefieldLocation{bf.id});
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
        if (unit.aura_cant_move_to_base) allow_to_base = false;  // Minotaur Reckoner
        if (allow_to_base) dests.push_back(BaseLocation{player});

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
            dests.push_back(BattlefieldLocation{bf.id});
        }
    }
    return dests;
}

bool GameEngine::enemyUnitAloneAt(PlayerId player, BattlefieldId bf) const {
    int units = 0, enemies = 0;
    for (const auto& [id, obj] : state_.objects.atLocation(LocationId{BattlefieldLocation{bf}})) {
        if (!obj.isUnit() || obj.battlefieldId() != bf) continue;
        ++units;
        if (obj.controller != player) ++enemies;
    }
    return units == 1 && enemies == 1;
}

std::vector<GameObjectId> GameEngine::handAndChampion(PlayerId player) const {
    const auto& ps = state_.player(player);
    std::vector<GameObjectId> out = ps.hand;
    if (ps.champion_zone != kInvalidId && state_.objectExists(ps.champion_zone) &&
        state_.getObject(ps.champion_zone).zone == ZoneType::ChampionZone)
        out.push_back(ps.champion_zone);
    return out;
}

bool GameEngine::ambushLocation(const Card* card_obj, PlayerId player,
                                const BattlefieldState& bf) const {
    if (bf.hasUnitsFrom(player, state_.objects)) return true;
    return card_obj && card_obj->ambushToEnemyBattlefields() &&
           bf.hasUnitsFrom(opponent(player), state_.objects);
}

bool GameEngine::unitPlayableAt(const Card* card_obj, PlayerId player,
                                const BattlefieldState& bf, const GameObject* unit) const {
    const auto& ps = state_.player(player);
    if (bf.blocks_unit_play) return false;  // Rockfall Path
    // [Ambush] "I may be played to a battlefield where you control Units"
    // (CR 822.1.b): a location permission of its own, not tied to the
    // [Reaction] timing it also grants — so it holds in the Main Phase too
    // (testing phase D37).
    if (unit && unit->hasKeyword(Keyword::Ambush) && ambushLocation(card_obj, player, bf))
        return true;
    // Play-to-location permissions the card prints (CR 355.2), and the ones
    // another card grants every unit this player plays.
    const unsigned extra = card_obj ? card_obj->extraPlayLocations() : 0u;
    const bool controlled = bf.controller.has_value() && *bf.controller == player;
    const bool open = !bf.controller.has_value();
    const bool enemy_occupied = bf.controller.has_value() && *bf.controller != player &&
                                bf.hasUnitsFrom(opponent(player), state_.objects);
    if (controlled) return true;  // default: controlled BFs
    if (open && ((extra & Card::kPlayToOpenBattlefield) ||
                 ps.grant_friendly_units_open_bf)) return true;   // Miss Fortune, Buccaneer
    if (enemy_occupied && (extra & Card::kPlayToOccupiedEnemyBattlefield)) return true;
    if (extra & Card::kPlayToAnyBattlefield) return true;
    if (((extra & Card::kPlayToLoneEnemyBattlefield) || ps.grant_units_lone_enemy_bf) &&
        enemyUnitAloneAt(player, bf.id)) return true;             // Arachnoid Horror
    return false;
}

std::vector<Intent> GameEngine::generateMainPhaseActions(PlayerId player) const {
    std::vector<Intent> actions;

    // Always can end turn
    actions.push_back(Intent::endTurn(player));

    // Standard Move: exhaust ready units to move them. One intent per unit
    // and destination; further units join the same move as it is declared
    // (CR 144.3, executeStandardMove).
    auto all_units = state_.allUnitsControlledBy(player);
    for (auto unit_id : all_units) {
        for (const auto& dest : standardMoveDestinations(state_.getObject(unit_id), player))
            actions.push_back(Intent::standardMove(player, {unit_id}, dest));
    }

    // Play units from hand — must be able to afford the cost.
    auto& ps = state_.player(player);
    const bool locked_out = ps.cant_play_cards_this_turn;  // Brynhir Thundersong
    // The Chosen Champion is played from its zone exactly like a unit from
    // hand (CR 420.1.a): the same lockout and the same valid locations
    // (CR 355.2).
    std::vector<GameObjectId> unit_sources = ps.hand;
    if (ps.champion_zone != kInvalidId && state_.objectExists(ps.champion_zone) &&
        state_.getObject(ps.champion_zone).zone == ZoneType::ChampionZone)
        unit_sources.push_back(ps.champion_zone);
    for (auto card_id : unit_sources) {
        if (locked_out) break;  // skip all play-from-hand actions
        auto& card = state_.getObject(card_id);
        if (!card.isUnit()) continue;
        if (card.card_def_id == kInvalidId) continue;  // tokens have no CardDef
        if (!canAfford(player, card_id)) continue;

        const Card* card_obj = card_registry_.get(card.card_def_id);

        Intent play_intent;
        play_intent.type = IntentType::PlayCard;
        play_intent.player = player;
        play_intent.card = card_id;

        // NARROWING hook (Perched Grimwyrm): the card dictates its own legal
        // play locations and suppresses the default base + BF plays entirely.
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
            const bool can_play_here = unitPlayableAt(card_obj, player, bf, &card);

            if (can_play_here) {
                Intent bf_play = play_intent;
                bf_play.play_location = BattlefieldLocation{bf.id};
                actions.push_back(bf_play);
            } else if (bf.paid_play_power > 0 &&
                       std::find(card.tags.begin(), card.tags.end(), bf.paid_play_tag) !=
                           card.tags.end() &&
                       canAfford(player, card_id, bf.paid_play_power)) {
                // "Any player may pay ... as an additional cost to play a <tag>.
                // If they do, they play it to this battlefield."
                Intent paid_play = play_intent;
                paid_play.play_location = BattlefieldLocation{bf.id};
                paid_play.pay_location_cost = true;
                actions.push_back(paid_play);
            }
        }
    }

    // Champion in the champion zone: its normal play is generated with the
    // units in hand above. Brynhir's lockout covers this play too.
    if (ps.champion_zone != kInvalidId && !locked_out) {
        auto& champ = state_.getObject(ps.champion_zone);
        if (champ.zone == ZoneType::ChampionZone) {
            // Alternative play cost (Jhin, Meticulous Killer): offer "play me for
            // <alt>" when the condition holds and the alt cost is affordable —
            // independent of whether the printed cost is affordable.
            const Card* cc = champ.card_def_id != kInvalidId
                ? card_registry_.get(champ.card_def_id) : nullptr;
            if (cc) {
                auto alt = cc->alternativePlayCost(state_, player);
                bool afford = false;
                if (alt.valid) {
                    if (alt.any_domain) {
                        for (int di = 0; di < static_cast<int>(Domain::Count); ++di)
                            if (canPayAdditionalCost(player, alt.energy, alt.power,
                                                      static_cast<Domain>(di))) {
                                afford = true; break;
                            }
                    } else {
                        afford = canPayAdditionalCost(player, alt.energy, alt.power,
                                                       alt.power_domain);
                    }
                }
                if (afford) {
                    Intent alt_play;
                    alt_play.type = IntentType::PlayCard;
                    alt_play.player = player;
                    alt_play.card = ps.champion_zone;
                    alt_play.play_location = BaseLocation{player};
                    alt_play.use_alt_play_cost = true;
                    actions.push_back(alt_play);
                }
            }
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

    // Equip gear to units (CR 818). One gate for every Equip, the same one
    // the execution pays by (canPayEquip): the gear's declared cost after
    // discounts must be payable for that unit, and an Equipment that is
    // already attached is not offered — its Rules Text, the Equip ability
    // included, is Inactive while attached (CR 718.2, 721.2).
    for (auto& [id, obj] : state_.objects.onBoard()) {
        if (!obj.isGear() || obj.controller != player) continue;
        if (!obj.location.has_value()) continue;

        Card* gear_card = card_registry_.get(obj.card_def_id);
        if (!gear_card || !gear_card->hasEquipAbility()) continue;

        for (auto& [uid, unit] : state_.objects.onBoard()) {
            if (!unit.isUnit() || unit.controller != player) continue;
            if (!unit.location.has_value()) continue;
            if (!canPayEquip(player, id, uid)) continue;

            Intent equip;
            equip.type = IntentType::ActivateAbility;
            equip.player = player;
            equip.ability_source = id;
            equip.targets = {uid};
            actions.push_back(equip);
        }
    }

    // Facedown cards: from the turn after they were hidden they have
    // [Reaction] (CR 811.6), which "is not restricted to Closed States or
    // Showdowns" and includes every other timing (CR 813.2) — its controller's
    // own Main Phase too (testing phase D25).
    generateHiddenPlayActions(player, IntentType::PlayActionCard, actions);

    // Hide cards with [Hidden] keyword facedown at controlled battlefields
    // (CR 811.1.b: "While this card is in your hand or in your Champion Zone").
    std::vector<GameObjectId> hide_sources = ps.hand;
    if (ps.champion_zone != kInvalidId && state_.objectExists(ps.champion_zone) &&
        state_.getObject(ps.champion_zone).zone == ZoneType::ChampionZone)
        hide_sources.push_back(ps.champion_zone);
    for (auto card_id : hide_sources) {
        auto& card = state_.getObject(card_id);
        if (!card.keywords.has(Keyword::Hidden)) continue;

        // The Hide cost — [A] (CR 811.1.b) unless an effect waives it or
        // lets Energy be paid instead — by the gate executeHideCard pays by.
        if (!canPayHideCost(player)) continue;

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

    // Ambush: play units with [Ambush] during showdowns — from the hand or
    // the Champion Zone (CR 420.1.a; testing phase D63).
    for (auto card_id : handAndChampion(player)) {
        if (locked_out) break;
        auto& card = state_.getObject(card_id);
        if (!card.isUnit() || !card.hasKeyword(Keyword::Ambush)) continue;
        if (!canAfford(player, card_id)) continue;

        const Card* amb = card.card_def_id != kInvalidId
            ? card_registry_.get(card.card_def_id) : nullptr;
        for (auto& bf : state_.battlefields) {
            if (bf.blocks_unit_play) continue;  // Rockfall Path
            if (state_.player(player).units_play_base_only) continue;  // Mageseeker Warden
            // A battlefield where the player has units, or (Rengar, Trophy
            // Hunter) one with enemy units.
            if (!ambushLocation(amb, player, bf)) continue;
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
            if (state_.player(player).units_play_base_only) continue;  // Mageseeker Warden
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

    // [Reaction] permanents: Reaction includes Action's permissions (CR 813.1.b).
    generateReactionPermanentActions(player, IntentType::PlayActionCard, actions);

    // Hidden cards have [Reaction] from the turn after they were hidden
    // (CR 811.1.b), and [Reaction] includes [Action] timing (CR 813.1.b).
    generateHiddenPlayActions(player, IntentType::PlayActionCard, actions);

    // Activated abilities with [Action] or [Reaction] timing (CR 806,
    // 813.1.b), a unit's own and those an aura grants it.
    generateActivateAbilityActions(player, actions, AbilityTiming::Action,
                                   IntentType::ActivateActionAbility);

    return actions;
}

namespace {
// Defined with generateSpellActions below.
std::vector<std::vector<GameObjectId>> spellTargetSets(
    const Card* card, const GameState& state, PlayerId player,
    const TargetRequirements& req, const std::vector<GameObjectId>& legal);
}  // namespace

// Permanents that are themselves [Reaction] cards (CR 813.1.c.1), played at
// Reaction / Action timing: `intent_type` is PlayReaction in a Closed State and
// PlayActionCard in a showdown. A unit still goes only where a unit may be
// played (CR 813.3.a); gear goes to the base, and [Quick-Draw] gear (which has
// [Reaction], CR 819.1.b) is attached as it resolves.
void GameEngine::generateReactionPermanentActions(PlayerId player, IntentType intent_type,
                                                   std::vector<Intent>& actions) const {
    const auto& ps = state_.player(player);
    if (ps.cant_play_cards_this_turn) return;
    std::vector<GameObjectId> sources = ps.hand;
    if (ps.champion_zone != kInvalidId && state_.objectExists(ps.champion_zone) &&
        state_.getObject(ps.champion_zone).zone == ZoneType::ChampionZone)
        sources.push_back(ps.champion_zone);
    for (auto card_id : sources) {
        const auto& card = state_.getObject(card_id);
        if (card.card_def_id == kInvalidId) continue;
        const Card* card_obj = card_registry_.get(card.card_def_id);
        if (!card_obj) continue;

        Intent play;
        play.type = intent_type;
        play.player = player;
        play.card = card_id;
        if (card.isGear()) {
            if (!card.hasKeyword(Keyword::QuickDraw) && !card_obj->hasReactionTiming()) continue;
            if (!canAfford(player, card_id)) continue;
            play.play_location = BaseLocation{player};
            actions.push_back(play);
        } else if (card.isUnit()) {
            if (!card_obj->hasReactionTiming()) continue;
            if (!canAfford(player, card_id)) continue;
            if (card_obj->restrictsPlayLocations()) {
                for (const auto& loc : card_obj->getPlayLocations(state_, player)) {
                    Intent restricted = play;
                    restricted.play_location = loc;
                    actions.push_back(restricted);
                }
                continue;
            }
            play.play_location = BaseLocation{player};
            actions.push_back(play);
            if (ps.units_play_base_only) continue;  // Mageseeker Warden
            for (const auto& bf : state_.battlefields) {
                if (!unitPlayableAt(card_obj, player, bf, &card)) continue;
                Intent bf_play = play;
                bf_play.play_location = BattlefieldLocation{bf.id};
                actions.push_back(bf_play);
            }
        }
    }
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

    // [Reaction] permanents: units that are themselves [Reaction] cards, and
    // [Quick-Draw] / [Reaction] gear (attached as it lands, CR 819.1.d).
    generateReactionPermanentActions(player, IntentType::PlayReaction, actions);

    // Ambush: play units with [Ambush] as Reactions to BFs where you have
    // units — from the hand or the Champion Zone (CR 420.1.a; testing phase D63).
    for (auto card_id : handAndChampion(player)) {
        if (locked_out) break;
        auto& card = state_.getObject(card_id);
        if (!card.isUnit() || !card.hasKeyword(Keyword::Ambush)) continue;
        if (!canAfford(player, card_id)) continue;

        const Card* amb = card.card_def_id != kInvalidId
            ? card_registry_.get(card.card_def_id) : nullptr;
        for (auto& bf : state_.battlefields) {
            if (bf.blocks_unit_play) continue;  // Rockfall Path
            if (state_.player(player).units_play_base_only) continue;  // Mageseeker Warden
            if (!ambushLocation(amb, player, bf)) continue;
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
            if (state_.player(player).units_play_base_only) continue;  // Mageseeker Warden
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

    // Hidden cards (they gain [Reaction] the turn after they were hidden).
    generateHiddenPlayActions(player, IntentType::PlayReaction, actions);

    // CR 813.1.c.2 — activated abilities with [Reaction] timing, a unit's
    // own and those an aura grants it. "Can't play cards" (Brynhir
    // Thundersong) does not stop abilities: an ability is not a card.
    generateActivateAbilityActions(player, actions, AbilityTiming::Reaction,
                                   IntentType::ActivateReactionAbility);

    return actions;
}

namespace {
// Every way a spell's targets can be locked in as it is played. A card whose
// target slots differ ("a friendly unit, then an enemy unit") lists its own
// through Card::enumerateLegalTargetSets; otherwise they are built from the
// requirement and the legal targets:
//   - one target: each legal target;
//   - "up to N" (optional): every non-empty set of at most N distinct targets
//     — a set, so each is listed once;
//   - exactly N: every ordered choice of N distinct targets, since the card
//     reads targets[0], targets[1], ... as different roles.
std::vector<std::vector<GameObjectId>> spellTargetSets(
    const Card* card, const GameState& state, PlayerId player,
    const TargetRequirements& req, const std::vector<GameObjectId>& legal) {
    if (card) {
        if (auto own = card->enumerateLegalTargetSets(state, player)) return *own;
    }
    std::vector<std::vector<GameObjectId>> sets;
    if (req.count <= 1) {
        for (auto t : legal) sets.push_back({t});
        return sets;
    }
    std::vector<GameObjectId> current;
    std::vector<bool> used(legal.size(), false);
    std::function<void(size_t)> build = [&](size_t from) {
        if (req.optional ? !current.empty()
                         : static_cast<int>(current.size()) == req.count)
            sets.push_back(current);
        if (static_cast<int>(current.size()) == req.count) return;
        for (size_t i = req.optional ? from : 0; i < legal.size(); ++i) {
            if (used[i]) continue;
            used[i] = true;
            current.push_back(legal[i]);
            build(i + 1);
            current.pop_back();
            used[i] = false;
        }
    };
    build(0);
    return sets;
}
}  // namespace

// CR 811: `player`'s facedown cards. A permanent goes to the battlefield it
// was hidden at (executePlayFromHidden / ChainManager); a spell's targets are
// tied to that battlefield, target by target (CR 811.1.d.2, .4).
// A card played from facedown is played ignoring its base cost (CR 811;
// executePlayFromHidden charges nothing), so there is no cost gate here and no
// discount can matter: what a target adds — its Deflect — is the whole price,
// and Card::enumerateLegalTargets already leaves out a target whose Deflect
// the player cannot pay.
void GameEngine::generateHiddenPlayActions(PlayerId player, IntentType intent_type,
                                           std::vector<Intent>& actions) const {
    const auto& ps = state_.player(player);
    if (ps.cant_play_cards_this_turn) return;   // playing it is playing a card
    for (auto& bf : state_.battlefields) {
        // "Your opponents' [Hidden] cards can't be revealed here."
        if (bf.opp_hidden_unrevealable[playerIndex(player)]) continue;
        // A facedown card belongs to the battlefield's controller (CR 107.3,
        // 811): once that player no longer controls it the card is on its
        // way to the trash (the next cleanup, CR 107.3.d / 323.6.a) and is
        // not played from there. In a game the cleanup that takes control
        // away removes the card at once; this is the same rule for a state
        // reached any other way (plans/09 9.PP3 d).
        if (!bf.controller.has_value() || *bf.controller != player) continue;
        for (auto card_id : bf.facedown) {
            if (!state_.objectExists(card_id)) continue;
            auto& card = state_.getObject(card_id);
            if (card.controller != player) continue;
            // Must be hidden on a previous turn to gain Reaction
            if (card.hidden_on_turn >= state_.turn.turn_number) continue;
            if (card.isSpell() && ps.cant_play_spells_this_turn) continue;
            // A hidden unit is played to that battlefield (CR 811.1.d.1); where
            // units can't be played, or not by this player, it is not played at
            // all — "can't" wins (CR 054.1; testing phase G22). The same rule as
            // an effect play (EffectExecutor::unitMayBePlayedTo).
            if (card.isUnit() && (bf.blocks_unit_play || ps.units_play_base_only)) continue;

            if (card.card_def_id != kInvalidId && card.isSpell()) {
                Card* card_obj = card_registry_.get(card.card_def_id);
                // A spell whose two targets are prompts (needsPlayTimeTargetPair)
                // is one play, as from hand; the pair is chosen at that
                // battlefield as it is finalized (CR 811.1.d.2), and with no
                // pair there it cannot be played from facedown (811.1.d;
                // testing phase D74).
                if (card_obj && card_obj->needsPlayTimeTargetPair()) {
                    if (!card_obj->hasLegalChoicesHiddenAt(state_, player, card_db_, bf.id))
                        continue;
                    Intent play;
                    play.type = intent_type;
                    play.player = player;
                    play.card = card_id;
                    actions.push_back(play);
                    continue;
                }
                if (card_obj) {
                    auto req = card_obj->getTargetRequirements();
                    if (req.count > 0) {
                        // CR 811.1.d.2: each target is chosen from the options
                        // at this battlefield, unless the card's own
                        // restriction on THAT target rules the battlefield out
                        // (811.1.d.4: each target is judged on its own). With
                        // no choice left under that rule the card cannot be
                        // played from facedown (811.1.d).
                        auto here = [&](GameObjectId t) {
                            if (!state_.objectExists(t)) return false;
                            auto tgt_bf = state_.getObject(t).battlefieldId();
                            return tgt_bf && *tgt_bf == bf.id;
                        };
                        auto all_targets = card_obj->enumerateLegalTargets(state_, player);
                        bool offered = false;
                        for (auto& tset : spellTargetSets(card_obj, state_, player, req,
                                                          all_targets)) {
                            bool ok = true;
                            for (size_t slot = 0; slot < tset.size(); ++slot)
                                if (!here(tset[slot]) &&
                                    !card_obj->hiddenTargetFreeOfBattlefield(
                                        static_cast<int>(slot))) { ok = false; break; }
                            if (!ok) continue;
                            Intent play;
                            play.type = intent_type;
                            play.player = player;
                            play.card = card_id;
                            play.targets = std::move(tset);
                            actions.push_back(play);
                            offered = true;
                        }
                        // "Any number of" / "up to N" includes none (CR
                        // 355.13): a spell whose choice is optional
                        // (TargetRequirements::optional) is playable from
                        // facedown choosing nothing when its battlefield has
                        // nothing to choose. Judge ruling 2026-10-11 (plans/09
                        // 9.Z4): this wins over CR 811.1.d's "no valid
                        // targets" — the spell resolves and does nothing.
                        // (From hand the same empty play is offered by
                        // generateSpellActions.)
                        if (!offered && req.optional) {
                            Intent play;
                            play.type = intent_type;
                            play.player = player;
                            play.card = card_id;
                            actions.push_back(play);
                        }
                        continue;
                    }
                }
            }

            // A spell that makes its choices in its finalize pass declares no
            // targets here, but it is still tied to this battlefield (CR
            // 811.1.d.2) and cannot be played from facedown with nothing to
            // choose there (811.1.d).
            if (card.card_def_id != kInvalidId && card.isSpell()) {
                const Card* hc = card_registry_.get(card.card_def_id);
                if (hc && !hc->hasLegalChoicesHiddenAt(state_, player, card_db_, bf.id)) continue;
            }
            // No-target hidden play (or permanent — its location is set
            // to the hidden-at BF by executePlayFromHidden).
            Intent play;
            play.type = intent_type;
            play.player = player;
            play.card = card_id;
            actions.push_back(play);
        }
    }
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
        // The play gate (CR 355 -> 356): targets are chosen before the cost
        // is determined, so a spell is offered with the targets it can be
        // paid for — a discount from the chosen object ("your spells that
        // choose me cost [1] or [A] less") can make it playable, and a
        // Deflect it cannot pay on top of its cost rules a target out.
        const bool afford_plain = canAfford(player, card_id);
        auto affordableChoosing = [&](const std::vector<GameObjectId>& tset) {
            if (afford_plain) {
                int deflect = 0;
                for (auto t : tset) deflect += deflectCostFor(state_, player, t);
                if (deflect == 0) return true;   // nothing the targets add
            }
            return canAffordChoosing(player, card_id, tset);
        };

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
        if (spell_card && !spell_card->hasLegalTargets(state_, player, card_db_)) {
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
        auto emit = [&](Intent play) { actions.push_back(std::move(play)); };

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
            // Offered while some target leaves it payable (the target itself
            // is picked, from the payable ones, as the spell is played).
            bool payable = legal_targets.empty() && afford_plain;
            for (auto t : legal_targets)
                if (affordableChoosing({t})) { payable = true; break; }
            // A pair chosen in a finalize pass: one target that pays for it
            // is not enough when no second target can go with it there
            // (testing phase D8) — a complete payable pair is looked for by
            // making the choices on the side, as for any finalize-pass spell
            // too dear on its own, or one whose choices carry a Deflect.
            if (payable && spell_card->needsPlayTimeTargetPair() &&
                (spell_card->choicesAtFinalization() & Card::kFinalizeResolve) &&
                (!afford_plain || anyDeflectFor(player)))
                payable = payableThroughAFinalizeChoice(player, card_id, nullptr, true);
            if (!payable) continue;
            Intent play;
            play.type = intent_type;
            play.player = player;
            play.card = card_id;
            // targets intentionally empty — agent picks via
            // pickTarget / pickTargetPair at resolve time.
            emit(play);
            continue;
        }

        if (req.count == 0) {
            // No play-time target. A spell that chooses in its finalize pass
            // is still paid for after those choices: offered when one of them
            // would make it payable.
            if (!afford_plain && !payableThroughAFinalizeChoice(player, card_id, nullptr))
                continue;
            Intent play;
            play.type = intent_type;
            play.player = player;
            play.card = card_id;
            emit(play);
        } else if (req.optional && legal_targets.empty()) {
            // Optional targeting with no targets — play without targets
            if (!afford_plain) continue;
            Intent play;
            play.type = intent_type;
            play.player = player;
            play.card = card_id;
            emit(play);
        } else {
            // One intent per legal choice of targets (one target, or a set
            // for a multi-target spell — see spellTargetSets).
            for (auto& tset : spellTargetSets(spell_card, state_, player, req, legal_targets)) {
                if (!affordableChoosing(tset)) continue;
                Intent play;
                play.type = intent_type;
                play.player = player;
                play.card = card_id;
                play.targets = std::move(tset);
                emit(play);
            }
        }
    }
}

// Defined with the cost code further down.
static int extraAnyPowerCost(const GameState& state, PlayerId player, const GameObject& card);
static bool costModifierApplies(const GameState& state, const PlayerState::CostModifier& mod,
                                bool own, PlayerId payer, const GameObject& card,
                                bool combat_active, int printed_energy_cost = -1);
static bool anyCombatInProgress(const GameState& state);

// What a play-from-trash grant charges for `card`: the grant replaces the
// card's Base Cost (CR 356.1.a) and then every increase and discount applies
// to it as to any other play (CR 356.3, 356.4) — the same pipeline as a play
// for the printed cost (netCardCost). The Power is paid in the grant's Domain.
GameEngine::TrashGrantCost GameEngine::trashGrantCost(
    PlayerId player, GameObjectId card_id, const PlayerState::TrashReplayGrant& grant,
    const std::vector<GameObjectId>& targets, bool prefer_power) const {
    TrashGrantCost out;
    // A discount applies to whatever cost is being paid (CR 356.4): the
    // grant's, as it would the card's own — what the chosen targets take off
    // included.
    const auto adj = targetCostDiscount(player, targets, prefer_power, nullptr);
    const auto net = netCardCost(player, card_id, adj, grant.energy, grant.power);
    out.energy = net.first;
    out.power = net.second;
    out.domains = resource::maskOf(grant.any_domain ? Domain::Count : grant.power_domain);
    out.extra_any = extraAnyPowerCost(state_, player, state_.getObject(card_id));
    return out;
}

const PlayerState::TrashReplayGrant* GameEngine::trashGrantFor(PlayerId player,
                                                               GameObjectId card_id) const {
    for (const auto& g : state_.player(player).trash_replay_grants)
        if (g.card == card_id) return &g;
    return nullptr;
}

bool GameEngine::anyDeflectFor(PlayerId player) const {
    for (const auto& [id, obj] : state_.objects.onBoard())
        if (obj.isUnit() && deflectCostFor(state_, player, id) > 0) return true;
    return false;
}

bool GameEngine::payableThroughAFinalizeChoice(
    PlayerId player, GameObjectId card_id, const PlayerState::TrashReplayGrant* grant,
    bool whole_choice) const {
    const auto& card = state_.getObject(card_id);
    const Card* cc = card.card_def_id != kInvalidId ? card_registry_.get(card.card_def_id)
                                                    : nullptr;
    if (!cc || cc->needsPlayTimeTarget() ||
        !(cc->choicesAtFinalization() & Card::kFinalizeResolve)) return false;
    // A pair spell checked whole (generateSpellActions): every complete
    // sequence of its choices is looked for, whatever the declarations say.
    if (whole_choice)
        return const_cast<GameEngine*>(this)->prePlayProbe(player, card_id, grant);
    // Only an object that takes something off the cost can change the answer.
    bool declared = false;
    for (const auto& [id, obj] : state_.objects.onBoard()) {
        if (!obj.location.has_value()) continue;
        if (targetCostDiscount(player, {id}, false, nullptr).energy_reduction <= 0 &&
            targetCostDiscount(player, {id}, true, nullptr).power_reduction <= 0) continue;
        if (cc->mayChooseAtFinalization(state_, player, id) != Card::FinalizeChoice::Yes) continue;
        if (!(grant ? canPayTrashGrant(player, card_id, *grant, {id})
                    : canAffordChoosing(player, card_id, {id}))) continue;
        declared = true;
        break;
    }
    if (!declared) return false;
    // An object that could pay for it, by the card's own declaration.
    // Whether the card's choices can actually be made that way — a mode
    // or a battlefield chosen first may rule the object out — is found by
    // making them: the play is offered only if a complete payable
    // sequence exists. (Nothing of the probe is kept or seen; `this` is
    // const to its callers in every way they can observe. Called after the
    // walk above, not from inside it: the probe plays on the state.)
    return const_cast<GameEngine*>(this)->prePlayProbe(player, card_id, grant);
}

bool GameEngine::canPayTrashGrant(PlayerId player, GameObjectId card_id,
                                  const PlayerState::TrashReplayGrant& grant,
                                  const std::vector<GameObjectId>& targets) const {
    return canPayTrashGrant(player, card_id, grant, targets, targets);
}

bool GameEngine::canPayTrashGrant(PlayerId player, GameObjectId card_id,
                                  const PlayerState::TrashReplayGrant& grant,
                                  const std::vector<GameObjectId>& targets,
                                  const std::vector<GameObjectId>& unpaid_deflect) const {
    const auto& card = state_.getObject(card_id);
    int deflect = 0;
    const Card* cc = card.card_def_id != kInvalidId ? card_registry_.get(card.card_def_id)
                                                    : nullptr;
    if (!cc || !cc->ignoresDeflect())
        for (auto t : unpaid_deflect) deflect += deflectCostFor(state_, player, t);
    const SpendFor f = spendForObject(card);
    bool has_choice = false;
    (void)targetCostDiscount(player, targets, false, &has_choice);
    for (int prefer_power = 0; prefer_power < (has_choice ? 2 : 1); ++prefer_power) {
        const auto cost = trashGrantCost(player, card_id, grant, targets, prefer_power == 1);
        if (!resource::canPayIn(state_, player, cost.energy, cost.power, cost.domains, f))
            continue;
        // "[A] more" and Deflect are owed on top, from what the grant's own
        // Power leaves.
        const int extra = cost.extra_any + deflect;
        if (extra <= 0 ||
            resource::payablePower(state_, player, Domain::Count, f) - cost.power >= extra)
            return true;
    }
    return false;
}

void GameEngine::generateTrashReplayActions(PlayerId player, bool action_ok,
                                             bool reaction_ok,
                                             std::vector<Intent>& actions) const {
    auto& ps = state_.player(player);
    if (ps.trash_replay_grants.empty() && !ps.may_play_from_trash) return;
    if (ps.cant_play_cards_this_turn) return;

    // Every card this player may play from their trash: one per grant, and —
    // under "You may play cards from your trash" — the rest of the trash at
    // their normal cost (grant == nullptr).
    std::vector<std::pair<GameObjectId, const PlayerState::TrashReplayGrant*>> playable;
    for (const auto& grant : ps.trash_replay_grants) {
        if (grant.card == kInvalidId || !state_.objectExists(grant.card)) continue;
        bool seen = false;
        for (const auto& e : playable) if (e.first == grant.card) seen = true;
        if (!seen) playable.emplace_back(grant.card, &grant);
    }
    if (ps.may_play_from_trash)
        for (auto cid : ps.trash) {
            bool seen = false;
            for (const auto& e : playable) if (e.first == cid) seen = true;
            if (!seen && state_.objectExists(cid)) playable.emplace_back(cid, nullptr);
        }

    for (const auto& [card_id, grant] : playable) {
        auto& card = state_.getObject(card_id);
        if (card.zone != ZoneType::Trash) continue;   // only while it is in the trash
        if (card.card_def_id == kInvalidId) continue;
        if (card.isSpell() && ps.cant_play_spells_this_turn) continue;
        if (!card.isSpell() && !card.isUnit() && !card.isGear()) continue;

        // The legality gates every play passes (canAfford's first half).
        if (!passesPlayGates(player, card_id)) continue;

        // Cost of this play with `targets` chosen: the grant's cost, or the
        // card's own (canAffordChoosing adds what the targets change).
        auto affordable = [&](const std::vector<GameObjectId>& targets) {
            if (!grant) return canAffordChoosing(player, card_id, targets);
            return canPayTrashGrant(player, card_id, *grant, targets);
        };
        auto make = [&](IntentType type, std::vector<GameObjectId> tgts,
                        std::optional<LocationId> where) {
            Intent play;
            play.type = type;
            play.player = player;
            play.card = card_id;
            play.play_source = Intent::PlaySource::Trash;
            play.targets = std::move(tgts);
            play.play_location = where;
            actions.push_back(play);
        };

        if (!card.isSpell()) {
            // A permanent from the trash is played when and where a permanent
            // from hand is: in a Neutral Open state (CR 355.2), a gear to
            // base, a unit to base or a battlefield its controller controls.
            if (!state_.turn.isNeutralOpen()) continue;
            if (!affordable({})) continue;
            const Card* cc = card_registry_.get(card.card_def_id);
            if (card.isUnit() && cc && cc->restrictsPlayLocations()) {
                for (const auto& loc : cc->getPlayLocations(state_, player))
                    make(IntentType::PlayCard, {}, loc);
                continue;
            }
            make(IntentType::PlayCard, {}, LocationId{BaseLocation{player}});
            if (!card.isUnit() || ps.units_play_base_only) continue;
            for (const auto& bf : state_.battlefields) {
                if (bf.blocks_unit_play) continue;
                if (bf.controller.has_value() && *bf.controller == player)
                    make(IntentType::PlayCard, {}, LocationId{BattlefieldLocation{bf.id}});
            }
            continue;
        }

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

        Card* spell_card = card_registry_.get(card.card_def_id);
        if (spell_card && !spell_card->hasLegalTargets(state_, player, card_db_)) continue;
        auto legal_targets = spell_card
            ? spell_card->enumerateLegalTargets(state_, player)
            : std::vector<GameObjectId>{};
        auto req = spell_card ? spell_card->getTargetRequirements()
                              : TargetRequirements{};

        IntentType intent_type = IntentType::PlayCard;
        if (state_.turn.isShowdownOpen()) intent_type = IntentType::PlayActionCard;
        else if (state_.turn.isClosedState()) intent_type = IntentType::PlayReaction;

        if (spell_card && req.count > 0 && spell_card->needsPlayTimeTarget()) {
            // The target is chosen as the spell is played: it must be payable
            // with at least one of them.
            bool any = spell_card->ignoresDeflect() && affordable({});
            for (auto t : legal_targets) if (!any && affordable({t})) any = true;
            if (any) make(intent_type, {}, std::nullopt);
        } else if (spell_card && req.count > 0 && spell_card->needsPlayTimeTargetPair()) {
            // The pair is picked by the card as it is played: payable on its
            // own, or with one of the objects it could choose.
            const bool plain = affordable({});
            bool any = plain;
            for (auto t : legal_targets) if (!any && affordable({t})) any = true;
            // As from hand: a complete payable pair, when a discount or a
            // Deflect could decide it (testing phase D8 / G20).
            if (any && (spell_card->choicesAtFinalization() & Card::kFinalizeResolve) &&
                (!plain || anyDeflectFor(player)))
                any = payableThroughAFinalizeChoice(player, card_id, grant, true);
            if (any) make(intent_type, {}, std::nullopt);
        } else if (req.count == 0 || (req.optional && legal_targets.empty())) {
            if (affordable({}) || payableThroughAFinalizeChoice(player, card_id, grant))
                make(intent_type, {}, std::nullopt);
        } else {
            for (auto& tset : spellTargetSets(spell_card, state_, player, req, legal_targets))
                if (affordable(tset)) make(intent_type, std::move(tset), std::nullopt);
        }
    }
}

void GameEngine::generateActivateAbilityActions(PlayerId player,
                                                  std::vector<Intent>& actions,
                                                  AbilityTiming timing,
                                                  IntentType intent_type) const {
    // [Reaction] grants every permission of [Action] (CR 813.1.b).
    auto timing_ok = [timing](const ActivatedAbility& ab) {
        switch (timing) {
            case AbilityTiming::Any:      return true;
            case AbilityTiming::Action:   return ab.is_action || ab.is_reaction;
            case AbilityTiming::Reaction: return ab.is_reaction;
        }
        return false;
    };
    // Phase 6r — multi-ability per Card. Loop over each ability descriptor
    // on the card; each gets its own affordability check and target loop.
    // Single-ability cards use the default Card::activatedAbilities() impl
    // which wraps the legacy single-ability virtuals into a one-element
    // vector — semantics unchanged for them.
    for (auto& [id, obj] : state_.objects.inPlay()) {
        if (obj.controller != player) continue;
        if (!obj.location.has_value() && obj.zone != ZoneType::LegendZone) continue;

        Card* card = card_registry_.get(obj.card_def_id);
        if (!card) continue;
        // Per-card "Use only if …" gate (Emperor of the Sands etc.).
        if (!card->canActivateAbility(state_, player)) continue;
        const auto& abilities = card->abilities();
        // Don't skip when the object carries no OWN abilities but has been
        // granted some by an aura (Forge/Gardens/Heimerdinger).
        if (abilities.empty() && obj.granted_abilities.empty()) continue;

        for (size_t ai = 0; ai < abilities.size(); ++ai) {
            const auto& ab = abilities[ai];
            const auto& act_cost = ab.cost;
            if (!timing_ok(ab)) continue;

            // Per-ability gate ([Empower]: only if not Empowered; "Disempower
            // this" costs need an Empowered status).
            if (!card->canActivateAbilityAt(state_, player, id, (int)ai)) continue;

            // Check activation cost: must be ready if exhaust required
            if (act_cost.exhaust && obj.is_exhausted) continue;
            if (!canPayActivation(player, *card, id, (int)ai, act_cost)) continue;
            if (act_cost.discard && act_cost.discard_count > 0) {
                int matching = 0;
                for (auto hid : state_.player(player).hand) {
                    if (act_cost.discard_type_filter == 0 ||
                        static_cast<int>(state_.getObject(hid).card_type) + 1 ==
                            act_cost.discard_type_filter) ++matching;
                }
                if (matching < act_cost.discard_count) continue;
            }
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
                                                          static_cast<int>(ai), id);
                if (legal.empty()) continue;
            } else if (abilities.size() == 1 &&
                        !card->hasLegalTargets(state_, player, card_db_)) {
                continue;
            }

            // Phase 6q+ — if this ability defers target selection to
            // resolve-time via pickTarget (action-vocab collision fix),
            // emit ONE intent with empty targets regardless of count.
            if (ab.needs_activation_time_target) {
                Intent activate;
                activate.type = intent_type;
                activate.player = player;
                activate.ability_source = id;
                activate.ability_index = static_cast<int>(ai);
                actions.push_back(activate);
                continue;
            }

            if (ab.targets.count == 0) {
                Intent activate;
                activate.type = intent_type;
                activate.player = player;
                activate.ability_source = id;
                activate.ability_index = static_cast<int>(ai);
                actions.push_back(activate);
            } else {
                auto legal_targets = card->enumerateLegalTargets(
                    state_, player, static_cast<int>(ai), id);
                for (auto target : legal_targets) {
                    Intent activate;
                    activate.type = intent_type;
                    activate.player = player;
                    activate.ability_source = id;
                    activate.ability_index = static_cast<int>(ai);
                    activate.targets = {target};
                    actions.push_back(activate);
                }
            }
        }

        // Aura-granted activated abilities (Forge/Gardens/Heimerdinger): each ref
        // names a granting card + ability index the bearer (this object) may now
        // use. Cost is read from the granter's descriptor and paid by the bearer.
        for (const auto& ref : obj.granted_abilities) {
            Card* gcard = card_registry_.get(ref.source_def_id);
            if (!gcard) continue;
            const auto& gabs = gcard->abilities();
            if (ref.ability_index < 0 ||
                ref.ability_index >= static_cast<int>(gabs.size())) continue;
            const auto& gab = gabs[ref.ability_index];
            if (!timing_ok(gab)) continue;
            const auto& gcost = gab.cost;
            if (ref.exhaust_cost_only && !gcost.exhaust) continue;     // not an [E] ability
            // The same gates as an own ability above, asked of the granting
            // card for the bearer: "Use only if ...", the per-ability gate,
            // the state-aware Energy reduction payAndQueueAbility applies, the
            // discard cost and the ability's targets.
            if (!gcard->canActivateAbility(state_, player)) continue;
            if (!gcard->canActivateAbilityAt(state_, player, id, ref.ability_index)) continue;
            if (gcost.exhaust && obj.is_exhausted) continue;           // bearer must be ready
            if (!canPayActivation(player, *gcard, id, ref.ability_index, gcost)) continue;
            if (gcost.discard && gcost.discard_count > 0) {
                int matching = 0;
                for (auto hid : state_.player(player).hand) {
                    if (gcost.discard_type_filter == 0 ||
                        static_cast<int>(state_.getObject(hid).card_type) + 1 ==
                            gcost.discard_type_filter) ++matching;
                }
                if (matching < gcost.discard_count) continue;
            }
            if (gcost.xp_cost > 0 && state_.player(player).xp < gcost.xp_cost) continue;
            if (gab.targets.count > 0 && !gab.targets.optional) {
                if (gcard->enumerateLegalTargets(state_, player, ref.ability_index, id).empty())
                    continue;
            } else if (gabs.size() == 1 && !gcard->hasLegalTargets(state_, player, card_db_)) {
                continue;
            }
            Intent activate;
            activate.type = intent_type;
            activate.player = player;
            activate.ability_source = id;
            activate.ability_index = ref.ability_index;
            activate.granted_ability_def = ref.source_def_id;
            actions.push_back(activate);
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Combat and Showdown
// ═══════════════════════════════════════════════════════════════════════════════

void GameEngine::runShowdown(BattlefieldId bf_id) {
    auto& bf = getBattlefield(bf_id);
    RIFT_TRACE(events_, "SHOWDOWN: BF#" + toString(bf_id) +
                     (bf.combat_staged ? " (combat)" : " (non-combat)"));

    bf.showdown_in_progress = true;
    state_.turn.ns_state = NeutralShowdownState::Showdown;
    state_.turn.oc_state = OpenClosedState::Open;

    events_.emit(ShowdownStartedEvent{bf_id, bf.combat_staged});
    // CR 335.1: the player with Focus receives priority only with no pending
    // Chain Items — "When a showdown begins here" abilities are finalized and
    // resolve before the first Focus decision.
    runChain();
    if (state_.game_over) return;

    // Showdown loop: players alternate focus, playing Action/Reaction spells.
    // Showdown closes when all players pass focus consecutively (CR 348.1).
    runShowdownLoop(bf_id);

    // `bf` is not used past this point: what resolved during the showdown may
    // have added a battlefield and reallocated the vector. Fetch it again.
    auto& bf_after = getBattlefield(bf_id);
    bf_after.showdown_in_progress = false;
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
        auto old_controller = bf_after.controller;
        bf_after.controller = sole_player;
        bf_after.is_contested = false;

        events_.emit(ControlChangedEvent{bf_id, old_controller, sole_player});

        // CR 468.5 / 471.1: a player Establishes Control (and so Conquers) only
        // "if they didn't already control this Battlefield".
        if (!old_controller.has_value() || *old_controller != sole_player)
            scoreConquer(sole_player, bf_id, old_controller);
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
    if (state_.game_over) {
        adv.kind = ShowdownAdvance::Kind::Done;
        return adv;
    }
    // Engine bound, as for the Main Phase: a showdown that takes this many
    // actions is one in which something repeats for free. (It used to close
    // the showdown quietly.)
    if (action_count >= kMaxShowdownActions)
        throw std::logic_error("GameEngine::advanceShowdown: " +
                               std::to_string(kMaxShowdownActions) +
                               " actions in one showdown at battlefield " + toString(bf_id));
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
            const auto passed = state_.turn.players_passed_focus;
            state_.turn.players_passed_focus.clear();  // reset passes
            last_activation_added_ = false;
            executeIntent(chosen);
            // HOT FEPR: the Cleanup and everything it puts on the chain (a
            // death, a delayed ability) before Focus passes (CR 335.1).
            runChain();
            // CR 430.2.a: "Priority and Focus will not pass from Add
            // abilities": an [Add] resolved at once keeps Focus with its
            // player, and changes nothing about who has passed (D31).
            if (last_activation_added_) {
                state_.turn.players_passed_focus = passed;
                return current_focus;
            }
            // After chain resolves, focus passes to opponent
            return opponent(current_focus);
        }
        default:
            if (state_.game_over) return std::nullopt;   // a concession
            // An option offered at Focus that nothing here executes: an
            // engine error, not a decision asked again until a bound.
            throw std::logic_error(std::string("GameEngine::resolveShowdownDecision: ") +
                                   toString(chosen.type) +
                                   " was chosen at Focus and cannot be executed there");
    }
}

void GameEngine::runShowdownLoop(BattlefieldId bf_id) {
    // Focus passing loop (CR 313, CR 348.1).
    // The player who applied Contested gets focus first. Players alternate.
    // When both pass focus consecutively without adding chain items, showdown closes.
    //
    // Bridge: legacy path calls queryAgent between advance + resolve.
    // The step-machine path (C-1 commit 9) will surface advance's legal
    // actions out to applyChoice() and feed the chosen intent back into
    // resolve — no thread, no condvar.
    state_.turn.players_passed_focus.clear();
    // Who gains Focus as the showdown begins (finding G3: it was always the
    // turn player). In a combat, the Attacker (CR 466.2.c.1.a: "If a
    // showdown opens as part of combat, this player gains Focus"); in a
    // non-combat showdown, the player who applied Contested (CR 345) — the
    // controller of the unit that applied it (CR 190.3.a.1), also when
    // another player's effect moved that unit there (judge question J2).
    PlayerId current_focus = state_.turn.turn_player;
    {
        const auto& bf = getBattlefield(bf_id);
        if (bf.combat_in_progress && bf.attacker.has_value())
            current_focus = *bf.attacker;
        else if (bf.contested_by != PlayerId::None)
            current_focus = bf.contested_by;
    }

    int action_count = 0;
    while (!state_.game_over) {
        auto adv = advanceShowdown(bf_id, current_focus, action_count);
        if (adv.kind == ShowdownAdvance::Kind::Done) return;

        state_.decision_index++;
        auto chosen = askAgent(current_focus, adv.legal, PendingQuery::Showdown);
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
    // CR 308.1: "If a Showdown or Combat is in progress, the turn is in a
    // Showdown State." Only [Action] / [Reaction] cards and abilities are
    // played until the combat ends (308.1.a) — at Focus as in any showdown,
    // by either player (finding D5: the combat used to stay a Neutral State,
    // and every spell was offered at Focus). combatResolutionStep puts the
    // turn back in a Neutral State.
    state_.turn.ns_state = NeutralShowdownState::Showdown;

    // Determine attacker/defender (CR 459.2.b)
    bf.attacker = bf.contested_by;
    bf.defender = opponent(bf.contested_by);
    RIFT_TRACE(events_, "COMBAT: BF#" + toString(bf_id) + " attacker=" +
                     toString(*bf.attacker) + " defender=" + toString(*bf.defender));

    // Assign combat designations to units
    for (auto& [id, obj] : state_.objects.atLocation(LocationId{BattlefieldLocation{bf_id}})) {
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
    // (Every object: a designation can still be on a unit that has left the
    // board, see processContestedBattlefields. Once per combat; under 0.1% of
    // a random game.)
    for (auto& [id, obj] : state_.objects) {
        if (obj.isUnit() && obj.combat_designation != CombatDesignation::None) {
            obj.recomputeMight();
        }
    }

    for (PlayerId p : {*bf.attacker, *bf.defender}) {
        auto& cps = state_.player(p);
        cps.last_combat_bf = bf_id;
        cps.last_combat_attacker = (p == *bf.attacker);
        cps.excess_combat_damage = 0;
    }
    {
        // CR 466.2.b: "Start of combat effects happen now. If the Combat
        // opened with a Combat Showdown, start of showdown effects happen
        // simultaneously." One batch; the attacker's abilities go on the
        // chain first (CR 466.2.d.1).
        const SimultaneousTriggers together(state_.chain, *bf.attacker);
        const PlayerId att = *bf.attacker, def = *bf.defender;
        events_.emit(CombatStartedEvent{bf_id, att, def});
        events_.emit(ShowdownStartedEvent{bf_id, /*is_combat=*/true});
    }

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
    // `bf` is not used past this point: chain items resolve from here on, and
    // one that adds a battlefield reallocates the vector. The sides are fixed
    // for the combat; the battlefield is fetched again by id where needed.
    const PlayerId attacker = *bf.attacker;
    const PlayerId defender = *bf.defender;
    runChain();
    if (state_.game_over) return;

    // Re-check combat viability AFTER triggers resolve — kills or
    // bounces in trigger resolution may have emptied one side.
    {
        auto att_after_trig = state_.unitsAt(BattlefieldLocation{bf_id}, attacker);
        auto def_after_trig = state_.unitsAt(BattlefieldLocation{bf_id}, defender);
        if (att_after_trig.empty() || def_after_trig.empty()) {
            getBattlefield(bf_id).combat_phase = CombatPhase::ResolutionStep;
            combatResolutionStep(bf_id);
            return;
        }
    }

    // Step 1: Combat Showdown — players can play Action/Reaction spells
    getBattlefield(bf_id).combat_phase = CombatPhase::ShowdownStep;
    runShowdownLoop(bf_id);

    // After showdown: check if combat can continue (CR 460.1)
    // Damage step only occurs if both sides still have units
    auto att_check = state_.unitsAt(BattlefieldLocation{bf_id}, attacker);
    auto def_check = state_.unitsAt(BattlefieldLocation{bf_id}, defender);

    if (att_check.empty() || def_check.empty()) {
        // One side was eliminated during showdown — skip damage, go to resolution
        getBattlefield(bf_id).combat_phase = CombatPhase::ResolutionStep;
        combatResolutionStep(bf_id);
        return;
    }

    // Step 2: Combat Damage
    getBattlefield(bf_id).combat_phase = CombatPhase::DamageStep;
    combatDamageStep(bf_id);

    // Step 3: Resolution
    getBattlefield(bf_id).combat_phase = CombatPhase::ResolutionStep;
    combatResolutionStep(bf_id);
}

void GameEngine::combatDamageStep(BattlefieldId bf_id) {
    // The sides by value: no BattlefieldState reference is held across the
    // damage assignment below.
    const PlayerId attacker = *getBattlefield(bf_id).attacker;
    const PlayerId defender = *getBattlefield(bf_id).defender;

    auto att_units = state_.unitsAt(BattlefieldLocation{bf_id}, attacker);
    auto def_units = state_.unitsAt(BattlefieldLocation{bf_id}, defender);

    if (att_units.empty() || def_units.empty()) return;

    if (!events_.logging()) {
        events_.noteUnheardLine();   // the DAMAGE_STEP line below, unread
    } else {
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
        RIFT_TRACE(events_, "DAMAGE_STEP: BF#" + toString(bf_id) +
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
    auto assignOneSide = [&](PlayerId assigner, int total_damage,
                             const std::vector<GameObjectId>& targets) -> std::optional<Intent> {
        return assignCombatDamage(assigner, total_damage, targets);
    };

    // CR 467.2.c: "Starting with the Attacker, each player assigns ...";
    // 467.2.c.1: "Assigning Damage is not Dealing Damage"; 467.2.c.1.a: "When
    // all Damage is assigned, it will be Dealt simultaneously." So the
    // defender assigns with no combat damage dealt yet, and what that damage
    // triggers is one batch, ordered by its controllers (CR 383.3.d) — not
    // the attacker's damage dealt, and its abilities pending, before the
    // defender is asked (finding D13).
    const auto att_assigned = assignOneSide(attacker, att_might, def_units);
    if (state_.game_over) return;   // a concession
    const auto def_assigned = assignOneSide(defender, def_might, att_units);
    if (state_.game_over) return;
    const SimultaneousTriggers together(state_.chain);
    if (att_assigned) resolveCombatDamageDecision(*att_assigned);
    if (def_assigned) resolveCombatDamageDecision(*def_assigned);
}

std::optional<Intent> GameEngine::assignCombatDamage(PlayerId assigner, int total_damage,
                                                    const std::vector<GameObjectId>& targets) {
    auto adv = advanceCombatDamage(assigner, total_damage, targets);
    if (adv.kind == CombatDamageAdvance::Kind::Skip) return std::nullopt;
    if (adv.kind == CombatDamageAdvance::Kind::NeedDecision) {
        state_.decision_index++;
        auto chosen = askAgent(assigner, adv.legal);
        if (on_decision) on_decision(state_, adv.legal, chosen);
        return chosen;
    }
    // Step by step (testing phase B7): more distinct assignments than one
    // decision offers. The assigner names, in the tier order, the unit that
    // takes lethal damage next (CR 460.2.c.3, 460.2.c.6); the last one named
    // takes what is left (460.2.c.4). Every legal assignment is reachable.
    const bool any_damage_lethal = anyDamageIsLethalFor(assigner);
    std::vector<GameObjectId> flat;
    std::vector<int> tier_of;
    combatDamageTiers(assigner, targets, flat, tier_of);
    std::vector<bool> taken(flat.size(), false);
    std::vector<DamageAssignment> cur;
    int remaining = total_damage;
    while (remaining > 0) {
        int tier = -1;
        for (size_t k = 0; k < flat.size(); ++k)
            if (!taken[k]) { tier = tier_of[k]; break; }
        if (tier < 0) break;
        std::vector<size_t> open;
        for (size_t k = 0; k < flat.size(); ++k)
            if (!taken[k] && tier_of[k] == tier) open.push_back(k);
        size_t pick = open.front();
        if (open.size() > 1) {
            std::vector<Intent> options;
            for (auto k : open) {
                Intent o;
                o.type = IntentType::MakeChoice;
                o.player = assigner;
                o.chosen_objects = {flat[k]};
                o.choice_label = "Combat damage: lethal to " + state_.getObject(flat[k]).name +
                                 " next";
                options.push_back(std::move(o));
            }
            state_.decision_index++;
            RIFT_TRACE_DECISION(events_, "DECISION #" + std::to_string(state_.decision_index) +
                             " (" + toString(assigner) + "): combat damage, which unit next [" +
                             std::to_string(options.size()) + " options]");
            const Intent chosen = askAgent(assigner, options);
            if (on_decision) on_decision(state_, options, chosen);
            if (state_.game_over) return std::nullopt;   // a concession
            for (auto k : open)
                if (!chosen.chosen_objects.empty() && chosen.chosen_objects.front() == flat[k])
                    pick = k;
        }
        const int lethal =
            combatDamageNeededForLethal(state_.getObject(flat[pick]), any_damage_lethal);
        const int assigned = std::min(remaining, lethal);
        cur.push_back({flat[pick], assigned});
        taken[pick] = true;
        remaining -= assigned;
    }
    if (remaining > 0 && !cur.empty()) cur.back().damage += remaining;
    return Intent::assignCombatDamage(assigner, std::move(cur));
}

void GameEngine::combatDamageTiers(PlayerId assigner, const std::vector<GameObjectId>& targets,
                                   std::vector<GameObjectId>& flat,
                                   std::vector<int>& tier_of) const {
    // Dune Surfer: "You ignore [Tank] while assigning combat damage here."
    bool ignore_tank = false;
    if (!targets.empty()) {
        if (auto tbf = state_.getObject(targets.front()).battlefieldId())
            for (const auto& b : state_.battlefields)
                if (b.id == *tbf) ignore_tank = b.ignore_tank[playerIndex(assigner)];
    }
    std::vector<GameObjectId> tiers[3];
    for (auto tid : targets) {
        const auto& obj = state_.getObject(tid);
        if (obj.hasKeyword(Keyword::Tank) && !ignore_tank) tiers[0].push_back(tid);
        else if (obj.hasKeyword(Keyword::Backline)) tiers[2].push_back(tid);
        else                                        tiers[1].push_back(tid);
    }
    flat.clear();
    tier_of.clear();
    for (int bi = 0; bi < 3; ++bi) {
        std::sort(tiers[bi].begin(), tiers[bi].end());
        for (auto id : tiers[bi]) {
            if (!canTakeCombatDamage(state_.getObject(id))) continue;   // CR 467.2.c.10
            flat.push_back(id);
            tier_of.push_back(bi);
        }
    }
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
    const bool any_damage_lethal = anyDamageIsLethalFor(assigner);
    auto buildDefault = [&]() -> std::vector<DamageAssignment> {
        return lethalCascade(state_, targets, total_damage, any_damage_lethal);
    };

    // CR 460.2.c.3 (strict lethality) + 460.2.c.4 (no over-assign) + CR 815
    // (Tank first) + CR 826 (Backline last): the only CR-legal damage
    // allocations are greedy-lethal cascades, varying only by the WITHIN-
    // bucket order of targets (CR 460.2.c.6 — any order within tied
    // priority). We partition into Tank / non-Tank-non-Backline / Backline
    // buckets, enumerate permutations within each, concatenate, and run
    // greedy-lethal on every ordering. Identical resulting allocations
    // are deduped.

    // The key is the sorted (target, damage) list itself (it used to be that
    // list printed into a string — plans/06 item 6.11).
    using DamageKey = std::vector<std::pair<GameObjectId, int>>;
    auto canonicalKey = [](const std::vector<DamageAssignment>& da) {
        // Order-independent key: sort by (target, damage) so two cascades
        // that produce the same FINAL marked-damage pattern dedupe to one.
        DamageKey kv;
        for (auto& a : da) if (a.damage > 0) kv.emplace_back(a.target_unit, a.damage);
        std::sort(kv.begin(), kv.end());
        return kv;
    };

    std::vector<Intent>& options = adv.legal;
    std::set<DamageKey> seen;

    auto emitIfNew = [&](std::vector<DamageAssignment>&& da) {
        if (seen.insert(canonicalKey(da)).second) {
            options.push_back(Intent::assignCombatDamage(assigner, std::move(da)));
        }
    };

    // Enumerate the distinct outcomes directly instead of permuting every
    // unit: walk the buckets in order, and at each step choose which remaining
    // unit of the current bucket takes lethal next, until the damage runs out.
    // Two orders that have killed the same set of units lead to the same
    // continuations, so each set is expanded once. (Permuting whole buckets is
    // factorial: 16 token defenders never finished.)
    std::vector<GameObjectId> flat;
    std::vector<int> bucket_of;
    combatDamageTiers(assigner, targets, flat, bucket_of);

    // Bounds: one decision offers at most kMaxDamageOptions assignments, and
    // the search must stay cheap on huge boards. Past either bound the
    // assignment is made step by step instead (assignCombatDamage), so no
    // legal assignment is withheld (testing phase B7).
    constexpr size_t kMaxDamageOptions = 64;
    constexpr int kMaxSearchNodes = 20000;
    emitIfNew(buildDefault());
    bool too_many = flat.size() > 60;

    if (!too_many) {
        std::set<uint64_t> expanded;
        int nodes = 0;
        std::vector<DamageAssignment> cur;
        // (A lambda that is handed itself: no std::function per combat.)
        auto walk = [&](auto&& self, uint64_t taken, int remaining) -> void {
            if (options.size() > kMaxDamageOptions || ++nodes > kMaxSearchNodes) {
                too_many = true;
                return;
            }
            // Current bucket = the first one that still has an untaken unit.
            int bucket = -1;
            for (size_t k = 0; k < flat.size(); ++k)
                if (!(taken >> k & 1)) { bucket = bucket_of[k]; break; }
            if (remaining <= 0 || bucket < 0) {
                auto da = cur;
                if (remaining > 0 && !da.empty()) da.back().damage += remaining;
                if (!da.empty()) emitIfNew(std::move(da));
                return;
            }
            for (size_t k = 0; k < flat.size(); ++k) {
                if ((taken >> k & 1) || bucket_of[k] != bucket) continue;
                const auto& t = state_.getObject(flat[k]);
                const int assigned =
                    std::min(remaining, combatDamageNeededForLethal(t, any_damage_lethal));
                const uint64_t next = taken | (uint64_t{1} << k);
                const int left = remaining - assigned;
                // Same set fully served -> same continuations: expand once.
                if (left > 0 && !expanded.insert(next).second) continue;
                cur.push_back({flat[k], assigned});
                self(self, next, left);
                cur.pop_back();
                if (options.size() > kMaxDamageOptions || nodes > kMaxSearchNodes) {
                    too_many = true;
                    return;
                }
            }
        };
        walk(walk, 0, total_damage);
    }
    if (too_many || options.size() > kMaxDamageOptions) {
        options.clear();
        adv.kind = CombatDamageAdvance::Kind::StepByStep;
        return adv;
    }

    adv.kind = CombatDamageAdvance::Kind::NeedDecision;
    return adv;
}

void GameEngine::resolveCombatDamageDecision(const Intent& chosen) {
    int total_damage = 0;
    for (auto& da : chosen.damage_assignments) total_damage += da.damage;

    RIFT_TRACE(events_, std::string("ASSIGN_DAMAGE: ") + toString(chosen.player) +
                     " assigns " + std::to_string(total_damage) + " damage:");
    for (auto& da : chosen.damage_assignments) {
        if (da.damage > 0) {
            auto& t = state_.getObject(da.target_unit);
            RIFT_TRACE(events_, "  -> " + t.name + " takes " +
                             std::to_string(da.damage) + " (was " +
                             std::to_string(t.damage_marked) + "/" +
                             std::to_string(t.current_might) + "M)");
            applyCombatDamage(da.target_unit, da.damage);
        }
    }
}

void GameEngine::combatResolutionStep(BattlefieldId bf_id) {
    // The sides by value, and the battlefield fetched again by id wherever it
    // is needed: chain items resolve in this step, and one that adds a
    // battlefield reallocates the vector.
    const PlayerId attacker = *getBattlefield(bf_id).attacker;
    const PlayerId defender = *getBattlefield(bf_id).defender;

    // Combat Cleanup: heal all units, kill lethally damaged (CR 461.1.a)
    processLethalDamage();

    // Heal surviving units (CR 461.1.a.1)
    for (auto& [id, obj] : state_.objects.atLocation(LocationId{BattlefieldLocation{bf_id}})) {
        if (obj.isUnit() && obj.isAtBattlefield()) {
            auto unit_bf = obj.battlefieldId();
            if (unit_bf && *unit_bf == bf_id) {
                obj.damage_marked = 0;
            }
        }
    }

    // Recall attackers if defenders still present (CR 461.1.a.2)
    auto att_remaining = state_.unitsAt(BattlefieldLocation{bf_id}, attacker);
    auto def_remaining = state_.unitsAt(BattlefieldLocation{bf_id}, defender);

    bool recalled = false;
    if (!att_remaining.empty() && !def_remaining.empty()) {
        recalled = true;
        // Both still have units = a tie. Normally only attackers recall
        // (CR 461.1.a.2). Symbol of the Solari (227): if the attacker controls
        // it, recall ALL units (attackers AND defenders) instead.
        const bool recall_all = state_.player(attacker).recall_all_on_attacker_tie;
        // A recall is not a move (CR 458): no UnitMovedEvent, no move count.
        for (auto uid : att_remaining) effect_executor_->recallToBase(uid);
        if (recall_all) {
            for (auto uid : def_remaining) effect_executor_->recallToBase(uid);
            RIFT_TRACE(events_, "COMBAT TIE: the attacker recalls ALL units "
                             "(recall_all_on_attacker_tie)");
        }
    }

    // CR 468.2: "Resolve any items on the chain from dealing combat damage and
    // the Combat Cleanup and associated FEPR before performing this step" —
    // death and recall triggers resolve before the result is determined, and
    // may change who is here (a Deathknell that plays a unit here).
    // (One call: the chain's loop ends only when a Cleanup leaves it empty,
    // lethal damage included.)
    if (!state_.game_over) runChain();

    // Determine combat result (CR 461.3)
    att_remaining = state_.unitsAt(BattlefieldLocation{bf_id}, attacker);
    def_remaining = state_.unitsAt(BattlefieldLocation{bf_id}, defender);

    // CR 468.3.d: "No Result" if units were recalled in the Combat Cleanup (a
    // tie), if both players have units here, or if neither has. Only
    // otherwise has the one player still here won (468.3.a).
    PlayerId sole_remaining = PlayerId::None;
    if (!att_remaining.empty() && def_remaining.empty()) {
        sole_remaining = attacker;
    } else if (att_remaining.empty() && !def_remaining.empty()) {
        sole_remaining = defender;
    }
    const std::optional<PlayerId> combat_winner =
        (recalled || sole_remaining == PlayerId::None)
            ? std::nullopt : std::optional<PlayerId>(sole_remaining);

    // CR 468.3: the result. "When you win a combat" abilities trigger now and
    // resolve before control is established (468.4).
    events_.emit(CombatResultEvent{bf_id, combat_winner});
    if (chain_manager_->chainExists() && !state_.game_over) runChain();
    if (state_.game_over) return;

    // CR 468.5: with no Showdown or Combat staged here, the player with units
    // remaining establishes control. Who is here is read again: the chain
    // above may have changed it.
    att_remaining = state_.unitsAt(BattlefieldLocation{bf_id}, attacker);
    def_remaining = state_.unitsAt(BattlefieldLocation{bf_id}, defender);
    if (!att_remaining.empty() && !def_remaining.empty()) {
        // CR 468.3.d.1: "If No Result was reached, and both players have units
        // remaining, stage a Showdown and a Combat at this battlefield." The
        // battlefield stays Contested by the same player, so the next combat
        // (with its showdown step) is staged for the same attacker; it begins
        // after this one has ended (468.7), in the Cleanup that follows
        // (CR 323.12, advanceMainPhase).
        auto& bf = getBattlefield(bf_id);
        bf.is_contested = true;
        bf.contested_by = attacker;
        bf.combat_staged = true;
        RIFT_TRACE(events_, "COMBAT: no result at BF#" + toString(bf_id) +
                         " with both players present -> another combat is staged");
    } else {
        // The battlefield is no longer Contested (468.5.a): nobody is
        // contesting it and no combat is staged for it.
        auto& bf = getBattlefield(bf_id);
        bf.is_contested = false;
        bf.contested_by = PlayerId::None;
        bf.combat_staged = false;
        sole_remaining = PlayerId::None;
        if (!att_remaining.empty()) sole_remaining = attacker;
        else if (!def_remaining.empty()) sole_remaining = defender;

        if (sole_remaining != PlayerId::None) {
            auto old_controller = bf.controller;
            bf.controller = sole_remaining;
            events_.emit(ControlChangedEvent{bf_id, old_controller, sole_remaining});
            // CR 468.5.c: hidden cards here that do not share the
            // battlefield's controller are removed as control is established.
            removeForeignHiddenCards(bf_id);
            // CR 468.5.d / 471.1: a defender who wins already controlled it —
            // no Conquer.
            if (!old_controller.has_value() || *old_controller != sole_remaining)
                scoreConquer(sole_remaining, bf_id, old_controller);
        } else {
            // CR 468.5.b: nobody's units remain: the battlefield is uncontrolled.
            const auto old_controller = bf.controller;
            bf.controller = std::nullopt;
            if (old_controller.has_value())
                events_.emit(ControlChangedEvent{bf_id, old_controller, std::nullopt});
            removeForeignHiddenCards(bf_id);
        }
        // CR 468.6: "Resolve any items on the chain from establishing control
        // and associated FEPR before performing this step" — the conquer
        // abilities resolve while the combat is still going on: the units
        // still have their designations, "this combat" effects still apply
        // (finding I2), and they read this combat's numbers (excess damage,
        // finding B4).
        if (chain_manager_->chainExists() && !state_.game_over) runChain();
        if (state_.game_over) return;
    }

    // CR 468.7: the combat ends. "Remove Attacker and Defender Designation
    // from all Units and Players" (468.7.a); "At the end of Combat or Combat
    // ends effects take place" (468.7.b: "When a combat that I was in ends");
    // "All 'this combat' effects expire simultaneously" (468.7.c: they last
    // while the combat is in progress — Fiora, Peerless; Fortified Position).
    // (Every object, for the same reason; once per combat, under 0.1%.)
    for (auto& [id, obj] : state_.objects) {
        if (obj.combat_designation != CombatDesignation::None) {
            obj.combat_designation = CombatDesignation::None;
            obj.recomputeMight();
        }
    }
    {
        auto& bf = getBattlefield(bf_id);
        bf.combat_in_progress = false;
        bf.combat_phase = CombatPhase::None;
        bf.attacker = std::nullopt;
        bf.defender = std::nullopt;
    }
    state_.turn.ns_state = NeutralShowdownState::Neutral;
    events_.emit(CombatEndedEvent{bf_id, combat_winner});
    // What the end of the combat triggered resolves before anything else
    // begins: a showdown or combat begins only in a Neutral Open State with
    // nothing pending (CR 323.11 / 323.12).
    if (chain_manager_->chainExists() && !state_.game_over) runChain();
}

// ═══════════════════════════════════════════════════════════════════════════════
// Scoring
// ═══════════════════════════════════════════════════════════════════════════════

// "Players can't score here until their third turn" (Forgotten Monument 523,
// through BattlefieldCard::minTurnToScore). Conquer and Hold are the two ways
// of Scoring (CR 471), so while this returns true there is no Score at all:
// no point, no ScoreEvent, the battlefield is not marked scored and no
// Conquer / Hold ability triggers (CR 473.2.c: "These will only trigger when
// the Battlefield is Scored"). That is deliberately different from
// `cannot_gain_points` ("can't gain points"), where the battlefield IS scored
// and only the point is withheld (CR 470.1). Pinned by
// VerifyCardsTest.Pin_2_20_ForgottenMonument523_NoScoreMeansNoScoreEventAndNoConquerTrigger.
bool GameEngine::isScoreGatedByTurn(PlayerId player, BattlefieldId bf) const {
    const auto& b = getBattlefield(bf);
    if (b.min_turn_to_score <= 0) return false;
    return state_.player(player).turns_taken < b.min_turn_to_score;
}

void GameEngine::scoreConquer(PlayerId player, BattlefieldId bf,
                              std::optional<PlayerId> previous_controller) {
    auto& ps = state_.player(player);

    // Turn-gated scoring (Forgotten Monument): can't score here until the
    // player has taken `min_turn_to_score` of their own turns.
    if (isScoreGatedByTurn(player, bf)) {
        RIFT_TRACE(events_, std::string("SCORE_GATED: ") + toString(player) +
                         " can't score BF#" + toString(bf) + " until turn " +
                         std::to_string(getBattlefield(bf).min_turn_to_score) +
                         " (turns_taken=" + std::to_string(ps.turns_taken) + ")");
        return;
    }

    // Can only score once per BF per turn (CR 465)
    if (ps.battlefields_scored_this_turn.count(bf)) return;

    // Only now has `player` conquered `bf`: a Conquer is one of the two ways
    // of Scoring ("gains Control of a Battlefield they did not yet Score this
    // turn", CR 471.1; "Establishing Control results in a Conquer if that
    // player has not yet scored this Battlefield this turn", CR 468.5.d). The
    // two returns above are control taken WITHOUT a conquer — a battlefield
    // that can't be scored yet, or one already scored this turn — so "a
    // battlefield you conquered this turn" (Perched Grimwyrm, Yone) does not
    // see them. From here on the conquer stands even when its point is
    // withheld or replaced (CR 470.1).
    {
        auto& bfs = getBattlefield(bf);
        bfs.conquered_on_turn = state_.turn.turn_number;
        bfs.conquered_by_player = player;
        bfs.conquered_from = previous_controller;
    }

    ps.battlefields_scored_this_turn.insert(bf);
    RIFT_TRACE(events_, std::string("CONQUER: ") + toString(player) + " conquers BF#" +
                     toString(bf) + " (score was " + std::to_string(ps.score) + ")");

    // Continuous "can't gain points" lock (Tianna Crownguard, set via aura).
    // Scoring is distinct from gaining points (CR 470.1): the battlefield is
    // still scored and its Conquer abilities trigger (473.2); only the point
    // is withheld.
    if (ps.cannot_gain_points) {
        RIFT_TRACE(events_, std::string("SCORE_BLOCKED: ") + toString(player) +
                         " can't gain points (cannot_gain_points)");
        events_.emit(ScoreEvent{player, bf, ScoreMethod::Conquer, ps.score, previous_controller});
        return;
    }

    // Otterpus: "If a player would score 1 point from conquering or holding
    // during their first or second turn, they draw 1 instead." The conquer /
    // hold still happened (its triggers fire via ScoreEvent); only the point
    // is replaced.
    if (state_.turn.early_score_draws_instead && ps.turns_taken <= 2) {
        RIFT_TRACE(events_, std::string("SCORE_REPLACED: ") + toString(player) +
                         " draws 1 instead of scoring (turn " +
                         std::to_string(ps.turns_taken) + ")");
        drawCards(player, 1);
        events_.emit(ScoreEvent{player, bf, ScoreMethod::Conquer, ps.score, previous_controller});
        return;
    }

    if (isWinningPointAttempt(player)) {
        if (canGainWinningPointViaConquer(player)) {
            ps.score++;
            RIFT_TRACE(events_, std::string("SCORE: ") + toString(player) + " +1 via Conquer -> " +
                             std::to_string(ps.score));
            events_.emit(ScoreEvent{player, bf, ScoreMethod::Conquer, ps.score, previous_controller});
        } else {
            RIFT_TRACE(events_, std::string("SCORE_BLOCKED: ") + toString(player) +
                             " winning point blocked (not all BFs scored), draw 1 instead");
            // Draw a card instead (CR 473.1.b.1). The battlefield was still
            // scored, so its Conquer abilities trigger (CR 473.2).
            drawCards(player, 1);
            events_.emit(ScoreEvent{player, bf, ScoreMethod::Conquer, ps.score, previous_controller});
        }
    } else {
        ps.score++;
        RIFT_TRACE(events_, std::string("SCORE: ") + toString(player) + " +1 via Conquer -> " +
                         std::to_string(ps.score));
        events_.emit(ScoreEvent{player, bf, ScoreMethod::Conquer, ps.score, previous_controller});
    }

    checkWinCondition();
}

void GameEngine::scoreHold(PlayerId player, BattlefieldId bf) {
    auto& ps = state_.player(player);

    if (isScoreGatedByTurn(player, bf)) {
        RIFT_TRACE(events_, std::string("SCORE_GATED: ") + toString(player) +
                         " can't score BF#" + toString(bf) + " until turn " +
                         std::to_string(getBattlefield(bf).min_turn_to_score) +
                         " (turns_taken=" + std::to_string(ps.turns_taken) + ")");
        return;
    }

    if (ps.battlefields_scored_this_turn.count(bf)) return;
    ps.battlefields_scored_this_turn.insert(bf);

    // Continuous "can't gain points" lock (Tianna Crownguard, set via aura).
    // The battlefield is still held and scored (CR 470.1, 473.2); only the
    // point is withheld.
    if (ps.cannot_gain_points) {
        RIFT_TRACE(events_, std::string("SCORE_BLOCKED: ") + toString(player) +
                         " can't gain points (cannot_gain_points)");
        events_.emit(ScoreEvent{player, bf, ScoreMethod::Hold, ps.score});
        return;
    }

    // Otterpus: "If a player would score 1 point from conquering or holding
    // during their first or second turn, they draw 1 instead." The conquer /
    // hold still happened (its triggers fire via ScoreEvent); only the point
    // is replaced.
    if (state_.turn.early_score_draws_instead && ps.turns_taken <= 2) {
        RIFT_TRACE(events_, std::string("SCORE_REPLACED: ") + toString(player) +
                         " draws 1 instead of scoring (turn " +
                         std::to_string(ps.turns_taken) + ")");
        drawCards(player, 1);
        events_.emit(ScoreEvent{player, bf, ScoreMethod::Hold, ps.score});
        return;
    }

    // Hold always grants Winning Point (CR 466.1.b.1)
    ps.score++;
    ps.hold_points_this_turn++;
    RIFT_TRACE(events_, std::string("SCORE: ") + toString(player) + " +1 via Hold BF#" +
                     toString(bf) + " -> " + std::to_string(ps.score));
    events_.emit(ScoreEvent{player, bf, ScoreMethod::Hold, ps.score});

    checkWinCondition();
}

bool GameEngine::isWinningPointAttempt(PlayerId player) const {
    // Against the points actually needed to win, which a battlefield may
    // change (the same number checkWinCondition uses).
    return state_.player(player).score >= effect_executor_->effectiveVictoryScore() - 1;
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
    // Step 0: Drop the cost modifiers the previous aura pass produced. A
    // CostModifier pushed from Card::applyPassiveAura is a continuous effect:
    // like aura_effects it is rebuilt from scratch on every pass (Ornn's Forge,
    // Eager Apprentice, Herald of Scales, Vex). Modifiers pushed by resolved
    // effects ("this turn", "your next spell") are not tagged and stay.
    for (auto pid : {PlayerId::Player1, PlayerId::Player2}) {
        auto& mods = state_.player(pid).cost_modifiers;
        mods.erase(std::remove_if(mods.begin(), mods.end(),
                       [](const PlayerState::CostModifier& m) { return m.from_aura; }),
                   mods.end());
    }
    const size_t standing_mods[2] = {
        state_.player(PlayerId::Player1).cost_modifiers.size(),
        state_.player(PlayerId::Player2).cost_modifiers.size()};

    // Step 1: Clear all aura effects from all objects
    // Every object of the game, not the objects in play: a continuous effect
    // ends for a card wherever the card is now (a unit that was bounced still
    // carries the last pass's effects until this clears them), and a hook may
    // have put an effect on a card in a hand or a trash. Which cards those
    // are is not recorded anywhere, so they cannot be picked out. It is the
    // largest walk left in the engine with the cleanup checksum: 4.9% of a
    // random game (the fields it writes are declared together in GameObject
    // for it — plans/09 item 9.KK3).
    // Which objects: every object of the game until a Cleanup has found the
    // piles inert (cleanup(): piles_inert_), then the objects that are not
    // in a pile, plus any object the last pass gave an effect to outside
    // play that has gone to a pile since (a card discarded from a hand).
    const auto clearAuraState = [&](GameObjectId id, GameObject& obj) {
        obj.aura_effects.clear();
        obj.aura_might_bonus = 0;
        obj.aura_keywords.reset();
        obj.aura_bonus_damage_taken = 0;
        obj.immune_to_damage = false;  // re-asserted below by applyPassiveAura
        obj.spells_targeting_me_cost_reduction = 0;  // Irelia, Graceful
        obj.granted_abilities = obj.temp_granted_abilities;  // Forge / Gardens / Heimerdinger re-add theirs; "this turn" grants persist
        obj.prevent_enemy_spell_ability_damage = false;      // Esteemed Hierophant
        obj.cant_be_moved_by_enemy = false;                  // Jagged Cutlass
        obj.aura_cant_move_to_base = false;                  // Minotaur Reckoner
        obj.empower_cost_reduction = 0;              // Risen Altar
        // The two below were passes of their own over every object (the fold
        // in Step 4 cleared the first; the second ran just before the hooks).
        // Nothing between here and there writes either.
        obj.aura_no_combat_damage = false;
        obj.untargetable_by_enemy = obj.untargetable_by_enemy_this_turn;
        // Statuses that do not survive leaving the board (Empowered, CR 442.2;
        // Stunned, CR 424.1) used to be dropped here for every object off the
        // board, at the next cleanup after it left. They are dropped at the
        // move now, with everything else a new object does not carry (CR
        // 124.1): every path off the board ends in EffectExecutor::leftBoard /
        // forgetBoardState. Nothing is left for this pass to mop up; the
        // checked build verifies that instead, in every game the engine
        // plays (the suite's whole games, the smoke and search batches —
        // plans/09 item 9.LL4). A position a test built by hand and handed to
        // a test hook is not checked: several tests put an exhausted rune or
        // an attached Equipment in a trash on purpose.
#ifdef RIFTBOUND_CHECK_BOARD_INDEX
        if (playing_a_game_ &&
            !obj.location.has_value() && obj.zone != ZoneType::LegendZone &&
            obj.zone != ZoneType::BattlefieldZone && carriesBoardState(obj))
            throw std::logic_error("GameEngine::recalculateAuras: " + obj.name + " (id=" +
                                   toString(id) + ", zone " +
                                   std::to_string(static_cast<int>(obj.zone.get())) +
                                   ") is off the board and still carries board state: it "
                                   "left the board by a path that does not call "
                                   "EffectExecutor::leftBoard");
#endif
        (void)id;
    };
    forEachCleanupObject(clearAuraState);
    if (piles_inert_) {
        for (auto id : aura_outside_play_) {
            auto it = state_.objects.find(id);
            if (it != state_.objects.end() && ObjectTable::inPile(it->second))
                clearAuraState(id, it->second);
        }
    }
    aura_outside_play_.clear();
    state_.turn.channel_phase_cap = 0;                  // Sandstone Chimera
    state_.turn.victory_score_bonus =
        effect_executor_->effectiveVictoryScore() - state_.mode.victory_score;
    state_.turn.early_score_draws_instead = false;      // Otterpus

    // Step 1a: Reset per-player passive counters derived from on-board
    // presence. Bumped below by the corresponding aura scan.
    for (auto pid : {PlayerId::Player1, PlayerId::Player2}) {
        auto& ps = state_.player(pid);
        ps.deathknell_double_count = 0;
        ps.bonus_damage_dealt = 0;   // Annie, Fiery
        ps.cannot_gain_points = false; // Tianna Crownguard
        ps.grant_friendly_units_open_bf = false; // Miss Fortune, Buccaneer
        ps.grant_units_lone_enemy_bf = false;    // Arachnoid Horror
        ps.units_play_base_only = false;         // Mageseeker Warden
        ps.tokens_enter_ready = false;           // Renata Glasc, Industrialist
        ps.units_enter_ready_by.clear();         // Magma Wurm
        ps.recall_all_on_attacker_tie = false;   // Symbol of the Solari
        ps.effects_cant_ready_my_units = false;  // Mageseeker Warden cl2
        ps.spells_have_repeat_energy = 0;        // Syndra, Transcendent
        ps.spells_have_repeat_power = 0;
        ps.repeat_cost_reduction = 0;            // Marai Spire
        ps.has_reveal_peek = false;              // Void Hatchling
        ps.additional_token_copy_sources = 0;    // "play that token and an additional copy"
        // Vendetta continuous effects.
        ps.skip_draw_phase = false;              // Endless Riches
        ps.may_play_from_trash = false;
        ps.trash_goes_to_banish = false;
        ps.flow_cost_reduction = 0;              // Stargazer
        ps.spell_tax_energy = 0;                 // Helm of Suppression
        ps.spell_tax_power = 0;
        ps.spell_discount_energy = 0;            // Applied Researchers
        ps.spell_discount_power = 0;
        ps.spells_cant_be_countered = false;     // Mel, Newly Awakened
        ps.extra_minus_might = 0;
        ps.locked_spell_names.clear();           // Fallen Feline
        ps.first_gear_ability_discount = 0;      // Piltovan Forge
    }
    // Per-BF aura-derived flags (Mageseeker Investigator / Noxus Saboteur /
    // Altar of Blood). Reset here; re-asserted by unit/BF applyPassiveAura.
    for (auto& bf : state_.battlefields) {
        bf.multi_move_tax[0] = bf.multi_move_tax[1] = 0;   // Mageseeker Investigator
        bf.opp_hidden_unrevealable[0] = bf.opp_hidden_unrevealable[1] = false;
        bf.ignore_tank[0] = bf.ignore_tank[1] = false;   // Dune Surfer
        bf.ignore_deflect_here = false;                  // Heisho
        bf.reaction_tax_power = 0;                       // Mystic Vortex
        bf.friendly_spell_discount_power = 0;            // Sandswept Tomb
        bf.paid_play_power = 0;                          // Dragon Roost
        bf.paid_play_tag.clear();
        bf.empower_cost_reduction[0] = bf.empower_cost_reduction[1] = 0;  // Risen Altar
    }

    // Step 1a': Equipment-granted keywords and stats. Before the card hooks,
    // so a hook whose effect depends on a keyword ("Units here with [Tank]
    // have +1 [M]") sees the ones an Equipment gives (hasKeywordInAuraPass).
    for (auto& [id, obj] : state_.objects.onBoard()) {
        if (!obj.isUnit() || !obj.location.has_value()) continue;
        for (auto gear_id : obj.attachments) {
            if (!state_.objectExists(gear_id)) continue;
            Card* gear_card = card_registry_.get(state_.getObject(gear_id).card_def_id);
            if (!gear_card) continue;
            // What else the effect text gives the bearer (a tag): before any
            // card hook reads it (Card::applyToBearer).
            gear_card->applyToBearer(state_, gear_id, id);

            // Keywords from effect_text. [Assault N] / [Shield N] are pushed
            // once, with their value, below — not also as a bare keyword,
            // which would count as a second instance of 1.
            int ea = gear_card->equippedAssault();
            int es = gear_card->equippedShield();
            auto ekw = gear_card->equippedKeywords();
            if (ekw.bits != 0) {
                for (int k = 0; k < static_cast<int>(Keyword::Count); ++k) {
                    if (ea > 0 && static_cast<Keyword>(k) == Keyword::Assault) continue;
                    if (es > 0 && static_cast<Keyword>(k) == Keyword::Shield) continue;
                    if (ekw.has(static_cast<Keyword>(k))) {
                        GameObject::AuraEffect ae;
                        ae.source = gear_id;
                        ae.keyword = static_cast<Keyword>(k);
                        obj.aura_effects.push_back(ae);
                    }
                }
            }
            // Assault/Shield/Deflect from effect_text
            if (ea > 0) {
                GameObject::AuraEffect ae;
                ae.source = gear_id;
                ae.keyword = Keyword::Assault;
                ae.keyword_value = ea;
                obj.aura_effects.push_back(ae);
            }
            if (es > 0) {
                GameObject::AuraEffect ae;
                ae.source = gear_id;
                ae.keyword = Keyword::Shield;
                ae.keyword_value = es;
                obj.aura_effects.push_back(ae);
            }
        }
    }

    // Step 1a'': "this combat" keywords resolved abilities gave (GameState::
    // combat_keywords): applied while their combat goes on and the unit is on
    // the board, whatever became of the card that gave them; dropped after.
    {
        auto& cks = state_.combat_keywords;
        cks.erase(std::remove_if(cks.begin(), cks.end(),
            [&](const GameState::CombatKeyword& ck) {
                if (ck.turn != state_.turn.turn_number || !state_.objectExists(ck.unit) ||
                    !state_.getObject(ck.unit).location.has_value())
                    return true;
                for (const auto& bf : state_.battlefields)
                    if (bf.id == ck.battlefield) return !bf.combat_in_progress;
                return true;
            }), cks.end());
        for (const auto& ck : cks) {
            GameObject::AuraEffect ae;
            ae.source = ck.unit;
            ae.keyword = ck.keyword;
            ae.keyword_value = ck.value;
            state_.getObject(ck.unit).aura_effects.push_back(ae);
        }
    }

    // Step 1b: Refresh per-object targeting-protection flags from each
    // card's canBeChosenByEnemy() override. Cards on board need their
    // flag set; off-board cards stay default-false. Cheap O(n) pass over
    // GameState::objects; collocated with aura recalc so all "static
    // attributes of on-board cards" get refreshed in one place.
    // applyPassiveAura is not told which instance it is called for, so a
    // card's hook covers every instance `controller` has in play. It is
    // therefore called ONCE per (card, controller) per pass — calling it per
    // instance applied every copy's effect once per copy (two Garens: +4).
    std::vector<std::pair<CardDefId, PlayerId>> aura_applied;
    bool piles_reached = false;   // a hook below gives cards in the piles an effect
    // Cards whose continuous effect is conditional on Might: they get a second
    // hook after the fold below (Card::applyMightDependentAura).
    std::vector<std::pair<const Card*, PlayerId>> might_dependent;
    // untargetable_by_enemy was reset for every object in Step 1, before any
    // hook: a hook below may set the flag on ANOTHER object ("Your units here
    // ... can't be chosen by enemy spells and abilities"), which a reset
    // inside the hook loop would undo for every object the loop reaches after
    // the hook ran.
    // The walk is over the objects in play (the board and the Legend Zones):
    // nothing else passes the test at the top of the body. A hook is a
    // continuous effect; it does not move objects.
    for (auto& [id, obj] : state_.objects.inPlay()) {
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

        // Card-defined passive auras — each card with an on-board instance
        // (or legend) gets a chance to broadcast its passive effect (Karthus
        // bumps the controller's deathknell_double_count, future cards add
        // their own).
        if (card) {
            const std::pair<CardDefId, PlayerId> key{obj.card_def_id, obj.controller};
            if (std::find(aura_applied.begin(), aura_applied.end(), key) != aura_applied.end())
                continue;
            aura_applied.push_back(key);
            // An effect given to cards in the piles: from here on, while such
            // a card is in play and until a Cleanup then finds the piles
            // inert again, every object is walked (cleanup(), (1)).
            if (card->continuousEffectReachesPiles()) {
                piles_reached = true;
                piles_inert_ = false;
            }
            card->applyPassiveAura(state_, obj.controller);
            if (card->hasMightDependentAura()) might_dependent.emplace_back(card, obj.controller);
        }
    }

    // Step 1b': a unit whose text an Equipment copies (Card::copiesBearerText,
    // CR 479.1.d — the Equipment's effect text speaks for the unit) has its
    // continuous effects once more per such Equipment. The hook is called per
    // (card, controller) and finds its own instances by card_def_id, so for
    // the extra calls every OTHER instance that controller has is masked (it
    // stays on the board as a unit the effect may apply to) and the
    // once-per-pass marker forEachSelfOnce left on this one is dropped.
    auto asTextCopy = [&](GameObjectId unit_id, const Card& card,
                          const std::function<void()>& apply) {
        const PlayerId controller = state_.getObject(unit_id).controller;
        const CardDefId def = state_.getObject(unit_id).card_def_id;
        std::vector<GameObjectId> masked;
        // (Every object: the hook finds its instances by card id in whatever
        // zone it looks. Only for a unit with a text-copying Equipment.)
        for (auto& [oid, other] : state_.objects)
            if (oid != unit_id && other.card_def_id == def && other.controller == controller) {
                other.card_def_id = kInvalidId;
                masked.push_back(oid);
            }
        const int copies = effect_executor_->textCopiesOn(unit_id);
        for (int i = 0; i < copies; ++i) {
            auto& effects = state_.getObject(unit_id).aura_effects;
            effects.erase(std::remove_if(effects.begin(), effects.end(),
                [unit_id](const GameObject::AuraEffect& ae) {
                    return ae.source == unit_id && ae.keyword == Keyword::Count &&
                           ae.keyword_value == kAuraOnceMarker;
                }), effects.end());
            apply();
        }
        for (auto oid : masked) state_.getObject(oid).card_def_id = def;
        (void)card;
    };
    std::vector<GameObjectId> text_copied;
    for (auto& [id, obj] : state_.objects.onBoard())
        if (obj.isUnit() && obj.location.has_value() && obj.card_def_id != kInvalidId &&
            !obj.attachments.empty() && effect_executor_->textCopiesOn(id) > 0)
            text_copied.push_back(id);
    piles_reached_ = piles_reached;
    for (auto id : text_copied) {
        const Card* card = card_registry_.get(state_.getObject(id).card_def_id);
        if (!card) continue;
        const PlayerId controller = state_.getObject(id).controller;
        asTextCopy(id, *card, [&]() {
            card->applyPassiveAura(state_, controller);
            // Its printed [Assault N] / [Shield N] / [Deflect N] are text too:
            // instances add up (CR 807, 809, 814).
            auto& unit = state_.getObject(id);
            const auto& def = card_db_.get(unit.card_def_id);
            auto again = [&](Keyword kw, int value) {
                if (!def.keywords.has(kw) || value <= 0) return;
                GameObject::AuraEffect ae;
                ae.source = id;
                ae.keyword = kw;
                ae.keyword_value = value;
                unit.aura_effects.push_back(ae);
            };
            again(Keyword::Assault, def.assault_value);
            again(Keyword::Shield, def.shield_value);
            again(Keyword::Deflect, def.deflect_value);
        });
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

    // Cost modifiers appended by the hooks above are aura-sourced: tag them so
    // the next pass (Step 0) rebuilds them instead of stacking another copy.
    for (auto pid : {PlayerId::Player1, PlayerId::Player2}) {
        auto& mods = state_.player(pid).cost_modifiers;
        for (size_t i = standing_mods[playerIndex(pid)]; i < mods.size(); ++i)
            mods[i].from_aura = true;
    }

    // (Steps 2, 3 and 3b parsed ability_text for aura wording. Every aura is
    // now the card's own Card::applyPassiveAura, applied in Step 1b / 1c.
    // Step 3c, Equipment keywords, moved up to Step 1a'.)

    // "Buffs give an additional +N [M] to friendly units this turn."
    for (auto& [id, obj] : state_.objects.onBoard()) {
        if (!obj.isUnit() || !obj.location.has_value() || obj.buff_count <= 0) continue;
        const int bonus = state_.player(obj.controller).buff_might_bonus_this_turn;
        if (bonus <= 0) continue;
        GameObject::AuraEffect ae;
        ae.source = id;
        ae.might_bonus = bonus * obj.buff_count;
        obj.aura_effects.push_back(ae);
    }

    // Step 4: Rebuild cached aura values on all objects
    // Whether the last fold met an effect of the "if they didn't already" kind
    // (Step 6 has nothing to do otherwise, and is skipped).
    bool any_conditional = false;
    auto foldAuras = [&]() {
        any_conditional = false;
        aura_outside_play_.clear();
        forEachCleanupObject([&](GameObjectId id, GameObject& obj) {
            // Outside play with something a hook gave it (an Equipment in a
            // hand with a granted keyword): remembered for the next pass.
            if (!obj.location.has_value() && obj.zone != ZoneType::LegendZone &&
                !inertInAPile(obj))
                aura_outside_play_.push_back(id);
            // No effects and not a unit on the board: the four cached values
            // are as Step 1 left them and there is no Might to recompute.
            // (Checked on every object, not only those in play: a hook may
            // put an effect on a card in a hand or a trash. 2.0% of a random
            // game.)
            if (obj.aura_effects.empty() && !(obj.isUnit() && obj.location.has_value()))
                return;
            obj.aura_might_bonus = 0;
            obj.aura_keywords.reset();
            obj.aura_no_combat_damage = false;
            obj.aura_bonus_damage_taken = 0;
            for (auto& ae : obj.aura_effects) {
                if (ae.only_if_absent && ae.keyword != Keyword::Count) any_conditional = true;
                obj.aura_might_bonus += ae.might_bonus;
                if (ae.keyword != Keyword::Count) {
                    obj.aura_keywords.set(ae.keyword);
                }
                // An aura-granted [Assault N] / [Shield N] is +N [M] while its
                // bearer is an attacker / a defender (CR 807 / 814); a bare
                // [Assault] / [Shield] is N = 1. recomputeMight only knows the
                // object's own assault_value / shield_value, so the granted
                // instances are folded in here.
                if ((ae.keyword == Keyword::Assault &&
                     obj.combat_designation == CombatDesignation::Attacker) ||
                    (ae.keyword == Keyword::Shield &&
                     obj.combat_designation == CombatDesignation::Defender)) {
                    obj.aura_might_bonus += std::max(1, ae.keyword_value);
                }
                if (ae.suppress_combat_damage) obj.aura_no_combat_damage = true;
                obj.aura_bonus_damage_taken += ae.bonus_damage_taken;
            }
            // Recompute might with new aura values
            if (obj.isUnit() && obj.location.has_value()) {
                obj.recomputeMight();
            }
        });
    };
    foldAuras();

    // Step 5: effects conditional on Might ("While I'm [Mighty], I have
    // ..."). They are decided on THIS pass's Might, which the fold above just
    // produced from every ordinary aura — not on what the previous pass left
    // in current_might, so the answer does not depend on the order the hooks
    // ran in, and such an effect never counts toward its own condition. What
    // they add is folded in afterwards.
    if (!might_dependent.empty()) {
        for (const auto& [card, controller] : might_dependent)
            card->applyMightDependentAura(state_, controller);
        for (auto id : text_copied) {
            if (!state_.objectExists(id)) continue;
            const Card* card = card_registry_.get(state_.getObject(id).card_def_id);
            if (!card || !card->hasMightDependentAura()) continue;
            const PlayerId controller = state_.getObject(id).controller;
            asTextCopy(id, *card, [&]() { card->applyMightDependentAura(state_, controller); });
        }
        foldAuras();
    }

    // Step 6: "... have [Keyword] if they didn't already" (AuraEffect::
    // only_if_absent). Decided here, after EVERY hook of the pass — ordinary
    // and Might-dependent — has added what it adds, so the answer does not
    // depend on the order the hooks ran in: such an effect is dropped from an
    // object that has the keyword printed or "this turn" (GameObject::keywords;
    // a [Deflect N] value given this turn), or from any other continuous
    // effect, and of several such effects on one object the first is kept.
    // (Every object's effects were read by the fold just above, the last one
    // to run before this step: when it met no such effect, there is none, and
    // this walk over every object is not made — it was 1.5% of a random game.)
    if (any_conditional) {
        bool dropped = false;
        forEachCleanupObject([&](GameObjectId, GameObject& obj) {
            auto& effects = obj.aura_effects;
            const auto conditional = [](const GameObject::AuraEffect& ae) {
                return ae.only_if_absent && ae.keyword != Keyword::Count;
            };
            if (std::none_of(effects.begin(), effects.end(), conditional)) return;
            KeywordSet kept;   // conditional keywords already granted once
            const size_t before = effects.size();
            effects.erase(std::remove_if(effects.begin(), effects.end(),
                [&](const GameObject::AuraEffect& ae) {
                    if (!conditional(ae)) return false;
                    bool has = obj.keywords.has(ae.keyword) || kept.has(ae.keyword) ||
                               (ae.keyword == Keyword::Deflect &&
                                obj.deflect_value + obj.temp_deflect_value > 0);
                    for (const auto& other : effects)
                        if (!other.only_if_absent && other.keyword == ae.keyword) has = true;
                    if (!has) kept.set(ae.keyword);
                    return has;
                }), effects.end());
            dropped = dropped || effects.size() != before;
        });
        if (dropped) foldAuras();
    }

    // Risen Altar: battlefield-scoped [Empower] discount lands on the units there.
    for (auto& [id, obj] : state_.objects.onBoard()) {
        if (!obj.isUnit()) continue;
        if (auto bf = obj.battlefieldId()) {
            for (auto& b : state_.battlefields)
                if (b.id == *bf)
                    obj.empower_cost_reduction +=
                        b.empower_cost_reduction[playerIndex(obj.controller)];
        }
    }

    refreshFlowGrants();
}

void GameEngine::refreshFlowGrants() {
    // [Flow] (Vendetta): a spell in its owner's trash may be played for its
    // Flow cost, then banished. Modeled as standing trash-replay grants that
    // are rebuilt from the trash every aura pass (banish_after marks them).
    for (auto pid : {PlayerId::Player1, PlayerId::Player2}) {
        auto& ps = state_.player(pid);
        ps.trash_replay_grants.erase(
            std::remove_if(ps.trash_replay_grants.begin(), ps.trash_replay_grants.end(),
                [](const PlayerState::TrashReplayGrant& g) {
                    return g.banish_after || g.standing;
                }),
            ps.trash_replay_grants.end());
        for (auto cid : ps.trash) {
            if (!state_.objectExists(cid)) continue;
            auto& obj = state_.getObject(cid);
            if (obj.card_def_id == kInvalidId) continue;
            const Card* c = card_registry_.get(obj.card_def_id);
            if (!c) continue;
            // "You may play me from your trash for <cost>" (any card type):
            // the same standing permission, without the banish.
            if (const auto tp = c->trashPlayCost(state_, pid, cid); tp.valid) {
                PlayerState::TrashReplayGrant tg;
                tg.card = cid;
                tg.standing = true;
                tg.energy = tp.energy;
                tg.power = tp.power;
                tg.power_domain = tp.power_domain;
                tg.any_domain = tp.any_domain;
                ps.trash_replay_grants.push_back(tg);
                continue;
            }
            if (!obj.isSpell()) continue;
            PlayerState::TrashReplayGrant g;
            g.card = cid;
            g.banish_after = true;
            g.standing = true;
            auto flow = c->flowCost();
            if (flow.valid) {
                g.energy = flow.energy;
                g.power = flow.power;
                g.power_domain = flow.power_domain;
                g.any_domain = flow.any_domain;
            } else {
                // Flow granted for the turn equal to the card's own cost
                // (Kennen, Storm of Shuriken). This runs for every spell in a
                // trash on every aura pass: a card with no counters is not
                // looked up at all, and the key is built once, not per lookup
                // (plans/06 item 6.11).
                if (obj.card_counters.empty()) continue;
                static const std::string kFlowGrantedTurn = "__flow_granted_turn";
                auto it = obj.card_counters.find(kFlowGrantedTurn);
                if (it == obj.card_counters.end() || it->second != state_.turn.turn_number)
                    continue;
                const auto& def = card_db_.get(obj.card_def_id);
                g.energy = def.energy_cost;
                g.power = def.power_cost;
                g.power_domain = def.domains.empty() ? Domain::Fury : def.domains[0];
            }
            // Stargazer: Flow plays cost [N] less, to a minimum of [1].
            if (ps.flow_cost_reduction > 0 && g.energy > 1)
                g.energy = std::max(1, g.energy - ps.flow_cost_reduction);
            ps.trash_replay_grants.push_back(g);
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
    // (Every object, not the board lists: the card that was taken may itself
    // have left the board before the source did, and its control still
    // reverts. One flag read per object; 1.3% of a random game.)
    // (Once the piles are known inert the walk leaves them out: a card in a
    // pile is its owner's and carries no lapsing control — forgetBoardState.)
    forEachCleanupObject([&](GameObjectId id, GameObject& obj) {
        if (!obj.control_reverts_on_source_leave) return;
        GameObjectId src = obj.control_source_obj;
        bool source_in_play = state_.objectExists(src) &&
            (state_.getObject(src).location.has_value() ||
             state_.getObject(src).zone == ZoneType::LegendZone);
        if (source_in_play) return;
        if (obj.controller != obj.control_revert_to) {
            RIFT_TRACE(events_, "CONTROL REVERTS: " + obj.name + " (id=" +
                             toString(id) + ") returns to " +
                             toString(obj.control_revert_to) +
                             " (source left the board)");
            obj.controller = obj.control_revert_to;
            events_.emit(ObjectStateChangedEvent{id, "controller_changed"});
        }
        obj.control_reverts_on_source_leave = false;
        obj.control_source_obj = kInvalidId;
        obj.control_revert_to = PlayerId::None;
    });
}

// ── What a Cleanup walks, and when it is not run at all ──────────────────────
//
// A Cleanup follows every step of a chain and every phase (CR 319): about 270
// a game, of which 98% find nothing to do. Two things keep those cheap.
//
// (1) The piles are left out. An object in a Main Deck, a Rune Deck, a Trash
// or Banishment is a new object (CR 124.1): it has no designation, no status,
// no damage, no lapsing control and no continuous effect, so no step of a
// Cleanup has anything to read from it or to reset on it. That is not assumed:
// the first Cleanup after the state was set from outside walks every object,
// as every Cleanup used to, and only if it then finds every pile object inert
// (inertInAPile) do later Cleanups walk ObjectTable::offPiles() instead. From
// there the property keeps itself: an object reaches a pile from the board
// through EffectExecutor::leftBoard / forgetBoardState, which resets all of
// it at the move, and from anywhere else (a hand, the chain) it can carry
// only what an aura hook gave it there, which recalculateAuras remembers
// (aura_outside_play_) and clears. A continuous effect given to a card IN a
// pile ("Your Equipment everywhere have [Quick-Draw]") is declared by its card
// (Card::continuousEffectReachesPiles): while one is in play every object is
// walked, until a Cleanup finds the piles inert again. What would break it is
// such an effect undeclared, or an object taken off the board by a path that
// skips leftBoard; the checked build (-DRIFTBOUND_CHECK_BOARD_INDEX
// =ON) verifies after every pass that the piles are still inert and that the
// "did this pass change anything" verdict over the reduced walk equals the
// verdict over every object, and throws naming the object.
//
// (2) A Cleanup that directly follows one that had nothing to do is not run.
// "Directly": the bus has been told nothing in between — no event, no log
// line, heard or not (EventBus::activity) — the chain is as it was and the
// state was not handed out for editing. The engine announces what it does
// (every state change of a game is traced or emitted, which is what the
// replay and the trigger system are built on), so nothing announced means
// the Cleanup would meet the state its predecessor left, and that one found
// nothing. The checked build does not skip: it runs the Cleanup and throws
// if it changed a checksummed field, announced anything or changed what
// either player observes (the feature vectors).
template <class F>
void GameEngine::forEachCleanupObject(F&& fn) {
    if (piles_inert_) {
        for (auto& [id, obj] : state_.objects.offPiles()) fn(id, obj);
    } else {
        for (auto& [id, obj] : state_.objects) fn(id, obj);
    }
}

bool GameEngine::inertInAPile(const GameObject& obj) {
    return obj.combat_designation == CombatDesignation::None &&
           !obj.control_reverts_on_source_leave &&
           obj.aura_effects.empty() && obj.aura_might_bonus == 0 &&
           obj.aura_keywords.bits == 0 && obj.aura_bonus_damage_taken == 0 &&
           !obj.immune_to_damage && obj.spells_targeting_me_cost_reduction == 0 &&
           !obj.prevent_enemy_spell_ability_damage && !obj.cant_be_moved_by_enemy &&
           !obj.aura_cant_move_to_base && obj.empower_cost_reduction == 0 &&
           !obj.aura_no_combat_damage &&
           obj.untargetable_by_enemy == obj.untargetable_by_enemy_this_turn &&
           obj.granted_abilities.size() == obj.temp_granted_abilities.size() &&
           (obj.granted_abilities.empty() ||
            std::equal(obj.granted_abilities.begin(), obj.granted_abilities.end(),
                       obj.temp_granted_abilities.begin(),
                       [](const GameObject::GrantedAbilityRef& a,
                          const GameObject::GrantedAbilityRef& b) {
                           return a.source_def_id == b.source_def_id &&
                                  a.ability_index == b.ability_index &&
                                  a.exhaust_cost_only == b.exhaust_cost_only;
                       }));
}

void GameEngine::verifyPilesInert(const char* where) const {
    for (const auto& [id, obj] : state_.objects) {
        if (!ObjectTable::inPile(obj) || inertInAPile(obj)) continue;
        throw std::logic_error(std::string("GameEngine::cleanup (") + where + "): " + obj.name +
                               " (id=" + toString(id) + ", zone " +
                               std::to_string(static_cast<int>(obj.zone.get())) +
                               ") is in a pile and carries a designation, a lapsing control "
                               "or a continuous effect: a Cleanup no longer visits the piles "
                               "(see the note above GameEngine::cleanup)");
    }
}

uint64_t GameEngine::cleanupShape() const {
    uint64_t h = 0x9E3779B97F4A7C15ULL;
    auto mix = [&](uint64_t x) { h = (h ^ x) * 0x100000001B3ULL; h ^= h >> 29; };
    mix(state_.chain.items.size());
    mix(state_.chain.resuming.has_value());
    mix(static_cast<uint64_t>(state_.turn.turn_player));
    mix(static_cast<uint64_t>(state_.turn.phase));
    mix(static_cast<uint64_t>(state_.turn.ns_state));
    mix(static_cast<uint64_t>(state_.turn.oc_state));
    mix(static_cast<uint64_t>(state_.turn.turn_number));
    mix(state_.turn.is_additional_turn);
    mix(state_.game_over);
    mix(state_.objects.size());
    for (auto pid : {PlayerId::Player1, PlayerId::Player2})
        mix(static_cast<uint64_t>(static_cast<uint32_t>(state_.player(pid).score)));
    mix(state_.battlefields.size());
    for (const auto& bf : state_.battlefields) {
        mix(bf.card_object_id.raw());
        mix(bf.controller ? static_cast<uint64_t>(*bf.controller) + 1 : 0);
        mix(bf.attacker ? static_cast<uint64_t>(*bf.attacker) + 1 : 0);
        mix(bf.defender ? static_cast<uint64_t>(*bf.defender) + 1 : 0);
        mix((bf.is_contested ? 1u : 0u) | (bf.showdown_staged ? 2u : 0u) |
            (bf.combat_staged ? 4u : 0u) | (bf.showdown_in_progress ? 8u : 0u) |
            (bf.combat_in_progress ? 16u : 0u));
        mix(bf.facedown.size());
    }
    return h;
}

void GameEngine::cleanup() {
    // Both shortcuts of the note above are for a game the engine plays: a
    // position a test builds by hand (and edits through a reference it kept)
    // gets the Cleanup of old, every object and every time.
    if (!playing_a_game_) forgetCleanupFacts();
#ifndef RIFTBOUND_CHECK_BOARD_INDEX
    // (2) above.
    if (quiet_cleanup_valid_ && events_.activity() == quiet_cleanup_activity_ &&
        !events_.holding() && cleanupShape() == quiet_cleanup_shape_)
        return;
#else
    const bool would_skip =
        quiet_cleanup_valid_ && events_.activity() == quiet_cleanup_activity_ &&
        !events_.holding() && cleanupShape() == quiet_cleanup_shape_;
    std::vector<float> seen_before[2];
    if (would_skip) {
        seen_before[0] = ml::extractStateFeatures(state_, PlayerId::Player1, card_db_);
        seen_before[1] = ml::extractStateFeatures(state_, PlayerId::Player2, card_db_);
    }
#endif
    quiet_cleanup_valid_ = false;
    const uint64_t activity_at_start = events_.activity();
    const size_t chain_at_start = state_.chain.items.size();
    const bool game_over_at_start = state_.game_over;
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
    auto checksumOver = [&](bool every_object) -> uint64_t {
        uint64_t h = 0xCBF29CE484222325ULL; // FNV-1a seed
        auto mix = [&](uint64_t x) {
            h ^= x;
            h *= 0x100000001B3ULL;
        };
        mix(state_.objects.size());
        // Every object of the game, not only those in play: a pass can change
        // a card that is in no list of the board index (a unit that just died
        // has its damage cleared in the trash; a control effect lapses on a
        // card in a hand), and "nothing changed" has to mean nothing.
        // This was the largest walk left in the engine: 12% of a random game
        // when each field was mixed in turn (eight multiplies in a chain per
        // object). Now an object's fields are combined without a chain and
        // only one multiply per object waits for the one before: 4.7% of a
        // random game (plans/09 item 9.KK3).
        // (With the piles known inert, the objects outside them: a pass
        // changes nothing in a pile that it does not also change here — a
        // card that reaches a pile during the pass leaves this walk, and a
        // card in a pile is touched by no step.)
        // An object's tags too: a hook may give one in the aura pass (Experimental
        // Hexplate: "I am a Mech."), which another hook of the same pass reads
        // (Rumble, Scrapper: "Your Mechs have +1 [M]"). A tag given after the
        // reader looked is a change: the next pass gives the bonus. Tags are
        // interned, so equal tag lists are one pointer.
        const auto mixObject = [&](GameObjectId id, const GameObject& obj) {
            const uint64_t flags =
                static_cast<uint64_t>(obj.zone.get()) |
                (static_cast<uint64_t>(obj.controller) << 8) |
                (obj.untargetable_by_enemy ? 1ULL << 16 : 0) |
                (obj.is_exhausted ? 1ULL << 17 : 0);
            mix(static_cast<uint64_t>(id.raw()) * 0x9E3779B97F4A7C15ULL ^
                static_cast<uint64_t>(static_cast<uint32_t>(obj.damage_marked)) * 0xC2B2AE3D27D4EB4FULL ^
                static_cast<uint64_t>(static_cast<uint32_t>(obj.current_might)) * 0x165667B19E3779F9ULL ^
                static_cast<uint64_t>(static_cast<uint32_t>(obj.aura_might_bonus)) * 0x27D4EB2F165667C5ULL ^
                static_cast<uint64_t>(reinterpret_cast<std::uintptr_t>(obj.tags.operator->())) *
                    0x94D049BB133111EBULL ^
                flags * 0xFF51AFD7ED558CCDULL);
        };
        if (every_object) {
            for (auto& [id, obj] : state_.objects) mixObject(id, obj);
        } else {
            for (auto& [id, obj] : state_.objects.offPiles()) mixObject(id, obj);
        }
        for (auto& bf : state_.battlefields) {
            mix(static_cast<uint64_t>(bf.id.raw()));
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
    auto checksum = [&]() { return checksumOver(!piles_inert_); };

    constexpr int kMaxCleanupPasses = 20;
    // What a pass ended with is what the next one starts with: nothing runs
    // between them but the win check, which changes none of the fields the
    // checksum reads (it ends the cleanup if it changes anything at all).
    std::optional<uint64_t> carried;
    int passes = 0;
    for (int pass = 0; ; ++pass) {
        ++passes;
        // Engine bound (CR 322: "repeating until a Cleanup occurs with no new
        // change"): a state that still changes after this many Cleanups in a
        // row is oscillating — two continuous effects undoing each other —
        // and that is a defect, reported as one. (The loop used to stop
        // quietly and leave the state wherever it was.)
        if (pass >= kMaxCleanupPasses)
            throw std::logic_error("GameEngine::cleanup: the state still changed after " +
                                   std::to_string(kMaxCleanupPasses) +
                                   " consecutive Cleanups (CR 322)");
        if (checkWinCondition()) return;
        uint64_t before = carried.has_value() ? *carried : checksum();
#ifdef RIFTBOUND_CHECK_BOARD_INDEX
        const bool reduced_walk = piles_inert_;
        const uint64_t before_every = reduced_walk ? checksumOver(true) : 0;
#endif
        processLethalDamage();
        revertLapsedControl();           // "until I leave the board" (Akshan)
        updateBattlefieldControl();
        recallMisplacedObjects();        // CR 323.6
        processContestedBattlefields();  // CR 323.2, 323.7-323.10
        recalculateAuras();
        uint64_t after = checksum();
#ifdef RIFTBOUND_CHECK_BOARD_INDEX
        // (Not when a card whose effect reaches the piles entered play during
        // the pass: the walk is over every object from there on.)
        if (reduced_walk && piles_inert_) {
            verifyPilesInert("after a pass");
            if ((before == after) != (before_every == checksumOver(true)))
                throw std::logic_error("GameEngine::cleanup: the pass changed an object in a "
                                       "pile and nothing outside the piles (or the reverse): "
                                       "the reduced walk and the walk over every object "
                                       "disagree on whether another Cleanup is due");
        }
#endif
        if (before == after) break;
        carried = after;
    }
    // (1) above: a Cleanup that walked every object and left every pile
    // object inert establishes the reduced walk for the ones that follow.
    if (!piles_inert_ && !piles_reached_ && playing_a_game_) {
        bool inert = true;
        for (const auto& [id, obj] : state_.objects)
            if (ObjectTable::inPile(obj) && !inertInAPile(obj)) { inert = false; break; }
        piles_inert_ = inert;
    }

    // ── "Becomes Mighty" edge detection (CR: a unit is Mighty at 5+ [M]) ──
    // Run once after the cleanup loop settles so the Might layers are final.
    // We remember per-unit Mighty state as a stamp, GameObject::mighty_seen:
    // the number of the last check (GameState::mighty_checks) that found the
    // unit on the board with 5+ [M]. A unit "was Mighty" if its stamp is the
    // previous check's number. (Not a string-keyed counter — plans/06 item
    // 6.11 — and not a flag that has to be cleared on every unit that is off
    // the board, which made this a walk over every object of the game:
    // plans/09 item 9.KK3. A unit that is not on the board is not visited and
    // its stamp goes stale, which is the same as clearing it.) We
    // emit a generic ObjectStateChangedEvent only on a <5 -> >=5 transition;
    // TriggerManager::onObjectStateChanged dispatches WhenAUnitBecomesMighty.
    // Falling below 5 re-arms the edge so it can fire again later this/next
    // turn. (Fiora, Worthy 500 / Grand Duelist 519.) No card-specific logic
    // here — the engine only detects and announces the state change.
    {
        const uint32_t previous = state_.mighty_checks;
        const uint32_t now = ++state_.mighty_checks;
        // The units to announce are collected first: an announcement queues
        // triggers, and the walk is over a board range.
        std::vector<GameObjectId> became;
        for (auto& [id, obj] : state_.objects.onBoard()) {
            if (!obj.isUnit()) continue;
            if (obj.current_might < 5) continue;   // its stamp stays behind: the edge is armed
            const bool was = previous != 0 && obj.mighty_seen == previous;
            obj.mighty_seen = now;
            if (!was) became.push_back(id);
        }
        // The units one Cleanup finds newly Mighty became so at the same
        // time as far as the game can tell: their abilities are one batch
        // (CR 383.3.d).
        // "Becomes Mighty" (CR 709) is noticed HERE, at Cleanups. One follows
        // every step of a chain (CR 319.3-319.5), so a unit that drops below
        // 5 when a cost is paid and comes back as the effect resolves is
        // seen both times. No Cleanup runs inside a resolution (CR 321): a
        // dip and return within one resolution is not an event for the game.
        const SimultaneousTriggers together(state_.chain);
        for (auto id : became) events_.emit(ObjectStateChangedEvent{id, "became_mighty"});
    }
    // Other "my Might becomes N or more" edges are the trigger manager's.
    events_.emit(StateSettledEvent{});

    // (2) above: one pass that changed nothing, StateSettled the only thing
    // announced, and nothing put on the chain for it.
    const bool quiet = passes == 1 && !events_.holding() && !state_.game_over &&
                       !game_over_at_start &&
                       events_.activity() == activity_at_start + 1 &&
                       state_.chain.items.size() == chain_at_start;
#ifdef RIFTBOUND_CHECK_BOARD_INDEX
    if (would_skip) {
        if (!quiet)
            throw std::logic_error("GameEngine::cleanup: a Cleanup that would not have been "
                                   "run (nothing announced since one that had nothing to do) "
                                   "changed the state or announced something (passes " +
                                   std::to_string(passes) + ", announcements " +
                                   std::to_string(events_.activity() - activity_at_start) +
                                   ", chain " + std::to_string(chain_at_start) + " -> " +
                                   std::to_string(state_.chain.items.size()) + ")");
        if (seen_before[0] != ml::extractStateFeatures(state_, PlayerId::Player1, card_db_) ||
            seen_before[1] != ml::extractStateFeatures(state_, PlayerId::Player2, card_db_))
            throw std::logic_error("GameEngine::cleanup: a Cleanup that would not have been "
                                   "run changed what a player observes");
    }
#endif
    if (quiet) {
        quiet_cleanup_valid_ = true;
        quiet_cleanup_activity_ = events_.activity();
        quiet_cleanup_shape_ = cleanupShape();
    }
}

bool GameEngine::checkWinCondition() {
    // The points needed to win, including battlefields that change it.
    const int effective_victory = effect_executor_->effectiveVictoryScore();

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
    // "Any amount of your damage is enough to kill" (CR 142.4.c) is asked only
    // for a unit that carries damage short of lethal, and at most once per
    // player per call: most cleanups find no such unit, and the question is a
    // walk over that player's units in play (plans/06 item 6.6). Nothing in
    // the loop below changes the answer (it only reads), so asking late and
    // remembering is the same as asking first.
    // The test itself is EffectExecutor::damageKills, shared with the kills a
    // card makes after dealing damage (killLethallyDamagedTogether).
    EffectExecutor::LethalRule rule;
    std::vector<GameObjectId> to_kill;
    for (auto& [id, obj] : state_.objects.onBoard()) {
        if (!obj.isUnit() || !obj.location.has_value()) continue;
        if (obj.damage_marked <= 0) continue;
        bool by_any_damage = false;
        if (!effect_executor_->damageKills(obj, rule, &by_any_damage)) continue;
        if (by_any_damage)
            RIFT_TRACE(events_, "ELDER_DRAGON: " + obj.name + " killed by any-damage rule");
        to_kill.push_back(id);
    }
    // They die at the same time: each one's last-known information is taken
    // while all of them are still on the board.
    const DeathBatchMark batch = effect_executor_->beginDeathBatch(to_kill);
    for (auto id : to_kill) {
        killUnit(id);
    }
    effect_executor_->endDeathBatch(to_kill, batch);
}

void GameEngine::updateBattlefieldControl() {
    for (auto& bf : state_.battlefields) {
        if (bf.combat_in_progress || bf.showdown_in_progress) continue;

        if (bf.controller.has_value()) {
            bool has_units = bf.hasUnitsFrom(*bf.controller, state_.objects);
            // CR 323.5: "... if the turn is in an Open State and there is no
            // Showdown or Combat ongoing there" — an Open State, Neutral or
            // Showdown (a showdown or combat at ANOTHER battlefield does not
            // keep this one; the one here was skipped above).
            if (!has_units && state_.turn.oc_state == OpenClosedState::Open) {
                auto old = bf.controller;
                bf.controller = std::nullopt;
                events_.emit(ControlChangedEvent{bf.id, old, std::nullopt});
            }
        }

        removeForeignHiddenCards(bf.id);
    }
}

void GameEngine::removeForeignHiddenCards(BattlefieldId bf_id) {
    auto& bf = getBattlefield(bf_id);
    // Remove facedown cards that belong to a player who doesn't control
    // this battlefield: "Remove all Hidden cards from all Battlefields
    // that are not controlled by the same player and place them in their
    // owner's Trash." (CR 323.6.a; CR 107.3.d). EVERY hidden card, with no
    // split by card type — a hidden unit or gear goes to the trash like a
    // hidden spell.
    // (Pinned by PerfEngineHiddenHourglass.NotPlayed_TheFacedownCardGoesToItsOwnersTrash;
    // a card that WAS played from facedown is a permanent on the board
    // and is recalled instead, CR 149.3 / 323.6 — recallMisplacedObjects.)
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
        settleInTrash(state_, events_, card_id);
        RIFT_DEBUG(events_, std::string("HIDDEN: ") + card.name +
                         " removed from facedown (control lost)");
    }
}

void GameEngine::recallMisplacedObjects() {
    // CR 323.6: "Recall all of the following Game Objects, unless an effect
    // would indicate otherwise: Unattached non-Unit Gear located at
    // Battlefields; ... Unattached Permanents and Runes in Bases other than
    // their controller's." (A recall is not a move.)
    // Which objects is decided here; the recall itself is the one recall
    // there is, EffectExecutor::recallToBase (attachments go along, CR
    // 719.3.a; "recalled" is announced). Collected first: recalling moves
    // attachments, and the loop walks the object map.
    std::vector<GameObjectId> misplaced;
    for (auto& [id, obj] : state_.objects.onBoard()) {
        if (!obj.location.has_value() || obj.is_hidden) continue;   // facedown cards stay put
        if (obj.controller == PlayerId::None) continue;
        const LocationId home{BaseLocation{obj.controller}};
        bool recall = false;
        if (obj.isGear() && !obj.attached_to.has_value() && obj.isAtBattlefield())
            recall = true;
        if ((obj.isPermanent() || obj.isRune()) && !obj.attached_to.has_value() &&
            obj.isAtBase() && *obj.location != home)
            recall = true;
        if (recall) misplaced.push_back(id);
    }
    std::sort(misplaced.begin(), misplaced.end());
    for (auto id : misplaced) {
        RIFT_TRACE(events_, "RECALL (CR 323.6): " + state_.getObject(id).name +
                         " (id=" + toString(id) + ")");
        effect_executor_->recallToBase(id);
    }
}

void GameEngine::processContestedBattlefields() {
    // CR 323.2: Attacker / Defender designations track where a Combat is.
    //  .a  a Unit at the Combat's battlefield without one gains its controller's;
    //  .b  a Unit there with the opposite one (it changed controller) is corrected;
    //  .c  a Unit anywhere else loses any designation it still has.
    // (Every object, not the board lists, because of .c: "anywhere else"
    // includes a unit that left the board with its designation — its death
    // trigger may still read it until this runs. 2.3% of a random game.)
    // (With the piles known inert — cleanup(), piles_inert_ — they are left
    // out: forgetBoardState takes the designation off at the move.)
    std::vector<GameObjectId> joined;
    forEachCleanupObject([&](GameObjectId id, GameObject& obj) {
        if (!obj.isUnit()) return;
        CombatDesignation want = CombatDesignation::None;
        if (auto u_bf = obj.battlefieldId()) {
            for (const auto& bf : state_.battlefields) {
                if (bf.id != *u_bf || !bf.combat_in_progress) continue;
                if (!bf.attacker.has_value() || !bf.defender.has_value()) continue;
                if (obj.controller == *bf.attacker) want = CombatDesignation::Attacker;
                else if (obj.controller == *bf.defender) want = CombatDesignation::Defender;
            }
        }
        if (obj.combat_designation != want) {
            obj.combat_designation = want;
            obj.recomputeMight();
            if (want != CombatDesignation::None) joined.push_back(id);
        }
    });
    // CR 466.2.c.3.a: a unit that becomes present during a combat gains its
    // designation "during the Cleanup following the action that caused it to
    // become present" — and that is when its Attack / Defend Triggers trigger
    // (CR 383.4.e / .f), once per combat. The units one Cleanup finds joined
    // at the same time: one batch (CR 383.3.d).
    if (!joined.empty()) {
        std::sort(joined.begin(), joined.end());
        const SimultaneousTriggers together(state_.chain);
        for (auto id : joined) events_.emit(ObjectStateChangedEvent{id, "joined_combat"});
    }

    // CR 190.3 / 323.7-323.10: Contested status and what it stages.
    for (auto& bf : state_.battlefields) {
        // CR 190.4.b: nothing changes while a Showdown or Combat is ongoing.
        if (bf.showdown_in_progress || bf.combat_in_progress) continue;

        const bool has_p1 = bf.hasUnitsFrom(PlayerId::Player1, state_.objects);
        const bool has_p2 = bf.hasUnitsFrom(PlayerId::Player2, state_.objects);
        auto present = [&](PlayerId p) {
            return p == PlayerId::Player1 ? has_p1 : p == PlayerId::Player2 ? has_p2 : false;
        };

        // CR 323.10 (190.3.b.1): remove Contested when the player who applied
        // it has no Units here. Whatever was staged on its account goes too.
        if (bf.is_contested) {
            // States built without a recorded contester (older snapshots,
            // tests) fall back to "anyone present".
            const bool contester_present = bf.contested_by != PlayerId::None
                ? present(bf.contested_by) : (has_p1 || has_p2);
            if (!contester_present) {
                RIFT_TRACE(events_, "CONTESTED_REMOVED: BF#" + toString(bf.id) +
                                 " (contester has no units here)");
                bf.is_contested = false;
                bf.contested_by = PlayerId::None;
                bf.showdown_staged = false;
                bf.combat_staged = false;
            }
        }

        // CR 190.3.a / 323.10.a: Units at an uncontested Battlefield that their
        // controller does not control apply Contested. This also covers Units
        // that "otherwise become present" by an effect (moved, played or
        // created there, or changing controller there). Turn player first.
        if (!bf.is_contested) {
            for (PlayerId p : {state_.turn.turn_player, opponent(state_.turn.turn_player)}) {
                if (p == PlayerId::None || !present(p)) continue;
                if (bf.controller.has_value() && *bf.controller == p) continue;
                bf.is_contested = true;
                bf.contested_by = p;
                events_.emit(ContestedEvent{bf.id, p});
                break;
            }
        }

        if (!bf.is_contested) {
            bf.showdown_staged = false;
            bf.combat_staged = false;
            continue;
        }

        if (has_p1 && has_p2) {
            // CR 323.8: Units of opposing players → a Combat is staged.
            if (!bf.combat_staged) {
                bf.combat_staged = true;
                bf.showdown_staged = false;  // promote showdown → combat
            }
        } else {
            // CR 323.7 / 323.9: only the contester is here → a Showdown.
            bf.combat_staged = false;
            bf.showdown_staged = true;
        }
    }
}


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
    // One implementation of drawing and Burn Out (CR 432) for the whole engine.
    effect_executor_->drawCards(player, count);
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

        RIFT_TRACE(events_, "  CHANNELED: " + rune.name + " (id=" +
                         toString(rune_id) + ", ready)");
        events_.emit(RuneChanneledEvent{rune_id, player});
        events_.emit(EnteredBoardEvent{
            rune_id, player, CardType::Rune, BaseLocation{player}, false});
    }
}

void GameEngine::shuffleDeck(PlayerId player) {
    deck::shuffle(state_, player, rng_);
}

void GameEngine::shuffleRuneDeck(PlayerId player) {
    auto& deck = state_.player(player).rune_deck;
    std::shuffle(deck.begin(), deck.end(), rng_);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Cost payment
// ═══════════════════════════════════════════════════════════════════════════════

int GameEngine::availablePower(PlayerId player, Domain domain) const {
    // Power comes from recycling a rune of matching domain (CR 163.2.b)
    // Each rune in base (exhausted or ready) of matching domain can be recycled for 1 Power
    int count = 0;
    auto base_loc = BaseLocation{player};
    for (auto& [id, obj] : state_.objects.atLocation(LocationId{base_loc})) {
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
    for (auto& [id, obj] : state_.objects.atLocation(LocationId{base_loc})) {
        if (obj.isRune() && obj.controller == player &&
            obj.location.has_value() && *obj.location == LocationId{base_loc}) {
            count++;
        }
    }
    return count;
}

// ─── Optional additional costs (CR 356.2.b) ────────────────────────────────
//
// Accelerate, Repeat and "you may pay X as an additional cost" are paid
// through canPayOptionalCost / payOptionalCost, which apply "Optional
// additional costs you pay cost [1] or [A] less" (Card::optionalCostDiscount,
// summed over the payer's cards in play; CR 356.4.f: a discount can take an
// additional cost to 0 and it still counts as paid).
int GameEngine::optionalCostDiscount(PlayerId player) const {
    int total = 0;
    for (const auto& [id, obj] : state_.objects.onBoard()) {
        if (obj.controller != player || !obj.location.has_value()) continue;
        if (obj.card_def_id == kInvalidId) continue;
        if (const Card* c = card_registry_.get(obj.card_def_id))
            total += c->optionalCostDiscount(state_, id);
    }
    return std::max(0, total);
}


bool GameEngine::canPayOptionalCost(PlayerId player, int energy, int power,
                                    resource::DomainMask domains, SpendFor spend_for) const {
    const int disc = optionalCostDiscount(player);
    for (int power_first = 0; power_first < 2; ++power_first) {
        const auto [e, p] = afterEitherDiscount(std::max(0, energy), std::max(0, power), disc,
                                                power_first == 1);
        if (resource::canPayIn(state_, player, e, p, domains, spend_for)) return true;
    }
    return false;
}

bool GameEngine::payOptionalCost(PlayerId player, int energy, int power,
                                 resource::DomainMask domains, SpendFor spend_for) {
    const int disc = optionalCostDiscount(player);
    const auto e_first = afterEitherDiscount(std::max(0, energy), std::max(0, power), disc, false);
    const auto p_first = afterEitherDiscount(std::max(0, energy), std::max(0, power), disc, true);
    const bool e_ok = resource::canPayIn(state_, player, e_first.first, e_first.second,
                                         domains, spend_for);
    const bool p_ok = resource::canPayIn(state_, player, p_first.first, p_first.second,
                                         domains, spend_for);
    if (!e_ok && !p_ok) return false;
    bool power_first = p_ok && !e_ok;
    if (e_ok && p_ok && e_first != p_first) {
        // "[1] or [A] less": the payer picks the half.
        Intent e; e.type = IntentType::MakeChoice; e.player = player;
        e.chosen_value = 0; e.choice_label = "cost [1] less";
        Intent pw = e; pw.chosen_value = 1; pw.choice_label = "cost [A] less";
        power_first = decideNow(player, {e, pw},
            "optional additional cost: [1] or [A] less").chosen_value.value_or(0) == 1;
    }
    const auto net = power_first ? p_first : e_first;
    return effect_executor_->payResources(player, net.first, net.second, domains, spend_for);
}

void GameEngine::maybePayOptionalAdditionalCost(PlayerId player, GameObjectId card_id,
                                                int extra_any_power,
                                                const Card::PlayCostAdjust& base) {
    if (!state_.objectExists(card_id)) return;
    auto& card = state_.getObject(card_id);
    if (card.card_def_id == kInvalidId) return;
    const Card* cdef = card_registry_.get(card.card_def_id);
    if (!cdef) return;
    auto cost = cdef->optionalAdditionalCost(state_, player);
    if (!cost.valid) return;
    // A discard that reduces the cost is an additional cost chosen BEFORE
    // payment (Card::additionalPlayCost — payAdditionalPlayCosts), so it is
    // NOT handled here.
    if (cost.discard_cards > 0 && cost.reduce_energy > 0) return;

    // Pure "you may discard N as an additional cost" (Zed, From the Shadows;
    // Ruthless Strike): offered whenever the hand can cover it.
    if (cost.discard_cards > 0) {
        if (static_cast<int>(state_.player(player).hand.size()) < cost.discard_cards) return;
        Intent no;  no.type  = IntentType::MakeChoice; no.player  = player; no.chosen_value  = 0;
        Intent yes; yes.type = IntentType::MakeChoice; yes.player = player; yes.chosen_value = 1;
        std::vector<Intent> dopts = {no, yes};
        state_.decision_index++;
        RIFT_TRACE_DECISION(events_, "DECISION #" + std::to_string(state_.decision_index) + " (" +
                         toString(player) + "): discard " +
                         std::to_string(cost.discard_cards) + " as additional cost for " +
                         card.name + "? [decline|pay] [2 options]");
        auto dchosen = askAgent(player, dopts);
        if (on_decision) on_decision(state_, dopts, dchosen);
        if (dchosen.chosen_value.value_or(0) == 1) {
            effect_executor_->discardCards(player, cost.discard_cards);
            if (cost.paid_flag && cost.paid_flag[0] != '\0')
                card.card_counters[cost.paid_flag] = 1;
        }
        return;
    }

    // [A] is any Domain, and several [A] need not share one.
    const resource::DomainMask domains =
        resource::maskOf(cost.any_domain ? Domain::Count : cost.power_domain);
    const SpendFor spend_for = spendForObject(card);
    // can't afford -> not offered (CR: choice only when payable)
    if (!canPayOptionalCost(player, cost.energy, cost.power, domains, spend_for)) return;
    // ... and only if the card itself can still be paid for afterwards: the
    // choice is made before any payment (CR 355.1.a), so it is judged on the
    // whole. Tried on a copy of the game, which is thrown away (as
    // prePlayProbe does: the game is moved aside, not copied over).
    {
        struct ForgetFacts {
            GameEngine& engine;
            ~ForgetFacts() { engine.forgetCleanupFacts(); }
        } forget_facts{*this};
        struct Parked {
            GameState& live;
            GameState game;
            explicit Parked(GameState& s) : live(s), game(std::move(s)) {}
            ~Parked() { live = std::move(game); }
        } parked(state_);
        struct Quiet {
            EventBus& bus;
            EventBus::Quiet q;
            explicit Quiet(EventBus& b) : bus(b), q(b.beginQuiet()) {}
            ~Quiet() { bus.endQuiet(q); }
        } quiet(events_);
        state_ = parked.game;
        forgetCleanupFacts();
        payOptionalCost(player, cost.energy, cost.power, domains, spend_for);
        if (!base.ignore_cost && !canAffordWith(player, card_id, extra_any_power, base)) return;
    }

    // Optional ("you may") -> yes/no agent decision. Mirrors the Accelerate flow.
    Intent decline; decline.type = IntentType::MakeChoice; decline.player = player; decline.chosen_value = 0;
    Intent accept;  accept.type  = IntentType::MakeChoice; accept.player  = player; accept.chosen_value  = 1;
    decline.declines = true;   // the "no" of a yes/no (Intent::declines)
    std::vector<Intent> opts = {decline, accept};
    state_.decision_index++;
    RIFT_TRACE_DECISION(events_, "DECISION #" + std::to_string(state_.decision_index) + " (" +
                     toString(player) + "): pay additional cost for " + card.name +
                     "? [decline|pay] [2 options]");
    auto chosen = askAgent(player, opts);
    if (on_decision) on_decision(state_, opts, chosen);

    if (chosen.chosen_value.value_or(0) == 1) {
        payOptionalCost(player, cost.energy, cost.power, domains, spend_for);
        if (cost.paid_flag && cost.paid_flag[0] != '\0') {
            card.card_counters[cost.paid_flag] = 1;
        }
        RIFT_TRACE(events_, "ADDITIONAL_COST: " + card.name + " paid optional additional cost");
    } else {
        RIFT_TRACE(events_, "ADDITIONAL_COST: " + card.name + " declined additional cost");
    }
}

// Net change to a card's POWER cost from continuous effects and cost modifiers
// (Helm of Suppression's "[A] more", Applied Researchers' "[A] less", Astral
// Heron's "[A][A] less"). Shared by canAfford and beginCostPayment so the two
// always agree.
// Extra POWER OF ANY DOMAIN ([A]) owed on top of a card's own cost: "cost [A]
// more" effects, which are not tied to the card's domains (Helm of
// Suppression taxes the opponent's spells; Mystic Vortex taxes [Reaction]
// cards during a showdown at its battlefield). Paid after the base cost.
static int extraAnyPowerCost(const GameState& state, PlayerId player, const GameObject& card) {
    int extra = 0;
    if (card.isSpell()) extra += state.player(player).spell_tax_power;
    // "(Hidden cards have [Reaction].)" — and [Ambush] is Reaction while played.
    const bool has_reaction = card.keywords.has(Keyword::Reaction) || card.is_hidden ||
                              card.keywords.has(Keyword::Ambush);
    if (has_reaction) {
        int tax = 0;
        // "During showdowns here": only while that battlefield's showdown is
        // open. A combat is a showdown during its Combat Showdown Step only
        // (CR 316.8.a, 466) — not once it has moved on to damage or resolution.
        for (const auto& bf : state.battlefields) {
            const bool showdown_open =
                bf.showdown_in_progress ||
                (bf.combat_in_progress && bf.combat_phase != CombatPhase::DamageStep &&
                 bf.combat_phase != CombatPhase::ResolutionStep);
            if (showdown_open && bf.reaction_tax_power > tax) tax = bf.reaction_tax_power;
        }
        extra += tax;
    }
    return std::max(0, extra);
}

// Whether one CostModifier applies to `payer` playing `card`. `own` says the
// modifier sits in the payer's own list (false: in the opponent's, where only
// "enemy" modifiers reach across). The single filter used for the Energy part,
// the Power part and the one-shot consume, so the three cannot drift.
// `printed_energy_cost` is the card's printed Energy cost (-1 = not known to
// the caller: a modifier limited by printed cost then does not apply).
static bool costModifierApplies(const GameState& state, const PlayerState::CostModifier& mod,
                                bool own, PlayerId payer, const GameObject& card,
                                bool combat_active, int printed_energy_cost) {
    if (mod.affects_enemy_only == own) return false;
    if (mod.max_printed_energy_cost >= 0 &&
        (printed_energy_cost < 0 || printed_energy_cost > mod.max_printed_energy_cost))
        return false;
    if (mod.next_spell_only && !card.isSpell()) return false;
    if (mod.next_unit_only && !card.isUnit()) return false;
    if (mod.units_only && !card.isUnit()) return false;
    if (mod.gear_only && !card.isGear()) return false;
    if (mod.first_gear_per_turn && state.player(payer).gears_played_this_turn > 0) return false;
    // "... from hand": the card is (or, while it is being paid for, was) in a
    // hand, and this play is not from the trash, the Champion Zone or an
    // effect playing it from elsewhere (PlayerState::current_play_source).
    if (mod.from_hand_only &&
        (card.zone != ZoneType::Hand ||
         state.player(payer).current_play_source != Intent::PlaySource::Hand))
        return false;
    if (mod.affects_non_token_only && card.super_type == SuperType::Token) return false;
    if (mod.combat_active_only && !combat_active) return false;
    if (!mod.required_tag.empty() &&
        std::find(card.tags.begin(), card.tags.end(), mod.required_tag) == card.tags.end())
        return false;
    return true;
}

static bool anyCombatInProgress(const GameState& state) {
    for (const auto& bf : state.battlefields)
        if (bf.combat_in_progress) return true;
    return false;
}

static int powerCostDelta(const GameState& state, PlayerId player, const GameObject& card,
                          const Card* card_class) {
    const auto& ps = state.player(player);
    int delta = 0;
    if (card.isSpell()) delta -= ps.spell_discount_power;
    delta -= ps.transient_power_discount;
    const bool combat_active = anyCombatInProgress(state);
    const int printed_energy = card_class ? card_class->def().energy_cost : -1;
    for (const auto& mod : ps.cost_modifiers) {
        if (mod.power_reduction == 0) continue;
        if (!costModifierApplies(state, mod, /*own=*/true, player, card, combat_active,
                                 printed_energy)) continue;
        delta -= mod.power_reduction;
    }
    for (const auto& mod : state.player(opponent(player)).cost_modifiers) {
        if (mod.power_reduction == 0) continue;
        if (!costModifierApplies(state, mod, /*own=*/false, player, card, combat_active,
                                 printed_energy)) continue;
        delta -= mod.power_reduction;
    }
    if (card_class) delta -= card_class->selfPowerCostReduction(state, player);
    return delta;
}

// What `card` costs `player` to play right now: its printed cost after every
// cost modifier and continuous effect. canAfford asks it to gate the play and
// beginCostPayment to charge it, so the two always agree.
namespace {
struct EffectiveCardCost {
    int energy = 0;
    int power = 0;
};
// `adj`: what an additional cost chosen before payment takes off
// (Card::PlayCostAdjust — "If you do, reduce my cost by [2]").
// `base`: the cost that replaces the printed Base Cost when the card is played
// "for [Cost]" (CR 356.1.a — Flow, a play-from-trash grant); increases and
// discounts then apply to it like to any other play (CR 356.3, 356.4).
EffectiveCardCost effectiveCardCost(const GameState& state, const CardDef& def,
                                    const Card* card_class, PlayerId player,
                                    const GameObject& card,
                                    const Card::PlayCostAdjust& adj = {},
                                    const EffectiveCardCost* base = nullptr) {
    const auto& ps = state.player(player);
    // "Ignoring" a cost sets that Base Cost to zero; cost increases can still
    // raise the total above zero (CR 356.1.b, 356.1.b.3).
    const bool ignore_energy = ps.transient_ignore_energy || adj.ignore_cost;
    const bool ignore_power = ps.transient_ignore_power || adj.ignore_cost;
    const int base_energy = ignore_energy ? 0 : (base ? base->energy : def.energy_cost);
    const int base_power = ignore_power ? 0 : (base ? base->power : def.power_cost);
    int increases = 0;        // "costs [N] more" (CR 356.3, before discounts)
    int free_reduction = 0;   // discounts with no minimum
    // Discounts that carry "to a minimum of [N]": the minimum applies only to
    // that discount (CR 356.4.e), so it limits how far ITS reduction goes and
    // never raises a cost that is already lower.
    std::vector<std::pair<int, int>> limited;   // (reduction, minimum)
    // Combat-active scope: a CostModifier with combat_active_only=true is
    // only consulted while combat is in progress on any battlefield.
    const bool combat_active = anyCombatInProgress(state);
    // The payer's own modifiers, then the OPPONENT's "enemy" modifiers, which
    // reach across to this player's plays.
    for (int side = 0; side < 2; ++side) {
        const bool own = side == 0;
        const auto& mods = own ? ps.cost_modifiers
                               : state.player(opponent(player)).cost_modifiers;
        for (const auto& mod : mods) {
            if (!costModifierApplies(state, mod, own, player, card, combat_active,
                                     def.energy_cost)) continue;
            increases += mod.energy_increase;
            if (mod.energy_reduction <= 0) continue;
            if (mod.min_cost > 0) limited.emplace_back(mod.energy_reduction, mod.min_cost);
            else free_reduction += mod.energy_reduction;
        }
    }
    // Self-cost reduction hook (e.g., Noxus Hopeful "[Legion] I cost [2] less").
    if (card_class) free_reduction += card_class->selfCostReduction(state, player);
    // Vendetta continuous spell-cost effects (set by cards' applyPassiveAura):
    // Helm of Suppression taxes the opponent's spells; Applied Researchers
    // discounts your own "to a minimum of [1]". Energy component only.
    if (card.isSpell()) {
        increases += ps.spell_tax_energy;
        if (ps.spell_discount_energy > 0) limited.emplace_back(ps.spell_discount_energy, 1);
    }
    // A per-target discount staged by executePlaySpell ("your spells that
    // choose me cost [1] less"), and what an additional cost took off.
    free_reduction += ps.transient_play_discount + adj.energy_reduction;

    int energy_needed = base_energy + increases;
    // Discounts may be applied in any order (CR 356.4.d.1); the limited ones
    // go first, which is never worse for the player.
    for (const auto& [reduction, minimum] : limited)
        energy_needed = std::min(energy_needed, std::max(minimum, energy_needed - reduction));
    energy_needed = std::max(0, energy_needed - free_reduction);   // CR 356.6
    const int power_needed =
        std::max(0, base_power + powerCostDelta(state, player, card, card_class) -
                        adj.power_reduction);
    return {energy_needed, power_needed};
}
}  // namespace

// (energy, power) `card_obj` costs `player` right now with `adj` applied.
// `base_energy` / `base_power` >= 0 replace the printed Base Cost (CR 356.1.a).
std::pair<int, int> GameEngine::netCardCost(PlayerId player, GameObjectId card_obj,
                                            const Card::PlayCostAdjust& adj,
                                            int base_energy, int base_power) const {
    const auto& card = state_.getObject(card_obj);
    if (card.card_def_id == kInvalidId) return {0, 0};
    const auto& def = card_db_.get(card.card_def_id);
    const EffectiveCardCost base{base_energy, base_power};
    const auto cost = effectiveCardCost(state_, def, card_registry_.get(card.card_def_id),
                                        player, card, adj, base_energy >= 0 ? &base : nullptr);
    return {cost.energy, cost.power};
}

// The play-legality gates that sit in front of every play, whatever it costs:
// the card's own "can't be played unless" and a name lock. Shared by canAfford
// and the play-from-trash generator, which charges a different cost.
bool GameEngine::passesPlayGates(PlayerId player, GameObjectId card_obj) const {
    if (!state_.objectExists(card_obj)) return false;
    const auto& gate_obj = state_.getObject(card_obj);
    if (gate_obj.card_def_id != kInvalidId) {
        if (const Card* gate_card = card_registry_.get(gate_obj.card_def_id))
            if (!gate_card->canBePlayed(state_, player)) return false;
    }
    // Fallen Feline: "opponents can't play spells with that name."
    if (gate_obj.isSpell()) {
        for (const auto& locked : state_.player(player).locked_spell_names)
            if (locked == gate_obj.name) return false;
    }
    return true;
}

bool GameEngine::canAfford(PlayerId player, GameObjectId card_obj, int extra_any_power) const {
    if (!passesPlayGates(player, card_obj)) return false;
    auto& card = state_.getObject(card_obj);
    if (card.card_def_id == kInvalidId) return false;  // tokens have no cost
    // An additional cost chosen before payment (CR 356): a mandatory one that
    // cannot be paid stops the play; one that can be paid counts toward
    // affordability at the most it can take off the cost.
    if (const Card* cc = card_registry_.get(card.card_def_id)) {
        const auto addl = cc->additionalPlayCost(state_, card_db_, player, card_obj);
        if (addl.exists) {
            if (addl.mandatory && !addl.payable) return false;
            if (addl.payable && canAffordWith(player, card_obj, extra_any_power, addl.best))
                return true;
            if (addl.mandatory) return false;
        }
    }
    return canAffordWith(player, card_obj, extra_any_power, Card::PlayCostAdjust{});
}

// Whether `player` has the Energy and Power for `card_obj` with `adj` taken
// off its cost. No legality gates: see canAfford.
bool GameEngine::canAffordWith(PlayerId player, GameObjectId card_obj, int extra_any_power,
                               const Card::PlayCostAdjust& adj) const {
    auto& card = state_.getObject(card_obj);
    if (card.card_def_id == kInvalidId) return false;
    const auto& def = card_db_.get(card.card_def_id);

    const auto cost = effectiveCardCost(state_, def, card_registry_.get(card.card_def_id),
                                        player, card, adj);
    const int energy_needed = cost.energy;
    const int power_needed = cost.power;

    // Count available runes in base, partitioned by ready/exhausted
    // and matching/non-matching-domain. The CR cost-payment ordering
    // (exhaust ready runes for energy, THEN recycle exhausted runes
    // for power) lets a single ready rune provide BOTH 1 energy and
    // 1 power: exhaust it in step 1, recycle it in step 2.
    auto base_loc = BaseLocation{player};
    int ready_total = 0;
    int matching_ready = 0;
    int matching_exhausted = 0;
    // (The player's base, from the board index: this is asked for every card
    // in hand at every decision — plans/06 item 6.5.)
    for (auto& [id, obj] : state_.objects.atLocation(LocationId{base_loc})) {
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
    const SpendFor spend_for = spendForObject(card);

    // Energy can come from the pool's existing energy (restricted Energy this
    // kind of card may use included) AND from exhausting ready runes.
    int energy_available = ready_total + pool.energy +
                           pool.restrictedEnergy(spend_for, showdownUnderway(state_));
    if (energy_available < energy_needed) return false;

    // Power can come from:
    //   • Pool's already-spent power for any of the card's domains
    //     (per-domain bucket).
    //   • Pool's universal power, and restricted [A] this kind of card may use.
    //   • Newly-recycled runes whose domains match the card. Recycle
    //     candidates = exhausted matching runes + matching runes we
    //     just exhausted in the energy step (the energy step exhausts
    //     `energy_needed` ready runes, and we're free to pick the
    //     matching-domain ones first when doing so).
    int power_from_pool = pool.universal_power + pool.restrictedPower(spend_for);
    for (auto d : def.domains) {
        int di = static_cast<int>(d);
        if (di >= 0 && di < static_cast<int>(Domain::Count)) {
            power_from_pool += pool.power[di];
        }
    }
    int power_remaining = std::max(0, power_needed - power_from_pool);
    // CR 164.2.b: a rune is recycled for Power whether it is ready or
    // exhausted, and a ready one can be exhausted for Energy first — so every
    // matching rune in base can pay Power, independently of the Energy check.
    if (matching_ready + matching_exhausted < power_remaining) return false;
    // "[A] more" extras: any rune in base can be recycled for them, on top of
    // the ones recycled for this card's own Power.
    const int extra_any = extraAnyPowerCost(state_, player, card) + extra_any_power;
    if (extra_any > 0) {
        const int any_available = resource::payablePower(state_, player, Domain::Count, spend_for) -
                                  power_needed;
        if (any_available < extra_any) return false;
    }
    return true;
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
    for (auto& [id, obj] : state.objects.atLocation(LocationId{base_loc})) {
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
    for (auto& [id, obj] : state.objects.atLocation(LocationId{base_loc})) {
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
    PlayerId player, GameObjectId card_obj, const Card::PlayCostAdjust& adj) {

    CostPaymentAdvance done;
    done.kind = CostPaymentAdvance::Kind::Done;
    done.deciding = player;

    auto& card = state_.getObject(card_obj);
    if (card.card_def_id == kInvalidId) {
        state_.cost_cursor.reset();
        return done;  // tokens cost nothing
    }
    const auto& def = card_db_.get(card.card_def_id);

    auto& ps = state_.player(player);
    const auto cost = effectiveCardCost(state_, def, card_registry_.get(card.card_def_id),
                                        player, card, adj);
    int energy_needed = cost.energy;
    int power_needed = cost.power;

    // ([Accelerate] a card on the board gives this play — Card::
    // grantsAccelerateTo — is decided with the rest of the play's choices,
    // before this payment: decideAccelerate / accelerateAvailable.)
    // Consume the one-shot modifiers that applied to this card ("your next
    // spell / unit / card costs ...") — from this player's list and, for the
    // ones that reach across, the opponent's. A one-shot that did not apply
    // (wrong card type, not in combat, ...) stays for the card it is meant for.
    {
        const bool combat_active = anyCombatInProgress(state_);
        for (int side = 0; side < 2; ++side) {
            const bool own = side == 0;
            auto& mods = own ? ps.cost_modifiers
                             : state_.player(opponent(player)).cost_modifiers;
            mods.erase(
                std::remove_if(mods.begin(), mods.end(),
                    [&](const PlayerState::CostModifier& m) {
                        if (!m.next_spell_only && !m.next_unit_only && !m.next_card_only)
                            return false;
                        return costModifierApplies(state_, m, own, player, card, combat_active,
                                                   def.energy_cost);
                    }),
                mods.end());
        }
    }

    RIFT_TRACE(events_, "PAY_COST: " + card.name + " E=" + std::to_string(energy_needed) +
                     " P=" + std::to_string(power_needed));

    // Spend pool energy first (no agent decision needed): Energy restricted to
    // this kind of card before unrestricted Energy.
    const SpendFor spend_for = spendForObject(card);
    ps.rune_pool.takeRestrictedEnergy(spend_for, showdownUnderway(state_), energy_needed);
    if (energy_needed > 0) {
        int from_pool = std::min(ps.rune_pool.energy, energy_needed);
        ps.rune_pool.energy -= from_pool;
        energy_needed -= from_pool;
    }

    // Spend floating Power the same way: restricted [A] this kind of card may
    // use, this card's domains, then [A].
    const int power_before_pool = power_needed;
    if (power_needed > 0) {
        ps.rune_pool.takeRestrictedPower(spend_for, power_needed);
        for (auto d : def.domains) {
            int di = static_cast<int>(d);
            if (di < 0 || di >= static_cast<int>(Domain::Count)) continue;
            int from_pool = std::min(ps.rune_pool.power[di], power_needed);
            ps.rune_pool.power[di] -= from_pool;
            power_needed -= from_pool;
        }
        int from_uni = std::min(ps.rune_pool.universal_power, power_needed);
        ps.rune_pool.universal_power -= from_uni;
        power_needed -= from_uni;
    }
    // Power from the pool is spent like a recycled rune's (Sivir, Mercenary;
    // testing phase I3).
    resource::notePowerSpent(ps, power_before_pool - power_needed);

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
        RIFT_TRACE(events_, "  EXHAUST_RUNE: " + rune.name + " (id=" +
                         toString(rune_id) +
                         ") for energy [agent choice]");
        rune.is_exhausted = true;
        events_.emit(ObjectStateChangedEvent{rune_id, "exhausted"});
        cur.energy_remaining--;
    } else if (cur.phase == Phase::PayingPower) {
        RIFT_TRACE(events_, "  RECYCLE_RUNE: " + rune.name + " (id=" +
                         toString(rune_id) + ", " +
                         (rune.is_exhausted ? "exhausted" : "ready") +
                         ") for power [agent choice]");
        auto base_loc = BaseLocation{cur.player};
        // Snapshot, move, announce, then the new object (CR 124.1) — the
        // order of every other way off the board.
        snapshotAsItLeaves(rune);
        rune.location = std::nullopt;
        rune.zone = ZoneType::RuneDeck;
        state_.player(cur.player).rune_deck.insert(
            state_.player(cur.player).rune_deck.begin(), rune_id);
        events_.emit(LeftBoardEvent{rune_id, cur.player, CardType::Rune,
            base_loc, ZoneType::RuneDeck, false});
        forgetBoardState(state_.getObject(rune_id), nullptr);
        cur.power_remaining--;
        state_.player(cur.player).power_spent_this_turn++;  // Sivir, Mercenary
    }

    return advanceCostPayment();
}

// What activating ability `index` of `source` costs in Energy and Power after
// reductions: the card's own and aura-computed ones (activationEnergyReduction)
// and, where the Energy reduction found nothing to take off, the Power-side
// alternative ("cost [1] or [A] less" — Card::activationPowerReductionFor).
ActivationCost GameEngine::netActivationCost(const Card& cost_card, PlayerId player,
                                             GameObjectId source, int index,
                                             ActivationCost cost) const {
    const int red = activationEnergyReduction(cost_card, player, source, index);
    const int unused = std::max(0, red - cost.energy);
    cost.energy = std::max(0, cost.energy - std::max(0, red));
    if (unused > 0 && cost.power > 0) {
        const int pred = cost_card.activationPowerReductionFor(state_, player, source, index,
                                                               unused);
        cost.power = std::max(0, cost.power - std::max(0, pred));
    }
    return cost;
}

bool GameEngine::canPayActivation(PlayerId player, const Card& cost_card, GameObjectId source,
                                  int index, const ActivationCost& printed) const {
    if (!state_.objectExists(source)) return false;
    const ActivationCost net = netActivationCost(cost_card, player, source, index, printed);
    return canPayAdditionalCost(player, net.energy, net.power,
        net.power_any_domain ? Domain::Count : net.power_domain,
        spendForObject(state_.getObject(source)));
}

// Extra (energy, power) charge on top of — or instead of — a card's base cost:
// Accelerate, Repeat, optional additional costs, Flow, activated abilities.
// `domain` is the power's Domain; Domain::Count means [A] (any domain, and the
// powers need not share one). The rule itself — floating resources, then
// runes exhausted for Energy and recycled for Power (CR 164.2) — is
// engine/resource_payment.h, shared with EffectExecutor::payAnyPower and the
// cards' own payment helpers.
//
// `for_unit` / `spend_for`: what the payment is for, so resources restricted
// to that purpose (RunePool::unit_energy, spell_power, ...) may be used.
bool GameEngine::canPayAdditionalCost(PlayerId player, int energy, int power,
                                       Domain domain, bool for_unit) const {
    return canPayAdditionalCost(player, energy, power, domain,
                                for_unit ? SpendFor::Unit : SpendFor::Other);
}

bool GameEngine::canPayAdditionalCost(PlayerId player, int energy, int power,
                                       Domain domain, SpendFor spend_for) const {
    return resource::canPay(state_, player, energy, power, domain, spend_for);
}

bool GameEngine::payAdditionalCost(PlayerId player, int energy, int power,
                                    Domain domain, bool for_unit) {
    return payAdditionalCost(player, energy, power, domain,
                             for_unit ? SpendFor::Unit : SpendFor::Other);
}

bool GameEngine::payAdditionalCost(PlayerId player, int energy, int power,
                                    Domain domain, SpendFor spend_for) {
    // The payer names the runes (testing phase D9).
    return effect_executor_->payResources(player, energy, power, resource::maskOf(domain),
                                          spend_for);
}


bool GameEngine::payTrashReplayGrant(PlayerId player, GameObjectId card_id,
                                     const std::vector<GameObjectId>& targets) {
    auto& ps = state_.player(player);
    auto it = std::find_if(ps.trash_replay_grants.begin(),
                           ps.trash_replay_grants.end(),
                           [&](const PlayerState::TrashReplayGrant& g) {
                               return g.card == card_id;
                           });
    if (it == ps.trash_replay_grants.end()) return false;
    const auto grant = *it;   // copy before erase

    // The grant's cost after increases and discounts (CR 356.3, 356.4) — the
    // same amounts generateTrashReplayActions checked (trashGrantCost).
    bool ok = state_.objectExists(card_id);
    if (ok) {
        // With an "or" discount from a target ([1] or [A] less), the Energy
        // half is taken when that leaves the cost payable, else the Power half.
        const SpendFor f = spendForObject(state_.getObject(card_id));
        auto cost = trashGrantCost(player, card_id, grant, targets, false);
        if (!resource::canPayIn(state_, player, cost.energy, cost.power, cost.domains, f))
            cost = trashGrantCost(player, card_id, grant, targets, true);
        ok = effect_executor_->payResources(player, cost.energy, cost.power, cost.domains, f);
        if (ok && cost.extra_any > 0)
            ok = effect_executor_->payResources(player, 0, cost.extra_any, resource::kAnyDomain, f);
    }
    if (ok && grant.banish_after && state_.objectExists(card_id))
        state_.getObject(card_id).banish_on_leave_chain = true;

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
        RIFT_TRACE(events_, "TRASH_REPLAY: " + std::string(toString(player)) +
                         " replayed card id=" + toString(card_id) +
                         " for [" + std::to_string(grant.energy) + "]+power");
    }
    return ok;
}

bool GameEngine::payCardCost(PlayerId player, GameObjectId card_obj,
                             const Card::PlayCostAdjust& adj) {
    // Bridge: legacy threaded path. Drives the cursor + queryAgent in
    // a loop. Same shape as the other commit-7 bridges
    // (runShowdownLoop, combatDamageStep). Step-machine dispatch
    // (commit 9) will use begin/advance/resolve directly.
    const int extra_any = state_.objectExists(card_obj)
        ? extraAnyPowerCost(state_, player, state_.getObject(card_obj)) : 0;
    auto adv = beginCostPayment(player, card_obj, adj);
    while (adv.kind == CostPaymentAdvance::Kind::NeedDecision) {
        // Always emit the decision snapshot (forced-choice visibility
        // — see prior commit). Trace label depends on phase.
        const char* label = state_.cost_cursor->phase ==
            GameState::CostPaymentCursor::Phase::PayingEnergy
            ? "pick rune to exhaust for energy"
            : "pick rune to recycle for power";
        state_.decision_index++;
        RIFT_TRACE_DECISION(events_, "DECISION #" +
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
        auto chosen = askAgent(player, adv.legal);
        if (on_decision) on_decision(state_, adv.legal, chosen);
        adv = resolveCostPaymentDecision(chosen);
    }
    if (extra_any > 0)
        effect_executor_->payAnyPower(player, extra_any,
            state_.objectExists(card_obj) ? spendForObject(state_.getObject(card_obj))
                                          : SpendFor::Other);
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
    // Its death was replaced when the batch it dies in began (CR 373.1.a,
    // EffectExecutor::beginDeathBatch): nothing to kill.
    if (unit.last_known.batch_replaced) return;
    auto controller = unit.controller;
    const bool replacements_settled = effect_executor_->deathReplacementsSettled(unit_id);
    // Last-known information for "When I die" (CR 383.2.c.3.b): captured now,
    // before gear is shed and the designation and statuses are cleared.
    effect_executor_->snapshotLastKnown(unit_id);
    RIFT_TRACE(events_, "KILL: " + unit.name + " (id=" + toString(unit_id) +
                     ", " + std::to_string(unit.current_might) + "M, dmg=" +
                     std::to_string(unit.damage_marked) + ")");

    // Everything that can replace this death — on the unit, on its
    // battlefield, among its controller's cards — in the order its controller
    // chooses (CR 372): EffectExecutor::applyDeathReplacements, shared with
    // EffectExecutor::killObject. A replaced unit keeps its gear on a recall
    // (CR 458, 719.3.a).
    // (Already done for a unit of a death batch, before any of them died.)
    if (!replacements_settled && effect_executor_->applyDeathReplacements(unit_id)) return;

    // No replacement — unit dies normally. Its gear is shed (CR 719.5) after
    // the death is announced below, so abilities that trigger on it see the
    // unit as it died.

    auto was_at = unit.location;
    int might = unit.current_might;

    // CR 183.1: tokens cease to exist when leaving the board. Route
    // them to Banishment (no trash entry) instead of trash. Matches
    // EffectExecutor::killObject — same rule, different kill path
    // (this one is combat lethal damage; killObject is for spell-
    // /ability-driven kills like Disintegrate).
    const bool is_token = unit.isToken();
    unit.zone = is_token ? ZoneType::Banishment : ZoneType::Trash;
    unit.last_location = unit.location;  // preserve for Deathknell triggers
    unit.location = std::nullopt;
    unit.damage_marked = 0;
    unit.combat_designation = CombatDesignation::None;
    if (!is_token) {
        state_.player(unit.owner).trash.push_back(unit_id);
        settleInTrash(state_, events_, unit_id);
    } else {
        RIFT_TRACE(events_, "  TOKEN CEASES TO EXIST (CR 183.1): " + unit.name +
                         " (id=" + toString(unit_id) + ")");
    }

    state_.turn.any_unit_died_this_turn = true;
    state_.player(controller).unit_died_this_turn = true;
    ++state_.player(controller).unit_deaths_this_turn;
    // Shadow Watcher: a friendly unit dying during its controller's Beginning
    // Phase (CR 315.2: the Beginning and Scoring Steps of that player's own
    // turn — isBeginningPhase; not the Awaken Phase, CR 315.1).
    if (isBeginningPhase(state_.turn.phase) && state_.turn.turn_player == controller) {
        state_.player(controller).unit_died_in_beginning_this_turn = true;
        ++state_.player(controller).unit_deaths_in_beginning_this_turn;
    }
    events_.emit(UnitDiedEvent{unit_id, controller,
        was_at.value_or(BaseLocation{controller}), might,
        state_.getObject(unit_id).last_known.killed_by});
    events_.emit(LeftBoardEvent{unit_id, controller, CardType::Unit,
        was_at.value_or(BaseLocation{controller}),
        is_token ? ZoneType::Banishment : ZoneType::Trash, true});
    detachAllGear(unit_id);
    // The death is announced and its triggers are queued (they read the unit
    // as it died from last_known): what is in the trash is a new object
    // (CR 124.1) — the same call EffectExecutor::killObject ends with.
    effect_executor_->leftBoard(unit_id);
}

void GameEngine::healAllUnits() {
    for (auto& [id, obj] : state_.objects.onBoard()) {
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
    // One token per decision, always: a replay consumes exactly one token at
    // each decision, so skipping one would shift every later decision. The
    // token identifies the Intent exactly (its slot plus a fingerprint of
    // every field) — see action_vocab.h.
    state_.action_history.push_back(
        ::riftbound::openspiel::historyToken(chosen, state_));
}

// ── The one place an agent is asked ─────────────────────────────────────────
// Every decision of the game — main phase, priority, showdown, mulligan, combat
// damage, a rune pick, a card's prompt — is asked here, and the answer is
// checked here before the engine acts on it or records it:
//
//   * It must be one of the options offered (Intent::operator==, every field
//     that carries meaning). An agent that hands back anything else — a
//     hand-built play, an ability of a card that is not in play, a MakeChoice
//     value that was not on the list — is a caller error: std::invalid_argument,
//     the same family as GameEngine::applyChoice's std::out_of_range for an
//     index outside the list. Nothing of the bad answer is executed. Cards
//     therefore never see an answer they did not offer.
//   * Concede is always accepted: a player may concede at any decision, and it
//     is what agents answer when they have nothing else (StepDriver on an empty
//     list, HumanAgent on shutdown).
//   * With nothing offered there is nothing to check an answer against.
//
// A caller that edits the state while the engine waits here (the play server's
// god mode, through mutableState()) is noticed (noteEditWhileParked). The
// options were built before the edit, so the answer is then held to the state
// as it is now: for a decision the engine can rebuild from the state alone
// (`kind`) it must be one of the REBUILT options — also when the caller swapped
// its list for them (recomputePendingLegalActions) — and for any other decision
// (a card's prompt, a cost, combat damage) it must be an offered option that
// names no object the edit moved, removed or handed to another player. A stale
// answer is refused the same way.
namespace {
void appendIntentObjects(const Intent& i, std::vector<GameObjectId>& out) {
    if (i.card != kInvalidId) out.push_back(i.card);
    if (i.ability_source != kInvalidId) out.push_back(i.ability_source);
    out.insert(out.end(), i.units_to_move.begin(), i.units_to_move.end());
    out.insert(out.end(), i.targets.begin(), i.targets.end());
    out.insert(out.end(), i.cards_to_mulligan.begin(), i.cards_to_mulligan.end());
    out.insert(out.end(), i.chosen_objects.begin(), i.chosen_objects.end());
    for (const auto& da : i.damage_assignments) out.push_back(da.target_unit);
}
bool offeredIn(const std::vector<Intent>& legal, const Intent& chosen) {
    return std::find(legal.begin(), legal.end(), chosen) != legal.end();
}
}  // namespace

GameEngine::ParkedRef GameEngine::parkedRefOf(GameObjectId id) const {
    ParkedRef r;
    r.id = id;
    r.exists = state_.objectExists(id);
    if (r.exists) {
        const auto& o = state_.getObject(id);
        r.zone = o.zone;
        r.controller = o.controller;
        r.location = o.location;
        r.hidden = o.is_hidden;
    }
    return r;
}

void GameEngine::noteEditWhileParked() {
    edited_while_parked_ = true;
    parked_refs_.clear();
    if (!parked_legal_ || pending_query_ != PendingQuery::None) return;
    // A decision that cannot be rebuilt: remember where every object its
    // options name is right now, before the edit lands.
    std::vector<GameObjectId> ids;
    for (const auto& option : *parked_legal_) appendIntentObjects(option, ids);
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    parked_refs_.reserve(ids.size());
    for (auto id : ids) parked_refs_.push_back(parkedRefOf(id));
}

Intent GameEngine::askAgent(PlayerId player, const std::vector<Intent>& legal,
                            PendingQuery kind) {
    pending_query_ = kind;
    pending_query_player_ = player;
    parked_legal_ = &legal;
    edited_while_parked_ = false;
    struct Unpark {   // also when the agent throws (a step fault, an abort)
        GameEngine& e;
        ~Unpark() {
            e.parked_legal_ = nullptr;
            e.pending_query_ = PendingQuery::None;
        }
    } unpark{*this};

    // A card's prompt (every option a MakeChoice) is answered by value, by
    // object or by a named card (Intent::chosen_card_def): that is all an
    // agent, a recorded game or a policy head has to tell the options apart.
    // An option that carries none of them, or two that carry the same, can
    // only be told apart by position — a card bug.
    if (legal.size() > 1 &&
        std::all_of(legal.begin(), legal.end(),
                    [](const Intent& i) { return i.type == IntentType::MakeChoice; })) {
        for (size_t a = 0; a < legal.size(); ++a) {
            const bool unnamed = !legal[a].chosen_value.has_value() &&
                                 legal[a].chosen_objects.empty() &&
                                 legal[a].chosen_card_def == kInvalidId;
            bool twin = false;
            for (size_t b = a + 1; b < legal.size() && !twin; ++b)
                twin = legal[a].chosen_value == legal[b].chosen_value &&
                       legal[a].chosen_objects == legal[b].chosen_objects &&
                       legal[a].chosen_card_def == legal[b].chosen_card_def;
            if (!unnamed && !twin) continue;
            std::string options;
            for (const auto& i : legal) {
                options += " [" + i.choice_label;
                if (i.chosen_value.has_value()) options += " value=" + std::to_string(*i.chosen_value);
                for (auto o : i.chosen_objects) {
                    options += " object=" + toString(o);
                    if (state_.objectExists(o)) options += "(" + state_.getObject(o).name + ")";
                }
                options += "]";
            }
            const std::string source = state_.chain.resuming.has_value() &&
                                       state_.objectExists(state_.chain.resuming->source)
                ? state_.getObject(state_.chain.resuming->source).name : std::string("?");
            throw std::logic_error(
                "GameEngine::askAgent: a prompt (resolving: " + source + ") has an option that " +
                (unnamed ? "carries neither a value nor an object"
                         : "is the same as another by value and object") +
                ". Options:" + options);
        }
    }

    Intent chosen = getAgent(player).selectAction(state_, legal);
    parked_legal_ = nullptr;
    // (A card's prompt: priority and Focus decisions are the chance to break
    // a loop, not what keeps one going — TR 505.3 against 505.5.)
    if (legal.size() > 1 && chosen.type == IntentType::MakeChoice) ++decisions_with_a_choice_;
    // A concession is accepted at every decision — and it ends the game at
    // every decision, here, not only where the caller happens to execute it
    // (it used to be ignored at Focus, at a prompt, in a payment: the showdown
    // loop asked again until its action bound closed the showdown).
    if (chosen.type == IntentType::Concede && !state_.game_over) {
        state_.game_over = true;
        state_.winner = opponent(player);
        state_.game_over_reason = toString(player) + std::string(" conceded");
        events_.emit(GameOverEvent{state_.winner, state_.game_over_reason});
    }

    auto refuse = [&](const std::string& why) {
        throw std::invalid_argument(
            "GameEngine: " + std::string(toString(player)) + " answered a decision with " +
            chosen.describe() + ", " + why + " (" + std::to_string(legal.size()) +
            " options were offered).");
    };
    if (chosen.type != IntentType::Concede && !legal.empty()) {
        if (!edited_while_parked_) {
            if (!offeredIn(legal, chosen)) refuse("which was not one of the options offered");
        } else if (auto fresh = recomputePendingLegalActions()) {
            if (!offeredIn(*fresh, chosen))
                refuse("which is not legal in the state as it was edited while the "
                       "decision was pending");
        } else {
            if (!offeredIn(legal, chosen)) refuse("which was not one of the options offered");
            std::vector<GameObjectId> ids;
            appendIntentObjects(chosen, ids);
            for (auto id : ids)
                for (const auto& before : parked_refs_)
                    if (before.id == id && !(before == parkedRefOf(id)))
                        refuse("which names an object that was edited while the decision "
                               "was pending");
        }
    }
    edited_while_parked_ = false;
    parked_refs_.clear();
    pending_query_ = PendingQuery::None;
    recordAppliedIntent(chosen);
    return chosen;
}

Intent GameEngine::queryAgent(PlayerId player) {
    auto actions = generateLegalActions();
    state_.decision_index++;
    if (player == PlayerId::Player1) state_.turn.turn_decisions_p1++;
    else state_.turn.turn_decisions_p2++;

    RIFT_TRACE_DECISION(events_, "DECISION #" + std::to_string(state_.decision_index) +
                     " (" + toString(player) + "): " +
                     std::to_string(actions.size()) + " legal actions");

    auto chosen = askAgent(player, actions, PendingQuery::MainPhase);

    // Whether the choice names a card the other player may not know.
    const auto names_hidden = [&] {
        std::vector<GameObjectId> named{chosen.card, chosen.ability_source};
        named.insert(named.end(), chosen.targets.begin(), chosen.targets.end());
        named.insert(named.end(), chosen.units_to_move.begin(), chosen.units_to_move.end());
        named.insert(named.end(), chosen.chosen_objects.begin(), chosen.chosen_objects.end());
        for (auto id : named)
            if (hiddenFrom(state_, id, opponent(player))) return true;
        return false;
    };
    if (!events_.logging()) {
        events_.noteUnheardLine();   // the CHOSE line below, unread
        if (names_hidden()) events_.noteUnheardLine();   // and its other player's copy
    } else {
        // Written twice when the choice names a card the other player may not
        // know (a card in hand being hidden or played, a facedown card): the
        // chooser's line names it, the other player's says "a hidden card"
        // (testing phase D11). Otherwise one line for both.
        const PlayerId other = opponent(player);
        bool redacted = false;
        auto describe = [&](bool for_other) {
            auto nameOf = [&](GameObjectId oid) -> std::string {
                if (!state_.objectExists(oid)) return "?" + toString(oid);
                if (for_other && hiddenFrom(state_, oid, other)) {
                    redacted = true;
                    return "(a hidden card)";
                }
                return state_.getObject(oid).name;
            };
            std::string choice_str = toString(chosen.type);
            if (chosen.card != kInvalidId && state_.objectExists(chosen.card))
                choice_str += " " + nameOf(chosen.card);
            if (chosen.ability_source != kInvalidId && state_.objectExists(chosen.ability_source))
                choice_str += " (src=" + nameOf(chosen.ability_source) + ")";
            if (!chosen.targets.empty()) {
                choice_str += " tgt=[";
                for (size_t i = 0; i < chosen.targets.size(); ++i) {
                    if (i > 0) choice_str += ",";
                    choice_str += nameOf(chosen.targets[i]);
                }
                choice_str += "]";
            }
            if (!chosen.units_to_move.empty()) {
                choice_str += " units=[";
                for (size_t i = 0; i < chosen.units_to_move.size(); ++i) {
                    if (i > 0) choice_str += ",";
                    choice_str += nameOf(chosen.units_to_move[i]);
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
                    const auto oid = chosen.chosen_objects[i];
                    choice_str += nameOf(oid) + "(id=" + toString(oid) + ")";
                }
                choice_str += "]";
            }
            if (chosen.chosen_battlefield != kInvalidId)
                choice_str += " bf=" + toString(chosen.chosen_battlefield);
            // Player tag at the front so the line scans independently of
            // the surrounding DECISION # context.
            return "CHOSE: [" + std::string(toString(player)) + "] " + choice_str;
        };
        const std::string full = describe(false);
        const std::string public_line = describe(true);
        if (!redacted) {
            RIFT_TRACE(events_, full);
        } else {
            RIFT_TRACE_FOR(events_, LogAudience::only(player), full);
            RIFT_TRACE_FOR(events_, LogAudience::only(other), public_line);
        }
    }

    if (on_decision) {
        on_decision(state_, actions, chosen);
    }
    return chosen;
}

AgentInterface& GameEngine::getAgent(PlayerId player) {
    return *agents_[playerIndex(player)];
}

BattlefieldState& GameEngine::getBattlefield(BattlefieldId id) {
    for (auto& bf : state_.battlefields) {
        if (bf.id == id) return bf;
    }
    throw std::runtime_error("Battlefield not found: " + toString(id));
}

const BattlefieldState& GameEngine::getBattlefield(BattlefieldId id) const {
    for (auto& bf : state_.battlefields) {
        if (bf.id == id) return bf;
    }
    throw std::runtime_error("Battlefield not found: " + toString(id));
}

// ═══════════════════════════════════════════════════════════════════════════════
// Equip/Attach (CR 716-725)
// ═══════════════════════════════════════════════════════════════════════════════

void GameEngine::detachAllGear(GameObjectId unit_id) {
    if (!state_.objectExists(unit_id)) return;
    // Through the executor so each gear's onUnattached hook runs (CR 719.5:
    // detached gear stays where it is).
    const auto attached = state_.getObject(unit_id).attachments;   // copy
    for (auto gear_id : attached) effect_executor_->unattachGear(gear_id);
    state_.getObject(unit_id).attachments.clear();
}

} // namespace riftbound
