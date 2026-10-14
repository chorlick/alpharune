#pragma once
/// @file game_engine.h
/// Main game engine — orchestrates the turn loop, phase sequencing,
/// and coordinates all subsystems.
///
/// The engine:
///   1. Sets up the game from two deck submissions
///   2. Runs the turn loop (awaken → scoring → channel → draw → main → end)
///   3. At each decision point, queries the agent for an action
///   4. Validates and executes actions
///   5. Emits events for subscribers (logger, renderer, future triggered abilities)
///   6. Detects game over conditions

#include "agents/agent_interface.h"
#include "cards/card_registry.h"
#include "core/card_db.h"
#include "core/events.h"
#include "core/game_state.h"
#include "core/intent.h"
#include "engine/chain_manager.h"
#include "engine/effect_executor.h"
#include "engine/resource_payment.h"
#include "engine/trigger_manager.h"
#include "engine/step_driver.h"
#include "rules/deck_validator.h"

#include <boost/signals2/connection.hpp>
#include <functional>
#include <memory>
#include <random>
#include <vector>

namespace riftbound {

/// Result of running a single game.
struct GameResult {
    PlayerId winner = PlayerId::None;
    int final_scores[2] = {0, 0};
    int total_turns = 0;
    int total_decisions = 0;
    std::string termination_reason;
};

// ─── Step machine ─────────────────────────────────────────────────────────────
// Pull-driven engine API: `beginGame()` + a `currentStep()` / `applyChoice()`
// loop owned by the caller, beside the push-driven `runGame(agent1, agent2)`.
// Both run the same game code. In step mode `runGame` executes inside a
// userspace fiber (step_driver.h) whose two agent seats yield to the caller at
// every decision — no OS thread, no condition variable. The OpenSpiel wrapper
// (RiftboundState) and the search agents drive the engine through it.

enum class StepKind : uint8_t {
    Done,            // Game terminated. state.game_over is true.
    NeedDecision,    // Engine is paused waiting on a player decision.
    // ChanceNode,   // Phase B-2 — random outcome required.
};

struct StepResult {
    StepKind kind = StepKind::Done;
    PlayerId perspective = PlayerId::None;  // who must decide
    std::vector<Intent> legal;              // populated when kind==NeedDecision
};

class GameEngine {
public:
    GameEngine(const CardDB& card_db, EventBus& event_bus,
               const CardRegistry& card_registry);
    ~GameEngine();
    GameEngine(const GameEngine&) = delete;
    GameEngine& operator=(const GameEngine&) = delete;

    /// Run a complete game. Returns the result.
    GameResult runGame(
        const DeckSubmission& deck1,
        const DeckSubmission& deck2,
        AgentInterface& agent1,
        AgentInterface& agent2,
        uint64_t seed = 0
    );

    // ── Step machine API ─────────────────────────────────────────────────────
    //
    // Pull-driven alternative to runGame(). Caller drives a loop:
    //
    //   StepResult sr = engine.beginGame(deck1, deck2, seed);
    //   while (sr.kind == StepKind::NeedDecision) {
    //       int idx = pick_one(sr.legal);
    //       sr = engine.applyChoice(idx);
    //   }
    //   const GameResult& r = engine.stepResult();
    //
    // Implemented on a fiber (see above): each `agent.selectAction(...)` inside
    // the engine suspends the fiber and returns control here; applyChoice
    // resumes it with the chosen option. The answer is always one of the
    // options offered (applyChoice takes an index into them).

    /// Begin a game in step-machine mode. Loads decks, initialises state,
    /// runs until the first NeedDecision or terminal.
    const StepResult& beginGame(
        const DeckSubmission& deck1,
        const DeckSubmission& deck2,
        uint64_t seed = 0);

    /// Resume a game mid-flight from a snapshotted GameState.
    ///
    /// Used by `RiftboundState::Clone()` to skip the replay path: instead
    /// of calling `beginGame` and re-applying `action_history` step-by-step
    /// (~5ms for a 100-action game), the clone constructs a fresh engine,
    /// substitutes the snapshotted GameState, and calls this method. The
    /// engine initialises subsystems (ChainManager, EffectExecutor,
    /// TriggerManager subscriptions on the supplied EventBus), then starts
    /// the engine fiber, which dispatches to the appropriate phase based
    /// on `state_.turn.phase` — no setup, no mulligans, no replay.
    ///
    /// Only a snapshot taken at a top-level main-phase decision can be
    /// resumed (`isResumableDecision`); anything else throws
    /// std::invalid_argument. Snapshots taken elsewhere — mulligan, chain
    /// priority, showdown, combat, cost payment, a card's play-time prompt
    /// — have to be rebuilt by `beginGame` + replaying the exact choices
    /// (`GameState::action_history`).
    ///
    /// `rng_state` is the snapshotted engine's `rngState()`. The RNG is
    /// part of the position: without it the resumed engine would shuffle
    /// differently from the engine it was copied from.
    ///
    /// `snapshot_step` is the decision the snapshot was taken at (who
    /// decides, and the legal list). It is how the engine tells a
    /// resumable snapshot from one that only looks like it, and the
    /// resumed engine is checked against it: std::logic_error if it does
    /// not arrive at the same decision.
    const StepResult& resumeFromSnapshot(GameState snapshot_state,
                                         const std::mt19937_64& rng_state,
                                         const StepResult& snapshot_step);

    /// True iff a snapshot of `state`, taken while an agent is being asked
    /// to choose from `legal`, can be handed to `resumeFromSnapshot`.
    static bool isResumableDecision(const GameState& state,
                                    const std::vector<Intent>& legal);

    /// The engine's RNG stream at this moment (for `resumeFromSnapshot`).
    const std::mt19937_64& rngState() const { return rng_; }

    /// Inspect what the engine is currently waiting on without advancing.
    /// Throws the engine's exception if the game failed (see applyChoice).
    ///
    /// Returns a view of the engine's own record of the pending decision,
    /// as do beginGame / resumeFromSnapshot / applyChoice: no copy of the
    /// legal list per call. The next applyChoice overwrites it — copy the
    /// StepResult (`auto step = ...`) to keep it across a choice.
    const StepResult& currentStep() const;

    /// Apply the chosen decision (index into the prior step's `legal`),
    /// then advance until the next NeedDecision or terminal. Returns the
    /// new step. Throws std::out_of_range for an index outside `legal`.
    ///
    /// If the engine throws while advancing, the exception is rethrown
    /// here, and again by every later applyChoice / currentStep: a game
    /// that failed is never reported as a finished one. `isStepDone()` is
    /// true afterwards and `stepResult().termination_reason` is
    /// "engine_error" with no winner.
    const StepResult& applyChoice(int legal_index);

    /// True iff the engine is no longer waiting on a decision: the game
    /// finished, or it failed (`stepFailed()`).
    bool isStepDone() const;

    /// True iff the step-machine game ended by an engine exception.
    bool stepFailed() const;

    /// Rethrows the engine's exception if `stepFailed()`; otherwise no-op.
    void rethrowIfStepFailed() const;

