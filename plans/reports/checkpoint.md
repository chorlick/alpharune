# checkpoint — final report (stage 5: 1.9, 1.12, 9.RR2, 9.SS1, 9.SS2)

Branch `fix/checkpoint`, head `d150457`. **Verified against integration `caf377a`**, which is merged in (plan files only).

## 1. Result

All five items are closed. Every gate passes on a clean rebuild of the final head.

Clean rebuild at `d150457` (`cmake --build build --target clean`, eight targets, `-j6`, both lane locks, `scripts/ccache_pch_safe.sh`, `CCACHE_NAMESPACE=checkpoint`, tests at `-O0`): **0 errors, 0 warnings**.

| Gate | Result |
|---|---|
| `riftbound_tests` | **3059 passed, 0 failed** (3018 at `2cb28f8` + 41 new) |
| `riftbound_openspiel_tests` | 27 passed (25 + 2 new) |
| `riftbound_clone_equiv_test` | 10/10 |
| smoke, 200 games, seed 20261010, 6 threads | rc 0, **P1 139 / P2 61 / 0 draws, 17 turns, 294 decisions** (unchanged) |
| fixed-seed traces, 19 files, 14,708,563 bytes | **byte-identical** to the binary at `2cb28f8`, after every item and at the end |
| random, 1000 games, 1 thread (5 runs, alternated) | 1.221 s before, 1.230 s after 1.12 (medians) |
| MCTS batch, 4 games, seed 7 | rc 0 |
| same with `RIFTBOUND_STEP_FAULT_AT=40` | rc 1, "injected engine fault at decision 40", 0 draws |
| `ismcts:sims=50`, 2 games, seed 20261010, ten runs | **10/10 clean**, ten identical outputs |
| `ismcts:sims=20`, 8 games, seed 1, ten runs | **10/10 clean**, ten identical outputs |
| suite with `-DRIFTBOUND_CHECK_BOARD_INDEX=ON` (separate build) | 3059 passed; OpenSpiel 27; clone-equivalence 10/10; smoke split the same; 0 warnings |
| `ctest -R riftbound_strong_id` | 26/26 |
| `collision_census`, 100 games | 0 collisions, 0 intents without an id |

Expected to change, measured:

| | At `2cb28f8` | At `d150457` |
|---|---|---|
| observation checksum (search microbench) | `94739f84565d4d16` | **`965b13284eb6dc5c`** |
| `mcts:sims=50`, 5 games, seed 20261010 | P1 4 / P2 1, 296 decisions per game, 2.92 / 2.95 s | P1 5 / P2 0, 232 decisions per game, 2.83 / 2.80 s |
| `ismcts:sims=20`, 8 games, seed 1 | P1 5 / P2 3, 274 decisions per game, 2.08 / 2.06 s | P1 4 / P2 4, 290 decisions per game, 2.91 / 2.90 s |

- Each batch run twice gives the same output, before and after.
- Checksum by step: `366dde83e658847e` after 1.9, `efc2aa14b545a08e` after 1.12, unchanged by 9.SS1, `965b13284eb6dc5c` after 9.SS2.

9.O2 is not mine and is left open; the 20 ISMCTS runs above are its stage 5 record.

## 2. Items closed

| Item | Outcome | Commit |
|---|---|---|
| 1.9 | FIXED | `395e932` |
| 9.RR2 | FIXED | `67f609c` |
| 1.12 | FIXED | `355aa98` |
| 9.SS1 | FIXED | `5693844` |
| 9.SS2 | FIXED (145 was not enough; now 1150) | `67d6877` |
| docs, `collision_census` target | | `a958050`, `621799d`, `d150457` |

All are ticked and moved to `plans/completed/`; `plans/remaining/01-openspiel-search.md` is deleted; `plans/remaining/` holds 9.O2 only.

### 1.9 — one ordering convention

