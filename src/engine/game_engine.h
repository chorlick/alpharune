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

#include <functional>
#include <memory>
#include <random>
#include <thread>
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

private:
    const CardDB& card_db_;
    EventBus& events_;
    GameState state_;
    std::mt19937_64 rng_;

    // Phase 2-3 subsystems
    std::unique_ptr<ChainManager> chain_manager_;
    std::unique_ptr<EffectExecutor> effect_executor_;
    std::unique_ptr<TriggerManager> trigger_manager_;

    // Phase 4: Card object system (shared, const after init)
    const CardRegistry& card_registry_;

    AgentInterface* agents_[2] = {nullptr, nullptr};

    // Step machine state (Phase 11 C-1).
    StepResult current_step_{};
    GameResult step_result_{};
    std::unique_ptr<StepDriver> step_driver_;
    std::thread step_thread_;

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
    void awakenPhase();
    void beginningStep();
    void scoringStep();
    void channelPhase();
    void drawPhase();
    void mainPhase();
    void endingStep();
    void expirationStep();

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
