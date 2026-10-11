# sweep-verify-cards — sixth tranche report (9.R1)

## 1. Result

- Branch `fix/sweep-verify-cards`, head `e2f9a07` (merged with the current `fix/integration`; working tree clean).
- Clean build (PCH-safe launcher, lane B, `-j3`), 0 warnings: **`riftbound_tests` 2643 passed, 0 failed
  (1 disabled: the Mystic Vortex test waiting on 9.R4); `riftbound_openspiel_tests` 24 passed;
  clone-equivalence 10/10.** Not run: smoke, MCTS batch, fault injection.
- This is against integration WITHOUT cleanup's strict-agent follow-up (you undid that merge). My seven
  tranche 5 tests that fail under the strict `choosing(id)` are untouched, as asked.

## 2. Closed

**9.R1 Fox-Fire 253 — FIXED** (`src/cards/spells/0253_fox_fire.cpp`, one commit, ticked in plans/09).
- What was wrong was more than the item said. Only the FIRST unit was chosen as the spell was played; every
  further unit was chosen at resolution with its current Might, so there was no group to re-judge. And if
  that first unit outgrew the total in response, the engine dropped it as an illegal target and the card
  started over on any unit on the board — a unit that was never chosen could die.
- Now (CR 355.11.a) the whole group is chosen as the spell is finalized (`kFinalizeResolve`, same prompt
  sequence as before: first unit, then "Stop here" or another unit that fits). At resolution the chosen units
  that are still units at a battlefield die unasked if they are at one battlefield with total Might 4 or less.
  Otherwise (CR 355.11.b / 355.11.b.1) the controller picks among the ORIGINAL units only — one battlefield,
  within the total, no "stop" — so the subset holds as many as fit, as the rule's own example requires
  ({2, 2} or {1, 1, 2}, never {1, 1}).
- Tests, all engine path, all red against the unfixed card (5 of 5, the pin included, because the old card
  chose at the wrong time):
  `FoxFire_TotalNoLongerFits_TheControllerChoosesASubsetOfTheChosenUnits` (the CR example),
  `FoxFire_TotalNoLongerFits_SubsetHoldsAsManyUnitsAsFit`,
  `FoxFire_TheOnlyChosenUnitOutgrowsTheTotal_NothingElseIsKilled`,
  `FoxFire_ChosenUnitsNoLongerAtOneBattlefield_OneGroupIsChosen`,
  `FoxFire_UnchangedBoard_KillsTheChosenUnitsWithNoFurtherPrompt`.
  They answer every prompt explicitly through a scripted picker of their own (`pickingInOrder`), not
  `EnginePathAgent::choosing(id)`, so the strict agent does not affect them.
- Existing Fox-Fire tests (ReviewFixC2Test.FoxFire_*, the two "more than twelve" tests) pass unchanged.

**Prompt change for the trainer:** Fox-Fire's unit prompts all come at finalization now (before the opponent
can respond), with a possible second round at resolution when the group no longer fits.

## 3. Not closed

Nothing of mine. Still with others, unchanged from the fifth report: 9.M29's last comment and 9.O1's engine
reader (9.R4), the Twisted Fate 200 question and the look-then-reveal judgement (9.R5), paid-flag residue
(9.R2), the `encodeAction` assertion (9.R3).

## 4. Needs another stream

No `shared:` commits; only `0253_fox_fire.cpp`, my test file and plans/09 changed.

## 5. New defects found (none fixed; all engine-side, seen while fixing Fox-Fire)

- Deflect for a `kFinalizeResolve` spell: the first chosen unit's Deflect is paid in the finalize pass (`commitChosenObject`), not together with the spell's cost — the engine's pay-with-the-cost path exists only for `needsPlayTimeTarget` spells. Units whose Deflect cannot be paid are not offered, so nothing is unpaid; only the moment differs.
- `generateHiddenPlayActions` does not consult `hasLegalTargets` for a spell that chooses in its finalize pass: a hidden Fox-Fire (or Moonfall, Rocket Barrage — same idiom) can be played from facedown at a battlefield with no eligible unit and do nothing, where CR 811.1.d says it cannot be played.
- Fox-Fire from hand still requires at least one eligible unit and has no "choose none" on its first prompt; CR 355.13 would allow zero targets. Kept as it was; decide whether a zero-target play should be legal.
- When no finalize pass runs for a spell (put on the chain mid-resolution, after `makeNewChoicesFor`, or a Repeat execution) its "at finalization" choices are made at resolution and take effect at once — the engine's existing rule for late choices, so the CR 355.11 re-choice cannot apply there.
