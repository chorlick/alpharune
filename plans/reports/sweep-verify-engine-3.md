# sweep-verify-engine — follow-up report 3 (2026-10-11)

## 1. Result

- Branch `fix/sweep-verify-engine`, head `462cfbf`. `fix/integration` merged in up to `cf28532`; integration has since moved to `cb35773` (sweep-triggers' sixth follow-up), which is NOT in this run.
- **Clean build** at `462cfbf`, all gate targets:
  - `riftbound_tests`: 2746 passed, 0 failed.
  - `riftbound_openspiel_tests`: 25 passed. `riftbound_clone_equiv_test`: 10/10.
  - Smoke 200 games rc=0, 0 draws. Fault injection rc=0 without, rc=1 with the injected fault.
  - ISMCTS, two command lines, ten runs each: 20/20 rc=0.
  - 0 warnings in the five engine targets.
- Red-first: one mutation build (`red_m6.txt`): the four behavioural tests below failed with the fixes disabled.

## 2. Items closed

- **9.Y1** FIXED. `GameEngine::commitEngineChosenTarget` is the one tail (Deflect, "chosen" event, `noteChosen`) for every target the engine asks for: at finalization and in `makeNewChoicesFor`'s generic branch. That branch no longer offers a target whose Deflect cannot be paid. A Repeat execution of such a spell uses the same branch. Tests: `VerifyEngineTest.MakeNewChoices_AReChosenTargetPaysDeflectAndCounts`, `...ATargetWhoseDeflectCannotBePaidIsNotOffered`.
- **9.Y2** FIXED as a guard, not a new path: `VerifyEngineTest.Registry_OnlySpellsDeclareRepeatCosts` fails when a registered non-spell overrides `repeatCosts()` (it proves it can fail on a test unit). CLAUDE.md says Repeat is spells-only in the engine. No card prints Repeat on an ability.
- **9.Y3** FIXED. An effect-played spell is marked `Finalized` and `ChainItemFinalizedEvent` is emitted as its choices are made. Test: `VerifyEngineTest.EffectPlayedSpell_IsFinalizedAndAnnouncedAsItIsPutOnTheChain`.
- **9.Z1 / 9.W1** FIXED. `splitDamageAmong(..., bool may_choose_none = false)`: the first prompt also offers "No more targets", after the units. Volibear, Furious 41 passes `true`. `VerifyCardsTest.VolibearFurious_FirstPromptOffersDamagingNoUnit` enabled. `VerifyEngineTest.Volibear_ASplitTargetThatLeftInResponseIsDropped` and the CR 355.14.b pin still pass.

## 3. Items not closed

None assigned to this stream are open.

## 4. Needs another stream

- **sweep-costs (9.N2 / 9.T1) — one of the two conditions I gave has changed.** `finalized_early_` no longer exists. An item whose choices are made before the chain's Finalize step is now marked `ChainItemStatus::Finalized` on the spot; `stepFinalize` only takes Pending items, so it is not asked again. If the play path makes a spell's choices early, set the status the same way (and emit `ChainItemFinalizedEvent` once). The other condition stands: Repeat choices need `repeats_paid` on the item first.
- Test of another stream changed: `VerifyCardsTest.VolibearFurious_FirstPromptOffersDamagingNoUnit` (enabled, comment updated).
- Not merged here: `fix/integration` after `cf28532`.

## 5. New defects found

None.

## New state fields

None in this follow-up. `GameEngine::finalized_early_` (call-stack bookkeeping from follow-up 2) is removed.
