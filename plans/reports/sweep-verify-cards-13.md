# sweep-verify-cards — thirteenth tranche report (9.HH1)

## 1. Result

- Branch `fix/sweep-verify-cards`, head `46dd31b`; **verified against `fix/integration` at `16e081b`**
  (merged in, no conflict; working tree clean; the mutant was restored before the gate — the gate script
  printed "dirty files: 0").
- The whole gate set on my worktree (lane B, `-j3` / `--threads 3`), clean build of all four targets, 0 warnings:
  - `riftbound_tests`: **2891 passed, 0 failed**
  - `riftbound_openspiel_tests`: **25 passed**
  - clone-equivalence: **10/10**
  - smoke, 200 games fiora vs yi_rebuild, seed 20261010: **rc=0, P1 139 / P2 61 / 0 draws**
  - MCTS batch without the fault: **rc=0**; with `RIFTBOUND_STEP_FAULT_AT=40`: **rc=1**, "injected engine fault
    at decision 40", 0 draws.

## 2. Closed — 9.HH1, ticked on this branch

sweep-triggers was right: my 9.EE3 claim ("the re-check was dead because the helper already drops the pick")
was pinned by nothing. `EffectExecutor::readyObject` refuses a ready or off-board rune itself, so the five
tests' board assertions could not tell whether `selectSeveral` had dropped the stale picks.

- Each of the five pins now also asserts the card's own count of what the helper handed back, read from its
  trace line: Dark Child 315 and Gentle Gemdragon 666 log nothing when the count is 0 (asserted absent);
  Targon's Peak 284 logs "readied 0 runes" (asserted present); Curator of the Sands 932 and Sona 73 exposed
  nothing, so they now log "readied N rune(s)" as the other three do (asserted "readied 0").
- **Mutation run** (`selectSeveral` keeps a stale pick: `if (it == options.end()) { picked.push_back(stored);
  continue; }`), whole suite, on the merged tree: **exactly six tests go red** —
  `VerifyCardsTest.DarkChild315_…`, `TargonsPeak_…`, `GentleGemdragon_…`, `CuratorOfTheSands_…`,
  `Sona_ARuneReadiedOrGoneInResponseIsSkipped`, and `SweepTriggersTest.Reflexive_HweiARuneReadiedOrGoneInResponseIsSkipped`.
  2885 others stay green. The helper file was restored from backup afterwards (no git state touched).
- **Other callers checked for a count that depends on a removed re-check: none.** Albus Ferros 230 re-checks
  each pick (`location` and `buff_count`) before counting what it channels for; Azir 497 and Bard 540 re-check
  each pick before counting moves; Fae Dragon 423 counts `buffUnit`'s return. In the five rune cards the count
  feeds only the trace line.

## 3. Not closed

Nothing of mine.

## 4. Needs another stream

No `shared:` commits. Changed: two card files (a trace line each in 932 and 73), five tests in my test file,
plans/09 (the 9.HH1 tick).

## 5. New findings

- Under the same mutant these tests of mine stay green, by design of the callers rather than of the helper, so they do not pin the helper either: `FaeDragon_AUnitPickedEarlierLeavesBetweenTwoPrompts_TheOtherPickIsBuffed` and `AlbusFerros_ABuffPickedEarlierIsGoneBetweenTwoPrompts_TheOtherIsSpent` (the cards' own guards skip the stale pick), `DarkChild315_TheFirstOfTwoNamedRunesGone_TheSecondIsStillReadied` and `Sona_StoppedEarly_TheFirstNamedRuneGone_TheSecondIsStillReadied` (board state only). The helper's drop is now pinned by the six tests above; these four pin the cards' visible behaviour.
- The count in a trace line is the only thing these five cards expose about the helper's result. If the trace text is ever reworded, the five pins must follow; a helper-level unit test with a test card would not have that coupling.
