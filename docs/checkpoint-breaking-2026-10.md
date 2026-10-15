# Checkpoint-breaking changes of the 2026-10 review fix run (stage 5)

Everything in the engine that changes what a model reads or writes: the state
feature vector, the action ids, history tokens and one `Intent` field. Nothing
recorded or trained against an earlier engine (a checkpoint, a replay of action
ids, a table keyed by `InfoSetId`) is valid after these commits. The other
engine changes of the run are in `docs/trainer-notes-2026-10.md`.

Review items: 1.9, 1.12, 9.RR2, 9.SS2, 9.UU1 (`plans/completed/`). The v6 interface
(`core/legal_action.h`, `ml/token_layout.h`) is not built yet; these fixes are
in the encoding the search agents run on today.

| | Before | After |
|---|---|---|
| `ml::kStateFeatureDim` | 6188 | **10944** (10932 of content, 12 of zero padding) |
| `ml::kCardVocabSize` / `openspiel::kNumCardDefIds` | 1100 | **2048** (section 0) |
| `RiftboundGame::NumDistinctActions()` | 13,371 (`kVocabSize`) | **130,174,976** (`kNumActions` = 127,124 slots x 1024 twin planes) |
| search-microbench observation checksum | `94739f84565d4d16` | `4a751d2371810e80` (`366dde83e658847e` after the feature reorder alone, `efc2aa14b545a08e` with the new action ids, `965b13284eb6dc5c` with 1150 card ids) |

## 0. The card id space: 1100 -> 2048

`kCardVocabSize` (and `openspiel::kNumCardDefIds`, `ml::kCardEmbeddingRows` - 1)
is the number of card ids every card-keyed block and range is sized by. Ids
1..955 are registered through Vendetta. It is a power of two now (user
decision, plans/09 item 9.UU1), with room for the two sets that are known, so
that neither release moves the layout again:

| Card ids | | Constant (`ml::`) |
|---|---|---|
| 1 .. 955 | registered, through Vendetta | `kLastRegisteredCardId` |
| 956 .. 1165 | reserved for Radiance: 210 ids (it needs 169 to 174, below) | `kRadianceFirstCardId`, `kRadianceLastCardId` |
| 1166 .. 1565 | reserved for Legacy, announced with about 400 cards | `kLegacyFirstCardId`, `kLegacyLastCardId` |
| 1566 .. 2048 | spare: 483 ids for later sets; the test-only cards of the test suite sit here | `kSpareFirstCardId` |

It was 1100 (145 reserved for Radiance: at least 24 short), then 1150 for a
day (9.SS2), now 2048.

How the count was derived from `cards/raw/gallery_raw.json` (192 Radiance entries):