    /// After the game has terminated under the step-machine flow,
    /// the GameResult. Undefined before isStepDone() returns true.
    const GameResult& stepResult() const { return step_result_; }

    /// Access the current game state (for external inspection).
    const GameState& state() const { return state_; }

    /// Mutable access, for tests that build a position by hand and for the
    /// play server's god mode. Not for the OpenSpiel wrapper, BatchRunner or
    /// agents: `action_history` is written by the engine itself
    /// (`recordAppliedIntent`). Taking it while the engine is waiting on an
    /// agent's answer counts as an edit of the position that decision was
    /// offered for: the answer is then checked against the edited state (see
    /// askAgent in game_engine.cpp), and a stale one is refused with
    /// std::invalid_argument.
    GameState& mutableState() {
        if (parked_legal_ && !edited_while_parked_) noteEditWhileParked();
        forgetCleanupFacts();   // the caller may write anything
        return state_;
    }

    /// Generate all legal actions for the player who must currently act.
    std::vector<Intent> generateLegalActions() const;

    /// For a caller that edits the state while the engine waits at a decision
    /// (the play server's god mode): the options of that decision rebuilt
    /// from the state as it is now, so a stale intent is not applied. Only
    /// decisions whose options are a function of the state alone can be
    /// rebuilt — the main-phase decision, a showdown's focus decision and a
    /// chain priority decision; for every other one (a card's prompt, a cost
    /// payment, combat damage, the mulligan) and when no decision is pending
    /// this returns nullopt and the caller keeps the list it has. Any intent
    /// of the returned list may be handed back as the agent's answer.
    std::optional<std::vector<Intent>> recomputePendingLegalActions() const;

    /// True if `player` may NOT yet score `bf` because of a turn gate
    /// (Forgotten Monument): their turns_taken < bf.min_turn_to_score.
    /// Public so the renderer/UI and tests can query the same rule the
    /// scoring path uses.
    bool isScoreGatedByTurn(PlayerId player, BattlefieldId bf) const;

    /// Decision callback — called at every decision point with
    /// (state, legal_actions, chosen_action). Set this before runGame().
    using DecisionCallback = std::function<void(
        const GameState&,
        const std::vector<Intent>&,
        const Intent&
    )>;
    DecisionCallback on_decision;

    // ── Test seams ──
    // These are normally internal helpers but expose them for unit tests
    // that exercise specific CR clauses (e.g. 466.1.b winning-point cases)
    // without driving a full combat path.
    // `from`: who controlled the battlefield before it was conquered.
    void testHook_scoreConquer(PlayerId p, BattlefieldId bf,
                               std::optional<PlayerId> from = std::nullopt) {
        scoreConquer(p, bf, from);
    }
    void testHook_scoreHold(PlayerId p, BattlefieldId bf) { scoreHold(p, bf); }
    void testHook_drawCards(PlayerId p, int n) { drawCards(p, n); }
    void testHook_processLethalDamage() { processLethalDamage(); }
    /// The pre-play probe (payableThroughAFinalizeChoice): runs of the last
    /// search, the most of any search, and the GameState copies made so far.
    int testHook_prePlayRunsLast() const { return pre_play_runs_last_; }
    int testHook_prePlayRunsMax() const { return pre_play_runs_max_; }
    long testHook_prePlayStateCopies() const { return pre_play_state_copies_; }
    long testHook_prePlayProbes() const { return pre_play_probes_; }
    long testHook_prePlayRunsTotal() const { return pre_play_runs_total_; }
    void testHook_cleanup() { cleanup(); }
    /// For the registry sweep that checks CR 403 removal against what the
    /// card would have done: with `keep`, a triggered ability that is not
    /// performed stays on the chain and resolves, as before finding D4.
    void testHook_keepUnperformedTriggers(bool keep) { keep_unperformed_triggers_ = keep; }
    EffectExecutor& testHook_executor() { return *effect_executor_; }
    bool testHook_canAfford(PlayerId p, GameObjectId card) const { return canAfford(p, card); }
    bool testHook_payCardCost(PlayerId p, GameObjectId card) { return payCardCost(p, card); }
    bool testHook_canPayAdditionalCost(PlayerId p, int energy, int power, Domain d) const {
        return canPayAdditionalCost(p, energy, power, d);
    }
    bool testHook_payAdditionalCost(PlayerId p, int energy, int power, Domain d) {
        return payAdditionalCost(p, energy, power, d);
    }
    void testHook_awakenPhase() { awakenPhase(); }
    void testHook_beginningStep() { beginningStep(); }
    void testHook_scoringStep() { scoringStep(); }
    void testHook_executeStandardMove(const Intent& i) { executeStandardMove(i); }
    void testHook_drawPhase() { drawPhase(); }
    /// The transition from one turn to the next (whose turn, which number).
    void testHook_nextTurn() { advanceTurnQueue(); }
    void testHook_setupBattlefields(const DeckSubmission& d1, const DeckSubmission& d2) {
        setupBattlefields(d1, d2);
    }
    void testHook_channelPhase() { channelPhase(); }
    void testHook_endingStep() { endingStep(); }
    void testHook_expirationStep() { expirationStep(); }
    // Phase 6q+ engine-audit CRITICAL #2 + #3 — exposed so tests can
    // exercise the showdown / combat trigger drain + action-type
    // dispatch in isolation without driving a full turn.
    void testHook_runCombat(BattlefieldId bf) { runCombat(bf); }
    std::optional<PlayerId> testHook_resolveShowdownDecision(
        BattlefieldId bf, const Intent& chosen, PlayerId current_focus,
        int& action_count) {
        return resolveShowdownDecision(bf, chosen, current_focus, action_count);
    }
    // Executes `i` as given. It does NOT pass through askAgent, so the intent
    // is not checked against the legal list of any decision: a test that
    // hand-builds one is responsible for it being a play the engine would
    // offer. (An activation is still refused by payAndQueueAbility when its
    // source is not in play or its "use only if" gate fails.) For a test
    // that activates an ability this means: put the source on the board (or
    // in the Legend Zone) under intent.player's control and satisfy
    // Card::canActivateAbility / canActivateAbilityAt first — an activation
    // "from" a trash, a hand, or of another player's card does nothing.
    void testHook_executeIntent(const Intent& i) { executeIntent(i); }
    // Exactly ONE aura pass (cleanup repeats it until the state settles).
    void testHook_recalculateAuras() { recalculateAuras(); }
    // The top-level main-phase decision, asked of `p`'s agent.
    Intent testHook_queryAgent(PlayerId p) { return queryAgent(p); }
    // One pass of the main-phase loop up to its next decision: runs any
    // staged showdown / combat. The decision itself is discarded.
    void testHook_advanceMainPhase() { int n = 0; advanceMainPhase(n); }
    // Resolve whatever is on the chain (triggers queued by an event a test
    // raised outside a play), with priority passes and finalization as in a game.
    void testHook_runChain() { if (!state_.chain.items.empty()) runChain(); }
    // Install agents directly without going through run(). Required so
    // tests can exercise paths that invoke chain priority loops
    // (runChain → processFEPR → getAgent).
    void testHook_setAgents(AgentInterface* a1, AgentInterface* a2) {
        agents_[0] = a1;
        agents_[1] = a2;
    }

