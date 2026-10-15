# Batch 5c-perf — game throughput (I4, N4)

Branch `fix/batch-5c-perf`, from master `582e857`, master `363848c` (batch 5b) merged in `fb711e0`. Nine commits
of optimisation, each one exact: **every fixed-seed game is the same game as on master.**

## 1. Result

1,000 games, 1 thread, fiora vs yi_rebuild, seed 20261010, under the run lock, the three builds alternating, five
rounds (10:15, load 2.4-3.3: lane B building beside me):

| build | games/s (5 runs) | mean games/s | decisions/game | mean decisions/s |
|---|---|---|---|---|
| `582e857` master before 5b | 574.5 591.0 610.4 577.0 617.6 | 594.1 | 288 | 171.1k |
| `363848c` master with 5b (the baseline) | 572.3 580.8 614.0 567.3 585.6 | 584.0 | 285 | 166.4k |
| final (`df1680a`) | 658.2 643.5 680.5 672.7 632.6 | **657.5** | 285 | **187.4k** |

**+12.6% games/s and decisions/s over master** (5b's cost included: master with 5b is 1.7% below master before it
in this window; 285 against 288 decisions is 5b's different games). Fuzz on 3 threads (100,000 games each,
same seeds, both builds): random 1,672 -> 1,923 games/s (+15%), random-player 1,710 -> 1,937 (+13%), pass=0
1,671 -> 1,824 (+9%).

**Not reached: the pre-batch-1 rate.** 783 games/s x 278 decisions = 218k decisions/s (measured on a quieter
machine); the final tree is at 187k on this one, **14% short per decision** (section 5 says what is left and why).

## 2. Profile before and after

perf here has no libelf, so a second build (`build-prof`: the same -O3 with `-fno-omit-frame-pointer -g1`) was
profiled and symbolised with nm / addr2line (scripts in the session scratchpad); malloc call sites by an
`LD_PRELOAD` shim that samples every 64th call's stack. 3,000 games each. Per game, at each tree's own rate:

| | before (`582e857`) | after | |
|---|---|---|---|
| a game | 1,683 us | 1,521 us | -10% |
| `cleanup` (all of it) | 27.2% = 458 us | 27.1% = 412 us | |
| — `recalculateAuras` | 14.8% = 249 us | 15.7% = 238 us | |
| `generateLegalActions` | 16.2% = 272 us | 10.6% = 161 us | -41% |
| signals2 dispatch (handlers included) | 16.3% = 275 us | 17.3% = 263 us | |
| `ObjectTable::lists` (board index) | 5.6% = 95 us | 4.4% = 67 us | |
| `prePlayProbe` | 4.4% = 75 us | 3.1% = 47 us | |
| `GameState` copies (probes, optional costs) | 3.2% = 54 us | 2.7% = 40 us | |
| malloc / free (libc self) | 12.9% | 12.7% | 9,174 mallocs a game before |

Counts per game (temporary counters, removed): 273 Cleanup calls, 247 run, 254 passes; **94% of the Cleanups
that run change nothing**, but nearly all of them follow a real change (a rune exhausted, a card played or
resolved), so batch 2's skip ("nothing announced since a Cleanup that found nothing") cannot be widened; 102
legal-action generations; 145 board-index rebuilds; 3.1 pre-play probes (half of them repeats, see 3.1).

## 3. The optimisations

Each: what it does, the gain measured alone (alternating runs, noise about +-2%), and its proof. **Proof** for
every commit: smoke (200 games, seed 20261010, 3 threads), both ISMCTS fixed-seed commands of batch 3 (output
and an md5 of the `sims=50` game's `--trace`), and `deck_fuzz --rows` on 3 x 3,200 games (random, random-player,
pass=0, seed 5150): **identical to master's**, re-recorded on the merged tree after batch 5b (smoke 136 / 64 / 0 /
288). Before the merge the same set was identical to `582e857`'s (128 / 72 / 0 / 288).