| Gallery entries | Count | New ids |
|---|---|---|
| numbered range `RAD-001/167` .. `RAD-167/167` (144 of the 167 are in the file; 23 numbers are not shown yet) | 144 (of 167) | **167** |
| alternate art, code `RAD-nnna/167`: same number, name, cost and Might as the plain entry; the text drops the reminder text | 16 | 0 |
| over-numbered `RAD-168/167` .. `RAD-184/167`: 8 legends, 1 battlefield and 1 champion unit (K'Sante) are second printings of a card of the range (same name and text) | 10 | 0 |
| over-numbered champion units with no entry of that name in the range as shown (Aphelios 179, Ezreal 180, Yone 181, Kayn 182, Sett 183): second printings of cards among the 23 not shown, or cards of their own | 5 | **0 to 5** |
| `RAD-SP1/005` .. `SP5`: champion units, second printings | 5 | 0 |
| basic runes `RAD-R01` .. `R06` and `R01a` .. `R06a` | 12 | 0 (the six basic runes have ids) |
| tokens the set's cards name that have no card yet: Bomb (gear, with rules text), 3-Might Mech (unit). Gold, Recruit and Bird are named too and have ids or need none | - | **2** |
| | 192 | **169 to 174** |

- Earlier sets, ids against the size printed on the cards: Origins 298 of
  /298, Proving Grounds 24 of /24, Vendetta 166 of /166 (+ 2 tokens); but
  Spiritforged 233 of /221 (+ 1 token) and Unleashed 225 of /219 (+ 6 tokens):
  those two have over-numbered cards that are cards of their own (the Seals,
  the Poros), which is why the five champions above are counted as a range.
- Headroom in Radiance's 210: 36 ids at the high count, 41 at the low one. It
  covers the 23 cards not shown yet naming more tokens than any set so far
  has registered (Unleashed: 6) and two dozen over-numbered cards.
- The engine's 955 cards against the gallery through Vendetta (1,189 entries):
  the 234 entries with no engine card are all second printings (102 alternate
  art `a`, 44 `*` variants, 74 over-numbered, 6 Vendetta `SP`, 6 Vendetta rune
  reprints) and two game aids that are not cards (`UNL-T04` Buff, `UNL-T08` XP
  Tracker). No game card is missing. One engine card is not in the gallery:
  Shadow Clone 955, a token.

What grows with it: the five zone blocks of the feature vector (5 x 2048
floats, 10,240 of the vector's 10,944) and every card-keyed action range (2049
keys each). `ml::kCardEmbeddingRows` (the v6 token layout, not built yet) is
2049 and `ml::kLayoutVersion` 3.

What the larger vector costs, measured against the build with 1150 ids (three
alternating runs each, medians; fiora vs yi_rebuild):

| | 1150 ids (6,442 floats) | 2048 ids (10,944 floats) | |
|---|---|---|---|
| `InfoSetId` per call | 5.03 us | 6.52 us | **+30%** (+1.5 us) |
| of which hashing (`computeInfoSetId`) | 2.15 us | 3.45 us | +61% |
| `extractStateFeatures` | 2.68 us | 3.03 us | +13% |
| `ObservationTensor` | 3.19 us | 3.83 us | +20% |
| `ismcts:sims=20`, 8 games, seed 1 | 2.92 / 3.00 / 4.34 s | 2.89 / 3.01 / 3.16 s | within the run-to-run spread |
| `mcts:sims=50`, 5 games, seed 20261010 | 2.80 / 2.84 / 4.19 s | 2.65 / 2.68 / 2.83 s | within the spread (it never asks for the key) |
| random, 1000 games, 1 thread | 1.25 / 1.27 / 1.29 s | 1.23 / 1.24 / 1.27 s | unchanged (it never builds the vector) |

- `InfoSetId` is slower by more than its spread (about 0.25 us): 0.33 ns per
  added float. With 1600 ids (8,704 floats) it would have cost about 5.8 us,
  +15%.
- In the ISMCTS batch that is 69,000 calls x 1.5 us = 0.10 s of 2.9 s (3.5%;
  1600 ids: about 0.05 s), which the batch's own spread hides.
- Both search batches play the same games as with 1150 ids, decision for
  decision: the action ids are larger numbers in the same order.

## 1. State features (`src/ml/feature_extractor.h`)

### The one ordering convention

**Everything is relative to the perspective player.** Wherever a block has one
entry per player, the perspective player ("self") is first and the opponent
second. A field that names a player is 1 = self, 2 = opponent, 0 = nobody (or
a yes/no "is it self"). No field says Player1 or Player2.

Consequence, tested over the whole vector at every decision of real games
(`FeaturePerspectiveGames.SeatSwapGivesTheSameVectorAtEveryDecision`): the
vector of a position seen from one seat equals the vector of the seat-swapped
position seen from the other seat.

- **The absolute seat is not a feature.** Nothing in the rules depends on it.
- **Who goes first is a feature**: `layout::kGlobalSelfStarts` (index 3), as
  before.
- **Battlefield slots are not reordered**: slot i is battlefield i for both
  players, the same number the action slots use. Which player brought each
  slot to the game is the new block `kBfContributedBy`.

### What changed

| Block | Before | After |
|---|---|---|
| battlefield controller (field 1 of each battlefield header) | 0 none, 1 = Player1, 2 = Player2 | 0 none, 1 = self, 2 = opponent |
| the two 20-float side blocks of each battlefield | Player1, then Player2 | self, then opponent |
| the 96-float per-unit extended block | per battlefield: Player1, then Player2 | per battlefield: self, then opponent |
| `kBfContributedBy`, 4 floats, appended | did not exist | per slot: 0 nobody (token battlefield, empty slot), 1 self, 2 opponent |

Every other block was already relative and is unchanged, value for value.
With the reorder, one (battlefield, side, unit) slot is the same unit in the
base side block, the extended block and the Tier-2 block.

### Layout (offsets are `ml::layout::*`; use the names)

| Offset | Size | Block | Content |
|---|---|---|---|
| 0 | 10 | `kGlobals` | turn number, phase, self is turn player, self is the starting player, score difference (self - opp), chain size, showdown, closed state, additional turn, delayed-ability count |
| 10 | 8 | `kChainItems` | 4 chain slots x {source card id, controlled by self} |
| 18 | 70 | `kSelfPlayer` | the perspective player's block |
| 88 | 70 | `kOppPlayer` | the opponent's block; hand identities and costs are 0 |
| 158 | 196 | `kBattlefields` | 4 slots x 49: header (9), **self side (20), opponent side (20)** |
| 354 | 8 | `kChainTargets` | 4 chain slots x 2 target card ids |
| 362 | 4 | `kCostModTypes` | {next spell only, next unit only}: self, opp |
| 366 | 8 | `kScoredFlags` | 4 slots x {scored by self, by opp} this turn |
| 374 | 96 | `kUnitExt` | 4 slots x **{self, opp}** x 3 units x {combat designation, attachment card id, Might delta, temporary Might} |
| 470 | 10240 | `kZones` | 5 x 2048 card counts: self Main Deck, self trash, self banishment, opp trash, opp banishment |
| 10710 | 2 | `kPlayLockout` | can't play cards this turn: self, opp |
| 10712 | 192 | `kUnitT2` | 4 slots x {self, opp} x 3 units x {assault, shield, deflect, buffs, temporary buffs, Tank, Backline, Ganking} |
| 10904 | 24 | `kExtGlobals` | ready base units (self, opp); cost-modifier sum (self, opp); additional turns (self, opp); contested by self x4; combat staged x4; showdown staged x4; facedown age x4; focus holder (1 self / 0 opp / -1 none); battlefield count |
| 10928 | 4 | `kBfContributedBy` | **new**: who brought each battlefield slot |
| 10932 | 12 | `kPadding` | **new**: always 0. It carries nothing; it rounds the length up to a multiple of 16 floats (64 bytes). A later block goes before it |

- Battlefield header, 9 fields: card id, controller, contested, combat in
  progress, showdown in progress, facedown count, self is attacker (-1 outside
  combat), combat phase, token battlefield.
- Side block, 20 fields: unit count, total Might, total damage, exhausted,
  stunned, Tank, Backline, Ganking, Assault, Shield, Deflect counts (11); then
  the top-3 units by Might: card ids (3), Might (3), damage (3).
- Helpers: `layout::bf(slot)`, `layout::bfSide(slot, side)`,
  `layout::unitExt(slot, side, unit)`, `layout::unitT2(slot, side, unit)`,
  `layout::zone(k)`; side 0 is self.
- `extractStateFeatures` throws `std::logic_error` if a block does not start
  at its `layout` offset.

### Info-set keys

`RiftboundState::InfoSetId(p)` is built as before: the hash of p's own vector,
mixed with p's seat, with who is deciding and, for the decider only, with the
action ids on offer. `ObservationString(p)` is its hex. Every value changed
(new vector, new action ids). The key is a function of what p knows: unchanged
by any re-deal of the cards p has not been shown
(`RiftboundStateTest.InfoSetKeyIsAFunctionOfWhatThePlayerKnows`).

## 2. Action ids (`src/openspiel/action_vocab.h`)

### What an id is now

```
id   = slot + kNumSlots * twin          kNumSlots = 127124, kTwinPlanes = 1024
slot = actionIdSlot(id)                 what the action is made of
twin = actionIdTwin(id)                 which of the legal intents with that description
```

- **No two distinct legal intents of a decision share an id, and every legal
  intent has one.** Before, an id was a (coarser) slot and only the first
  intent of a slot could be chosen: in random games 39% of the decisions lost
  at least one option and 44% of the legal intents were unreachable.
- **The slot** (`actionSlot(intent, state)`) needs no legal list and means the
  same in every position: "play card 250 to battlefield 1", "choose the
  opponent's card 300", "answer 1".
- **The twin number** tells apart intents with one slot: two copies of a card,
  a spell offered once per target, a prompt offered once per rune. The intents
  of a decision that share a slot are ordered by `compareIntents` (field by
  field; objects by object id) and numbered 0, 1, 2, ... The first has
  `id == slot`.
- **What the order does not depend on**: the order the generator listed the
  options in, and anything the deciding player cannot know. So in every deal
  of the hidden cards that the player cannot tell apart, the same ids are on
  offer and each means the same choice. Both are checked at every decision of
  146 random games (`tests/test_action_ids.cpp`).
- **What a twin number is not**: stable across positions. It says "another
  action with this description"; two units of one card that swap places swap
  twin numbers. A policy over ids should treat plane k > 0 that way, or score
  slots and break ties itself. Per-legal-action scoring (v6) has no such limit.
- `kTwinPlanes` bounds the intents that can share a slot. The most the walk
  meets is 64. Past the bound `actionIds` gives `kNoActionId` and
  `RiftboundState::LegalActions()` throws `std::logic_error`.

### Slot ranges

`kCardKeys` = 2049: key 0 is "an object with no card id" (a token without a
card class), keys 1..2048 are card ids. `kPlaceKeys` = 8: 0 = none or a base,
1..7 = battlefield slot + 1.

| Verb | First slot | Size | Key |
|---|---|---|---|
| `Reserved` | 0 | 1 | never an action |
| `EndTurn`, `PassPriority`, `PassFocus`, `Concede` | 1, 2, 3, 4 | 1 each | |
| `PlayFirstDecision` | 5 | 2 | 0 second, 1 first |
| `MulliganDecision` | 7 | 38 | 0 keep; 1 + p: hand position p; 9..36: a pair of positions (i < j), row by row; 37: anything else |
| `ChooseBattlefield` | 45 | 8 | position in the player's battlefield list |
| `AssignCombatDamage` | 53 | 16 | number of units given damage (15 = 15 or more) |
| `StandardMove` | 69 | 16392 | unit card x 8 + destination |
| `Play` | 16461 | 65568 | ((card x 8) + where it lands) x 4 + variant: 0 normal, 1 alternative cost, 2 location cost, 3 not from hand or the Champion Zone |
| `HideCard` | 82029 | 16392 | card x 8 + battlefield |
| `ActivateAbility` | 98421 | 8196 | source card x 4 + ability index (own ability, no target in the intent) |
| `ActivateAtTarget` | 106617 | 2049 | source card (the intent names its target: Equip) |
| `ActivateGranted` | 108666 | 2049 | the card that grants the ability |
| `ChooseOwnObject` | 110715 | 2049 | card of the first chosen object, controlled by the decider |
| `ChooseOpposingObject` | 112764 | 2049 | ... controlled by the opponent |
| `ChooseNamedCard` | 114813 | 2049 | the card named |
| `ChooseValue` | 116862 | 17 | the integer answer 0..15; 16 = any other value |
| `PlaceOptionalTrigger`, `DeclineOptionalTrigger`, `PayTriggeredCost`, `DeclineTriggeredCost` | 116879, 118928, 120977, 123026 | 2049 each | source card |
| `SideboardSwap` | 125075 | 2049 | card taken out |

### Every range that moved or was added

| Before (first slot, size) | After | What changed in meaning |
|---|---|---|
| singletons 0..4, `PlayFirstDecision` 5 (2) | same | nothing |
| `MulliganDecision` 7 (3): how many cards | 7 (38) | which hand positions |
| `ChooseBattlefield` 10 (8): position + 1 | 45 (8) | the position itself (`Intent::battlefield_pool_index`) |
| `AssignCombatDamage` 18 (128): a hash of targets and amounts | 53 (16) | number of units hit; the distribution is the twin |
| `StandardMove` 146 (8): destination | 69 (16392) | unit card and destination |
| `Play` 154 (1100): card | 16461 (65568) | card, location, variant |
| `HideCard` 1254 (1100): card | 82029 (16392) | card and battlefield |
| `ActivateAbility` 2354 (4400): card x 4 + index, the index replaced by a hash of the target's card for a gear | 98421 (8196) | the index is always the index |
| | `ActivateAtTarget` 106617 (2049) | new: Equip |
| | `ActivateGranted` 108666 (2049) | new: a granted ability, by granting card (it shared the source's slot) |
| `MakeChoice` 6754 (1117): 0 no object or no card id, 1..1100 card of the first object or the named card, then 16 values | `ChooseOwnObject` 110715, `ChooseOpposingObject` 112764, `ChooseNamedCard` 114813 (2049 each), `ChooseValue` 116862 (17) | whose object; a named card apart from an object; a slot for values outside 0..15 |
| trigger verbs 7871, 8971, 10071, 11171 and `SideboardSwap` 12271 (1100 each) | 116879 .. 125075 (2049 each) | key 0 added; none of these intents is generated today |

An object with no card id used to take the first slot of its verb's range
(card 1's slot for a move, a play or an activation; the "no object" slot of a
prompt). It now has key 0 of every card-keyed range.

### API

| Before | After |
|---|---|
| `encodeAction(intent, state)` -> the action id | `actionSlot(intent, state)` -> the slot only |
| | `actionIds(legal, state)` -> the id of every intent of a legal list, parallel to it |
| | `actionIdOf(intent, legal, state)`, `actionIdSlot(id)`, `actionIdTwin(id)`, `makeActionId(slot, twin)`, `slotVerb(slot)`, `compareIntents(a, b)` |
| `decodeAction(slot, legal, state)` -> the first intent with that slot | `decodeAction(id, legal, state)` -> the one intent with that id |
| `kVocabSize` | `kNumSlots`, `kTwinPlanes`, `kNumActions` |
| `RiftboundState::LegalActions()`: one id per slot | one id per distinct legal intent (an intent the engine lists twice is one) |
| `RiftboundState::actionHistory()`: the ids applied | the **slots** of the intents applied; the ids of a game driven through `ApplyAction` are OpenSpiel's `History()` |

To apply an intent chosen from `legalIntents()`: `ApplyAction(actionIds(legalIntents(), engineState())[i])`.

## 3. History tokens (`GameState::action_history`)

A token is still `slot | fingerprint << 20`:

- bits 0..19: the slot of the chosen intent (`historyTokenSlot`; always a
  slot now, there is no "no slot" value);
- bits 20..62: a 43-bit fingerprint of every field `Intent::operator==`
  compares, object ids included, now with `battlefield_pool_index`.

**Every token value changed** (new slots, one more fingerprinted field). A
token does not hold the twin number: the fingerprint is what identifies the
intent in its legal list, and working the twin out for every decision of every
game cost 4% of random play. Replay is unchanged: `findHistoryToken`, lazy
`Clone()` and `syncToHistory` match whole tokens.

## 4. `Intent`

- **New field `Intent::battlefield_pool_index`**, of the new id kind
  `BattlefieldPoolIndex` (`core/types.h`): the setup decision
  `ChooseBattlefield` says which card of the player's battlefield list
  (`PlayerState::battlefield_pool`) they bring. It used to travel in
  `chosen_battlefield`.
- `Intent::chosen_battlefield` now means one thing: the battlefield slot a
  card is hidden at (`HideCard`).
- `Intent::chooseBattlefield(player, BattlefieldPoolIndex)`; `operator==` and
  the token fingerprint include the field.
- `ml::featurizeAction` (25 floats) is unchanged in layout and values.

## 5. What did not change

- Games played by agents that use neither the features nor the action ids:
  the 200-game random smoke (fiora vs yi_rebuild, seed 20261010) is P1 139 /
  P2 61 / 0 draws, 17 turns, 294 decisions, and 19 fixed-seed trace and replay
  files are byte-identical to the build before the stage.
- Card behaviour and rules.

## 6. What does change for the search agents

`MctsAgent` and `IsMctsAgent` now see every legal option. Fixed-seed batches
play different games than before; each is still a function of its seeds (run
twice, same output).

| Batch (fiora vs yi_rebuild, agent is P1, opponent random) | Before | After |
|---|---|---|
| `mcts:sims=50`, 5 games, seed 20261010 | P1 4 / P2 1, 296 decisions per game, 2.9 s | P1 5 / P2 0, 232 decisions per game, 2.8 s |
| `ismcts:sims=20`, 8 games, seed 1 | P1 5 / P2 3, 274 decisions per game, 2.1 s | P1 4 / P2 4, 290 decisions per game, 2.9 s |

Where the time goes, measured with counters in the agent and the wrapper (a
build of each commit with the same instrumentation; not in the tree):

| `ismcts:sims=20`, 8 games | Before | After |
|---|---|---|
| decisions the agent was asked | 1,148 | 1,196 |
| of which searched (2 or more actions) | 584 | 772 |
| actions at a searched decision, mean / max | 2.66 / 19 | 5.27 / 31 |
| actions per node visited, mean | 2.06 | 4.15 |
| simulations | 11,680 | 15,440 |
| tree nodes created | 11,596 | 15,327 |
| `ApplyAction` calls / per simulation / each | 39,582 / 3.39 / 30.1 us | 53,060 / 3.44 / 31.7 us |
| `InfoSetId` calls / each | 51,762 / 6.3 us | 69,159 / 6.7 us |
| `LegalActions` encoding (slots, twins, sort): calls / each / total | 104,088 / 108 ns / 0.011 s | 138,742 / 223 ns / 0.031 s |
| search time / per simulation | 2.07 s / 177 us | 2.85 s / 185 us |

- The batch takes 38% longer because it runs 32% more simulations: 188
  decisions that used to offer one action id (several legal intents on one
  slot, so the agent answered without searching) are now searched.
- A simulation costs 5% more: the actions applied are heavier on average and
  the info-set key hashes a longer list of offered ids.
- Working out the ids (slots, the twin order, the sort) is 1% of the search
  time. Nothing there was worth caching.

| `mcts:sims=50`, 5 games | Before | After |
|---|---|---|
| decisions searched | 826 | 648 |
| actions at a decision, mean / max | 1.70 / 8 | 3.40 / 21 |
| simulations | 41,300 | 32,400 |
| `ApplyAction` calls / per simulation / each | 104,362 / 2.53 / 19.2 us | 70,400 / 2.17 / 27.5 us |
| search time / per simulation | 2.84 s / 69 us | 2.72 s / 84 us |

- The games are shorter (232 decisions against 296), so fewer searches; each
  simulation costs 22% more because an applied action is 43% dearer on
  average. With one action per node most applies were forced single steps;
  the tree now applies plays and moves with their follow-up prompts. The cost
  of one clone and one `ApplyAction` at a fixed position is unchanged
  (`riftbound_clone_advance_microbench`: 4.4 / 75 / 33 us before, 4.5 / 74 /
  34 us after). I did not split the 27.5 us further (how much is the replay
  of a clone taken at a prompt).

Five and eight games say nothing about strength.

## 7. Checklist for the trainer

1. Size nothing by `NumDistinctActions()` (130 million). Score legal actions,
   or use `kNumSlots` (127,124) for a per-description head and the twin number
   as a tie-break.
2. Replace `encodeAction` with `actionIds` (ids) or `actionSlot` (descriptions).
3. Replace feature offsets written as numbers with `ml::layout::*`.
4. Treat the battlefield controller and both per-side blocks as self / opponent.
5. Drop every stored `InfoSetId`, action id and history token.
6. `Intent::chooseBattlefield` takes a `BattlefieldPoolIndex`.
7. `tools/collision_census.cpp` compiles against the new API and should now
   report 0 collisions for any deck.

## 8. Batch 5b (testing phase T2 / L2, 2026-10-15): nothing shown follows hidden order

A second break, after the 2026-10 one: the layout and the id range are the same
(`kStateFeatureDim` 10944, `kNumActions` 130,174,976), but the meaning of some
values changed, so vectors, action ids, history tokens and `InfoSetId`s recorded
before it are not comparable with those after. No trainer checkpoint existed.

**Why.** Object ids were handed out in deck-list order, and the deck lists are
grouped by card: the id of a hidden card nearly named it. Several things a
player is shown were functions of ids (below). Now ids carry no information and
nothing a player is shown is a function of them.

| What | Before | After |
|---|---|---|
| Object ids of Main Deck / Rune Deck cards | deck-list order (1, 2, 3, ... per list line) | a random order per game (`GameEngine::setIdLabelSalt`; a stream of its own, so the deal is unchanged) |
| Action slot of an object the decider cannot see (opponent's hand / facedown card, a deck card it was not shown) | its card id (L2) | key 0 ("no card the decider can see") |
| Twin number (`actionIds`, `actionIdOf`) | intents of one slot ordered field by field, objects by id | ordered by `compareIntentsAsSeen`: what the decider sees (`visibleDescription`, which includes which pending chain items name an object), then the public order of the objects named (hand / trash / banishment entry order), ids only between intents the decider sees as identical |
| A prompt answer of several objects (`MakeChoice`, e.g. "choose 2 runes to keep") | slot keyed by the first object listed | keyed by the object the decider sees first; the objects are a set |
| Mulligan slot positions | positions in the hand as dealt | positions in the hand sorted by card (`knowledge::sortedHand`) |
| Self hand cards and costs (`kSelfPlayer` hand block, 10 + 10) | hand order (as dealt / drawn) | sorted by card id (`knowledge::sortedHand`) |
| Units in the battlefield side blocks, the extended and Tier-2 unit blocks, base units | Might descending, ties in object-table (id) order; the Tier-2 block sorted separately, so with a tie one slot could describe two different units | one total order (`UnitOrderKey`: Might descending, then every field written), shared by all three blocks |
| Base gear ("top 2") | object-table order | the same total order |
| Chain item source / target card ids | always written | 0 for an object the perspective player cannot see |
| `featurizeAction` object card ids | always written | 0 for an object the decider cannot see |
| A simultaneous trigger batch on the chain and its order prompt | queued in object-table order; option value k = k-th queued | the batch is put in an order of what both players see (source card where both may see it, the trigger, the keyword ability) before it is ordered; option value k = k-th in that order |
| "Up to N" target sets (generic generator, Singularity) | listed in ascending id order | each set listed in the order of `visibleObjectDescription` |

`InfoSetId` is now computed by one function, `openspiel::infoSetIdFor`, which
`RiftboundState::InfoSetId` calls (same hash as before over the new vector).

**For the trainer.** Order the options of a decision by action id
(`RiftboundState::LegalActions`, or `actionIds`), never by the index in
`StepResult::legal`: the engine's list is the generator's order, which may
still follow object ids. Do not feed object ids, history-token fingerprints
or the numbers in log lines (`id=N`, `chain_item=N`) to a model.

Tests: `HiddenOrderLeak.*` (`tests/test_hidden_order_leak.cpp`),
`ActionVocab.APromptNamesAHiddenCardByNoCard`,
`ActionVocab.MulliganSlotsSayWhichCards` (hand order), and the seat-swap test
(its mirror now swaps the knowledge tracker's facts too).

## 9. Batch 5d (testing phase T1, T4, T6, N1, M2-M4, 2026-10-15): what a decision asks, per-observer queries

No change to the layout, `kStateFeatureDim`, `kNumActions`, the slot ranges or the
history token. No trainer checkpoint existed. What a trainer consumes that changed:

| What | Before | After |
|---|---|---|
| `StepResult` | kind, perspective, legal | + `prompt` (`core/prompt.h`): what the decision asks. Holds object ids: never a feature; read it through `describePrompt` |
| Decision API | — | `GameEngine::pendingPrompt()`, `AgentInterface::decide(state, legal, prompt)`, `RiftboundState::decisionPrompt()` / `describeDecision(player)` |
| Per-observer queries | `KnowledgeView` | + `KnowledgeView::knownRuneDeckTop` / `knownRuneDeckBottom` / `ownRuneDeckComposition`; `ObserverView` (`engine/observer_view.h`): public piles in full, the chain with its links, derived facts (Might, keywords, lethal, combat, resources, timing, the opponent's legal pool) |
| `openspiel::visibleDescription` / twin order | a card in a Main Deck: zone and card | + the slot the decider knows it at (two copies among looked-at cards are different choices): the twin number of such an option may differ |
| Engine games | — | other games than before for the same seed where things happen at once (trash order of units dying together, trigger batch order, recycle shuffles, the mulligan's set-aside order) and where M2 / M3 / M4 change what is offered or who is asked |

`describePrompt` (`engine/prompt_description.h`) is the decision's question as one
player may be shown it: `PromptKind` (a stable categorical; new kinds are added at
the end), who decides and who asks (relative: 1 = the observer), the asking card if
visible, which ability, where on the chain, the words, and the options in action-id
order with role, value, the objects as `visibleObjectDescription` has them, the named
card and the action id. Equal descriptions in positions a player cannot tell apart
(`HiddenOrderLeak.*`); one description never means two questions (`PromptCensus`).

Trainer notes: key a prompt by `PromptKind` + the asking card + `promptKey`, never by
the index in `StepResult::legal`; a `ChainItem` option is named by its place on the
chain (`chain_position`), not its id.
