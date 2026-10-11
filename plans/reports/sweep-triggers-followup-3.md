# sweep-triggers — third follow-up report (9.J5, 9.K2)

## 1. Result

- Branch `fix/sweep-triggers`, head `4f83bfa`, worktree clean. `fix/integration` is merged in (the last merge brought one file under `plans/`).
- Clean rebuild (`--target clean` first, lane B, `-j3`, `ccache_pch_safe.sh`) of all four targets at `4f83bfa`:
  - **full test suite: 2499 passed, 0 failed** (1 disabled, as before)
  - `riftbound_openspiel_tests`: 20 passed, 0 failed
  - clone-equivalence: 10/10 passed, rc=0
  - smoke: 40 random games, rc=0
- **Fault-injection check from `gate.sh`, run this time:**
  - MCTS batch without the variable: rc=0
  - with `RIFTBOUND_STEP_FAULT_AT=40`: rc=1, "a game failed: game 1 [seed=7]: RIFTBOUND_STEP_FAULT_AT: injected engine fault at decision 40", no draw reported

Both boxes are ticked in `plans/09-found-during-fix-run.md`.

## 2. Items closed

| Item | Outcome | What changed | Test |
|---|---|---|---|
| **9.J5** `killObject` guard | FIXED | The `zone == Trash` early return is removed. One guard remains, "not on the board" (CR 429.2.a), with a comment naming where a dead or banished-on-death unit can be. | `SweepTriggersTest.KillObject_AUnitBanishedOnDeathIsNotKilledAgain` |
| **9.K2** batch stamp misses given Might / keywords | FIXED | The stamp now includes what a unit was given: "this turn" keywords, temp Assault / Shield / Deflect values, temp buffs, Empowered (beside the temp Might and buffs already there). Computed totals stay out, so an aura ending mid-batch is still not a change. | `SweepTriggersTest.DeathBatch_ASurvivorGivenAKeywordIsSnapshottedAfresh` |

The 9.J5 test banishes the unit with the "banish it instead" flag (Smite's), not with Endless Riches 809 itself; the second kill hits the same guard either way.

## 3. Mutation run (one build)

| Fix disabled | Red test |
|---|---|
| guard put back to `zone == Trash` only | `KillObject_AUnitBanishedOnDeathIsNotKilledAgain` (and `ExecutorReviewFixTest.OffBoard_*`, 2 tests) |
| "this turn" keywords left out of the stamp | `DeathBatch_ASurvivorGivenAKeywordIsSnapshottedAfresh` |

## 4. Needs another stream

Nothing. Both changes are in `src/engine/effect_executor.cpp` and my test file.

## 5. New defects found

None in this round.
