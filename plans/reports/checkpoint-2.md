# checkpoint — interim report 2 (9.RR2, 1.12, docs)

Branch `fix/checkpoint`, head `a958050`. Three commits on top of the merged tranche 1 (`395e932`): `67f609c` (9.RR2), `355aa98` (1.12), `a958050` (docs). Not yet merged with the current `fix/integration`; that merge and 9.SS1 come next.

## 1. Result

1.12 and 9.RR2 are fixed. Over every decision of 146 random games no two distinct legal intents share an action id and every legal intent can be chosen through OpenSpiel.

Incremental builds, `-j6`, both lane locks, 0 warnings. The clean rebuild, the checked build, the fault-injection batch and the ten-times ISMCTS runs are left for the final head.

| Gate at `355aa98` | Result |
|---|---|
| `riftbound_tests` | 3044 passed, 0 failed |
| `riftbound_openspiel_tests` | 27 passed |
| `riftbound_clone_equiv_test` | 10/10 |
| smoke, 200 games, seed 20261010 | P1 139 / P2 61 / 0 draws, 17 turns, 294 decisions (unchanged) |
| fixed-seed traces, 19 files, 14,708,563 bytes | byte-identical to `2cb28f8` |
| random, 1000 games, 1 thread (5 runs, alternated) | 1.221 s before, 1.230 s after (medians) |
| observation checksum | `94739f84565d4d16` -> `366dde83e658847e` (1.9) -> **`efc2aa14b545a08e`** (1.12) |

## 2. Collision census (the test that was written first)

Same walk, old encoding and new. A collision is two distinct legal intents of one decision with one action id; under the old wrapper only the first could be chosen.

| Walk | Decisions | Distinct legal intents | Before: decisions with a collision | Before: unreachable intents | After |
|---|---|---|---|---|---|
| fiora and yi_rebuild, all four pairings, 48 games | 14,690 | 52,010 | 5,682 (39%) | 22,760 (44%) | 0 / 0 |
| 49 decks built from the whole pool (682 different cards), mirror and cross, 98 games | 30,579 | 114,760 | 11,363 (37%) | 45,629 (40%) | 0 / 0 |

Before, by decision kind (repository decks / pool decks, colliding of total):

| Kind | Repository decks | Pool decks |
|---|---|---|
| mulligan | 96 of 96 | 196 of 196 |
| prompt answered with objects | 4,418 of 5,458 | 7,971 of 9,564 |
| main phase | 1,011 of 2,430 | 2,835 of 5,673 |
| prompt with objects or a value | 27 of 437 | 154 of 1,365 |
| chain priority | 73 of 4,164 | 104 of 10,172 |
| showdown focus | 53 of 1,001 | 86 of 1,949 |
| combat damage | 4 of 259 | 17 of 684 |

Before, unreachable intents by type (pool decks): MakeChoice 32,495 of 56,049; PlayCard 5,516 of 18,037; StandardMove 5,046 of 12,300; MulliganDecision 1,568 of 2,156; ActivateAbility 683 of 4,237; the other types under 100 each.

No pool-deck game failed in the engine.

## 3. The design of 1.12

`id = slot + kNumSlots * twin` (`src/openspiel/action_vocab.h`).

- **Slot** (68,348): what the action is made of. Per sub-bullet of the item:
  - mulligan: which hand positions (it was the count);
  - Standard Move: (unit card, destination) (it was the destination);
  - a prompt's object: separate ranges for the decider's and the opponent's (mirror match);
  - every card-keyed range has key 0 for an object with no card id. Reach confirmed: tokens without a card class, 468 prompt options and 1,136 Standard Moves in the pool walk. They landed on card 1's slot, or all on one prompt slot;
  - Play: (card, where it lands, normal / alternative cost / location cost / not from hand). The play to a battlefield and the play to the base shared a slot too;
  - ability index: never overwritten. Equip and granted abilities have ranges of their own; the gear-target hash is gone;
  - combat damage: the number of units hit. The 128-bucket hash (documented as 16) is gone;
  - options differing only in `ability_index`: twins now, and `askAgent` already refuses such a prompt.
