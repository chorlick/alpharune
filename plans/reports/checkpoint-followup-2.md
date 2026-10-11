# checkpoint — follow-up 2 (9.UU1, 9.VV1)

Branch `fix/checkpoint`, head `a9d8792`. **Verified against integration `79ca655`**, merged in as `c1f70dc` (made with `-m`, trailers present).

## 1. Result

Both items are closed and every gate passes on a clean rebuild of `a9d8792`. **One gate number changes and is meant to: the random-play smoke is now P1 138 / P2 62, 295 decisions** (it was 139 / 61, 294). That comes from the 9.VV1 fix, not from the id space; section 3 says why.

Clean rebuild (`--target clean`, eight targets, `-j6`, both lane locks, tests at `-O0`): 0 errors, 0 warnings.

| Gate | Result |
|---|---|
| `riftbound_tests` | **3065 passed, 0 failed** (3060 + 5 new) |
| `riftbound_openspiel_tests` | 27 passed |
| `riftbound_clone_equiv_test` | 10/10 |
| smoke, 200 games, seed 20261010, 6 threads | rc 0, **P1 138 / P2 62 / 0 draws, 17 turns, 295 decisions** (7 of the 200 games changed, by 9.VV1) |
| fixed-seed traces, 19 files, 14,708,563 bytes, fresh directory | **byte-identical** to `2cb28f8` |
| observation checksum | `965b13284eb6dc5c` -> **`4a751d2371810e80`** (9.UU1) |
| `mcts:sims=50` 5 games seed 20261010; `ismcts:sims=20` 8 games seed 1 | the same games as at `14e6462` (per-game lines identical); each run twice, identical |
| MCTS batch, 4 games, seed 7 | rc 0 |
| same with `RIFTBOUND_STEP_FAULT_AT=40` | rc 1, "injected engine fault at decision 40", 0 draws |
| `ismcts:sims=50`, 2 games, seed 20261010, ten runs | 10/10 clean, ten identical outputs |
| `ismcts:sims=20`, 8 games, seed 1, ten runs | 10/10 clean, ten identical outputs |
| suite with `-DRIFTBOUND_CHECK_BOARD_INDEX=ON` (separate build) | 3065 passed; OpenSpiel 27; clone-equivalence 10/10; smoke 138 / 62; 0 warnings |
| `ctest -R riftbound_strong_id` | 26/26 |
| `collision_census`, 100 games | 0 collisions, 0 intents without an id |

`plans/remaining/` holds only its README, which says nothing is open.

## 2. 9.UU1 — card id space 2048, vector padded — FIXED (`a7b07ec`)

| | Before (1150 ids) | After |
|---|---|---|
| `ml::kCardVocabSize` / `openspiel::kNumCardDefIds` | 1150 | **2048** |
| `kStateFeatureDim` | 6442 | **10944** (10932 of content + 12 of padding) |
| `kCardKeys` | 1151 | 2049 |
| `kNumSlots` | 71,448 | 127,124 |
| `NumDistinctActions` | 73,162,752 | 130,174,976 |
| `kCardEmbeddingRows` | 1151 | 2049 |
| `kLayoutVersion` | 2 | 3 (I treat it as a new layout) |

- **Ranges, named at the definition** (`feature_extractor.h`): 1..955 registered (`kLastRegisteredCardId`), 956..1165 Radiance (210), 1166..1565 Legacy (400), 1566..2048 spare (483).
- **Padding**: new block `layout::kPadding`, 12 floats, always zero, last in the vector. The length is a multiple of 16 floats (64 bytes), by `static_assert`. A later block goes before it.
- **Test-only card ids**: 74 constants in eight test files sat in 956..1165, which is now Radiance's range. They moved to the spare range (+800). `FeaturePerspective.NoTestCardIdSitsInARangeReservedForASet` keeps it so; it allows the three ids of `test_review_fix_integrity.cpp` that are outside the id space on purpose.
- **Unchanged by this item**: the random smoke (139 / 61 / 0, 17 turns, 294 decisions at `a7b07ec`), the 19 traces, clone-equivalence, the census (0 / 0), and both search batches' games.

### What the larger vector costs

Three alternating runs each, medians, against the build with 1150 ids.