- **Everything is relative to the perspective player.** Self first, opponent second; a field that names a player is 1 = self, 2 = opponent, 0 = nobody.
- Three blocks were absolute and are fixed: the battlefield controller, the two side blocks of each battlefield, the 96-float extended block. Every other block was already relative.
- New 4-float block `kBfContributedBy`: who brought each battlefield slot, the one thing left that depended on the seat. The absolute seat is not a feature; who goes first already was.
- Every block has a named offset in `ml::layout`; the extractor throws if a block does not start there.
- Seat-swap test over the whole vector at 6,837 decisions of 24 real games. Red before: 13,400 / 177,348 / 19,196 differing features in the three blocks, nothing else.

### 1.12 — no two legal intents share an action id

`id = slot + kNumSlots * twin`.

- **Slot** (71,448): what the action is made of. Mulligan by hand positions; Standard Move by (unit card, destination); a prompt's object by whose it is; key 0 in every card-keyed range for an object with no card id; Play by (card, place, normal / alternative cost / location cost / not from hand); the ability index never overwritten; Equip and granted abilities in ranges of their own; combat damage by the number of units hit.
- **Twin** (0..1023): tells apart the legal intents of one decision that share a slot, in a canonical order of the intents themselves. The walk has met 64 at most.
- **Independent of hidden information:** the order reads only the intents; an intent names only what the deciding player is shown. Tested at every decision: any re-deal of what the player has not been shown gives the same ids, and so does any order of the legal list.
- `NumDistinctActions` = 73,162,752, fixed. Past 1024 twins `LegalActions` throws rather than drop an option.
- History tokens stay slot + fingerprint. Putting the full id in the token cost 4% of random play, measured, so the twin is not recorded; `actionHistory()` returns slots and OpenSpiel's `History()` has the ids.
- One limit, stated in the header and the trainer note: a twin number means "another action with this description", not the same object across positions.

### 9.RR2

`Intent::battlefield_pool_index` of the new id kind `BattlefieldPoolIndex`. `chosen_battlefield` is only the slot a card is hidden at.

### 9.SS1 — the knowledge tracker hears every reveal and every move

- **Reveals and looks, one place.** `knowledge::recordReveal` is the handler of `CardRevealedEvent`. It calls `knowledge::shown(state, card, seers)` for exactly the players the event names, and bumps `observed_cards`.
  - I read all 32 emission sites in 28 card files and the 4 in the engine.
  - 14 card-level `seen` / `seenInDeck` calls that duplicated an event are removed.
  - First reached the tracker now: the hand reveals (Sabotage, Mindsplitter, Decree of Strength, Insightful Investigator, Ashe, Bone Skewer) and Blind Fury's reveal.
  - Looks that announce no event call `shown` directly: Void Hatchling, Ornn 381, Lightning Rush, Baited Hook, and Fate Weaver, which told nobody.
- **`recordReveal` and the tracker are two things fed by one handler**, not one mechanism: `observed_cards` counts how often a card was shown and is never taken back; the tracker says what is known now.
- **Moves, one place.** A card enters a hand only through `deck::putInHand` / `deck::drawTop` and a facedown pile only through `deck::putFacedown`. Two source-scan tests keep it so.
- **A known card going back to a hidden zone** follows the tracker's own rules: left on top after a look it stays known to the looker; shuffled in it loses its place; moved unseen out of a hand it makes that hand uncertain; chosen in the open from a revealed hand and recycled, both players know it at the bottom (`recycleCards(..., shown)`, used by Sabotage and Decree of Strength).
- **Two tracker defects found and fixed.** I added `knowledge::check` at every decision of the 146-game walk; it failed at 30 decisions of one fiora mirror game, before any of my changes. Cause: Rift Herald's look at the top three of an 18-card deck.
  - A card removed from below a slot known from the other end left that slot one too high.
  - A known card picked privately out of a look kept its known slot.
- **Flat features unchanged**: checksum `efc2aa14b545a08e` before and after this item.

Every kind of move and whether the tracker hears it:

| Move | Where it is made | Heard | Since |
|---|---|---|---|
| deck -> hand (draw) | `deck::drawTop` (`EffectExecutor::drawCards`) | yes | before, now one function |
| deck -> held by a look -> hand / top / bottom | `deck::takeTop` / `take`, then `putInHand` / `putOnTop` / `putOnBottom` | yes | before |
| deck -> trash / banishment / board (burn, mill, play) | `deck::takeTop` / `take` | yes | before |
| hand, board, trash, banishment -> deck | `recycleCards`, `deck::put*` | yes | before; `shown` parameter new |
| trash -> deck on burn-out, shuffle | `deck::putToShuffle`, `deck::shuffle` | yes | before |
| mulligan | `deck::putOnBottomTogether` | yes | before |
| board -> hand | `EffectExecutor::bounceToHand` -> `deck::putInHand` | **yes** | 9.SS1 |
| trash / banishment / Champion Zone -> hand | 11 card files -> `deck::putInHand` | **yes** | 9.SS1 |
| chain -> hand (countered, "return it to hand") | `disposeCounteredCard` -> `deck::putInHand` | **yes** | 9.SS1 |
| hand or Champion Zone -> facedown (Hide) | `executeHideCard` -> `deck::putFacedown` | **yes** | 9.SS1 |
| facedown -> hand | Pack of Wonders -> `deck::putInHand` | yes | before, now one function |
| hand or facedown -> board, chain, trash, banishment | many | not needed | public zones are read from the state; the fact is dropped at the next prune |
| board <-> board, off-board public zones | `leftBoard` paths | not needed | nothing hidden |
| Rune Deck (look, reveal, recycle) | executor | **no** | not tracked at all: plans/09 9.NN4, in the trainer notes |

### 9.SS2 — the card id space

145 reserved ids are not enough. Radiance needs **169 to 174**:

| Gallery entries (192) | Count | New ids |
|---|---|---|
| numbered range 001..167 (144 in the file, 23 not shown yet) | 144 | 167 |
| alternate art `a` | 16 | 0 |
| over-numbered, second printings of range cards (8 legends, 1 battlefield, K'Sante) | 10 | 0 |
| over-numbered champions with no range entry of that name shown (Aphelios, Ezreal, Yone, Kayn, Sett) | 5 | 0 to 5 |
| `SP` champions, second printings | 5 | 0 |
| basic runes | 12 | 0 |
| tokens named that have no card: Bomb, 3-Might Mech | | 2 |

- `kCardVocabSize` / `kNumCardDefIds` 1100 -> **1150**: 195 reserved, 21 to 26 spare.
- Why the five champions are a range: Spiritforged and Unleashed have over-numbered cards of their own (233 ids of /221, 225 of /219).
- Derived: `kStateFeatureDim` 6442, `kCardKeys` 1151, `kNumSlots` 71,448, `NumDistinctActions` 73,162,752, `kCardEmbeddingRows` 1151, `kLayoutVersion` 2.
- **Engine against the gallery through Vendetta:** the 234 gallery entries with no engine card are all second printings (102 alternate art, 44 `*` variants, 74 over-numbered, 6 `SP`, 6 rune reprints) plus two game aids (Buff, XP Tracker). No game card is missing. One engine card is not in the gallery: Shadow Clone 955, a token.

## 3. Collision census

Same walk, old encoding and new.

| Walk | Decisions | Distinct legal intents | Before: decisions with a collision | Before: unreachable intents | After |
|---|---|---|---|---|---|
| fiora and yi_rebuild, all four pairings, 48 games | 14,690 | 52,010 | 5,682 (39%) | 22,760 (44%) | 0 / 0 |
| 49 decks built from the whole pool (682 cards), 98 games | 30,579 | 114,760 | 11,363 (37%) | 45,629 (40%) | 0 / 0 |

Before, colliding decisions by kind (repository decks / pool decks): mulligan 96 of 96 / 196 of 196; prompt answered with objects 4,418 of 5,458 / 7,971 of 9,564; main phase 1,011 of 2,430 / 2,835 of 5,673; prompt with objects or a value 27 / 154; chain priority 73 / 104; showdown focus 53 / 86; combat damage 4 / 17.

## 4. Why the search batches take the time they do

Measured with counters in the agent and the wrapper, the same instrumentation built into each commit (not in the tree).

| `ismcts:sims=20`, 8 games | Before | After |
|---|---|---|
| decisions asked / searched | 1,148 / 584 | 1,196 / 772 |
| actions at a searched decision, mean / max | 2.66 / 19 | 5.27 / 31 |
| actions per node visited | 2.06 | 4.15 |
| simulations / nodes | 11,680 / 11,596 | 15,440 / 15,327 |
| `ApplyAction`: calls / per simulation / each | 39,582 / 3.39 / 30.1 µs | 53,060 / 3.44 / 31.7 µs |
| `InfoSetId`: calls / each | 51,762 / 6.3 µs | 69,159 / 6.7 µs |
| ids (slots, twins, sort): calls / each / total | 104,088 / 108 ns / 0.011 s | 138,742 / 223 ns / 0.031 s |
| search time / per simulation | 2.07 s / 177 µs | 2.85 s / 185 µs |

- **38% longer because 32% more simulations**: 188 decisions that offered one action id are now searched.
- A simulation costs 5% more.
- **The id computation is 1% of the search.** Nothing there to cache; I tried a per-state cache earlier and it changed nothing.

| `mcts:sims=50`, 5 games | Before | After |
|---|---|---|
| decisions searched | 826 | 648 |
| actions at a decision, mean / max | 1.70 / 8 | 3.40 / 21 |
| simulations | 41,300 | 32,400 |
| `ApplyAction`: calls / per simulation / each | 104,362 / 2.53 / 19.2 µs | 70,400 / 2.17 / 27.5 µs |
| search time / per simulation | 2.84 s / 69 µs | 2.72 s / 84 µs |

- Shorter games, so fewer searches; each simulation costs 22% more because an applied action is 43% dearer on average.
- Clone and `ApplyAction` at a fixed position cost the same as before (`riftbound_clone_advance_microbench`).
- Not split further: how much of the 27.5 µs is the replay of a clone taken at a prompt.

## 5. Pinned values that changed (by design)

| Test | Old | New | Why |
|---|---|---|---|
| `StrongIds.HistoryTokensAndSlotsAreTheNumbersOfTheBinaryBeforeIdsWereTypes`, renamed `HistoryTokensAndSlotsArePinned` | tokens 3611872453840603442, 9183008626965481778, 7576281683122461517, 4557704407380131852, 3408968969468510209; slots 2354, 6989, 12 | tokens 7960844909697619989, 4496770096613617802, 4030418850666315000, 9050218172478652462, 2124341512652193793; slots 55317, 64760, 46 | new slot ranges; one more fingerprinted field; wider card ranges |
| `ActionVocab.VocabSizeIsReasonable`, now `SizesArePinned` | 1000 < `kVocabSize` < 100000 | `kCardKeys` 1151, `kNumSlots` 71448, `kTwinPlanes` 1024, `kNumActions` 73162752, eight verb offsets | exact numbers |
| `ActionVocab.HistoryTokenTellsApartIntentsThatShareASlot`, now `AHistoryTokenFindsItsIntentAmongTwins` | asserted that mulligans, moves and plays share a slot and decode returns the first | distinct ids for all; shared slots only for true twins | it pinned the defect |
| `ActionVocab.CollapsingPlayVariantsAliasIntentionally`, now `PlayTimingVariantsShareASlot` | same assertion | same | name only |
| `ActionVocab.MakeChoiceNoObject` | slot 0 of the prompt range | the "another value" slot | no "no object" slot exists |
| `RiftboundStateTest.ActionHistoryIsOneSlotPerAppliedAction` | `actionHistory()` == applied ids | == slots of the applied ids, and `History()` == the ids | an id is no longer a slot |
| `RiftboundGameTest.AdvertisedShapesMatchTheEngine` | `kVocabSize` | `kNumActions` | |
| `PerfSearchTest.FeaturesStillMatchTheStateQueriesTheyReplaced` | side columns Player1, Player2; ready-base offset `kStateFeatureDim - 24` | self, opponent; `ml::layout::kExtGlobals` | 1.9 |
| `PerfSearchHash.SkippingZeroRunsDoesNotChangeTheInfoSetId` | run lengths up to 6188 | 6192 and 6442 added | new vector lengths |
| comments in three test files | "reserved 956..1100 range" | "956..1150" | 9.SS2 |

No test deleted or weakened. `tests/test_action_vocab.cpp` went from 25 to 36 tests.

## 6. Trainer notes

`docs/checkpoint-breaking-2026-10.md` is the note the plan asked for: the id space and its derivation, the feature layout block by block with offsets, every action range that moved, the token format, the `Intent` field, the search measurements and a seven-step checklist. In short:

- `kStateFeatureDim` 6442, `kCardVocabSize` 1150; use `ml::layout::*` for offsets.
- `encodeAction` is gone: `actionIds(legal, state)` for ids, `actionSlot` for descriptions. Do not size anything by `NumDistinctActions` (73 million).
- Every stored `InfoSetId`, action id and history token is invalid.
- `Intent::chooseBattlefield` takes a `BattlefieldPoolIndex`.
- `recycleCards` has a third, defaulted parameter; cards put a card into a hand with `deck::putInHand`.
- `tools/collision_census.cpp` compiles against the new API; it is now also a non-default target of the engine build (`--target collision_census`), built and run here.

`CLAUDE.md` is updated: state features, action ids, action history, the knowledge tracker, the id kinds, adding a feature block.

## 7. Needs another stream

No `shared:` commits. Nothing left for another stream.

## 8. New findings

- **Hand reveals are announced as private.** Sabotage, Mindsplitter, Decree of Strength, Insightful Investigator and Ashe emit `CardRevealedEvent` with `revealed_to` = the caster, though the text says "They reveal their hand". In 1v1 the result is the same (the owner knows their hand). Not changed.
- **Rune Deck knowledge is not tracked** (already 9.NN4): Void Hatchling's look and Twisted Fate's reveal of a rune reach nobody.
- **Two of my new tests do not show the gap their comment names.** `KnowledgeReveals.AnOpponentsRevealedTopCardIsKnownToBoth` (Blind Fury) and `ALookAtTheTopCardsIsKnownBeforeTheOfferMadeFromThem` (Fate Weaver) pass before and after: they check the state after the card resolved, by when the old code had also told the tracker. They pin the behaviour; the red tests for 9.SS1 are the five hand-reveal and move tests and the two source scans.
- **"A hidden card played from facedown" has no new test.** It needs no hook (a public zone is read from the state) and the existing `KnowledgeTracker.ACardThatBecomesPublicIsForgottenByPrune` covers it.
- **Three tests loop over card ids `1..1100`** (`test_review_fix_sweep_triggers.cpp` twice, `test_review_fix_verify_engine.cpp`). Still right today (registered ids end at 955, test cards sit below 1100); they should read `ml::kCardVocabSize` when Radiance cards are added.

Play UI, deferred: none noticed.

## 9. How the work was run

- Both lane locks for every configure, build, full-suite run and timing run.
- **One merge commit has no trailers**: `7e70b67` ("Merge branch 'fix/integration' into fix/checkpoint") was made by `git merge --no-edit`; my amend to add the trailers was denied by the permission system and I did not retry. Every other commit carries both trailers.
- Commits: `395e932` 1.9; `67f609c` 9.RR2; `355aa98` 1.12; `a958050` docs; `5693844` 9.SS1; `67d6877` 9.SS2; `621799d` the `collision_census` target; `d150457` the search measurements in the doc; three merges of `fix/integration`.
- Scratch builds outside the tree (session scratchpad, `checkpoint/`): the baseline `2cb28f8` with search counters, and the checked-index build.
- The red run for 9.SS1 was made by reverting `src/` to the previous commit under the new test file and restoring it from a patch.
- No `rm`. One file removed with `git rm` (`plans/remaining/01-openspiel-search.md`, emptied).