    // CR-legal combat damage allocations a real damage step would emit.
    // Exposed in public for unit tests; the struct + implementation
    // live in the combat-damage section below.
    struct CombatDamageAdvance {
        /// NeedDecision: `legal` is every distinct legal assignment, one
        /// decision. StepByStep: there are more than kMaxDamageOptions of them
        /// (or too many to count cheaply); the assignment is made one unit at a
        /// time (GameEngine::assignCombatDamage), so none is withheld.
        enum class Kind : uint8_t { NeedDecision, Skip, StepByStep };
        Kind kind = Kind::Skip;
        PlayerId assigner = PlayerId::None;
        std::vector<Intent> legal;
    };
    CombatDamageAdvance testHook_advanceCombatDamage(
        PlayerId assigner, int total_damage,
        const std::vector<GameObjectId>& targets);
    /// The assigner's whole decision (one list, or step by step), not dealt.
    std::optional<Intent> testHook_assignCombatDamage(
        PlayerId assigner, int total_damage, const std::vector<GameObjectId>& targets) {
        return assignCombatDamage(assigner, total_damage, targets);
    }

    /// The play gate for a spell choosing `targets`: its cost after what the
    /// targets take off, plus their Deflect, payable together. With no targets
    /// this is canAfford.
    bool canAffordChoosing(PlayerId player, GameObjectId card_id,
                           const std::vector<GameObjectId>& targets) const;
    /// The same, when some of the targets' Deflect is already paid: discounts
    /// are taken from all of `targets`, Deflect is owed for `unpaid_deflect`.
    bool canAffordChoosing(PlayerId player, GameObjectId card_id,
                           const std::vector<GameObjectId>& targets,
                           const std::vector<GameObjectId>& unpaid_deflect) const;
    /// Whether `player` can activate `gear`'s Equip on `unit` right now.
    bool canPayEquip(PlayerId player, GameObjectId gear, GameObjectId unit) const;
    /// Whether `player` can pay to hide a card right now.
    bool canPayHideCost(PlayerId player) const;

private:
    const CardDB& card_db_;
    EventBus& events_;
    GameState state_;
    std::mt19937_64 rng_;

    // Connection for the CardRevealedEvent subscriber installed in
    // the constructor. Stored so the destructor can disconnect — if
    // a session swaps engines on the same EventBus (the play-server
    // restart loop), leftover subscribers from the prior engine would
    // dereference a dead `this` when the new engine emits a reveal.
    boost::signals2::connection card_revealed_conn_;
    boost::signals2::connection trace_stderr_conn_;

    // Phase 2-3 subsystems
    std::unique_ptr<ChainManager> chain_manager_;
    std::unique_ptr<EffectExecutor> effect_executor_;
    std::unique_ptr<TriggerManager> trigger_manager_;

    // Phase 4: Card object system (shared, const after init)
    const CardRegistry& card_registry_;

    AgentInterface* agents_[2] = {nullptr, nullptr};

    // Which state-derived decision an agent is being asked right now (see
    // recomputePendingLegalActions). Not game state: it describes the call
    // stack, and is None whenever no agent is inside selectAction for one of
    // these three.
    enum class PendingQuery : uint8_t { None, MainPhase, Showdown, ChainPriority };
    PendingQuery pending_query_ = PendingQuery::None;
    PlayerId pending_query_player_ = PlayerId::None;

    // What askAgent is waiting on, and whether the state was edited meanwhile
    // (mutableState() during a decision). Like pending_query_, this describes
    // the call stack, not the game.
    struct ParkedRef {
        GameObjectId id = kInvalidId;
        bool exists = false;
        ZoneType zone = ZoneType::MainDeck;
        PlayerId controller = PlayerId::None;
        std::optional<LocationId> location;
        bool hidden = false;
        bool operator==(const ParkedRef&) const = default;
    };
    const std::vector<Intent>* parked_legal_ = nullptr;
    bool edited_while_parked_ = false;
    std::vector<ParkedRef> parked_refs_;
    ParkedRef parkedRefOf(GameObjectId id) const;
    void noteEditWhileParked();
    /// Ask `player`'s agent to choose from `legal`, check the answer is one
    /// of them (std::invalid_argument otherwise) and record it in
    /// action_history. Every decision goes through here. `kind`: which
    /// state-derived decision this is, if any (recomputePendingLegalActions).
    Intent askAgent(PlayerId player, const std::vector<Intent>& legal,
                    PendingQuery kind = PendingQuery::None);

    // Step machine state (Phase 11 C-1, fiber refactor 6w).
    // No more std::thread — the engine runs in a userspace fiber
    // owned by step_driver_. See step_driver.h for why (TL;DR:
    // branching CFR algorithms clone the state thousands of times
    // per decision and the OS thread limit gets exhausted).
    StepResult current_step_{};
    GameResult step_result_{};
    std::unique_ptr<StepDriver> step_driver_;

    /// Internal helper: rebuild current_step_ from the StepDriver's
    /// snapshot. Called after beginGame and after each applyChoice once
    /// the driver has either suspended at a decision or terminated.
    void refreshStepFromDriver();

    // ── Setup ──
    void setupGame(const DeckSubmission& deck1, const DeckSubmission& deck2);
    void setupPlayer(PlayerId player, const DeckSubmission& deck);
    void setupBattlefields(const DeckSubmission& deck1, const DeckSubmission& deck2);
    /// Who takes the next turn (CR 734-738) and the turn number.
    void advanceTurnQueue();
    void drawOpeningHands();
    void runMulligans();
    void determineTurnOrder();

    // ── Turn loop ──
    void runTurnLoop();
    void runTurn(PlayerId player);

    /// Internal — resume a turn that's already in progress (used by
    /// resumeFromSnapshot). Dispatches based on state_.turn.phase to
    /// the right starting phase, then runs to end of turn. After this
    /// returns, runTurnLoop's normal transition logic takes over.
    void runTurnFromPhase(PlayerId player, TurnPhase resume_at);

    void awakenPhase();
    void beginningStep();
    void scoringStep();
    void channelPhase();
    void drawPhase();
    void mainPhase();
    /// Loop-body of mainPhase() without the entry side effects (ns_state/
    /// oc_state/priority_holder reset, PhaseChangedEvent emit). Called by
    /// runTurnFromPhase when resuming a snapshot that was already mid-
    /// MainPhase — the snapshot already reflects the entry effects, so
    /// running them again would clobber it.
    void mainPhaseLoop(int action_count = 0);
    void endingStep();

