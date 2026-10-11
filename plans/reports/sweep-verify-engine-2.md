# sweep-verify-engine — follow-up report 2 (2026-10-11)

## 1. Result

- Branch `fix/sweep-verify-engine`, head `62bcdae` (`fix/integration` at `24466c3` merged in; one plan-file conflict, resolved).
- **Clean build** at `62bcdae`, all gate targets:
  - `riftbound_tests`: 2703 passed, 0 failed.
  - `riftbound_openspiel_tests`: 25 passed. `riftbound_clone_equiv_test`: 10/10.
  - Smoke 200 games rc=0, 0 draws. Fault injection: rc=0 without, rc=1 with `RIFTBOUND_STEP_FAULT_AT=40`.
  - The two ISMCTS command lines, ten runs each: 20/20 rc=0.
  - 0 warnings in the five engine targets.
- Red-first: one more mutation build (`red_m5.txt` in the scratchpad): the five new tests and the corrected Academy test were red with the fixes disabled.

## 2. Items closed

- **9.T4** FIXED, all three cases:
  1. A spell an effect plays (`setQueueSpell`) makes its choices as it is put on the chain, before the rest of the effect that played it. `VerifyEngineTest.EffectPlayedSpell_MakesItsChoicesAsItIsPutOnTheChain`.
  2. `makeNewChoicesFor` runs a finalize pass at once for a card whose choices belong at finalization (CR 750-755); a cost paid in its first pass is not paid again. `VerifyEngineTest.MakeNewChoices_AFinalizePassSpellChoosesAgainAtOnce`.
  3. Each Repeat execution's choices are made as the item is finalized (CR 820.2), a fresh set each (820.2.a), stored in `ChainItem::repeat_choices`. Play-time targets are no longer reused. `VerifyEngineTest.Repeat_TheExtraExecutionChoosesAtFinalization`.
  - `runFinalizePass` no longer returns when another item is mid-resolution; it sets that item aside for the pass and puts it back.
  - Unchanged: a card that only ever chooses as it resolves (`needsPlayTimeTargetPair` with no finalize opt-in).
- **9.U1** FIXED: `GameObject::granted_keywords`; Turn to Dust 632 and Fading Memories 180 set it; an attached Equipment that prints [Temporary] and is also given it dies. `VerifyEngineTest.Temporary_GivenToAttachedEquipmentThatAlsoPrintsItKillsIt`.
- **9.U2** FIXED: `channelPhase` reads `TurnState::starting_player`. `VerifyEngineTest.ChannelPhase_TheExtraRuneGoesToWhoeverDidNotGoFirst`.
- Split damage with nothing to target at finalization: pinned by `VerifyEngineTest.Pin_SplitDamage_NothingToTargetAtFinalizationDealsNothing` (CR 355.14.b).

## 3. Items not closed

None assigned to this stream are open.

## 4. Needs another stream

- **`chain_manager.cpp` lines touched for 9.T4** (only these): in `ChainManager::stepResolve`, the Repeat loop — the comment above it (4 lines reworded) and, after the per-execution reset of the resuming item, a 10-line block that loads `ri.repeat_choices[r]` (targets, chosen_objects, resume_point, resume_data, may_answer, choices_finalized, finalize_costs_paid) before `runResumable`. Nothing in `stepExecuteAndPass` or `stepFinalize` (the earlier 9.F2 edit made `stepFinalize` void).
- **Where 9.T4 meets sweep-costs' 9.N2 / 9.T1** (not on integration at `24466c3`; both boxes still open): the shared entry is `GameEngine::chooseTargetsAtFinalize`. It is now a wrapper — "already chosen early" check, `chooseFirstExecution` (the old body), then the Repeat choices — and `runFinalizePass` is called from it, from `makeNewChoicesFor` and (through the wrapper) from `setQueueSpell`. If sweep-costs moves a from-hand spell's finalize pass ahead of `payCardCost`, two things must hold:
  - the Repeat choices have to be made after `repeats_paid` is known (it is set on the item right after `addSpell`, after the Repeat costs are paid). If the first execution's choices move earlier, either the Repeat block stays at the chain's Finalize step, or the early call must go through the wrapper only once `repeats_paid` is set; the wrapper skips the Repeat block when `repeat_choices` is already filled.
  - an item whose choices were made early must not be asked again at the Finalize step. `finalized_early_` (engine member) does that for `setQueueSpell`; the same list can serve the play path.
- **Trainer**: a Repeat execution of a spell with play-time targets now asks for its own targets at finalization (one extra prompt per target and Repeat paid).
- Tests of another stream corrected: `C0Test.TheAcademy_GrantedRepeatCostsTheWholeBaseCost` (2 prompts: the Repeat yes/no and the repeat's own target).
- New tests use explicit agents (`PickInOrder`, lambdas, `script`); none relies on bare `choosing(id)`. They pass with cleanup's strict picker, which is in the merged tree.

## 5. New defects found

- `makeNewChoicesFor`, generic branch (targets locked in by the Play intent): the re-chosen targets pay no Deflect and are not passed to `noteChosen`, unlike targets chosen through `chooseTargetsAtFinalize` / `pickTarget`. This now also applies to a Repeat execution of such a spell. Not changed.
- A Repeat on an activated or triggered ability (CR 820.1.a allows it) has no engine path: only `executePlaySpell` offers Repeat. No current card needs it, as far as `repeatCosts()` overrides show.
- `ChainItemStatus` of an effect-played spell stays Pending until the next Finalize step although its choices are made (the `ChainItemFinalizedEvent` is emitted there). Nothing reads the status in between today.

## New state fields

`ChainItem::repeat_choices` (per-execution choices), `GameObject::granted_keywords`. Neither is in the feature layout. No change to `kStateFeatureDim`, `kCardVocabSize` or the action vocabulary.
