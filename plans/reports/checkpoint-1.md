# checkpoint — interim report 1 (item 1.9)

Branch `fix/checkpoint`, head `395e932`, based on integration `2cb28f8` (not moved). One commit, mergeable on its own. 1.12 and 9.RR2 are still in progress.

## 1. Result

1.9 is fixed: the state vector has one ordering convention and it is tested over the whole vector.

Incremental build (not a clean rebuild; the clean rebuild is done once at the final head), `-j6`, both lane locks, 0 warnings.

| Gate | Result |
|---|---|
| `riftbound_tests` | 3030 passed, 0 failed (3018 + 12 new) |
| `riftbound_openspiel_tests` | 26 passed (25 + 1 new) |
| `riftbound_clone_equiv_test` | 10/10 |
| smoke, 200 games, seed 20261010, 6 threads | P1 139 / P2 61 / 0 draws, 17 turns, 294 decisions (unchanged) |
| fixed-seed traces, 19 files, 14,708,563 bytes | byte-identical to `2cb28f8` |
| `mcts:sims=50`, 5 games, seed 20261010 | same games as at `2cb28f8` (output identical but the version line); two runs identical |
| `ismcts:sims=20`, 8 games, seed 1 | same games as at `2cb28f8`; two runs identical |
| search microbench observation checksum | `94739f84565d4d16` -> **`366dde83e658847e`** (expected to change) |

Not run for this interim: the checked build, the fault-injection batch, the ten-times ISMCTS runs.

## 2. Item closed

**1.9 — FIXED.**

- **Re-verified first.** Three blocks were absolute, every other block was already relative:
  - the battlefield controller (1 = Player1, 2 = Player2);
  - the two 20-float side blocks of each battlefield (Player1 then Player2);
  - the 96-float extended per-unit block (same order).
- **The convention: relative to the perspective player, everywhere.** Self first, opponent second; a field that names a player is 1 = self, 2 = opponent, 0 = nobody. Why: the network and `InfoSetId` then see the same vector for the same situation from either seat, and one (battlefield, side, unit) slot is the same unit in the base, extended and Tier-2 blocks.
- **One new block, 4 floats: who brought each battlefield slot** (`layout::kBfContributedBy`). Slot order is setup order (Player1's battlefield, then Player2's), the one thing left in the vector that depended on the seat. `kStateFeatureDim` 6188 -> 6192.
- **The absolute seat is not a feature.** Nothing needs it: the symmetry test passes over the whole vector. Who goes first already was one (`layout::kGlobalSelfStarts`, index 3).
- Every block has a named offset in `ml::layout` (`feature_extractor.h`); the extractor throws if a block does not start where the layout says.
- `InfoSetId(p)` is unchanged in construction: the hash of p's own vector, mixed with p's seat, the decider and, for the decider only, the offered action ids. `ObservationString` is its hex. ISMCTS keys on (decider, that string).

Tests (all red or new):

| Test | What it shows |
|---|---|
| `FeaturePerspective.*` (8) | one hand-built position per block where the seats differ, read from both seats |
| `FeaturePerspectiveGames.SeatSwapGivesTheSameVectorAtEveryDecision` | `features(X, P1) == features(mirror(X), P2)` and the reverse, whole vector, 6,837 decisions of 24 random games (both decks, both mirror matches). Red before: 13,400 differing features in the controller field, 177,348 in the side blocks, 19,196 in the extended block; nothing else differed |
| `FeaturePerspectiveGames.MirroringTwiceIsTheSamePositionAndTheSeatsDoDiffer` | the mirror is an involution and not the identity |
| `FeaturePerspectiveGames.HiddenInformationStaysMaskedAtEveryDecision` | 7,446 re-deals of the opponent's hand, deck and facedown identities and of both deck orders: the player's vector never moves, the opponent's own does |
| `RiftboundStateTest.InfoSetKeyIsAFunctionOfWhatThePlayerKnows` | 3,070 keys (1,535 of the decider): unchanged by a re-deal of what that player has not been shown |

## 3. Pinned values that changed (by design)

| Test | Old | New | Why |
|---|---|---|---|
| `PerfSearchTest.FeaturesStillMatchTheStateQueriesTheyReplaced` | side columns read as Player1, Player2 | read as self, opponent | the side blocks are perspective-relative now |
| same | ready-base offset `kStateFeatureDim - 24` | `ml::layout::kExtGlobals` | four floats were appended after the block |
| `PerfSearchHash.SkippingZeroRunsDoesNotChangeTheInfoSetId` | run lengths up to 6188 | 6192 added (6188 kept) | new vector length |

No test deleted or weakened.

## 4. Trainer notes (so far)

- `kStateFeatureDim` is 6192. Offsets 0..6187 keep their meaning except the three reordered blocks; 6188..6191 are new.
- Battlefield controller: 0 nobody, 1 self, 2 opponent.
- Per battlefield, the side blocks (base 2 x 20, extended 2 x 12) are self then opponent.
- Use `ml::layout::*` for offsets instead of numbers.
- Every `InfoSetId` / `ObservationString` value changed.

The full note (`docs/checkpoint-breaking-2026-10.md`) comes with the final report.

## 5. New defects found

- **The knowledge tracker is not told when a card shows a player the opponent's hand.** Sabotage 156 ("They reveal their hand. Choose a non-unit card from it") emits `CardRevealedEvent` for each card and then offers them as options, but nothing calls `knowledge::seen`, so `GameKnowledgeView::about` says the caster does not know cards it is choosing between. 28 card files emit `CardRevealedEvent`; I checked only this one. The flat features and the search do not read the tracker, so nothing in the current encoding is wrong because of it. Not fixed (v6 tracker).

Play UI, deferred: none noticed.