    void expirationStep();

public:
    /// Pure helper: clear keywords that a GameObject obtained via a
    /// "this turn" grant (giveTemporaryKeyword), unless the keyword is
    /// also printed on the CardDef or currently granted by an active
    /// aura. Called per-object during the expiration step but exposed
    /// publicly so per-card tests can verify the revoke without
    /// spinning up the full engine subsystem stack.
    static void expireTemporaryKeywords(GameObject& obj, const CardDB& db);

    /// Restore control of any object whose "until <source> leaves the board"
    /// controlling source is no longer in play (Akshan, Mischievous). Run as
    /// part of cleanup; exposed publicly so per-card tests can verify the
    /// reversion without the full engine loop.
    void revertLapsedControl();
private:
    /// Body of the expiration step (heal, expire turn-buffs, empty rune
    /// pools, expire delayed abilities). Pulled out so expirationStep
    /// can loop the body per CR 317.2.f.1.
    void doExpirationBody();

    // ── Main phase: advance to the decision, then resolve it ──
    //
    // mainPhaseLoop() alternates the two halves with queryAgent() in the
    // middle: advanceMainPhase() runs to the next decision point,
    // resolveMainPhaseDecision() applies the chosen Intent and continues
    // through chain and cleanup. Showdowns, combats and the chain run inside
    // them and ask their own decisions (in step mode each one suspends the
    // engine fiber).
    struct MainPhaseAdvance {
        enum class Kind : uint8_t { NeedDecision, Done };
        Kind kind = Kind::Done;
        std::vector<Intent> legal;   // populated when kind==NeedDecision
    };
    /// Runs the per-iteration prelude (contested-BF processing, staged
    /// showdown/combat resolution, legal-action generation). Returns
    /// either NeedDecision (legal actions ready for the agent) or Done
    /// (main phase exiting — game over, action cap reached, or no
    /// legal actions).
    MainPhaseAdvance advanceMainPhase(int& action_count);
    /// Executes the chosen Intent, runs any resulting chain + cleanup,
    /// processes a second cleanup-trigger chain pass. Returns true if
    /// the main phase loop should continue; false on EndTurn or if the
    /// game ended during resolution.
    bool resolveMainPhaseDecision(const Intent& intent, int& action_count);

    // ── Showdown loop: advance to the focus decision, then resolve it ──
    //
    // Same shape as the main-phase split above; runShowdownLoop() is the
    // only caller and asks the focus holder in between.
    struct ShowdownAdvance {
        enum class Kind : uint8_t { NeedDecision, Done };
        Kind kind = Kind::Done;
        PlayerId focus_holder = PlayerId::None;
        std::vector<Intent> legal;   // populated when kind==NeedDecision
    };
    /// One iteration of the focus-passing loop's prelude. Sets focus +
    /// priority + open state for `focus_holder`, generates legal actions,
    /// returns Done if all players passed focus or the safety cap fired.
    ShowdownAdvance advanceShowdown(BattlefieldId bf_id, PlayerId focus_holder,
                                     int& action_count);
    /// Apply the chosen showdown intent (PassFocus or PlayActionCard).
    /// Returns the new focus holder if the showdown should continue, or
    /// std::nullopt to close the showdown.
    std::optional<PlayerId> resolveShowdownDecision(BattlefieldId bf_id,
                                                     const Intent& intent,
                                                     PlayerId current_focus,
                                                     int& action_count);

    // ── Combat damage assignment ──
    //
    // The damage step has TWO agent queries — one for each side's
    // assignment, asked one at a time by combatDamageStep. Each side's
    // split has the same advance/resolve shape:
    // advance computes the legal options, resolve applies the picked one.
    // CombatDamageAdvance is declared up in the public test-seams block
    // (so testHook_advanceCombatDamage can return it).
    /// Compute legal damage-distribution Intents for `assigner` to deal
    /// `total_damage` to `targets`. Returns Skip when nothing to assign
    /// (zero damage or no targets); else NeedDecision with the candidate
    /// distributions. Emits one Intent per CR-legal cascade ordering,
    /// deduped on the resulting marked-damage pattern.
    ///
    /// CR enforcement:
    ///   - 460.2.c.3 (strict lethality): each cascade assigns full lethal
    ///     to a target before any damage spills to the next.
    ///   - 460.2.c.4 (no over-assign): only the final assigned target
    ///     receives the trailing leftover.
    ///   - 815 (Tank first) + 826 (Backline last): bucket order is fixed;
    ///     within each bucket all permutations are enumerated (CR 460.2.c.6).
    CombatDamageAdvance advanceCombatDamage(PlayerId assigner,
                                             int total_damage,
                                             const std::vector<GameObjectId>& targets);
    /// Apply the chosen damage assignment. Mirrors what the in-place
    /// path does: emits per-unit DamageDealtEvent for each assignment.
    void resolveCombatDamageDecision(const Intent& intent);
    /// Ask `assigner` for their assignment (CR 467.2.c): one decision among
    /// every distinct legal assignment, or — when there are more than
    /// kMaxDamageOptions — one decision per unit, "which unit takes lethal
    /// damage next" in the tier order, the last one taking what is left.
    /// nullopt when there is nothing to assign.
    std::optional<Intent> assignCombatDamage(PlayerId assigner, int total_damage,
                                             const std::vector<GameObjectId>& targets);
    /// The units that can take `assigner`'s combat damage, in tier order
    /// (Tank, others, Backline; CR 815, 826, 460.2.c.6), ids ascending within
    /// a tier, with each one's tier.
    void combatDamageTiers(PlayerId assigner, const std::vector<GameObjectId>& targets,
                           std::vector<GameObjectId>& flat, std::vector<int>& tier_of) const;

    // ── Mulligan ──
    //
    // Per CR 118, each player gets one mulligan decision in turn order.
    // No reentrancy (a single decision per player, executed atomically
    // via executeMulligan).
    struct MulliganAdvance {
        enum class Kind : uint8_t { NeedDecision, Done };
        Kind kind = Kind::Done;
        PlayerId deciding = PlayerId::None;
        std::vector<Intent> legal;   // populated when kind==NeedDecision
    };
    /// Compute the mulligan options for `player`. Returns Done if the
    /// player has no mulligan choices (rare — usually means hand was
    /// drawn empty), else NeedDecision with all valid Mulligan intents
    /// (keep hand, mulligan 1, mulligan 2, etc.).
    MulliganAdvance advanceMulligan(PlayerId player);
    /// Apply the chosen mulligan intent. No-op if not a MulliganDecision.
    void resolveMulliganDecision(const Intent& chosen);

