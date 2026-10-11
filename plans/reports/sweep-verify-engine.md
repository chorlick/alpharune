# sweep-verify-engine — report (2026-10-11)

## 1. Result

- Branch `fix/sweep-verify-engine`, head `92c611d`; `fix/integration` merged in three times (last before `92c611d`).
- **Clean build** (`cmake --build build --target clean`, then all gate targets) at `92c611d`:
  - `riftbound_tests`: 2648 passed, 0 failed.
  - `riftbound_openspiel_tests`: 25 passed (ResumeStress included). `riftbound_clone_equiv_test`: 10/10.
  - Smoke 200 games rc=0, 0 draws. Fault injection: rc=0 without, rc=1 with `RIFTBOUND_STEP_FAULT_AT=40`.
  - The two ISMCTS command lines of perf-search section 5, ten runs each: 20/20 rc=0, no "did not reach the snapshotted decision", no exception in any log.
  - 0 warnings in `riftbound_tests`, `riftbound`, `riftbound_openspiel_tests`, `riftbound_clone_equiv_test`, `riftbound_search_microbench`.
- Red-first: four mutation builds (fixes disabled by file backup in the scratchpad). Every test named below as "red" failed there and passes on the real tree. Lists: `scratchpad/sweep-verify-engine/red_m1.txt`, `red_m2.txt`, `red_m3.txt`; the Mystic Vortex test was run alone.

## 2. Items closed

FIXED unless marked. Closing text and test names are on each plan line.

**Engine trusts its caller**
- **9.G4** `GameEngine::askAgent` is the one place an agent is asked; the answer must be one of the options offered. **Throw, not refuse-and-continue:** `std::invalid_argument`, as `applyChoice` throws `std::out_of_range` for a bad index. Concede is always accepted.
- **9.G5** Same check for every intent type at every decision. `payAndQueueAbility` also refuses an activation whose source is not in play / not the player's / fails its gates (this stops the test seam `testHook_executeIntent`, which bypasses decisions; the hook's comment says what a test must set up).
  - Cost: below noise. 200-game batch, 1 thread: 0.59 s with the check, 0.58 s without. `clone_advance_microbench` ApplyAction median 112–114 µs with, 111–112 µs without (replayed); 88 µs both (resumed).
- **9.I6** `resolveSpell` hands `onTrigger` / `onEquippedTrigger` a fixed empty list.
- **9.C3** A stale intent is refused: an edit through `mutableState()` during a decision is noticed; the answer must be in the rebuilt list (three state-derived decisions) or name no edited object (nested).
- **9.P3** `runFinalizePass` leaves no item resolving when an answer is refused.

**Card-facing**
- **9.A19** (+ **9.G10**) `AuraEffect::only_if_absent`; Spirit's Refuge 63.
- **9.A21** `CostModifier::from_hand_only`; Jayce 406.
- **9.A13** `ActivationCost::banish_self`; The Zero Drive 412.
- **9.G1** location helpers leave out blocked battlefields; masks in 704, 515, 475 and Arise! 513's copy deleted.
- **9.G8** `Card::killAsCost` / `markCostUnpaid` / `costUnpaid`; 765, 449. Found on the way: a Fan banished instead of dying still moved the attacker.
- **9.G12** `boardObjects`, sorted `chooseableUnits`; 13 cards + 108 + 406.
- **9.A22, 9.D3, 9.C5, 9.M3** no pick caps; `selectSeveral` replaces `pickSeveral` / `selectAnyNumber` (230, 423, 372, 497, 540 migrated).
- **9.M4** split damage: targets at finalization, division at resolution (Alpha Strike 751, Volibear 41). A `kFinalizeResolve` spell with play-time targets now gets its finalize pass.
- **9.R4 / 9.O1** reaction tax only while that battlefield's showdown is open.

**Rules-bearing engine items**
- **9.M1** multi-unit Standard Move is one action.
- **9.M9** CR 323.6 recalls go through `recallToBase`.
- **9.M11** decided from CR 471.1 / 468.5.d: control without a Score is not a conquer; the stamp moved below the two early returns.
- **9.M6** decided from CR 718.2: a granted [Temporary] kills attached Equipment, a printed one stays inactive.
- **9.N3** Ambush loops — and the two reaction-to-attack loops, which had the same omission.

**Tests only:** **9.M5** (Smite on the lethal-damage path), **9.M7** (`noteChosen`), **9.R3** (MakeChoice answers get distinct action ids, in `openspiel_tests.cpp`).

