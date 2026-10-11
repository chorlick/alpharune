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
    void testHook_cleanup() { cleanup(); }
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
        enum class Kind : uint8_t { NeedDecision, Skip };
        Kind kind = Kind::Skip;
        PlayerId assigner = PlayerId::None;
        std::vector<Intent> legal;
    };
    CombatDamageAdvance testHook_advanceCombatDamage(
        PlayerId assigner, int total_damage,
        const std::vector<GameObjectId>& targets);

    /// The play gate for a spell choosing `targets`: its cost after what the
    /// targets take off, plus their Deflect, payable together. With no targets
    /// this is canAfford.
    bool canAffordChoosing(PlayerId player, GameObjectId card_id,
                           const std::vector<GameObjectId>& targets) const;
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
    /// here") are the caller's.
    bool unitPlayableAt(const Card* card_obj, PlayerId player,
                        const BattlefieldState& bf) const;

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
    TrashGrantCost trashGrantCost(PlayerId player, GameObjectId card_id,
                                  const PlayerState::TrashReplayGrant& grant) const;
    bool canPayTrashGrant(PlayerId player, GameObjectId card_id,
                          const PlayerState::TrashReplayGrant& grant, int deflect) const;
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
    /// Make the choices the rules place at finalization for `item` (targets).
    void chooseTargetsAtFinalize(ChainItem& item);
    void runFinalizePass(ChainItem& item);
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
    bool payTrashReplayGrant(PlayerId player, GameObjectId card_id);

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
    void maybePayOptionalAdditionalCost(PlayerId player, GameObjectId card_id);

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
