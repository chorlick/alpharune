# 1. OpenSpiel / search correctness

These make MCTS, ISMCTS and anything trained on them wrong without any visible
failure. Fix before trusting search or training output.

## Bugs

- [x] **1.1 `CurrentPlayer()` reports the turn player, not the decider** [verified] — FIXED: `StepDriver` is installed as two per-seat agents (`seat(player)`), so `StepResult::perspective` / `CurrentPlayer()` is the seat the engine asked. `HumanAgent::pendingPlayer()` fixed with 1.10; test: StepApiTest.PerspectiveIsTheDecidingPlayerNotTheTurnPlayer, RiftboundStateTest.CurrentPlayerIsTheDeciderAndLegalActionsAreSorted (riftbound_openspiel_tests)
  - Where: `src/engine/step_driver.cpp:84` (`paused_player_ = state.turn.turn_player`), consumed at `game_engine.cpp:164`, `openspiel/riftbound_state.cpp:157-166`, copied into clones at `:333`.
  - The engine queries non-turn players at `game_engine.cpp:536` (second mulligan), `:1242` / `:1285` (finalize, re-choice), `:2235` (chain priority), `:3820` (showdown focus), `:3995` (combat damage). Only `setupBattlefields` (`:398-406`) swaps `turn_player` temporarily.
  - Failure: P1 plays a spell, P2 gets priority, `CurrentPlayer()` returns P1. MCTSBot tags P2's replies as P1's moves and maximises P1's return; `ObservationTensor(CurrentPlayer())` shows the wrong hand.
  - `Intent::player` on the legal list is correct. `HumanAgent::pendingPlayer()` (`human_agent.h:74`) has the same defect.
  - Fix direction: pass the deciding player into `StepDriver::selectAction` (the agent interface already knows it via `getAgent(player)`).

- [x] **1.2 `MctsAgent` / `IsMctsAgent` root perspective from a stale `priority_holder`** [confirmed] — FIXED: the agents no longer read `priority_holder`; they search from a mirror engine replayed to the live decision, whose `CurrentPlayer()` is the seat asked (`LiveMirror` in mcts_agent.cpp, `RiftboundState::syncToHistory`). No card writes `priority_holder` (grep: only chain_manager.cpp and game_engine.cpp); test: LiveMirrorTest.MirrorMatchesARunGameGameAtEveryDecision, MctsAgentTest.PlaysAFullGameAsEitherSeat
  - Where: `src/agents/mcts_agent.cpp:294-298`, used at `:314` and `:441`.
  - `priority_holder` is assigned only at `chain_manager.cpp:264`, `game_engine.cpp:953` and `:3734`, and never cleared when the chain empties.
  - Failure: P1 plays a spell, both pass, `priority_holder` stays P2. P1's next main-phase decision is searched with `perspective = P2`, so `BestChild()` picks the move best for the opponent.
  - Card code was not checked for writes to `priority_holder`.

- [x] **1.3 Clones do not carry RNG state** [verified] — FIXED: `resumeFromSnapshot` takes the snapshotted engine's RNG stream (`rngState()`), and clones carry it. Clones of one position still share one future, as `Clone()` must; the per-turn action count is carried too; test: StepApiTest.ResumedEngineContinuesTheSameGameAndRngStream, StepApiTest.ResumedEngineKeepsTheMainPhaseActionCount, RiftboundStateTest.CloneTakenAtAnyDecisionPlaysTheSameGame
  - Where: `game_engine.cpp:209` (`rng_.seed(engine_seed…)` in `resumeFromSnapshot`), reached from `riftbound_state.cpp:131`.
  - `rng_` lives on the engine, not in `GameState`.
  - Failure: original and clone diverge on the next shuffle (`effect_executor.cpp:441`, `:905`); every clone of a position also replays the identical stream, so all simulations see one shuffle outcome.

- [x] **1.4 `ensureLive()` replay fallback lands in a different game** [verified] — FIXED: `action_history` now holds exact history tokens (slot + Intent fingerprint, action_vocab.h); a non-resumable clone is rebuilt from its nearest resumable ancestor by replaying them and verified against the snapshot (throws on mismatch, no index-0 fallback; an illegal action id throws). Stale comments rewritten; test: StepApiTest.ReplayingHistoryTokensRebuildsTheSameGame, ActionVocab.HistoryTokenTellsApartIntentsThatShareASlot, RiftboundStateTest.CloneTakenAtAnyDecisionPlaysTheSameGame
  - Where: `riftbound_state.cpp:132-153`.
  - Snapshots that are not a "clean boundary" (mid-chain, showdown, combat, cost payment, mulligan) are rebuilt by `beginGame` plus replaying lossy vocab slots, taking the first intent that encodes to each slot and `break`ing silently on a miss.
  - Failure: live game mulliganed card B, slot is "mulligan count 1", replay sets aside card A; hands diverge, a later slot is not legal, replay stops early. Later actions hit the `legal_index = 0` fallback (`:369-374`).
  - Reach: for `MctsAgent`, every root where a chain exists or a cost is being paid. Also costs a full game replay per simulation.
  - The comment at `:82-93` claims "full correctness"; `riftbound_state.h:57-64` admits the replay diverges.

