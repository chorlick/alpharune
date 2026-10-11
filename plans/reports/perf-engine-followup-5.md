# perf-engine — fifth follow-up report (plans/09 section NN)

## 1. Result

- Branch `fix/perf-engine`, head `6fa204b`, working tree clean. Verified against `fix/integration` `49714f7` (merged in).
- 9.NN1, 9.NN2 and 9.NN3 are fixed; 9.NN4 is not a defect and is pinned, with one small change to the Hatchling look. All four are ticked.
- Clean build (lane A lock, `-j3`, PCH-safe launcher), 0 warnings:
  - `riftbound_tests` 2975 passed, 0 failed (2964 before; 11 new).
  - `riftbound_openspiel_tests` 25 passed; clone-equivalence 10/10.
  - Smoke 200 games seed 20261010: P1 139 / P2 61 / 0 draws, 17 turns, 293 decisions — unchanged.
  - Search-microbench observation checksum `94739f84565d4d16` — unchanged.
  - MCTS batch rc 0; fault injection rc 1 as wanted.
  - Checked build (`-DRIFTBOUND_CHECK_BOARD_INDEX=ON`): suite 2975 passed, smoke 139/61/0, no throw.
  - ISMCTS: `sims=50` ×2 games 10/10 clean; `sims=20` ×8 games 10/10 clean.
- **Fixed-seed traces: one game differs, by one line.** Fiora vs Yi, seed 7 (and the batch replay that embeds it): the line `KILL: First Mate (id=66, 3M, dmg=3)` is gone and the sub-numbers of that decision's later lines shift by one. First Mate's death in that cleanup is replaced by Zhonya's Hourglass. The replacement is now applied as the batch begins (9.NN1), before the engine reaches the point where it logged `KILL:` for a unit it then did not kill. Every other line, the game's result and its ending are identical. All other traces are byte-identical.

Each new test was red in one build with the four fixes reverted, except the two named as pins.

## 2. 9.NN1 — replacements on simultaneous deaths

- **What it does now.** `beginDeathBatch` applies whatever replaces any member's death before any member dies, with all of them on the board. It then snapshots the members that still die. CR 373.1.a and 370.4.
- **Order.** The turn player's units first (CR 373.1), then the other player's; within a player, the order the units were listed. Each unit's own replacements are still offered in the order its controller chooses — `applyDeathReplacements` is unchanged and its existing tests pass.
- **A replaced member** is marked (`LastKnown::batch_replaced`); the kill call the loop then makes for it does nothing. A member that still dies is not asked about replacements a second time.
- **How the batch knows who dies — the choice.** The guard is told, by a tag on its constructor:
  - `DeathBatch(executor, units)`: every listed unit is killed.
  - `DeathBatch(executor, units, DeathBatch::LethalOnly)`: the guard keeps the listed units that are on the board with lethal damage, which is exactly the test the kill loops make.
  - I chose this over having each card collect the dying set because it is one token per site and leaves every kill loop as it was.
- **Sites.** There are 28 batches in 27 card files, not 23. I read all of them: 21 are `LethalOnly`, 7 kill everyone listed (Bottled Constellation 854, Cull the Weak 209 twice, Fox-Fire 253, The Ruination 742, Cataclysmic Duel 877, Warwick 159). The cleanup batch in `processLethalDamage` already lists exactly the dying.

Tests (`PerfEngineDeathBatch`):
- `ACleanupAppliesSorakaBeforeAnyoneDies_InBothKillOrders` — Soraka, Wanderer 550 and a smaller friendly unit die in one cleanup; the unit is recalled in both kill orders. Red before when Soraka was killed first.
- `ACardsBatchAppliesSorakaBeforeAnyoneDies` — the same through a card-style batch.
- `ALethalOnlyBatchLeavesTheUnitsThatDoNotDieAlone` — a listed unit that survives the damage keeps its "next time it would die" replacement unused.
- `EveryCardBatchSaysWhoDies` — source scan: a batch is `LethalOnly` exactly when its loop tests `hasLethalDamage`; counts 21 and 7.

## 3. 9.NN2 — the snapshot travels with the chain item