    // ── Cost payment ──
    //
    // payCardCost is a cursor-based state machine (GameState::cost_cursor):
    // it wraps begin / advance / resolve in a loop that asks the paying
    // player for each rune pick.
    struct CostPaymentAdvance {
        enum class Kind : uint8_t { NeedDecision, Done };
        Kind kind = Kind::Done;
        PlayerId deciding = PlayerId::None;
        std::vector<Intent> legal;   // populated when kind==NeedDecision
    };
    /// Initialise the cost cursor for `card_obj` and run the prelude
    /// (cost-modifier application + pool-energy spend). Returns Done
    /// if no agent decisions are needed (e.g., the entire cost was
    /// covered by pool energy + zero power); else NeedDecision with
    /// the first rune-pick options. Side effects: writes
    /// state_.cost_cursor; consumes one-shot CostModifier entries.
    CostPaymentAdvance beginCostPayment(PlayerId player, GameObjectId card_obj,
                                        const Card::PlayCostAdjust& adj = {});
    /// Inspect the cursor and produce the next NeedDecision (or Done
    /// if all payments are complete). Pure read of state — does NOT
    /// mutate runes. Used internally by resolveCostPaymentDecision
    /// after each rune pick to surface the next choice.
    CostPaymentAdvance advanceCostPayment();
    /// Apply the chosen rune pick (a MakeChoice intent with
    /// chosen_objects = {rune_id}). Updates rune state, decrements
    /// the cursor's remaining count, and returns the next decision
    /// (or Done when the cursor finishes). Asserts a cursor exists.
    CostPaymentAdvance resolveCostPaymentDecision(const Intent& chosen);

    // ── Actions ──
    void executeIntent(const Intent& intent);
    void executePlayCard(const Intent& intent);
    void executePlaySpell(const Intent& intent);
    /// The rest of a spell's play once its choices are made: payment, Repeat,
    /// the chain.
    void finishPlaySpell(const Intent& intent, std::vector<GameObjectId> play_targets,
                         bool target_chosen_at_play, std::vector<GameObjectId> cost_targets);
    void executeStandardMove(const Intent& intent);
    void executeMulligan(const Intent& intent);
    void executeAssignCombatDamage(const Intent& intent);
    void executePassFocus(const Intent& intent);
    void executeHideCard(const Intent& intent);
    void executePlayFromHidden(const Intent& intent);

    // ── Chain ──
    void runChain();
    void resolveSpell(const ChainItem& item);
    void resolvePermanent(const ChainItem& item);
    Intent queryAgentForChain(PlayerId player,
                               const std::vector<Intent>& actions);

    // ── Legal action generation ──
    std::vector<Intent> generateMainPhaseActions(PlayerId player) const;
    std::vector<Intent> generateMulliganActions(PlayerId player) const;
    std::vector<Intent> generateShowdownActions(PlayerId player) const;
    std::vector<Intent> generateClosedStateActions(PlayerId player) const;
    void generateSpellActions(PlayerId player, bool action_ok, bool reaction_ok,
                               std::vector<Intent>& actions) const;
    /// Emits Play intents (play_source=Trash) for spells the player has a
    /// PlayerState::TrashReplayGrant for (Death from Below). Mirrors
    /// generateSpellActions' timing gate + target shapes, but checks the
    /// grant's OVERRIDE cost for affordability rather than the printed cost.
    void generateTrashReplayActions(PlayerId player, bool action_ok,
                                     bool reaction_ok,
                                     std::vector<Intent>& actions) const;
    /// Plays of permanents that are themselves [Reaction] cards ([Reaction]
    /// units, [Quick-Draw] / [Reaction] gear) at Reaction or Action timing.
    void generateReactionPermanentActions(PlayerId player, IntentType intent_type,
                                          std::vector<Intent>& actions) const;
    /// Activated abilities `player` may use now — their own and those an aura
    /// grants them. `timing` filters by the ability's timing permission:
    /// Any in the Neutral Open State, Action in a showdown ([Action] or
    /// [Reaction], CR 813.1.b), Reaction in a Closed State; `intent_type` is
    /// the matching Activate* intent.
    enum class AbilityTiming : uint8_t { Any, Action, Reaction };
    void generateActivateAbilityActions(PlayerId player,
                                         std::vector<Intent>& actions,
                                         AbilityTiming timing = AbilityTiming::Any,
                                         IntentType intent_type = IntentType::ActivateAbility) const;
    /// Plays of `player`'s facedown cards (CR 811): from the turn after it was
    /// hidden a card has [Reaction], so it is offered in Closed States and in
    /// showdowns alike. `intent_type` as for generateReactionPermanentActions.
    void generateHiddenPlayActions(PlayerId player, IntentType intent_type,
                                   std::vector<Intent>& actions) const;
    /// Where `unit` (ready, controlled by `player`) may go with a Standard
    /// Move right now (CR 144.4). Empty when it cannot move.
    std::vector<LocationId> standardMoveDestinations(const GameObject& unit,
                                                     PlayerId player) const;
    /// Battlefields where exactly one unit stands and it is an enemy of `player`.
    bool enemyUnitAloneAt(PlayerId player, BattlefieldId bf) const;
    /// Whether `player` may play the unit `card` to `bf` by the ordinary rule
    /// (a battlefield they control, CR 355.2) or a permission the card prints
    /// or another card grants the player. Paid plays ("pay ... to play it
    /// here") are the caller's. `unit` (the card being played) adds what its
    /// [Ambush] permits: a battlefield where `player` has units (CR 822.1.b),
    /// widened by Card::ambushToEnemyBattlefields (822.1.d) — at any timing
    /// the play is offered at; the [Reaction] half is the Ambush generators'.
    bool unitPlayableAt(const Card* card_obj, PlayerId player,
                        const BattlefieldState& bf,
                        const GameObject* unit = nullptr) const;
    /// [Ambush]'s location (CR 822.1.b, 822.1.d): `bf` has units of `player`,
    /// or — for a card that widens it — enemy units.
    bool ambushLocation(const Card* card_obj, PlayerId player,
                        const BattlefieldState& bf) const;
    /// The cards `player` may play a unit from as from a hand: the hand and
    /// the Chosen Champion in its zone (CR 420.1.a).
    std::vector<GameObjectId> handAndChampion(PlayerId player) const;

    // ── Queries ──
    Intent queryAgent(PlayerId player);

    /// Append the chosen Intent's history token (action_vocab.h) to
    /// GameState::action_history. Called by askAgent, the one selectAction
    /// dispatch site, in both runGame and step-machine games, so the history is the
    /// one exact record of the decisions applied: replaying it from the
    /// same decks and seed rebuilds this game (RiftboundState, MctsAgent).
    /// This is the only writer.
    void recordAppliedIntent(const Intent& chosen);
    AgentInterface& getAgent(PlayerId player);

    // ── Board operations ──
    GameObjectId instantiateCard(CardDefId def_id, PlayerId owner);
    void drawCards(PlayerId player, int count);
    void channelRunes(PlayerId player, int count);
    void shuffleDeck(PlayerId player);
    void shuffleRuneDeck(PlayerId player);

    // ── Aura recalculation (Phase 5b) ──
    void recalculateAuras();

    // ── Cleanup ──
    void cleanup();
    bool checkWinCondition();
    void processLethalDamage();
    void updateBattlefieldControl();
    void processContestedBattlefields();

    // ── Combat ──
    void runShowdown(BattlefieldId bf);
    void runShowdownLoop(BattlefieldId bf);
    void runCombat(BattlefieldId bf);
    void combatDamageStep(BattlefieldId bf);
    void combatResolutionStep(BattlefieldId bf);

