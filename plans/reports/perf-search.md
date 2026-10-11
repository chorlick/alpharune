# perf-search — final report

## 1. Result

- Branch `fix/perf-search`, head `b549cc4`, rebased onto `fix/integration` at `fc2275d` with no conflicts.
- After the rebase: reconfigured with the PCH-safe launcher (`integration/scripts/ccache_pch_safe.sh`), `cmake --build build --target clean`, full rebuild at `-j3` on lane B.
- **Full test suite: 2257 passed, 0 failed** (`riftbound_tests`, 1 disabled test that was already disabled).
- `riftbound_openspiel_tests`: 20 passed, 0 failed. `riftbound_clone_equiv_test`: 10/10.
- Fault injection (`RIFTBOUND_STEP_FAULT_AT=40`): rc=1 with "injected engine fault", as required.
- Smoke, 200 games fiora vs yi_rebuild, seed 20261010, 6 threads: rc=0, P1 137 / P2 63 / 0 draws. A 4-game mcts-vs-ismcts batch also completed.
- No compiler warnings in the files this stream changed (one `-Wdangling-else` in `openspiel_tests.cpp` fixed).

## 2. Items closed

All four are FIXED and ticked in `plans/06-performance.md` with the numbers under each item.

| Item | What changed | Before | After |
|---|---|---|---|
| 6.7 | `~StepDriver` unwinds the parked fiber instead of playing the game out | teardown of an advanced clone 464 us (replayed) / 438 us (resumed) | 47 us / 36 us |
| 6.4 | `currentStep` / `applyChoice` / `beginGame` / `resumeFromSnapshot` return `const StepResult&`; `StepDriver` points at the parked legal list; `legalIntents()` returns a reference; `Prior` encodes each Intent once | `CurrentPlayer` 128 ns, `legalIntents` 113 ns, `LegalActions` 170 ns | 11 ns, 5 ns, 61 ns |
| 6.8 | one process-wide const card table (`openspiel::sharedCards()`) instead of one per `LoadGame` | `LoadGame` 1385 us | 135 us (the two deck files, still read per game on purpose) |
| 6.9 | one pass over the objects per extraction (`BoardIndex`), buffer-reusing overload, per-thread buffers in `InfoSetId` / `ObservationTensor`, zero-run skipping in `computeInfoSetId` | `InfoSetId` 34.0 us, `ObservationTensor` 6.3 us, `extractStateFeatures` 5.7 us, `computeInfoSetId` 27.6 us | 5.0 us, 3.3 us, 2.7 us, 2.2 us |

Carry-over row "6.7: a clone at a non-resumable decision with no resumable ancestor replays from game start" is also FIXED: the first clone that replays a long way leaves the last resumable decision it passed on the shared snapshot, siblings start there, and the original state adopts it. Per clone at decision 100: 1588 us -> 255 us. The first clone of such a position costs more than before (2.0 ms vs 1.6 ms) because it takes snapshots on the way; that is bounded to the last 64 decisions.

Wall time, 1 thread, medians of three runs alternated between old and new binaries:

| Batch | Before | After 6.7 | Final |
|---|---|---|---|
| `mcts:sims=50` vs random, 5 games, seed 20261010 | 36.7 s | 10.9 s | 11.3 s (identical game results in all three) |
| `ismcts:sims=20` vs random, 8 games, seed 1 | 19.8–21.9 s | 5.6–6.3 s | 3.3 s |
| random vs random, 200 games, 6 threads, seed 20261010 | 0.095 s | 0.095 s | 0.095 s |