**Dead code, comments, warnings**
- **5.13, 5.14** remainder (sub-bullets in plans/05). `combat_staged` left set after combat was a real defect (visible to decisions and the feature vector until the caller cleared it).
- **9.F2** three dead variables removed (`stepFinalize` now void). **9.F3** card.h.
- **9.M2, 9.M8, 9.M10, 9.M12** comments and trace strings. The Zilean field rename of 9.M12 came from 9.M16 on integration; mine was dropped in the merge.
- **8.16** every bullet verified with its test.
- **9.P2** CLAUDE.md describes what this stream added.

## 3. Items not closed

- **9.T4** (late choices: effect-played spell, `makeNewChoicesFor`, Repeat execution): assigned after the final run started; not started at `92c611d`. Findings so far, from reading the code:
  - Effect-played spell (`setQueueSpell`): the item is added Pending and gets its finalize pass at the next Finalize step, after the resolving item has finished — not "when it is put there".
  - `makeNewChoicesFor`: a `needsPlayTimeTarget` spell is re-chosen at once; a `kFinalize*` card has its answers cleared and is asked as it resolves.
  - Repeat: `ChainManager::stepResolve` clears the answers and asks as each extra execution runs; play-time targets are reused. CR 820.2 puts those choices in the Make Relevant Choices step.
  - Needs: per-execution choice storage on `ChainItem` and a three-line change in `stepResolve` (sweep-triggers' file). Where it meets sweep-costs' 9.N2 / 9.T1: `chooseTargetsAtFinalize` → `runFinalizePass` is the shared entry; theirs changes when it runs relative to `payCardCost` on a play from hand, 9.T4 adds callers on paths with no play.

## 4. Needs another stream

- `shared:` commit `7505633`: `ActivationCost::banish_self` (effect_types.h).
- Edits in other streams' files, each small: `events.cpp` (CleanupNeededEvent), `chain_manager.{h,cpp}` and `trigger_manager.cpp` (9.F2), `openspiel_tests.cpp` (9.R3), CLAUDE.md (9.P2).
- Tests of other streams corrected, reason at each: the four `*_DoesNotOfferABattlefieldUnitsCannotBePlayedTo`, Promising Future (`..._IsRefused`), Spirit's Refuge order test, Mystic Vortex (enabled) in `test_review_fix_verify_cards.cpp`; Alpha Strike / Volibear split tests in `test_rules_compliance_cards.cpp` and `test_choice_helpers.cpp`; Zero Drive in `test_audit_fixes_0.cpp`.
- Done here although owned by sweep-verify-cards: **9.M31** (Fox-Fire / Overt Operation caps), the Death Mark comment of **9.M29**. Their boxes are still unticked.
- **9.P1** (cleanup): play_server should call `recomputePendingLegalActions()` for showdown and priority decisions too.
- **Trainer**: two prompt sequences changed — split damage asks targets ("No more targets" first from the second prompt), then points; effect-played unit locations are numbered over what is offered.
- cleanup's strict `EnginePathAgent`: my tests use `script(...)`, lambdas or `choosing(id)` only where the object prompt is the only prompt; `VerifyEngineTest.EnemyChoices_...` and the Harnessed Dragon control use bare `choosing(id)` with a single target prompt.

## 5. New defects found

- Brush 561's swap-back had no test (now `VerifyEngineTest.Pin_Brush_WhenScoredRestoresTheBattlefieldItReplaced`).
- `beginningStep`: an Equipment that prints [Temporary] and is also given it is read as printed only (the object does not record a second instance). Said at the code; no card combination does it today.
- `GameEngine::channelPhase` derives "the player who went first" from `turn_number == 0` instead of `TurnState::starting_player`. Works for 1v1 alternating turns; not changed.
- `ChainManager::stepExecuteAndPass`: after a god-mode edit, `recomputePendingLegalActions` can offer a play that the priority window had dropped as failed (9.D10's list is local to the window).
- `splitDamageAmong`: when nothing can be targeted at finalization the item is marked done, so it deals nothing even if a legal unit appears before it resolves. That follows CR 355.14.b (no targets were chosen); noted because the old code would have asked at resolution.

## New state fields

`GameObject::AuraEffect::only_if_absent`, `PlayerState::CostModifier::from_hand_only`, `ActivationCost::banish_self`. None is in the feature layout. `CleanupNeededEvent` is gone from the event bus. No change to `kStateFeatureDim`, `kCardVocabSize` or the action vocabulary.