    // ── Scoring ──
    /// `previous_controller`: who controlled `bf` before `player` took it
    /// (nullopt = it was uncontrolled).
    void scoreConquer(PlayerId player, BattlefieldId bf,
                      std::optional<PlayerId> previous_controller = std::nullopt);
    void scoreHold(PlayerId player, BattlefieldId bf);
    bool isWinningPointAttempt(PlayerId player) const;
    bool canGainWinningPointViaConquer(PlayerId player) const;

    // ── Cost payment ──
    /// Check if a player can afford a card's cost with available runes.
    /// `extra_any_power`: [A] owed on top of everything else (a battlefield's
    /// pay-to-play-here cost).
    bool canAfford(PlayerId player, GameObjectId card_obj, int extra_any_power = 0) const;
    /// The play-legality gates in front of every play (Card::canBePlayed, a
    /// spell-name lock), without the cost.
    bool passesPlayGates(PlayerId player, GameObjectId card_obj) const;
    /// The Energy / Power half of canAfford, with `adj` taken off the cost.
    bool canAffordWith(PlayerId player, GameObjectId card_obj, int extra_any_power,
                       const Card::PlayCostAdjust& adj) const;

    /// Auto-pay a card's cost by exhausting/recycling runes.
    /// Returns false if payment failed (shouldn't happen if canAfford was checked).
    bool payCardCost(PlayerId player, GameObjectId card_obj,
                     const Card::PlayCostAdjust& adj = {});
    /// Choose and pay the additional cost `card_id` prints, before its cost.
    /// Card::forgetPlayPayments for `card_id`, as a play of it begins.
    void forgetPlayPayments(PlayerId player, GameObjectId card_id);
    Card::PlayCostAdjust payAdditionalPlayCosts(PlayerId player, GameObjectId card_id,
                                                int extra_any_power);
    /// What a spell's chosen targets take off its cost.
    Card::PlayCostAdjust targetCostDiscount(PlayerId player,
                                            const std::vector<GameObjectId>& targets,
                                            bool prefer_power, bool* has_choice) const;
    std::pair<int, int> netCardCost(PlayerId player, GameObjectId card_obj,
                                    const Card::PlayCostAdjust& adj,
                                    int base_energy = -1, int base_power = -1) const;
    struct TrashGrantCost {
        int energy = 0;
        int power = 0;
        resource::DomainMask domains = resource::kAnyDomain;
        int extra_any = 0;
    };
    /// What a grant play costs with `targets` chosen: the grant's cost after
    /// increases and discounts (CR 356.3-4), those from the chosen targets
    /// included ("your spells that choose me cost [1] or [A] less" — the
    /// Power half when `prefer_power`).
    TrashGrantCost trashGrantCost(PlayerId player, GameObjectId card_id,
                                  const PlayerState::TrashReplayGrant& grant,
                                  const std::vector<GameObjectId>& targets,
                                  bool prefer_power) const;
    /// Whether the grant's cost and the targets' Deflect can be paid together
    /// (with either half of an "or" discount). The one gate for the
    /// trash-replay generator and the play-time target choice.
    bool canPayTrashGrant(PlayerId player, GameObjectId card_id,
                          const PlayerState::TrashReplayGrant& grant,
                          const std::vector<GameObjectId>& targets) const;
    /// The same, when some of the targets' Deflect is already paid.
    bool canPayTrashGrant(PlayerId player, GameObjectId card_id,
                          const PlayerState::TrashReplayGrant& grant,
                          const std::vector<GameObjectId>& targets,
                          const std::vector<GameObjectId>& unpaid_deflect) const;
    /// The standing or pushed grant for `card_id`, if `player` has one.
    const PlayerState::TrashReplayGrant* trashGrantFor(PlayerId player,
                                                       GameObjectId card_id) const;
    Card::HideCost hideCostFor(PlayerId player) const;
    int firstGearAbilityDiscount(PlayerId player) const;
    Card::EquipTerms equipTerms(PlayerId player) const;