- `ChainItem::subject_as_it_left` holds the subject's `last_known`, copied when the ability triggers. It is set at every place `TriggerManager` sets `triggering_subject`; a reflexive trigger inherits it.
- Readers: `asItLeft(ctx, id)` and `controllerWhenItLeft(ctx, id)` in `card_helpers.h`. They return the item's snapshot when `id` is the resolving item's subject, and the object's latest snapshot otherwise.
- Converted: Draven 469, Unsung Hero 488, Loyal Poro 718, Lonely Poro 777 and 359, Baccai 865, Noxian Emissary 915, Kog'Maw 190.
- Left as they are, on purpose:
  - Solari Shrine 72 and Vanguard Helm 228 read in `triggerCondition`, which runs as the death is announced; the object's snapshot is that death's at that moment.
  - Hidden Blade 213 and Detonate 328 read immediately after their own kill.

Tests (`PerfEngineLookBack`):
- `ACardThatDiesTwice_EachDeathAbilityReadsItsOwnDeath` — a taken, buffed unit dies at battlefield 0, is replayed by its owner at battlefield 1, is stunned and dies again; both abilities are on the chain; each reads its own controller, Might, buff, Stunned and location.
- `KogMaw_DiesTwice_EachDeathknellDealsWhereThatDeathWas`.

## 4. 9.NN3 — rune-recycle sites

- `resource::payIn`, `GameEngine::advanceCostPayment` and `plainPay` now snapshot the rune, move it, announce it, then reset it — the order of every other leave path.
- The snapshot is `snapshotAsItLeaves` in `core/board_state.h`, for paths with no executor. `LastKnown::exhausted` is new.
- **Found on the way:** `plainPay` emitted no `LeftBoardEvent` at all, so "When you recycle a rune" never heard a rune recycled there. It does now. As reported last time, that site is reached only by an executor with no engine behind it.

Tests (`PerfEngineRuneLeaves`, with a test unit that has "When you recycle a rune"):
- `PaidThroughTheEnginesCostPayment` and `PaidThroughResourcePay` — the unit sees the rune exhausted and stunned at the announcement, and reads it as it left when its ability resolves.
- `PaidByAnEffectPlayWithoutAnEngine` — an event listener, since a bare executor has no trigger manager.

## 5. 9.NN4 — knowledge of the Rune Deck

**Verdict: not a defect. Rune order and identity cannot reach any observer.** Nothing was added to the tracker.

| Consumer | What it gets about a Rune Deck | Where that is guaranteed |
|---|---|---|
| `ObservationTensor` | its size only | `ml/feature_extractor.cpp` writes `ps.rune_deck.size()` and nothing else from it |
| `InfoSetId` | the same | it is a hash of the feature vector plus the decision |
| `KnowledgeView` | "identity unknown" for every object in a Rune Deck, both observers | `knowledge_tracker.cpp`, the `RuneDeck` return in `about` |
| Play server | the cards only when `reveal_hidden`; the count otherwise | `buildZoneJson`, `show_decks` |
| Log | see below | |

- Pins: `PerfEngineRuneDeck.TheOrderOfARuneDeckIsInNoObservationOrInfoSetId` (features and info-set id equal for both perspectives across reordered Rune Decks; a size change does show) and `.TheLookIsPrivateAndNothingAboutTheRuneDeckIsRecorded` (through the engine with Twisted Fate and a Hatchling).
- The play-server row is from reading the code; I did not write a test against the server.
- Comment at `EffectExecutor::lookBeforeReveal` names the three guarantees.

**One change, red before.** The Rune Deck look I added last time put the rune's name in the two option labels. Labels are not audience-limited, so I took the name out. The rune is now named on one line only, `RIFT_TRACE_PRIVATE` to the player who looked — the private form the Main Deck peek uses. The public "recycled the peeked top rune" line names nothing.

**Missing feature, for the stage 5 / v6 token layout:** per observer, the known top and bottom runes of each Rune Deck. A human at the table knows the rune they looked at and left, and both know the rune Twisted Fate revealed and put on the bottom; an agent cannot use either today. No feature, layout or vocabulary was changed.

## 6. New findings

1. **Order across units in a batch is not the controller's choice.** Within one player's dying units the batch applies replacements in listed order. CR 373 speaks of how replacements with the same controller are applied across simultaneous events; I did not settle whether that gives the controller the choice of which unit goes first. It matters only when one replacement can save one of several units (Zhonya's Hourglass with two dying units).
2. **The Main Deck Hatchling look gives a fair-play human nothing to read.** `lookBeforeReveal` for the Main Deck records the look in the tracker but writes no private line and its two options carry no label, and the play server does not read the tracker. I did not run the play UI to confirm what the player sees.
3. **`LethalOnly` uses `hasLethalDamage`**, as the card loops do. A unit that would die only by Elder Dragon's "any amount of your damage" rule is neither a member nor killed by those loops; it dies in the next cleanup, as before.
