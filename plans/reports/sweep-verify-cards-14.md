# sweep-verify-cards — fourteenth tranche report (9.HH5)

## 1. Result

- Branch `fix/sweep-verify-cards`, head `5c87493`; **verified against `fix/integration` at `d203ba8`**
  (already contained in the branch; working tree clean, mutant restored before the gate — "dirty files: 0").
- The whole gate set on my worktree (lane B, `-j3` / `--threads 3`), clean build of all four targets, 0 warnings:
  - `riftbound_tests`: **2892 passed, 0 failed**
  - `riftbound_openspiel_tests`: **25 passed**
  - clone-equivalence: **10/10**
  - smoke, 200 games fiora vs yi_rebuild, seed 20261010: **rc=0, P1 139 / P2 61 / 0 draws**
  - MCTS batch without the fault: **rc=0**; with `RIFTBOUND_STEP_FAULT_AT=40`: **rc=1**, "injected engine fault
    at decision 40", 0 draws.

## 2. Closed — 9.HH5, ticked (tests only)

- `VerifyCardsTest.SelectSeveral_AStoredPickNoLongerInTheListIsDroppedAndNothingIsAskedAgain` drives the
  shared helper through a test card in my test file's namespace (`SelectSeveralProbeCard`, id 1060 in the
  reserved range, registered by the test in its own registry): "when I conquer, choose up to three other
  friendly units". It names them at finalization and, as it resolves, writes down exactly what
  `selectSeveral` hands back.
- Scenario: three picks made before the opponent's first priority (asserted there); in response one picked
  unit leaves the board and a new candidate appears; at resolution the helper returns the other two, in pick
  order with no gap, and no fourth prompt arrives (strict script, prompt count 3).
- **Mutation run** (`selectSeveral` keeps a stale pick), whole suite: **seven tests red** — this one, the five
  rune pins (`DarkChild315_…`, `TargonsPeak_…`, `GentleGemdragon_…`, `CuratorOfTheSands_…`,
  `Sona_ARuneReadiedOrGoneInResponseIsSkipped`) and `SweepTriggersTest.Reflexive_HweiARuneReadiedOrGoneInResponseIsSkipped`;
  2885 others green. The helper was restored from backup afterwards.
- No card file and no engine file was touched: the only change is in
  `tests/cards/test_review_fix_verify_cards.cpp` (350 tests now) and the tick in plans/09.

## 3. Not closed

Nothing of mine.

## 4. Needs another stream

Nothing. No `shared:` commits.

## 5. New findings

- The helper-level test covers the "pick no longer in the list" half and "a longer list asks nothing new". Not covered at helper level: a duplicate-entry list (an object listed k times, as Albus Ferros does for buffs) with one instance gone between passes — Albus only asks at resolution, so no card exercises it across a finalize pass today.
