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
#include "engine/trigger_manager.h"
#include "engine/step_driver.h"
#include "rules/deck_validator.h"

#include <boost/signals2/connection.hpp>
#include <functional>
#include <memory>
#include <random>
// <thread> no longer needed — step machine uses a userspace fiber
// (see step_driver.h). Kept commented for hg-archaeology; remove
// later once we're sure nothing else in this header transitively
// depended on the include.
// #include <thread>
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

// ─── Step machine (Phase 11 C-1) ──────────────────────────────────────────────
// Pull-driven engine API. Replaces the push-driven `runGame` recursion with
// `beginGame()` + a `currentStep()`/`applyChoice()` loop owned by the caller.
// Lets `Clone()` collapse to memcpy(GameState) — no worker thread, no halting
// agent. See Phase 11 Phase C-1 in CLAUDE.md.
//
// Methods are stubbed in step (2) of the rollout; subsequent commits fill in
// the subroutine conversions (main phase, chain, combat, cleanup). External
// callers can start adopting the surface now; the existing `runGame` path
// remains the implementation behind the BatchRunner / Agent flow until the
// step machine is fully wired.

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

    // ── Step machine API (Phase 11 C-1, in progress) ────────────────────────
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
    // Once implemented, this entire loop runs without any worker threads or
    // condition variables — Clone() = memcpy(GameState). Phase C-1's remaining
    // commits replace each subroutine that currently calls
    // `agent.selectAction(...)` with a resumable step that returns control
    // here instead.
    //
    // Currently stubbed (throws std::logic_error). Old runGame remains the
    // production path until step (7) of the rollout flips OpenSpiel + Batch
    // Runner over.

    /// Begin a game in step-machine mode. Loads decks, initialises state,
    /// runs until the first NeedDecision or terminal.
    StepResult beginGame(
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
    /// TriggerManager subscriptions on the supplied EventBus), then spawns
    /// the worker thread which dispatches to the appropriate phase based
    /// on `state_.turn.phase` — no setup, no mulligans, no replay.
    ///
    /// Currently supports resuming from the post-mulligan phases
    /// (AwakenPhase through ExpirationStep). Resuming during Setup or
    /// Mulligan throws — those paths still go through `beginGame` +
    /// replay (rare in practice — clones during mulligan are at most a
    /// few per game vs. hundreds during main phases).
    StepResult resumeFromSnapshot(GameState snapshot_state,
                                    uint64_t engine_seed);

    /// Inspect what the engine is currently waiting on without advancing.
    StepResult currentStep() const;

    /// Apply the chosen decision (index into the prior step's `legal`),
    /// then advance until the next NeedDecision or terminal. Returns the
    /// new step.
    StepResult applyChoice(int legal_index);

    /// True iff `currentStep().kind == StepKind::Done`.
    bool isStepDone() const;

    /// After the game has terminated under the step-machine flow,
    /// the GameResult. Undefined before isStepDone() returns true.
    const GameResult& stepResult() const { return step_result_; }

    /// Access the current game state (for external inspection).
    const GameState& state() const { return state_; }

    /// Mutable access. Phase C-1: needed by the OpenSpiel wrapper to
    /// record applied action IDs on `GameState::action_history`. Will be
    /// the single state-mutation entry point once the engine is driven
    /// as a step machine (see Phase 11 Phase C-1 in CLAUDE.md). Do not
    /// use from BatchRunner / agent code — they should see state as read-only.
    GameState& mutableState() { return state_; }

    /// Generate all legal actions for the player who must currently act.
    std::vector<Intent> generateLegalActions() const;

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
    void testHook_scoreConquer(PlayerId p, BattlefieldId bf) { scoreConquer(p, bf); }
    void testHook_scoreHold(PlayerId p, BattlefieldId bf) { scoreHold(p, bf); }
    void testHook_drawCards(PlayerId p, int n) { drawCards(p, n); }
    void testHook_processLethalDamage() { processLethalDamage(); }
    void testHook_cleanup() { cleanup(); }
    void testHook_executeStandardMove(const Intent& i) { executeStandardMove(i); }
    void testHook_drawPhase() { drawPhase(); }
    void testHook_endingStep() { endingStep(); }
    // Phase 6q+ engine-audit CRITICAL #2 + #3 — exposed so tests can
    // exercise the showdown / combat trigger drain + action-type
    // dispatch in isolation without driving a full turn.
    void testHook_runCombat(BattlefieldId bf) { runCombat(bf); }
    std::optional<PlayerId> testHook_resolveShowdownDecision(
        BattlefieldId bf, const Intent& chosen, PlayerId current_focus,
        int& action_count) {
        return resolveShowdownDecision(bf, chosen, current_focus, action_count);
    }
    void testHook_executeIntent(const Intent& i) { executeIntent(i); }
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

    // ── Repeat (CR 820) — optional additional cost ──
    //
    // Exposed in public for unit tests (test_jhin_deck.cpp parses
    // ability_text fragments). The action generator + executePlaySpell
    // consume these via the private helpers in the cost-payment section.
    struct RepeatCost {
        int energy = 0;
        int power = 0;
        Domain power_domain = Domain::Count;  // Count = universal [A]
        bool valid = false;
    };
    static RepeatCost parseRepeatCost(const std::string& ability_text);

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

    // Phase 2-3 subsystems
    std::unique_ptr<ChainManager> chain_manager_;
    std::unique_ptr<EffectExecutor> effect_executor_;
    std::unique_ptr<TriggerManager> trigger_manager_;

    // Phase 4: Card object system (shared, const after init)
    const CardRegistry& card_registry_;

    AgentInterface* agents_[2] = {nullptr, nullptr};

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
    void mainPhaseLoop();
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
private:
    /// Body of the expiration step (heal, expire turn-buffs, empty rune
    /// pools, expire delayed abilities). Pulled out so expirationStep
    /// can loop the body per CR 317.2.f.1.
    void doExpirationBody();

    // ── Main phase, structured for step-machine resumability (C-1 commit 5) ──
    //
    // `mainPhase()` above wraps these two halves with the legacy
    // queryAgent() bridge in the middle. Once C-1's later commits wire
    // the step machine through, the engine's outer loop will call these
    // directly: advanceMainPhase() to reach the next decision point,
    // resolveMainPhaseDecision() to apply the chosen Intent and continue
    // through chain/cleanup. The split here is purely structural — every
    // line of behavior matches the prior mainPhase() body. Sub-calls
    // (runShowdown, runCombat, runChain, cleanup) still use the in-place
    // recursive path until C-1 commits 6+7 convert them.
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

    // ── Showdown loop, structured for step-machine resumability (C-1 commit 7) ──
    //
    // Same shape as the main-phase split above. The legacy
    // runShowdownLoop() body is the in-place recursive path; this
    // advance/resolve pair is the surface that the future step machine
    // (C-1 commit 9) will call directly. Currently runShowdownLoop()
    // itself is the only caller — the bridge through queryAgent / focus
    // alternation lives there until commit 9 flips the runtime.
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

    // ── Combat damage assignment, structured for step-machine resumability ──
    //
    // The damage step has TWO agent queries — one for each side's
    // assignment. The step-machine path will surface these one at a
    // time. Each side's split has the same advance/resolve shape:
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

    // ── Cleanup, structured for step-machine resumability (C-1 commit 8) ──
    //
    // Cleanup has NO agent-query sites — the SBA pass is purely
    // state-derived (lethal damage, BF control changes, aura recalc,
    // win check). Triggers fired by killUnit are queued onto the chain
    // by TriggerManager and resolved in a SEPARATE runChain pass by
    // the caller (see resolveMainPhaseDecision). So cleanup itself
    // never yields — its "advance" always returns Done.
    //
    // The split is documented for symmetry with the other phase
    // subroutines; in practice callers can keep using cleanup() too.
    void advanceCleanup();

    // ── Mulligan, structured for step-machine resumability (C-1 commit 8) ──
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

    // ── Cost payment, structured for step-machine resumability ──
    //
    // Splits payCardCost into a cursor-based state machine so the
    // step-machine dispatch (commit 9) can drive it without going
    // through the legacy threaded queryAgent path. Currently
    // payCardCost itself remains the production entry point — it
    // wraps the begin/advance/resolve methods in a queryAgent loop
    // (the bridge — same shape as runShowdownLoop / mainPhase).
    //
    // After commit 9, the step-machine dispatch uses begin/advance/
    // resolve directly: when an executePlayCard / executePlaySpell /
    // ActivateAbility intent enters the engine, the dispatch calls
    // beginCostPayment, yields each rune-pick decision via
    // applyChoice, and resumes the original operation once
    // cost_cursor->phase == Done.
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
    CostPaymentAdvance beginCostPayment(PlayerId player, GameObjectId card_obj);
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
    void executeEndTurn(const Intent& intent);
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
    std::vector<Intent> generateCombatDamageActions(PlayerId player) const;
    void generateSpellActions(PlayerId player, bool action_ok, bool reaction_ok,
                               std::vector<Intent>& actions) const;
    void generateActivateAbilityActions(PlayerId player,
                                         std::vector<Intent>& actions) const;

    // ── Queries ──
    Intent queryAgent(PlayerId player);

    /// Encode the chosen Intent via action_vocab and append the slot ID
    /// to GameState::action_history. Called from every selectAction
    /// dispatch site so that MCTS-style agents (which reconstruct an
    /// OpenSpiel state by replaying action_history) see the same
    /// sequence of decisions the live engine just applied. Without this,
    /// engine-driven games (`GameEngine::runGame` with AgentInterface
    /// agents) leave action_history empty and MCTS plans against the
    /// initial game state for every decision — silently degrading the
    /// agent to legal_actions[0].
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
    void scoreConquer(PlayerId player, BattlefieldId bf);
    void scoreHold(PlayerId player, BattlefieldId bf);
    bool isWinningPointAttempt(PlayerId player) const;
    bool canGainWinningPointViaConquer(PlayerId player) const;

    // ── Cost payment ──
    /// Check if a player can afford a card's cost with available runes.
    bool canAfford(PlayerId player, GameObjectId card_obj) const;

    /// Auto-pay a card's cost by exhausting/recycling runes.
    /// Returns false if payment failed (shouldn't happen if canAfford was checked).
    bool payCardCost(PlayerId player, GameObjectId card_obj);

    /// Additional-cost helpers (CR 805 Accelerate, future additional costs).
    /// `energy` is paid by exhausting ready runes; `power` is paid by
    /// recycling runes whose `domains` include `domain`. The two pools may
    /// overlap (a ready domain-matching rune may be either exhausted for
    /// energy OR recycled for power — preference is to recycle exhausted
    /// matching runes first to preserve ready energy). Auto-pays; no agent
    /// interaction. The caller is responsible for any other side effects
    /// (e.g. flipping `is_exhausted` on a unit that paid Accelerate to
    /// enter ready — that's resolvePermanent's job).
    bool canPayAdditionalCost(PlayerId player, int energy, int power,
                                Domain domain) const;
    bool payAdditionalCost(PlayerId player, int energy, int power,
                            Domain domain);

    // ── Repeat (CR 820) — optional additional cost ──
    //
    // Action gen consults `parseRepeatCost()` (public-section static helper
    // above) on a card's ability_text. If the text contains
    // "[Repeat] [N]" / "[Repeat] [N][D]" / "[Repeat] [D]", every legal play
    // intent is duplicated once per affordable Repeat count and
    // `chosen_value = R` is set on each duplicate. The chain item's
    // `repeats_paid` is then populated in executePlaySpell from
    // `intent.chosen_value`, which the chain manager uses to re-resolve the
    // spell `R` extra times (CR 820.2). `total_energy_spent` = base + R *
    // repeat_energy; Forgotten Library / Virtuoso read this for cost gating.
    //
    // Limitation: parses the FIRST [Repeat] [...] only — Curtain Call's
    // three modal Repeat costs are handled by the card's own onResolve
    // modal logic, NOT by this helper.

    /// Pay an extra Repeat tranche on top of an already-paid base cost.
    /// Returns false if the additional payment can't be satisfied (which
    /// indicates a bug in the affordability gate in action gen).
    bool payRepeatCost(PlayerId player, const RepeatCost& cost);

    /// Count available Energy from ready runes (exhaust potential).
    int availableEnergy(PlayerId player) const;

    /// Count available Power of a specific domain (recycle potential).
    int availablePower(PlayerId player, Domain domain) const;

    /// Count available universal Power [A] (from recycling any rune).
    int availableAnyPower(PlayerId player) const;

    // ── Utilities ──
    void moveUnit(GameObjectId unit, LocationId destination);
    void killUnit(GameObjectId unit);
    void healAllUnits();
    void emptyRunePools();
    PlayerId activePlayer() const;
    BattlefieldState& getBattlefield(BattlefieldId id);
    const BattlefieldState& getBattlefield(BattlefieldId id) const;

    // ── Battlefield Replace/Swap-back (CR 438) ──
    /// Replace a battlefield with a token battlefield. Original goes to Banishment.
    void replaceBattlefield(BattlefieldId original_bf, GameObjectId token_bf_card,
                            PlayerId controller);
    /// Swap back: when token BF leaves, restore original from Banishment.
    void swapBackBattlefield(BattlefieldId token_bf);

    // ── Equip/Attach (CR 716-725) ──
    /// Attach a gear to a unit. Returns true if successful.
    bool attachGearToUnit(GameObjectId gear, GameObjectId unit);
    /// Detach all gear from a unit (e.g., when unit leaves board).
    void detachAllGear(GameObjectId unit);
};

} // namespace riftbound