- **Twin** (0..1023): tells apart the legal intents of a decision that share a slot, in a canonical order of the intents themselves (`compareIntents`: field by field, objects by id). The walk meets 64 at most (a spell offered once per pair of targets).
- **How the tie-break is independent of hidden information:** the order reads only the intents, never the state, and an intent names only objects the deciding player is shown. Tested at every decision of the walk: any re-deal of the opponent's hand, deck and facedown cards that the player has not seen, and any order of both decks, gives the same ids; so does any order of the legal list.
- **`NumDistinctActions` = 69,988,352**, fixed. Past 1024 twins `LegalActions` throws instead of dropping an option.
- **History tokens are still slot + fingerprint.** I first put the full id in the token; working out the twin for every decision of every game cost 4% of random play (1.25 s -> 1.31 s per 1000 games), so the token keeps the slot only. The fingerprint identifies the intent, so replay, lazy clones and `syncToHistory` are exact as before. `RiftboundState::actionHistory()` therefore returns slots; ids are in OpenSpiel's `History()`.
- **One limit, stated in the header and the trainer note:** a twin number means "another action with this description", not the same object across positions (two units of one card that swap places swap twin numbers).

9.RR2: `Intent::battlefield_pool_index` of the new id kind `BattlefieldPoolIndex`; `chosen_battlefield` is only the slot a card is hidden at.

## 4. Search batches (expected to change)

| Batch, agent is P1 vs random | At `2cb28f8` | At `355aa98` | Reproducible |
|---|---|---|---|
| `mcts:sims=50`, 5 games, seed 20261010 | P1 4 / P2 1, 296 decisions per game, 2.92 / 2.95 s | P1 5 / P2 0, 232 decisions per game, 2.58 / 2.81 s | yes, two runs identical |
| `ismcts:sims=20`, 8 games, seed 1 | P1 5 / P2 3, 274 decisions per game, 2.08 / 2.06 s | P1 4 / P2 4, 290 decisions per game, 2.82 / 2.91 s | yes, two runs identical |

Clone and `ApplyAction` cost per call is unchanged (`riftbound_clone_advance_microbench`: 4.4 / 75 / 33 µs before, 4.5 / 74 / 34 µs after). I did not find out why the ISMCTS batch takes 40% longer beyond "different games and trees"; a per-state cache of the legal ids changed nothing and is not in the tree.

## 5. Pinned values that changed (by design)

| Test | Old | New | Why |
|---|---|---|---|
| `StrongIds.HistoryTokensAndSlotsAreTheNumbersOfTheBinaryBeforeIdsWereTypes`, renamed `StrongIds.HistoryTokensAndSlotsArePinned` | tokens 3611872453840603442, 9183008626965481778, 7576281683122461517, 4557704407380131852, 3408968969468510209; slots 2354, 6989, 12 | tokens 7960844909697617589, 4496770096613615152, 4030418850666312200, 9050218172478652462, 2124341512652193793; slots 52917, 61960, 46 | new slot ranges; the fingerprint has one more field |
| `ActionVocab.VocabSizeIsReasonable`, now `ActionVocab.SizesArePinned` | 1000 < `kVocabSize` < 100000 | `kNumSlots` 68348, `kTwinPlanes` 1024, `kNumActions` 69988352, eight verb offsets | exact numbers instead of a range |
| `ActionVocab.HistoryTokenTellsApartIntentsThatShareASlot`, now `AHistoryTokenFindsItsIntentAmongTwins` | asserted that two mulligans, two moves and two plays share a slot and that decode returns the first | asserts distinct ids for all, shared slots only for true twins | the collisions it pinned are the defect |
| `ActionVocab.CollapsingPlayVariantsAliasIntentionally`, now `PlayTimingVariantsShareASlot` | same assertion | same assertion | name only |
| `ActionVocab.MakeChoiceNoObject` | slot 0 of the prompt range | the "another value" slot | no "no object" slot exists; no legal list holds such an option |
| `RiftboundStateTest.ActionHistoryIsOneSlotPerAppliedAction` | `actionHistory()` == applied ids | == the slots of the applied ids, and `History()` == the ids | an id is no longer a slot |
| `RiftboundGameTest.AdvertisedShapesMatchTheEngine` | `kVocabSize` | `kNumActions` | |
| (from tranche 1) `PerfSearchTest.FeaturesStillMatchTheStateQueriesTheyReplaced`, `PerfSearchHash.*` | see interim 1 | | |

No test deleted. `tests/test_action_vocab.cpp` went from 25 to 36 tests.

## 6. New findings

- **A unit returned to its owner's hand is not recorded by the knowledge tracker either** (`EffectExecutor::bounceToHand` makes no `knowledge::moved` call). Found by the info-set test: a chain item's source card was in the opponent's hand and the tracker said the player did not know it. Goes with 9.SS1.
- `tools/collision_census.cpp` is not built by the engine's CMake (the trainer builds it). I updated it to the new API and checked it with `g++ -fsyntax-only` only.

Play UI, deferred: none noticed.
