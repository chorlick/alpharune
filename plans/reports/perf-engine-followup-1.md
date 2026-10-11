# perf-engine — follow-up 1 (plans/09 section KK: 9.KK2, 9.KK3, 9.KK4)

Branch `fix/perf-engine`, head `8d4b029`. **Verified against integration `40f8737`**, merged in before the clean build (it brought two lines of `plans/STATUS.md`).

## 1. Result

Clean rebuild (`cmake --build build --target clean`, all six targets, `-j3`, lane A lock, `scripts/ccache_pch_safe.sh`, fresh PCH): **0 warnings**.

| Gate | Result |
|---|---|
| `riftbound_tests` | **2927 passed, 0 failed** |
| `riftbound_openspiel_tests` | 25 passed |
| `riftbound_clone_equiv_test` | 10/10 |
| smoke, 200 games, seed 20261010, 6 threads | rc 0, **P1 139 / P2 61 / 0 draws, 17 turns, 293 decisions** (unchanged) |
| MCTS batch, 4 games, seed 7 | rc 0 |
| same with `RIFTBOUND_STEP_FAULT_AT=40` | rc 1, "injected engine fault at decision 40", 0 draws |
| search microbench observation checksum | `94739f84565d4d16` (unchanged) |
| trace / debug / HTML replay with a listener, 19 files | byte-identical to the binary at `1e453fa` |
| `ismcts:sims=50`, 2 games, seed 20261010, ten runs | **10/10 clean** |
| `ismcts:sims=20`, 8 games, seed 1, ten runs | **10/10 clean** |
| suite with `-DRIFTBOUND_CHECK_BOARD_INDEX=ON` (separate build, final sources) | 2927 passed; OpenSpiel 25; clone-equivalence 10/10; smoke split the same |

Medians of three, binaries alternated, lane lock held, nothing else compiling (load 1.0–1.7):

| Batch | At `1e453fa` | Before this follow-up (`555a4d6`) | Now |
|---|---|---|---|
| random vs random, 1000 games, 1 thread | 3.008 s | 1.628 s | 1.387 s |
| random vs random, 200 games, 6 threads | 0.108 s | 0.063 s | 0.054 s |
| `mcts:sims=50` vs random, 5 games | 5.19 s | 3.64 s | 3.60 s |
| `ismcts:sims=20` vs random, 8 games | 3.24 s | 2.51 s | 2.47 s |
| Irelia-deck batch | 805 ms | 407 ms | 345 ms |

## 2. Items closed

| Item | Outcome | random 1000×1 | Irelia batch | `mcts:sims=50` | `ismcts:sims=20` |
|---|---|---|---|---|---|
| 9.KK4 | FIXED | 1.631 → 1.652 s, then 1.633 → 1.626 s | 407 → 408 ms | 3.59 → 3.60 s | 2.46 → 2.52 s |
| 9.KK3 | FIXED | 1.647 → 1.375 s | 409 → 350 ms | 3.65 → 3.56 s | 2.46 → 2.51 s |
| 9.KK2 | FIXED | 1.382 → 1.352 s | 352 → 342 ms | 3.59 → 3.52 s | 2.47 → 2.43 s |

ISMCTS differences in these rows are inside its run-to-run spread (about 3%).

**9.KK4 — a const state can be queried from several threads.**
- Chosen: the guarded rebuild. The stale flag is atomic, a clean table is read with one atomic load and no lock, and the first query of a stale table rebuilds under a mutex while the others wait.
- Why not eager lists at the points a state becomes shared: the engine cannot see those points. A trainer shares a `const GameState&` by handing it to a thread pool.
- Cost: not measurable. Two sessions gave +1.3% and −0.4% on the random batch, which is the batch's own spread. I also built a variant that skipped the range count in pure counting loops; it measured no better and I dropped it.
- Test: `PerfEngineTable.AConstStateCanBeQueriedFromSeveralThreadsAtOnce` — four threads, 400 rounds, each right after a move; every answer equals a walk and the lists are rebuilt exactly once per round.
- On the old code: the same scenario, built standalone against the table as it was, ended in heap corruption or a segfault in 5 runs of 5. Against the new table: clean in 3 of 3.
- The checked build runs the test too.
- Writing to a state while another thread reads it is still not safe, as for any data.

**9.KK3 — the walks over every object left in the engine.**

Timed in place with temporary timers over 1000 random games:

| Walk | Before | After | What was done |
|---|---|---|---|
| cleanup checksum | 194 ms (11.9%) | 65 ms (4.7%) | fields combined without a multiply chain per field; a pass reuses the previous pass's closing value |
| aura clear | 89 ms (5.4%) | 67 ms (4.9%) | the fields it writes are declared together |
| designation reset | 43 ms (2.7%) | 32 ms (2.3%) | fields at the front of the object |
| "becomes Mighty" | 40 ms (2.5%) | gone | a stamp on the unit that goes stale off the board; only units on the board are visited |
| aura fold | 32 ms (2.0%) | 27 ms (2.0%) | fields together |
| control reversion | 32 ms (2.0%) | 18 ms (1.3%) | its flag at the front of the object |
| "if they didn't already" pass | 30 ms (1.8%) | 0 in this batch | skipped unless the fold just before met such an effect |
| per-turn expirations (three) | 27 ms (1.7%) | 25 ms (1.8%) | — |
| per-combat walks (two) | 2 ms | 2 ms | — |
| **total** | **490 ms of 1.63 s (30%)** | **236 ms of 1.38 s (17%)** | |

