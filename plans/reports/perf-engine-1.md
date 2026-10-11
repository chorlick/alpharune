# perf-engine — interim report 1 (6.1, 6.6, 6.11, 9.X1)

Branch `fix/perf-engine`, head `6b7abc8`, four commits on top of integration `1e453fa` (integration has not moved). This tranche can be merged while the stream continues; the remaining items are listed in section 3.

## 1. Result

| Gate | Result |
|---|---|
| Build, all six targets, `-j3`, lane A, PCH-safe launcher | OK, **0 warnings** |
| `riftbound_tests` | 2909 run: **2908 passed, 1 failed** on the build of `09ca63d`'s sources — the one failure was my own new test asking for more compared log lines than the games produce (7453 against a bar of 10000). The bar is 3000 in the committed file and the test passes (run on the next build). No existing test failed. |
| `riftbound_openspiel_tests` | 25 passed |
| `riftbound_clone_equiv_test` | 10/10 |
| smoke, 200 games, seed 20261010, 6 threads | rc 0, **P1 139 / P2 61 / 0 draws, 17 turns, 293 decisions** (unchanged) |
| MCTS batch (4 games, seed 7) | rc 0 |
| same with `RIFTBOUND_STEP_FAULT_AT=40` | rc 1, failure named |
| search microbench observation checksum | `94739f84565d4d16` (unchanged) |

This is not the final gate: no clean rebuild yet and the ISMCTS ten-run check is still to do. Both come with the final report.

## 2. Items closed

All timings: medians of three runs, old and new binaries alternated, lane A lock held for every run, nothing else compiling (load average 1.1–1.8, 0 `cc1plus`).

| Item | Outcome | Before | After |
|---|---|---|---|
| 6.1 | FIXED | random 1000 games, 1 thread: 2.974 s | 2.438 s |
| | | random 200 games, 6 threads: 0.109 s | 0.090 s |
| | | `mcts:sims=50`, 5 games: 5.15 s | 4.69 s |
| | | `ismcts:sims=20`, 8 games: 3.27 s | 3.02 s |
| | | Irelia-deck batch: 802 ms | 681 ms |
| 6.6 | FIXED | random 1000 games: 2.433 s | 2.410 s |
| 6.11 (all three) | FIXED | random 1000 games: 2.410 s | 2.354 s |
| | | Irelia-deck batch: 681 ms | 650 ms |
| 9.X1 | FIXED | — | 19 checks pass |

**6.1 — a log line nobody reads is not built.**
- `EventBus::logging()` is true while a slot is connected to `on_log` and the bus is not in a quiet stretch. `on_log` is now a `LogSignal`: it connects like a Boost signal and knows whether anyone is connected without a mutex.
- All 910 log call sites in `src/` use `RIFT_TRACE` / `RIFT_DEBUG` / `RIFT_TRACE_PRIVATE` / … (`core/events.h`), which evaluate the message only while `logging()` is true. The conversion was scripted; the script flagged no message with a side effect. Thirteen sites that built a string ahead of the call are guarded by hand.
- The `[D:NNN.NN]` counters still advance for an unread line, so a listener that attaches in mid-game sees the prefixes it would have seen from the start.
- A quiet stretch (the pre-play probes) now drops events as they are emitted instead of storing and discarding them, and builds no line. That is most of the Irelia-batch gain.
- **Evidence that output is unchanged with a listener:** `--trace --debug`, `--debug` and the HTML replay of six fixed-seed games, plus a three-game batch replay — 19 files, 50,120 lines of stdout, 14.7 MB — are byte-identical between the binary before and after.
- Tests (`tests/test_review_fix_perf_engine.cpp`): `PerfEngineLog.{AMessageNobodyReadsIsNotBuilt, EveryMacroDeliversWhatItsFunctionDelivers, AnUnreadDecisionLineStillStartsANewDecision, AListenerThatLeavesStopsTheBuilding, AQuietStretchBuildsNothingAndIsStillCounted}`, `PerfEngineGame.{AListenerAttachedInMidGameIsHandedWhatOneAttachedFromTheStartWas, AGameIsTheSameWithAndWithoutAListener}`.