- [x] **1.5 `action_history` recorded twice per action** [verified] — FIXED: `recordAppliedIntent` is the only writer (the wrapper's two pushes are gone) and always records one entry; test: StepApiTest.ActionHistoryHasOneExactTokenPerDecision, RiftboundStateTest.ActionHistoryIsOneSlotPerAppliedAction
  - Where: `riftbound_state.cpp:376` (and `:151` in replay) plus `recordAppliedIntent` (`game_engine.cpp:6440-6444`, called from ten sites).
  - Failure: history becomes `[a0,a0,a1,a1,…]`, which breaks the 1.4 replay; `actionHistory()` is twice the documented length and every clone copies it.

- [x] **1.6 "Clean boundary" test accepts snapshots taken at mid-play prompts** [confirmed for one prompt, plausible for the rest] — FIXED: `GameEngine::isResumableDecision(state, legal)` requires the legal list to offer EndTurn (only `generateMainPhaseActions` produces it), and `resumeFromSnapshot` refuses anything else and checks it arrived at the snapshotted decision; test: StepApiTest.OnlyTheTopLevelMainPhaseDecisionIsResumable, StepApiTest.ResumeRefusesEveryNonResumableDecisionOfARealGame
  - Where: `riftbound_state.cpp:94-110`; prompts at `game_engine.cpp:1611-1626` (Brazen Buccaneer decline/accept), `:1877-1883` (Repeat), `:5511`, `:5546`.
  - At those yields the card has left hand but there is no chain item and no `cost_cursor`, so the snapshot looks clean.
  - Failure: `resumeFromSnapshot` re-enters `mainPhaseLoop`; the pending yes/no is lost, the half-played card is in no zone vector, and `DoApplyAction` applies index 0.

- [x] **1.7 Engine exceptions become a silent bogus terminal** [verified] — FIXED: the fiber keeps `std::current_exception()` (and lets `forced_unwind` through); `beginGame` / `applyChoice` / `currentStep` rethrow it, `stepFailed()` reports it, `RiftboundState::IsTerminal` / `Returns` / `Clone` rethrow instead of scoring a draw; test: StepApiTest.EngineExceptionIsRethrownNotReportedAsAFinishedGame, RiftboundStateTest.EngineExceptionSurfacesInsteadOfADraw
  - Where: `game_engine.cpp:266-271`, `step_driver.cpp:51-58` (`catch (...) {}` then done).
  - Failure: any throw mid-game yields `isStepDone()==true`, `game_over==false`, winner `None`. Trainer and MCTS score an engine crash as a draw.
  - The blanket catch would also swallow boost.context's `forced_unwind` if a suspended fiber were destroyed.
  - Fix direction: stash `std::current_exception()` and rethrow from `applyChoice` / `currentStep`.

- [x] **1.8 `Clone()` drops OpenSpiel's `history_` and `move_number_`** [confirmed] — FIXED: `Clone()` copies `history_` and `move_number_`; at the cap `CurrentPlayer()` is terminal and `LegalActions()` empty; `MaxGameLength()` is the same `kMaxGameLength` (600); test: RiftboundStateTest.CloneKeepsMoveNumberAndHistory, RiftboundStateTest.AtTheLengthCapTheStateIsTerminalInEveryRespect
  - Where: `riftbound_state.cpp:54-70`, `:327-340`; cap at `:252`.
  - Failure: an original at move 600 is terminal, its clone is not. `clone_equivalence_test.cpp` would report "terminal mismatch" on long games.
  - Related: at the cap `IsTerminal()` is true while `LegalActions()` is non-empty and `CurrentPlayer()` is not terminal; `MaxGameLength()` returns 10000 (`riftbound_game.h:37`).

- [ ] **1.9 State features mix absolute and perspective-relative ordering** [confirmed]
  - Where: `src/ml/feature_extractor.cpp:355-358`, `:389-392` versus `:541-542`.
  - Battlefield controller is encoded P1=1 / P2=2; the per-side base block and the 96-float extended block are P1-then-P2; the Tier-2 block is self-then-opp. No feature gives the absolute seat.
  - Failure: for `perspective = Player2`, "bf0 side0 unit0" is the opponent's unit in the base block and P2's own in Tier-2. The comment at `:496-497` says these line up.
  - `tests/test_observation_tensor.cpp` checks only hand and deck masking.
  - Changing the layout invalidates trainer checkpoints.

- [x] **1.10 `HumanAgent` consumes stale clicks and has a data race** [confirmed] — FIXED: `setChosenAction` returns bool and refuses a choice when the engine is not waiting, the index is out of range or the decision already has an answer; `setChosenAction(index, decision_id)` also checks the id; `selectAction` clears a leftover choice; `atDecision()` locks; concede / `pendingPlayer()` use the deciding seat (optional `HumanAgent(PlayerId seat)`). io-cli still has to send the decision id (4.3); test: HumanAgent.OutOfRangeIndexIsRefused, ChoiceSentBetweenDecisionsIsDropped, ChoiceForAnotherDecisionIdIsRefused, PendingPlayerAndAbortUseTheDecidingSeat, ConfiguredSeatIsThePendingPlayerAndTheConceder
  - Where: `src/agents/human_agent.h:71-88`, `:109-115`, `:148-150`.
  - `selectAction` never clears `pending_choice_` on entry, `setChosenAction` has no decision-id check, `atDecision()` reads a plain `bool` unlocked. Out-of-range index silently becomes action 0 (`:90-92`).
  - `selectAction` also concedes on behalf of `turn_player` on abort, not the aborting seat (`:57`, `:83`).

## Integrity

- [x] **1.11 `LegalActions()` is not sorted** [confirmed; downstream impact plausible] — FIXED: sorted and deduplicated in one helper (`encodeLegal`); test: RiftboundStateTest.CurrentPlayerIsTheDeciderAndLegalActionsAreSorted
  - Where: `riftbound_state.cpp:183-205`. OpenSpiel's contract is ascending order. The bots used here tolerate it.

- [ ] **1.12 Slot collisions remove legal options** [confirmed; documented as "accepted" in `action_vocab.h:26-30`]
  - Where: `action_vocab.cpp:59-64`, `:96-98`, `:181-190`; dedup at `riftbound_state.cpp:189`.
  - Mulligan encodes the count only (cannot choose which card).
  - `StandardMove` encodes the destination only ("move A" = "move A+B").
  - `MakeChoice` encodes the first object's def id; in a mirror match "target my X" and "target their X" are one slot.
  - An object with an invalid def id encodes to slot 0, same as skip/decline (reach not confirmed).
  - Champion play and its alt-cost play share a slot (`action_vocab.cpp:101-107`).
  - `MakeChoice` options that differ only in `ability_index` collide: Party Favors (71) — the opponent can never choose Runes under OpenSpiel (07 item 7.33).
  - Gear-target hash overwrites `ability_index` (`:143-152`). Comment at `:73-83` says 16 buckets; code uses 128.

- [x] **1.13 ISMCTS node key ignores the pending decision** [plausible, not reproduced] — FIXED: defect proven (94 same-key / different-action-set pairs in 8 random games with the board-only key). `InfoSetId` now folds in who decides and, for the decider, the legal action set (`ml::mixDecisionIntoInfoSetId`); test: RiftboundStateTest.SameObservationKeyMeansSameLegalActions, CfrUtil.DecisionIsPartOfTheInfoSetId
  - Where: `riftbound_state.cpp:271-292`, `mcts_agent.cpp:405-406`.
  - Key hashes state features only (no `cost_cursor`, prompt or `priority_holder`), with `allow_inconsistent_action_sets=false`. Two consecutive decisions with identical features share a node; UCB can return an action illegal in the second.

- [x] **1.14 Unpinned constants and missing tests** [confirmed] — FIXED: static_assert in token_layout.h; feature-size test, and `ObservationTensor` checks the size instead of padding; new tests for RiftboundState / MctsAgent / IsMctsAgent (new binary `riftbound_openspiel_tests`), ScriptedAgent, cfr_util, multi-element vocab lists; `buildOpenSpielStateAt` removed and mcts_agent.h rewritten; `registry` parameter kept but ignored with an empty default (removing it would reject game strings from openspiel_match.cpp and the trainer); default decks exist; test: StepApiTest.StateFeaturesAreExactlyStateFeatureDimOnRealStates, TokenLayout.CardIdSpacesAgree, ScriptedAgentTest.*, CfrUtil.*, RiftboundGameTest.*
  - `token_layout.h:37` hard-codes `kCardEmbeddingRows = 1101` with no `static_assert`.
  - No test asserts `extractStateFeatures().size() == kStateFeatureDim`; `ObservationTensor` hides a mismatch by truncating or zero-padding (`riftbound_state.cpp:311-313`).
  - No tests for `RiftboundState` (Clone, `CurrentPlayer`, `LegalActions`), `MctsAgent`, `ScriptedAgent` or `cfr_util`. `test_action_vocab.cpp` round-trips single-element legal lists only, so it cannot see a collision.
  - `cfr_util.h:47` cites `tests/test_cfr_util.cpp`, which does not exist.
  - `buildOpenSpielStateAt` (`mcts_agent.cpp:204-215`) is unused; `mcts_agent.h:5-18` still describes history replay.
  - `riftbound_game.h:12` / `riftbound_game.cpp:38` keep a `registry=cards/ban-list.csv` parameter for a file that no longer exists.
  - The game's default `deck1` / `deck2` point at `decks/leblanc_test.txt`, deleted in the working tree (07 item 7.40).

## Checked and clean

- No hidden-information leak in `extractStateFeatures`: opponent hand ids are gated on `is_self`, opponent deck is never emitted, facedown cards appear only as count and age.
- `cfr_util.cpp`: no numerical bug for finite inputs.
- `ScriptedAgent` always returns a legal action.