1. **`abf7c9a` A main-phase decision's legal actions are generated once** (with 2: +7%, the largest). `advanceMainPhase`
   generated the list to see whether a decision is due and `queryAgent` generated it again for the same state, with
   nothing in between: every play gate and pre-play probe of a main-phase decision ran twice (6.6% of a game).
   `mainPhaseLoop` now hands the list over (`queryAgent(player, actions)`). Exact: the second generation read the
   state the first left (a probe moves the game aside and back; its only side effect, `forgetCleanupFacts`, is
   idempotent).
2. **`4ee8212` `historyToken`'s fingerprint** (with 1: +7%): FNV-1a multiplies once per byte; a zero byte only multiplies,
   so the zero bytes above a word's highest non-zero byte are one multiply by the prime's power. Same value: checked
   against the old loop on 5e7 words of every shape (zero, 1 byte, 2 bytes, one byte at any position, negative).
3. **`1ce4074` The board index re-reads only the objects that moved** (+1.7%). A zone or location change marked the
   whole index stale and the next query walked every object (145 times a game). The tracked fields now report their
   object (`IndexSlot` in `tracked_field.h`); the index keeps the table's entries in its order with each entry's
   lists; an update re-reads the changed objects and refills the lists from those facts. The order is rebuilt only
   when an object is added or removed — the only changes that can reorder an `unordered_map` — so the lists keep the
   table's order (5b's N1: nothing that is processed in table order moves). The checked build compares every update
   with a walk, as it compared every query. New test `PerfEngineTable.AnUpdateThatReadsOnlyTheChangedObjectsMatchesAWalk`
   (3,000 random moves, additions, removals, copies, swaps); a scratch check ran 80,000 more on the checked variant.
4. **`65b2343` An aura pass reuses its (card, controller) list**: the vector grew again on every pass, the single
   most frequent allocation of a game (14% of all mallocs). Kept on the engine between passes; a nested pass takes
   a fresh one.
5. **`a723a55` The aura reset copies granted abilities only when there are some** (both empty for almost every
   object).
6. **`f85dcd2` `refreshFlowGrants` looks each trash card up once** (not `objectExists` + `getObject`).
7. **`0d97080` With no combat under way, the designation pass looks up no battlefield** (every designation is None).
8. **`175e957` The ability generator skips an object with nothing to activate before asking its gate**
   (`canActivateAbility` reads only the state).
   (4-8 together: +2%; each is exact by construction: same calls with the same arguments, minus calls whose result
   was not used, of functions that only read the state.)
9. **`f6896e3` The Cleanup's after-pass checksum is taken by the aura pass's last fold** (+2.2%). The fold is the
   last walk of a pass that writes a field the checksum reads and visits the objects the checksum walks, in the same
   order; it now mixes each object's word after folding it (`cleanupObjectWord`), and the Cleanup adds the
   battlefields and scores (`finishChecksum`). One walk fewer per pass. The checked build walks again and throws on
   a difference.
10. **`df1680a` The spell generator asks affordability after the timing and target gates** (+1%): in a Closed
    State most spells in hand stop at the timing gate, and `canAfford` was asked of each first.

## 4. Gate (final tree, master merged)

- Clean rebuild (`riftbound`, `riftbound_tests`, `riftbound_openspiel_tests`, `riftbound_clone_equiv_test`,
  `deck_fuzz`; lane A, `-j3`): **0 warnings**, in our code or elsewhere. Checked build (`build-checked`): 0 warnings.
- `riftbound_tests` **3,480 passed** (1 disabled, as on master; +1 new test). `riftbound_openspiel_tests` 33 passed.
  `riftbound_clone_equiv_test` 10/10. Checked-build suite: **3,480 passed**.
- No-change proofs on the final tree: section 3, all identical to master `363848c`.
- MCTS batch, `mcts:sims=8`, 4 games, seed 7: rc 0, P1 1 / P2 3, the same games as master's binary. With
  `RIFTBOUND_STEP_FAULT_AT=40`: rc 1, "a game failed: game 1 [seed=7]: RIFTBOUND_STEP_FAULT_AT: injected engine
  fault at decision 40", 0 draws.