    /// Additional-cost helpers (CR 805 Accelerate, future additional costs).
    /// `energy` is paid by exhausting ready runes; `power` is paid by
    /// recycling runes whose `domains` include `domain`. The two pools may
    /// overlap (a ready domain-matching rune may be either exhausted for
    /// energy OR recycled for power — preference is to recycle exhausted
    /// matching runes first to preserve ready energy). Auto-pays; no agent
    /// interaction. The caller is responsible for any other side effects
    /// (e.g. flipping `is_exhausted` on a unit that paid Accelerate to
    /// enter ready — that's resolvePermanent's job).
    /// `printed` after every activation-cost reduction, Energy and Power.
    ActivationCost netActivationCost(const Card& cost_card, PlayerId player,
                                     GameObjectId source, int index,
                                     ActivationCost printed) const;
    /// Whether `player` can pay the Energy and Power of ability `index` of
    /// `source` (whose descriptor comes from `cost_card`) after reductions,
    /// using resources restricted to that kind of source. The one predicate
    /// for the ability generators and payAndQueueAbility.
    bool canPayActivation(PlayerId player, const Card& cost_card, GameObjectId source,
                          int index, const ActivationCost& printed) const;
    /// Apply `assigned` combat damage to a unit through the replacement
    /// effects that govern it (CR 467.2.c.5).
    void applyCombatDamage(GameObjectId target, int assigned);
    /// Whether `damager`'s damage kills in any amount (CR 142.4.c).
    bool anyDamageIsLethalFor(PlayerId damager) const;
    int activationEnergyReduction(const Card& card, PlayerId player,
                                  GameObjectId source, int index) const;
    /// Make the choices the rules place at finalization for `item`: those of
    /// its (first) execution and, for a spell with Repeat paid, those of each
    /// additional execution (CR 820.2).
    void chooseTargetsAtFinalize(ChainItem& item);
    void chooseFirstExecution(ChainItem& item);
    // (An item whose choices are made early — a spell an effect plays — is
    // marked ChainItemStatus::Finalized on the spot, which is what keeps the
    // chain's Finalize step from asking again.)
    // The guard against the repeat choices choosing repeats. Call-stack
    // bookkeeping, not game state.
    bool choosing_repeat_ = false;
    /// Deflect, "chosen" events and the choice count for a target the engine
    /// asked for (chooseFirstExecution, makeNewChoicesFor).
    void commitEngineChosenTarget(const Card& card, PlayerId chooser, GameObjectId t,
                                  ChainItemId item_id);
    void runFinalizePass(ChainItem& item);
    bool keep_unperformed_triggers_ = false;   // testHook_keepUnperformedTriggers
    // Decisions with more than one option asked so far (askAgent), and the
    // count as the chain's loop last started: a chain that reaches its bound
    // with no such decision in between is a loop of mandatory actions.
    uint64_t decisions_with_a_choice_ = 0;
    uint64_t chain_run_choices_at_start_ = 0;
    // The chain's next id as the spell being played began its play
    // (executePlaySpell -> finishPlaySpell; placeBelowWhatItsPlayTriggered).
    ChainItemId spell_play_first_new_item_ = kInvalidId;
    /// CR 383.3.d: before the oldest pending item is finalized, the batch of
    /// simultaneous triggers it belongs to is ordered — the turn player's
    /// abilities first (the attacker's in a combat-start batch, CR 466.2.d.1),
    /// each player choosing the order of their own when it can differ.
    void orderPendingTriggers();
    /// CR 355.1.a / 805: the choice to pay [Accelerate] is made before the
    /// unit's cost is paid, and the cost is paid with it. Returns the cost
    /// adjustment to pay with (the Accelerate cost added when chosen) and
    /// records the decision on the card for resolvePermanent.
    /// Whether the unit `card_id` has [Accelerate] for this play: its own
    /// keyword, or one a card on `player`'s board gives a unit played from
    /// where this one is (`from_hand`; Card::grantsAccelerateTo).
    bool accelerateAvailable(PlayerId player, GameObjectId card_id, bool from_hand) const;
    /// The [1][C] of [Accelerate] after "optional additional costs you pay
    /// cost [1] or [A] less": the (energy, power) to pay, the payer choosing
    /// the half; nullopt if neither is payable.
    std::optional<std::pair<int, int>> chooseAccelerateHalf(
        PlayerId player, const std::function<bool(int, int)>& payable);
    bool chooseAccelerateHalfPayable(PlayerId player,
                                     const std::function<bool(int, int)>& payable) const;
    bool askAccelerate(PlayerId player, const GameObject& card);
    /// [Accelerate] for a play that pays no cost of its own first (from
    /// facedown, "ignoring its cost", a unit with no Domain): offered if
    /// payable, paid on its own. True if paid (the unit enters ready).
    bool offerAccelerateOnItsOwn(PlayerId player, GameObjectId card_id, bool from_hand);
    Card::PlayCostAdjust decideAccelerate(PlayerId player, GameObjectId card_id,
                                          const Card::PlayCostAdjust& prepay,
                                          int extra_any_power);
    /// The play is announced (CardPlayedEvent, "played not from hand") once
    /// the permanent has entered the board (CR 383.4.a.2).
    void announcePermanentPlayed(GameObjectId card_id, PlayerId player);
    /// Puts chain item `item_id` below every item queued since `first_new`
    /// (CR 354: the card went on the chain as its play began, so what paying
    /// for it triggered is newer and resolves first).
    void placeBelowWhatItsPlayTriggered(ChainItemId item_id, ChainItemId first_new);
    /// The finalize pass itself: runs `item`'s callback with `finalizing` set
    /// until it asks nothing more, and returns the item with its choices.
    /// `answer`, when given, answers the pass's prompts instead of the agents
    /// (prompt index, player, options); returning nothing stops the pass there
    /// (`*completed` = false) with whatever it did so far left in the state.
    using FinalizeAnswerer =
        std::function<std::optional<Intent>(size_t, PlayerId, const std::vector<Intent>&)>;
    ChainItem collectFinalizeChoices(ChainItem item, const FinalizeAnswerer* answer = nullptr,
                                     bool* completed = nullptr);

    // ── A spell's choices made before its cost is paid (CR 355 -> 356) ──
    /// What a pre-play pass is judged by: can the spell be paid for with
    /// these objects chosen (their Deflect already paid)?
    using PrePlayPayable = std::function<bool(const std::vector<GameObjectId>&)>;
    /// One run of `pre`'s finalize pass from the state `from`, answering
    /// prompt k with option `script[k]`; past the script it either stops
    /// (`stop_after_script`: the next prompt is returned) or takes option 0.
    struct ScriptedPass {
        bool completed = false;
        ChainItem done;
        std::vector<int> option_counts;      // options of each prompt met
        // For each prompt met, the options that name an object with which the
        // spell is payable at that point (filled for a search's runs only).
        std::vector<std::vector<int>> discount_options;
        std::vector<Intent> next_options;    // the first unscripted prompt, if stopped there
        PlayerId next_player = PlayerId::None;
    };
    ScriptedPass runScriptedPass(const GameState& from, const ChainItem& pre,
                                 const std::vector<int>& script, bool stop_after_script);
    /// The same run from the state as it stands (the caller has put it at the
    /// start of the pass): no copy of the state is made.
    ScriptedPass runScriptedPassHere(const ChainItem& pre, const std::vector<int>& script,
                                     bool stop_after_script, bool note_discounts = false);
    /// Puts `state_` back at the start of the pass for another run.
    using ToPassStart = std::function<void()>;
    /// Whether some continuation of `script` completes the pass and leaves
    /// the spell payable. `at_start`: the state already is at the start of
    /// the pass, so the first run needs no reset. Leaves `state_` as its last
    /// run left it — the caller puts back what it needs (plans/09 item
    /// 9.HH4: each reset is a copy of the whole GameState). The sequences are
    /// walked exhaustively, those that choose an object that makes the spell
    /// payable first (9.II1): where a payable sequence exists it is
    /// found in a few runs however many other objects a prompt offers.
    /// Nothing it does
    /// is seen: no event, no log line, no agent asked. Bounded: after
    /// kPrePlayProbeRuns runs with sequences left and none found it throws a
    /// std::logic_error naming the card (the answer is unknown; "none" would
    /// withhold a play). A throw of the card's pass (a contract error) is
    /// passed on.
    bool prePlaySequenceExists(const ToPassStart& to_start, bool at_start, const ChainItem& pre,
                               const std::vector<int>& script, const PrePlayPayable& payable);
    /// The real pass for a spell that is too dear on its own: at every prompt
    /// only the options from which a payable completion exists are offered,
    /// so the play, once offered, cannot end unpaid. Requires the pass to be
    /// repeatable (same state and answers -> same prompts); throws a
    /// std::logic_error naming the card when it is not.
    ChainItem choosePrePlayViable(const ChainItem& pre, const PrePlayPayable& payable);
    /// The generators' question: with `card_id` leaving its zone now, is
    /// there a complete sequence of its choices that leaves it payable?
    bool prePlayProbe(PlayerId player, GameObjectId card_id,
                      const PlayerState::TrashReplayGrant* grant);
    /// Set once the turn loop of a game is running (runGame, the step machine,
    /// a resumed snapshot): the state is then the engine's own work and not a
    /// position a test built by hand. Read by the checked build's assertions.
    bool playing_a_game_ = false;