- Every walk that stays has a comment at its site saying why it must see every zone and its share.
- I did not use a dirty set. Which off-board cards carry a status is written by card code in several hundred places; a set of them would be exactly the kind of index that can go stale.
- The checksum decides "did this pass change anything" by equality only. A different mixing function gives the same decision short of a 64-bit collision, as the old one did.
- `GameObject`'s members were reordered (the most-read fields first, the aura-reset fields together). No field was added to or removed from the feature layout.
- Tests: `PerfEngineCards.BecomingMightyIsAnnouncedOncePerCrossing` (extended), `PerfEngineCards.AnAuraPassClearsWhatTheLastOneLeftOnACardOffTheBoard`, plus the unchanged-behaviour gates.

**9.KK2 — the loops in `src/cards`.**
- All 359 range-for loops over `state.objects` were classified. **Converted 337** (331 to `onBoard()`, 6 to `inPlay()`). **Kept 22**, each with a `// (Every object: …)` comment.
- How the 337 were checked:
  - 290: a parser of the loop proved the body does nothing for an object without a location before anything else happens. I listed every function called in those bodies: accessors, `push_back` and the aura helpers.
  - 21: read one by one because the body calls the executor. They ready, buff, give "this turn" stats, or collect ids for a kill or bounce done after the loop. None moves an object inside the loop.
  - 26: read one by one because the filter sits in a helper (`readiable`, `movable`, `isFodder`, …) or has an unusual shape.
- The 22 kept, by reason:

| Reason | Loops |
|---|---|
| finds its own instances by card id, in any zone | 11 (4 legends, Kayn, Fiora Peerless, Crimson Pigeons, Brutalizer, Shurelya's Requiem, Jagged Cutlass, Cursed Sarcophagus) |
| facedown cards | 3 (Mushroom Pouch, Pack of Wonders, Monster Harpoon) |
| Equipment by attachment / Sand Soldiers by name, any zone | 2 (Purifier, Emperor of the Sands) |
| Equipment in a hand (Quick-Draw) | 1 (Jax) |
| Legends | 1 (Royal Entourage) |
| battlefield cards | 1 (Black Flame Altar) |
| Stunned units in any zone | 1 (Monch) |
| combat designations | 1 (Crimson Pigeons) |
| `Card::costStateFingerprint` | 1 |

- Four "find my legend" loops were converted first and put back. One test fixture builds a legend without putting it in the Legend Zone and calls the hook directly; the old loop found it and behaviour had to stay.
- The gain on these batches is small (2%) because the two smoke decks play few cards with such loops. Ranking from the gprof profile at `1e453fa`: `WujuBladesmanStarter::applyPassiveAura` 1.6%, `Sacrifice::hasLegalTargets`, `PitRookie::onTrigger` and Rampage's unit list 0.8% each. I did not re-profile after.
- Test: `PerfEngineSources.CardCodeWalksEveryObjectOnlyWhereItIsListed` — a source scan of `src/cards` that fails on a walk over every object that is not on its allow-list (21 files, each with its reason) or has no comment.
- The checked build passed the whole suite, so no converted loop moves an object under its range on any path the suite reaches. Paths the suite does not reach are covered only by the reading above.

## 3. Items not closed

None of mine. 9.KK1 (the per-object copy) belongs to the strong-ids stage.

## 4. Needs another stream / trainer notes

- No `shared:` commits.
- Trainer: `GameObject`'s member order changed and `was_mighty` (added in tranche 1) is now `uint32_t mighty_seen`; `GameState` has a new `uint32_t mighty_checks`. Code that constructs a `GameObject` field by field is unaffected.
- Trainer: a `const GameState` may now be queried from several threads. This replaces the note in the previous report.

## 5. New defects found (not fixed)

Found while reading the card loops. Each was left as it behaves today.

- **Monch 597** counts a Stunned enemy unit in any zone: a unit that was stunned and then bounced still counts until the Ending Step.
- **Emperor of the Sands 512** gives [Weaponmaster] to every object named "Sand Soldier" its controller has, in any zone, and **Purifier 502** gives [Assault] through any Equipment with `attached_to` set, on the board or not. Probably harmless; neither checks the board.
- **Test fixture:** `addLegend` in `tests/cards/test_rules_compliance_cards.cpp` creates a legend without `zone = LegendZone`. Any card code that looks for its legend among the objects in play will not find it in those tests.

## 6. How the work was run

- Lane A lock for every configure, build, full-suite run and timing run.
- Run without the lock: trace captures, single microbench runs, filtered runs of a few tests, and the standalone two-thread program.
- Temporary timers were added to `game_engine.cpp` to time each walk and removed again (restored from a backup copy); nothing of them is committed.
- No `rm` in this follow-up.