- Almost all of the MCTS gain is 6.7. 6.4 is real per call but invisible in wall time; 6.9 shows up in ISMCTS, which keys every node on the hash.
- The 200-game random batch does not touch any of the four items (no search agent, no OpenSpiel state), so it is flat. That is expected, not a failed change.
- ISMCTS games are not reproducible run to run with a fixed seed (OpenSpiel's ISMCTS iterates an absl hash map), so its times compare similar work, not identical work.
- Values are unchanged: the microbench checksum over every observation value of 150 decisions x 2 seats is identical before and after, and the tests compare against a byte-wise reference hash and against the state queries the single pass replaced.

Measurement conditions: lane B lock held for every run; another lane was compiling during most runs (0–6 `cc1plus` processes, load average 3–6 on 8 cores). Each before/after pair was run back to back, alternating binaries.

One number I got wrong and corrected: an earlier 6.4 commit message claimed `Clone()` went from 8.7 us to 0.6 us. That was an allocator artefact, not a gain: glibc bills the fastbin consolidation after a clone's teardown to the next large allocation, which is sometimes inside `Clone()` and sometimes not (with `GLIBC_TUNABLES=glibc.malloc.mxfast=0` it is 0.4 us in every binary). The commit message was amended; `Clone()` itself is a pointer copy before and after.

Tests added:
- `tests/test_review_fix_perf_search.cpp`: `StepDriverTeardown.DestroyingAParkedDriverUnwindsInsteadOfDraining`, `PerfSearchTest.{DroppingAnEngineMidGameDoesNotPlayTheGameOut, AnEngineCanBeDroppedAtEveryDecisionOfAGame, AFailedEngineStillReportsItsFailureAndCanBeDropped, CurrentStepIsAViewOfThePendingDecision, FeaturesStillMatchTheStateQueriesTheyReplaced, ExtractingIntoAReusedBufferMatchesAFreshExtraction}`, `PerfSearchHash.SkippingZeroRunsDoesNotChangeTheInfoSetId`.
- `src/openspiel/openspiel_tests.cpp`: `RiftboundGameTest.EveryGameSharesOneCardTable`, `RiftboundStateTest.{ObservationTensorAndKeyMatchThePlainFunctions, OnlyTheFirstCloneOfAPositionReplaysTheWholeGame}`.
- The 6.7 and 6.4 tests were confirmed to fail on the old code. The 6.8 test compares addresses that were distinct by construction before. The 6.9 tests pin values that must not change, so they pass before and after by design.
- New benchmark: `riftbound_search_microbench` (`src/openspiel/search_microbench.cpp`); `riftbound_clone_advance_microbench` now also times teardown.

## 3. Items not closed

None.

## 4. Needs another stream

- No `shared:` commits. Two files outside the stream's list were touched for 6.4 and 6.7, both in the item's own "Where" list: `src/engine/game_engine.h` / `.cpp` (four return types to `const StepResult&`, and `step_driver_.reset()` first in the destructor).
- `src/main.cpp` still builds its own `CardRegistry` + `CardDB` at startup, so a process that also runs a search agent holds two card tables (once per process, not per game). Pointing `main.cpp` at `openspiel::sharedCards()`, or the reverse, is a small change in the CLI stream's file.
- Trainer note (source-compatible for most callers, but worth knowing):
  - `GameEngine::currentStep()` / `applyChoice()` / `beginGame()` / `resumeFromSnapshot()` and `RiftboundState::legalIntents()` now return references that the next applied choice overwrites. `auto step = engine.currentStep();` still copies; `const auto& step = ...` held across an `applyChoice` is now stale.
  - `RiftboundGame` no longer owns its card table; `cardDb()` / `cardRegistry()` are unchanged.
  - New: `ml::extractStateFeatures(state, perspective, db, out)` and `RiftboundState::decisionsReplayedToBuild()`.
  - `InfoSetId` / `ObservationString` / `ObservationTensor` values and the feature layout are unchanged.
- The build was switched to the PCH-safe launcher as instructed. Timings taken before the switch with binaries built after a header edit were discarded and re-measured from a clean build.
- Two short smoke runs after the final build were started without the lane lock by mistake (a 200-game random batch and a 4-game search batch, about 15 s together).

## 5. New defects found

- **Intermittent ISMCTS game failure, not reproduced on demand.** `GameEngine::resumeFromSnapshot` threw "the resumed engine did not reach the snapshotted decision" twice, both on binaries from before most of this stream's changes:
  - base `0ca1b19`: `--agent1 ismcts:sims=50 --agent2 random --games 2 --threads 1 --seed 20261010`, fiora vs yi_rebuild, game 1;
  - after 6.7 only: `ismcts:sims=20`, `--games 8 --seed 1`, game 1, once in about nine runs.
  It did not recur in 18 further runs (12 on the final pre-rebase binary, 6 on the after-6.7 one), so I cannot say whether it is still present. It means a decision that `isResumableDecision` accepts resumed to a different legal list. It may be the same defect as the carry-over row about `mainPhaseLoop` reaching its decision with an item on the chain, which integration has since changed; that is a guess, not a finding.
- **ISMCTS is not reproducible with a fixed seed** (see above). The cause is in OpenSpiel (`ISMCTSBot` builds its final policy by iterating an absl hash map, whose order is randomised per process), read from `is_mcts.cc`, not proven by experiment. MCTS is reproducible.