**6.6 — `processLethalDamage`.** "Any amount of your damage is enough to kill" is asked only for a unit carrying damage short of lethal, once per player per call (it was 220,273 walks per 1000 random games). The `ability_text` searches the item names were already gone. Tests `PerfEngineCards.{TheElderRuleIsAskedOnlyForAUnitCarryingDamageShortOfLethal, DamageAUnitDidToItsOwnSideIsNotTheOtherPlayersDamage}`, both shown red on the old code.

**6.11 — three small items.**
- `card_counters["__was_mighty"]` is `GameObject::was_mighty`. Nothing else read the counter. Test `PerfEngineCards.BecomingMightyIsAnnouncedOncePerCrossing`.
- `refreshFlowGrants`: a spell with no counters is not looked up; the key is a static string. The rebuild itself stays (the grants are a continuous effect).
- `advanceCombatDamage`: the dedup key is the sorted (target, damage) list, the walk a lambda handed itself. Same options, same order.
- Not measured one by one: together they are 2.3% of the random batch, and the first is nearly all of it.

**9.X1 — the launcher check.** Three new cases in `scripts/check_ccache_pch_launcher.sh`:
- C: the PCH rebuilt by something other than the launcher after a header change, beside the old `.gch.sum` → the next unit is compiled, the stale `.sum` is gone.
- D: after `clean` (it removes the `.gch` and leaves the `.sum`: confirmed) → no unit hits on the old `.sum`, whoever builds the next PCH.
- E: a failed PCH compile leaves neither file; the repaired build is a cache hit again.
- All 19 checks pass. Against a copy of the launcher without the "older than its `.gch`" rule, exactly the three checks that depend on it fail.
- The one state the rule does not catch is an older `.gch` put back by hand with its timestamps kept. Nothing in cmake, ninja, ccache or the compiler does that; I left the launcher as it is.

## 3. Items not closed yet (in progress, not blocked)

| Item | State |
|---|---|
| 6.10 | Written, compiling. `GameState::objects` becomes an `ObjectTable`: the same map, plus lists of the objects in play by location. `GameObject::zone` / `location` are tracked fields, so any assignment marks the lists stale; the next query rebuilds them. |
| 6.2 | The text parsing it names is already gone. The remaining passes over all objects are being cut with 6.10. |
| 6.3, 6.5 | Written on top of 6.10 (trigger types and activated abilities cached per card, `CardRegistry::get` a table lookup, scans over the board only). Not yet built or measured. |
| 9.HH4, 9.II1 | Designed, not started. 6.1 already took the Irelia batch from 802 ms to 650 ms. |
| `main.cpp` card table | ALREADY FIXED on integration (`af4442f`, 9.E3). I will confirm it in the final report. |

## 4. Needs another stream / trainer notes

- No `shared:` commits.
- Trainer: `EventBus::on_log` is a `LogSignal`, not a `boost::signals2::signal`. `on_log.connect(fn)` and the returned connection work as before; code that names the signal's type, or calls a signal member other than `connect`, `disconnect_all_slots`, `num_slots`, `empty`, needs a look.
- Trainer: a message passed to a `RIFT_*` macro is not evaluated when nobody listens. Engine code only; nothing in the trainer should call them.

## 5. New defects found (not fixed, not in any plan)

Both from a gprof profile of `mcts:sims=50`:
- `knowledge::prune` (`src/engine/knowledge_tracker.cpp`) builds a `std::map` of every card in both hands and decks at every decision: about 8% of the MCTS batch with `ownerOf` and the tree operations.
- `RiftboundState::ToString` is called 2.5 million times in a five-game `mcts:sims=50` batch (about 4% of it). I have not looked for the caller.