    // ── What the last Cleanup established (see cleanup()) ──
    // Both are facts about the state as the engine left it; whoever replaces
    // or edits the state from outside (mutableState, a snapshot, a probe's
    // copy) calls forgetCleanupFacts and the next Cleanup starts from nothing.
    //
    // piles_inert_: every object in a pile (a Main Deck, a Rune Deck, a
    // Trash, Banishment) is known to carry nothing a Cleanup reads or resets
    // — no designation, no lapsing control, no continuous effect. A Cleanup
    // then walks ObjectTable::offPiles() where it used to walk every object.
    bool piles_inert_ = false;
    // The last aura pass ran the hook of a card whose effect reaches the
    // piles (Card::continuousEffectReachesPiles): piles_inert_ is not
    // established while it is in play.
    bool piles_reached_ = false;
    // Objects that are not in play (a hand, the chain, a Champion Zone) and
    // were given a continuous effect by the last aura pass: if one of them
    // is in a pile by the next pass, that pass still clears it.
    std::vector<GameObjectId> aura_outside_play_;
    // The last Cleanup found nothing to do and announced nothing but
    // StateSettled, to which nothing reacted; `quiet_cleanup_activity_` is
    // EventBus::activity() as it ended and `quiet_cleanup_shape_` what
    // cleanupShape() said then.
    bool quiet_cleanup_valid_ = false;
    uint64_t quiet_cleanup_activity_ = 0;
    uint64_t quiet_cleanup_shape_ = 0;
    /// A digest of what a Cleanup reads besides the objects and is written by
    /// the engine's flow without necessarily being announced: the chain's
    /// size, the turn's player, phase and states, each battlefield's
    /// controller and showdown / combat flags, the scores, the object count.
    /// Part of the "nothing happened since" test of cleanup() (2), so that
    /// an unannounced write of one of these never lets a Cleanup be skipped.
    uint64_t cleanupShape() const;
    void forgetCleanupFacts() {
        piles_inert_ = false;
        piles_reached_ = false;
        quiet_cleanup_valid_ = false;
        aura_outside_play_.clear();
    }
    /// Every object a Cleanup has to look at: all of them, or the ones that
    /// are not in a pile once piles_inert_ is established.
    template <class F> void forEachCleanupObject(F&& fn);
    /// Whether a pile object is as a Cleanup would leave it (piles_inert_).
    static bool inertInAPile(const GameObject& obj);
    /// Checked builds: throws if an object in a pile is not inert.
    void verifyPilesInert(const char* where) const;
    static constexpr int kPrePlayProbeRuns = 256;
    /// Runs of the last prePlaySequenceExists search, and the most any search
    /// of this engine has made (tests: how near a card comes to the bound).
    int pre_play_runs_last_ = 0;
    int pre_play_runs_max_ = 0;
    long pre_play_state_copies_ = 0;   // GameState copies made for probes and viable passes
    long pre_play_probes_ = 0;         // prePlayProbe calls (the generators' question)
    long pre_play_runs_total_ = 0;     // finalize-pass runs made by all searches
    /// Whether a spell that chooses in its finalize pass could be paid for by
    /// choosing something that takes enough off its cost (the generators'
    /// half of executePlaySpell's choices-before-payment).
    bool payableThroughAFinalizeChoice(PlayerId player, GameObjectId card_id,
                                       const PlayerState::TrashReplayGrant* grant) const;
    /// A spell's finalize-pass choices made before its cost was paid; adopted
    /// by chooseTargetsAtFinalize when the spell goes on the chain.
    std::optional<ChainItem> prefinalized_spell_;
    /// "You may make new choices for it" (CR 750-755).
    void makeNewChoicesFor(ChainItemId item_id);
    /// One agent decision among `options` for `player`, logged like any other.
    Intent decideNow(PlayerId player, const std::vector<Intent>& options, const std::string& what);
    /// Pay an activated ability's costs and add it to the chain (no chain run).
    bool payAndQueueAbility(const Intent& intent);
    /// CR 323.6: recall unattached gear at battlefields and permanents / runes
    /// sitting in a base other than their controller's.
    void recallMisplacedObjects();
    /// (Re)create the chain manager, effect executor and trigger manager bound
    /// to state_ / events_.
    void initSubsystems();
    /// Rebuild the standing [Flow] play-from-trash grants (called from recalculateAuras).
    void refreshFlowGrants();
    bool canPayAdditionalCost(PlayerId player, int energy, int power,
                                Domain domain, bool for_unit = false) const;
    bool payAdditionalCost(PlayerId player, int energy, int power,
                            Domain domain, bool for_unit = false);
    bool canPayAdditionalCost(PlayerId player, int energy, int power,
                              Domain domain, SpendFor spend_for) const;
    bool payAdditionalCost(PlayerId player, int energy, int power,
                           Domain domain, SpendFor spend_for);

    /// Pays the matching PlayerState::TrashReplayGrant's override cost for
    /// `card_id` (auto-spends via payAdditionalCost; an [A] grant picks the
    /// first payable domain) and consumes the grant. Returns true if a grant
    /// was found and paid, false otherwise (caller falls back to printed cost).
    bool payTrashReplayGrant(PlayerId player, GameObjectId card_id,
                             const std::vector<GameObjectId>& targets = {});

    // ── Optional additional costs (CR 356.2.b): Accelerate, Repeat, "you may pay" ──
    /// "Optional additional costs you pay cost [1] or [A] less": the discount
    /// `player`'s cards in play give (Card::optionalCostDiscount).
    int optionalCostDiscount(PlayerId player) const;
    bool canPayOptionalCost(PlayerId player, int energy, int power,
                            resource::DomainMask domains, SpendFor spend_for) const;
    bool payOptionalCost(PlayerId player, int energy, int power,
                         resource::DomainMask domains, SpendFor spend_for);

    /// "You may pay X as an additional cost to play me." Offers the agent a
    /// yes/no during the play action (called from executePlayCard /
    /// executePlaySpell after the base cost is paid, before CardPlayedEvent).
    /// On accept, pays the card's Card::optionalAdditionalCost and sets
    /// card_counters[paid_flag] = 1. No-op if the card has none or can't pay.
    /// `extra_any_power` / `base`: what is still to be paid for the card
    /// itself afterwards — the optional cost is offered only if that stays
    /// payable (the choice comes before any payment, CR 355.1.a).
    void maybePayOptionalAdditionalCost(PlayerId player, GameObjectId card_id,
                                        int extra_any_power,
                                        const Card::PlayCostAdjust& base);

    /// Count available Power of a specific domain (recycle potential).
    int availablePower(PlayerId player, Domain domain) const;

    /// Count available universal Power [A] (from recycling any rune).
    int availableAnyPower(PlayerId player) const;

    // ── Utilities ──
    void moveUnit(GameObjectId unit, LocationId destination);
    void killUnit(GameObjectId unit);
    void healAllUnits();
    void emptyRunePools();
    BattlefieldState& getBattlefield(BattlefieldId id);
    const BattlefieldState& getBattlefield(BattlefieldId id) const;

    // Replacing a battlefield with a token and swapping the original back are
    // EffectExecutor::replaceBattlefieldWithToken and the token battlefield's
    // own card (Brush): the engine has no second copy of either.

    // ── Equip/Attach (CR 716-725) ──
    // Attaching goes through EffectExecutor::attachGear (the one attach path).
    /// Detach all gear from a unit (e.g., when unit leaves board).
    void detachAllGear(GameObjectId unit);
};

} // namespace riftbound