- **Fuzz, optimised, 3 x 100,000** (`--decks 5000 --games-per-pair 20 --seed 5150701 --known
  docs/testing-known-failures.txt`): random, random-player, random-player:pass=0 — **0 failures** of any kind (no
  known, no new, no turn cap, no game without a winner or a reason, no chain of 200+, none hung). **All 300,000
  games identical row by row to master's `deck_fuzz` on the same seeds** (`--rows`; not only a subset).
- **Fuzz, checked, 3 x 20,000** (`--decks 1000 --games-per-pair 20 --seed 5150702`, same `--known`): **0 failures**,
  so no index update, skipped Cleanup or fused checksum disagreed with its recomputation in 60,000 games; and the
  60,000 rows are identical to master's (optimised) `deck_fuzz` on the same seeds.

## 5. What was not done as asked, and what remains

- **The pre-batch-1 rate is not reached** (187k against 218k decisions/s, -14%). What the time is now (section 2):
  the Cleanup that follows every step of a chain (27%: 247 a game, 94% of them changing nothing, each still
  re-deriving every continuous effect: ~1.7 us), legal-action generation (11%), signals2 dispatch with the trigger
  handlers (17%, of which signals2's own machinery — a mutex, a shared_ptr copy and a slot list walk per emit —
  is about 6%), allocation (13% of samples are malloc/free, ~9,000 calls a game, spread over Intent vectors, id
  vectors and the per-game setup), per-game setup and teardown (~4%: 110 objects, the bus and its connections).
- **The per-hook aura declaration (I4's idea) was judged and not built.** The hooks themselves are about 1.5% of a
  game; the aura pass's cost is the engine's walks around them (clear, fold, checksums), which run because the state
  did change. Skipping a pass needs to know that none of its inputs changed since the last one, and the state has no
  write tracking outside `zone` / `location`: a hook's declaration ("I read units' Might at my battlefield") is
  useless without knowing when those things were written. Making that possible means tracked fields for the
  hundred-odd fields hooks read and their thousands of write sites — the same mechanism as the board index, at
  that scale. That is the one structural lever left for the Cleanup; it is a project, not a perf batch.
- **Tried and dropped (no gain beyond noise):** a zone change within its list class not marking the index stale
  (cards played to the chain; ~3% of index rebuilds avoided); refilling only the lists a change touches; the four
  no-argument card virtuals (`canBeChosenByEnemy`, `continuousEffectReachesPiles`, `hasMightDependentAura`,
  `mightThreshold`) read from primed static facts. The walks are memory-bound: a virtual call or a skipped list
  fill is not what they wait on.
- **Not attempted, the remaining candidates by size:** a GameState copy per pre-play probe and per optional
  additional cost (2.7%; also the main cost of search, where every node copies); `refreshFlowGrants` reading every
  trash card on every aura pass (2.9%: cold pile objects; only a cache of what a trash card is would avoid it);
  `canAffordWith` walking the base runes once per card per generation (3%); `Intent` copies and their vectors
  (an `Intent` is 7 vectors; small-vector storage would cut most of the per-decision allocations); signals2 (the
  coordinator ruled out reworking the bus).
- The timings were taken beside lane B's builds (load 2.4-4.3); each comparison alternated the builds inside one
  run-lock hold, so the ratios hold, the absolute rates are lower than on an idle machine.

## 6. Files

`src/engine/game_engine.{h,cpp}` (1, 4-10), `src/openspiel/action_vocab.cpp` (2), `src/core/tracked_field.h`,
`src/core/object_table.h` (3), `tests/test_review_fix_perf_engine.cpp` (new test), `CLAUDE.md` (the board index
paragraph), `plans/completed/10-testing-phase.md` (I4, N4).
