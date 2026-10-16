# Batch 7-perf — throughput, safe part (P1), the prompt carrier (Q2), the last id-order ties (Q1)

Branch `fix/batch-7-perf`, from master `c2615e4`; master (batch 6, `0faab64`) merged after Q1.
Commits: Q1 `54b50f9`; merge `31f3541`; parking slots `07243d4`; Q2 `c20d3eb`; option lists reserved `e61bab7`;
plans and this report on top.

## 1. Result

fiora v yi_rebuild, 2,000 games, 1 thread, seed 20261010, the builds alternating inside one run-lock hold, six
rounds (load 1.6-3.0; other streams' runs between my holds). Instructions and cycles are user-space `perf stat`;
instructions are deterministic to 0.1% and are the noise-free measure of each change.

| build | games/s (mean / median) | decisions/game | decisions/s | instructions/game | cycles/game (median) |
|---|---|---|---|---|---|
| merged Q1 tree, unoptimised (`31f3541`) | 638.5 / 635.2 | 287 | 183.3k | 10.385M | 6.076M |
| + parking slots | 650.8 / 657.0 | 287 | 186.8k | 10.239M | 6.019M |
| + Q2 | 657.2 / 669.2 | 287 | 188.6k | 10.211M | 6.017M |
| + option lists reserved (final, `e61bab7`) | **654.5 / 645.1** | 287 | **187.8k** | **10.136M** | **5.972M** |

**Final against the unoptimised tree: instructions -2.4%, cycles -1.7%, games/s +2.5% (mean).** Against the
pre-batch-1 rate (783 games/s x 278 decisions = 218k decisions/s, measured on a quieter machine): 187.8k,
**14% short**, as after batch 5c-perf (187.4k). The batch 5d slowdown (Q2) is recovered in part; the rest of the gap
is what P1's unsafe part names (section 7). Master `c2615e4` measured at the start: 602 games/s mean (571-622),
10.36M instructions/game.

Fuzz (3 threads, 100,000 games each, same seeds): random 1,851 -> 1,915 games/s, random-player 1,893 -> 1,843,
pass=0 1,760 -> 1,858 (single runs beside other streams; ±5% noise).

ISMCTS (`ismcts:sims=100` v random, 4 games, seed 20261010, 465 ISMCTS decisions, the same games on both builds):
25.4 ms per ISMCTS decision before, 24.5 ms after (medians of three alternating runs, 23.8-28.9 and 24.5-31.5,
load 3.4-4.9): no change beyond noise. The search's time is in world sampling and engine rebuilds
(`editHiddenInformation` 31%, of which `ensureLive` 15%), `InfoSetId` (21%) and the simulations (27%); its
GameState copies are copy-constructions into new engines, which the parking slots do not touch.

## 2. Q1: the last id-order ties (the one game change; committed first)

The 600-pair relabelled run on master: 4 pairs became other games (219, 311, 323, 370) and one showed the
decider different options (459). A first run of games 600-1199 found four more (705, 846, 1032, 1127), all
present on master. Causes and fixes:

| game | cause | fix |
|---|---|---|
| 219 | Reckoner's Arena queued its units' conquer abilities in id order (`std::sort` of ids) | `inSeenOrder` |
| 311, 323 | Downwell returned units and gear in table order (the order they enter the hands; Ripper's Bay's triggers) | `inSeenOrder` (also Angler Beast) |
| 370 | three Brutalizers on one Sand Soldier token: only one attached this turn (+2 Might), all described alike; the Weaponmaster choice fell to ids | `visibleObjectDescription` gives a gear's bearer as it stands and the Might the gear itself gives it |
| 459 | two Zero Drives triggering on one death: which one banished the unit was in nothing shown; later an Equip between "identical" drives fell to ids | an object's "banished with" cards (`tracked_objects`) in `visibleObjectDescription` and in `canonicalBatchOrder`'s key |
| 1127 | the Awaken Phase readied objects one by one in table order, each its own trigger batch (three Jayces' "When I become ready") | the readying is one `SimultaneousTriggers` batch (CR 315.1.b, 383.3.d) |
| 846 | the observation's gear slot named the first gear attached (attach order followed which twin an earlier choice took) | the lowest card among the unit's gear (no layout change; `docs/checkpoint-breaking-2026-10.md` section 10) |
| 705, 1032 | `knownDeckTop` / `knownDeckBottom`: where facts of different spans start at one slot, whichever the id-ordered table listed first decided whether the end was known | judged from the spans (the widest fill that exactly that many facts fit in; a wider fact is known to start after them) |

The relabelled test now fails on any pair that becomes another game or is not compared to the end (was 1 in 50).
Results: games 0-599 and 600-1199 on the Q1 tree 1,200 of 1,200 compared to the end; on the merged final tree
300 of 300 (gate). Tests: `HiddenOrderLeak.ThePairsBatch5dLeftAreTheSameGameUnderOtherObjectIds` (the nine games,
each failing on master), `KnowledgeTracker.ADeckEndIsJudgedFromTheSpansNotFromTheFactsOrder` (red on master, shown).
The test's runner is shared, with `RIFTBOUND_LEAK_GREP` and `RIFTBOUND_LEAK_DUMP_AT` for finding where a pair parts.
Games that change: 52 / 34 / 39 of 3,200 fuzz rows (random / random-player / pass=0); smoke 137/63 unchanged.

## 3. Profile before and after

Frame-pointer build, 3,000 games each; before = master `c2615e4`, after = the merged final tree (batch 6 merged:
slightly different games).

| | before | after |
|---|---|---|
| mallocs per game | 6,873 | 6,269 (-8.8%) |
| libc self (allocator, memcpy, locks) | 12.5% | 11.8% |
| `GameState::operator=` (probe copies) | 2.9% | 2.0% |
| `prePlayProbe` | 3.1% | 2.5% |
| `generateLegalActions` | 10.7% | 9.6% |
| `cleanup` / `recalculateAuras` | 26.1% / 15.2% | 27.6% / 16.1% |
| signals2 dispatch | 17.0% | 17.0% |
| `refreshFlowGrants` / `canAffordWith` | 2.6% / 2.2% | 2.9% / 2.2% |

The allocator time that shows is large blocks, not the small vectors: `_int_malloc` / `_int_free` under the
`std::vector<Intent>` buffers (an Intent is ~280 bytes), map nodes of GameState copies and per-game setup. glibc's
tcache makes a small allocation about 20 instructions.

## 4. The optimisations kept

Proof for each commit (scripts in the session scratchpad `batch-7-perf/`): smoke (200 games, seed 20261010),
both ISMCTS fixed-seed commands (output and the md5 of the sims=50 `--trace`), `deck_fuzz --rows` on 3 x 3,200
games (random, random-player, pass=0): **identical to the merged Q1 tree** for every stage.

1. **`07243d4` Parking slots** (instructions -1.3%, cycles -0.6%; before the merge -1.4% / -1.3%). The three
   probes (pre-play, [Add], optional additional cost) moved the game aside and copied it into the moved-from
   state, allocating a whole GameState (~110 map nodes) and freeing it after. `parkGame` swaps the game into a slot
   of its nesting depth (its objects stay where references point), the copy is assigned over the slot's previous
   copy (libstdc++ reuses the nodes and copies the source's node order and bucket count: the copy a constructor
   makes), `unparkGame` swaps back. Test `Batch7Perf.ACopyAssignedOverAnotherStateIsTheCopyAConstructorMakes`.
2. **`c20d3eb` Q2** (instructions -0.3%, cycles -0.8%). The decision's prompt is parked by pointer
   (`askAgentAbout`; `pendingPrompt()` reads through it) instead of copied, and the chain prompt's name is read by
   reference. About 40% of the +0.73% batch 5d measured; the rest is the carrier's construction and moves, which
   are what it carries (kind, ids, the words already built).
3. **`e61bab7` Option lists reserved** (instructions -0.7%, cycles -0.7%). Main-phase and showdown lists grew by
   doubling (for sixteen options five ~280-byte-Intent buffers and fifteen moves); reserved for sixteen.

## 5. Tried and dropped (exact, measured, no gain)

- **`refreshFlowGrants` asking only cards whose class answers** (facts read from the class at registration by a
  template `registerCard`): instructions -0.09%, cycles +0.5%. Each trash card costs ~16 cycles; the time is the
  object lookup, not the virtual call the line profile pointed at.
- **`Intent`'s id lists with inline storage** (boost small_vector, 2 inline; explicit conversion to std::vector):
  mallocs -12.6% per game, instructions -0.17%, cycles +0.5%: the bigger Intent (+32 bytes) costs back what the
  small allocations saved.
- **A per-generation summary of the base runes for `canAffordWith`**: instructions **+0.8%** (the summary costs more
  than the walks it replaces: few cards asked per generation).

## 6. Gate (final tree `e61bab7`, master merged)

- Clean rebuild (`riftbound_tests riftbound riftbound_openspiel_tests riftbound_clone_equiv_test deck_fuzz
  search_strength balance`, lane B, -j3): **0 warnings** (from ccache, which replays compiler output); checked build
  0 warnings.
- `riftbound_tests` **3,585 passed** (1 disabled); checked build 3,585 passed; `riftbound_openspiel_tests` 34;
  `riftbound_clone_equiv_test` 10/10.
- Leak tests, `RIFTBOUND_LEAK_GAMES=300`: relabelled **300 of 300 compared to the end**, 85,401 decisions, 691,316
  log lines; variants 84,261 decisions, 0 options naming an object the decider cannot see; passive opponent 300
  pairs.
- No-change proofs: section 4, every stage identical to the merged Q1 tree.
- MCTS `mcts:sims=8` x2, 4 games, seed 7: rc 0, P1 3 / P2 1, 0 draws; with `RIFTBOUND_STEP_FAULT_AT=40`: rc 1, "a
  game failed: game 1 [seed=7]: RIFTBOUND_STEP_FAULT_AT: injected engine fault at decision 40", 0 draws.
- Fuzz, optimised, 3 x 100,000 (`--decks 5000 --games-per-pair 20`, seeds 7150701 / 7150702 / 7150703, `--known`):
  **0 failures**, 0 turn caps, 0 without a winner or a reason, 0 hung (pass=0: 2 games with a chain of 200+ items,
  listed, not failures); **all 300,000 rows identical** to the unoptimised merged tree on the same seeds.
- Fuzz, checked, 3 x 20,000 (seed 7150711): **0 failures**; all 60,000 rows identical to the unoptimised tree.

## 7. What remains (P1 reworded, `plans/remaining/10-testing-phase.md`)

The part that needs the owner: the Cleanup after every chain step (~27%; skipping a pass needs write tracking of
the fields aura hooks read), signals2 dispatch (17%) and the per-game bus setup and teardown (~3%; the bus is out of
scope), and search's GameState copy-constructions into new engines. The three dropped candidates are recorded there
as measured.

## 8. New items and notes

- **Q3** (QUESTION if wanted): Reckoner's Arena's activated conquer abilities are queued one by one with no order
  prompt; CR 383.4.g.1 "as if it had just triggered" suggests the controller orders them (383.3.d). Now in seen
  order.
- Process notes: one command typed `rm -f` with no operand (a no-op; it removed nothing) — against the shell rule.
  The run lock was held for long stretches by another stream's 100,000-game checked fuzz and ISMCTS round-robin;
  my measurement windows waited for it, so the timings were taken in short windows between other streams' runs.
  The pre-merge stage binaries were proven identical too (same proofs against the pre-merge Q1 tree).

## 9. Files

`src/engine/game_engine.{h,cpp}` (Q1 Awaken batch and trigger key; parking slots; Q2; reserve),
`src/openspiel/action_vocab.cpp`, `src/engine/knowledge_tracker.cpp`, `src/ml/feature_extractor.cpp`, three card
files (Q1); tests `test_hidden_order_leak.cpp`, `test_knowledge_tracker.cpp`, `test_batch7_perf.cpp` (new);
`CLAUDE.md` (seen order and descriptions, parking slots, the parked prompt); `docs/checkpoint-breaking-2026-10.md`
section 10; `plans/{remaining,completed}/10-testing-phase.md`.