| | 1150 ids (6,442 floats) | 2048 ids (10,944 floats) | |
|---|---|---|---|
| `InfoSetId` per call | 5.03 µs | 6.52 µs | **+30%** |
| of which hashing (`computeInfoSetId`) | 2.15 µs | 3.45 µs | +61% |
| `extractStateFeatures` | 2.68 µs | 3.03 µs | +13% |
| `ObservationTensor` | 3.19 µs | 3.83 µs | +20% |
| `ismcts:sims=20`, 8 games, seed 1 | 2.92 / 3.00 / 4.34 s | 2.89 / 3.01 / 3.16 s | within the spread |
| `mcts:sims=50`, 5 games, seed 20261010 | 2.80 / 2.84 / 4.19 s | 2.65 / 2.68 / 2.83 s | within the spread |
| random, 1000 games, 1 thread | 1.25 / 1.27 / 1.29 s | 1.23 / 1.24 / 1.27 s | unchanged |

- **`InfoSetId` is slower by more than its spread** (about 0.25 µs): 1.5 µs per call, 0.33 ns per added float.
- **With 1600 ids it would have been about 5.8 µs (+15%)**: an estimate from the per-float cost at 8,704 floats, not a build.
- In the ISMCTS batch that is 69,000 calls x 1.5 µs = about 0.10 s of 2.9 s (3.5%; 1600 ids: about 0.05 s). The batch's own spread hides it.
- The 4.34 s and 4.19 s runs are one-off spikes of the "before" binary, not a trend.

### Pinned values that changed

| Test | Old | New |
|---|---|---|
| `ActionVocab.SizesArePinned` | `kCardKeys` 1151, `kNumSlots` 71448, `kNumActions` 73162752; offsets Play 9277, HideCard 46109, ActivateAbility 55317, ChooseOwnObject 62223, ChooseValue 65676 | 2049, 127124, 130174976; 16461, 82029, 98421, 110715, 116862 |
| `StrongIds.HistoryTokensAndSlotsArePinned` | slots 55317, 64760; tokens 7960844909697619989, 4496770096613617802, 4030418850666315000 | slots 98421, 115048; tokens 7960844909697663093, 4496770096613665396, 4030418850666365288 (the other two tokens and slot 46 are unchanged) |
| `PerfSearchHash.SkippingZeroRunsDoesNotChangeTheInfoSetId` | run lengths up to 6442 | 10944 added |
| `FeaturePerspective.BattlefieldSlotsSayWhoBroughtThem` | `kStateFeatureDim == kBfContributedBy + 4` | `kPadding == kBfContributedBy + 4` |
| `PerfEngine...` registry lookup of an unregistered id | `get(999)` | `get(2047)` (999 is a Radiance id now) |
| test-only card ids, 74 constants and one `d.id` | 956..1165 | +800 |

New: `FeaturePerspective.TheVectorEndsInZeroPaddingToAMultipleOfSixteenFloats`, `FeaturePerspective.NoTestCardIdSitsInARangeReservedForASet`.

Docs: `docs/checkpoint-breaking-2026-10.md` (section 0 rewritten with the range table and the cost table; every derived number), `CLAUDE.md`.

## 3. 9.VV1 — Switcheroo left unpayable — FIXED (`6b32588`, `a9d8792`), in the narrowed scope

### Root cause

None of the suspects in the item. It is the order of payment.

- A spell that makes its choices before its cost pays a chosen unit's Deflect when the choice is made, ahead of its own cost.
- `EffectExecutor::payAnyPower` paid that by recycling the first rune in line. Two things went wrong with it:
  - a ready rune was recycled without being exhausted, so its Energy was lost;
  - a rune of the spell's own Domain went although another would have done.
- `GameEngine::canAffordWith`, which makes the offer and filters the choices, judges cost and Deflect as paid together. So the play was offered, the Deflect unit was on the list, and after the Deflect the cost could not be paid.
- In the game: Switcheroo ([2] + 2 Chaos) chose Steel Paws ([Deflect]); the Deflect took a rune the cost needed; `executePlaySpell` threw.

### Fix

While a spell is making its pre-play choices, `payAnyPower` pays through the new `resource::payAnyPowerAheadOfCost`: it takes what the cost cannot use first (floating Power and runes of other Domains), and exhausts a ready rune for its Energy before recycling it. Both are legal (CR 164.2.b) and are what the affordability check already assumed.

### Why the registry-level tests missed it, and the gap closed

- `FinalizePassSpellDeclarationsMatchTheirPickLists`, `FinalizePassSpell_NoRegisteredSpellComesNearTheProbeBound` and `CardContract_NoRegisteredCardEndsItsFinalizePassAnywhereElse` fund every play with floating Energy and Power and face no [Deflect]. No rune was ever recycled ahead of a cost.
- New sweep, `DeflectBeforeCost.NoRegisteredSpellThatChoosesBeforeItsCostIsLeftUnpayableByADeflect`: the 90 registered spells that choose before their cost, played from runes alone, every enemy unit with [Deflect], the spell's own runes first in line.

| | Before the fix | After |
|---|---|---|
| plays offered | 628 | 628 |
| completed | 574 | 628 |
| offered, chosen for, could not be paid | **54** (Falling Star 29 and others) | 0 |
| plays that paid a Deflect on the way | 387 | 441 |

Two engine-path tests rebuild the game's situation and were red first: `TheRuneRecycledForDeflectIsNotOneTheSpellNeeds`, `AReadyRuneRecycledForDeflectStillGivesItsEnergy`.

### The smoke changes

- 7 of the 200 smoke games play differently (seeds 20261037, 20261040, 20261091, 20261110, 20261114, 20261165, 20261179). One changes winner (20261179), which is the 139 -> 138.
- I traced two of them (20261037, 20261179): the first differing line is a Deflect paid ahead of a cost, `PAY_ADD ... exhausted 0, recycled 1` -> `exhausted 1, recycled 1`. The rune now gives its Energy first, the cost that follows exhausts one rune fewer, and the random agent meets other prompts.
- The 19 fixed-seed trace files are byte-identical: in those games the Deflect was paid with an exhausted rune of another Domain, as before.

### Reproductions

- vex vs vex, seed 20261016: finishes (P1 wins, 17 turns).
- 40 games at seed 20261010 for every pairing and seating of fiora, yi_rebuild, leblanc, vex: 640 games, 0 errors, all rc 0.

| Deck 1 \ Deck 2 (P1 wins / P2 wins, avg decisions) | fiora | yi_rebuild | leblanc | vex |
|---|---|---|---|---|
| fiora | 18 / 22, 394 | 28 / 12, 313 | 32 / 8, 411 | 25 / 15, 322 |
| yi_rebuild | 16 / 24, 264 | 22 / 18, 239 | 23 / 17, 328 | 18 / 22, 256 |
| leblanc | 13 / 27, 414 | 21 / 19, 327 | 19 / 21, 495 | 19 / 21, 318 |
| vex | 18 / 22, 288 | 22 / 18, 249 | 23 / 17, 339 | 26 / 14, 276 |

(These numbers are from `6b32588`; the batch was run again at `a9d8792` and all 16 were clean.)

## 4. Not done this round, by the user's decision

The 500-games-per-pairing runs of the five decks, the `zed.txt` substitution, the 100,000-game whole-pool fuzz, the per-card coverage count and the re-runnable fuzz target. None was started.

## 5. For the testing phase

No other failing seed turned up: the 640 games above and the 146-game walk of the suite (49 pool decks included) are clean.

Things the testing phase should know:

- **The gate's smoke baseline is 138 / 62, 295 decisions from this commit on.**
- The pre-play Deflect payment is the only payment I changed. `ChainManager::payDeflectForChoices` (spells offered once per target, Deflect paid as the item goes on the chain) still pays through the plain rule. I did not establish whether it runs before or after such a spell's cost; if before, it has the same fault.
- `canAffordWith` treats Energy and Power as independent. That is right only because every payment exhausts before it recycles; any new payment that recycles a ready rune ahead of another cost breaks an offer the same way.

## 6. How the work was run

- Both lane locks for every configure, build, suite run, gate run and timing run.
- The user's `leblanc.txt`, `vex.txt` and `zed.txt` are copied into the worktree's `decks/` for the runs and are **untracked**: not committed, nothing touched in the main checkout.
- The first gate run of this round showed 3 differing trace files: my new payment wrote a differently worded `PAY_ADD` line. It now writes the line the old payment wrote, and a fresh capture is byte-identical.
- My first plan note for 9.VV1 (in `6b32588`) said the smoke was unchanged. That was wrong: I had not re-run it. `a9d8792` corrects the plan text.
- Red runs were made by reverting one source file under the new test file and restoring it from a patch.
- No `rm`. `plans/remaining/09-found-during-fix-run.md` was removed with `git rm` (emptied).
